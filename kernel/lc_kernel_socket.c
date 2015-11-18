#include "lc_kernel.h"
#include "lc_queue.h"

#include "agent_msg_host_info.h"
#include "agent_msg_vm_info.h"
#include "agent_enum.h"

#include "lc_header.h"

extern pthread_mutex_t lc_msg_mutex;
extern pthread_cond_t lc_msg_cond;

char remain_buf[sizeof(lc_msg_t)];
int remain_buf_len = 0;
int remain_buf_flag = 0;

int lc_ker_msg_sanity_check(lc_mbuf_hdr_t *hdr)
{
    if (hdr &&
        ((hdr->magic == MSG_MG_DRV2KER) ||
         (hdr->magic == MSG_MG_APP2KER) ||
         (hdr->magic == MSG_MG_VCD2KER) ||
         (hdr->magic == MSG_MG_VCD2APP) ||
         (hdr->magic == MSG_MG_KER2APP) ||
         (hdr->magic == MSG_MG_UI2KER) ||
         (hdr->magic == MSG_MG_UI2APP))) {
        return 1;
    }
    return 0;
}

int lc_bus_msg_process(amqp_connection_state_t *conn)
{
    int pb_hdr_len = get_message_header_pb_len();

    lc_msg_t *msg = NULL;
    lc_mbuf_hdr_t *hdr = NULL;

    amqp_envelope_t envelope;
    amqp_frame_t frame;
    amqp_message_t message;
    header_t new_hdr;
    int is_new_hdr = 0;

    char buf[LC_BUS_BUFFER_SIZE] = {0};
    int buf_len;

    buf_len = lc_bus_consume_message(conn,
            0, &envelope, &frame, &message, buf, sizeof(buf));

    if (buf_len >= pb_hdr_len &&
            unpack_message_header((uint8_t *)buf, pb_hdr_len, &new_hdr) == 0 &&
            pb_hdr_len + new_hdr.length == buf_len) {
        /* this is a protobuf message */
        is_new_hdr = 1;
        LCPD_LOG(LOG_DEBUG, "The message in sender=%d type=%d len=%d\n",
                 new_hdr.sender, new_hdr.type, new_hdr.length);

    } else {
        hdr = (lc_mbuf_hdr_t *)buf;
        if (buf_len < sizeof(lc_mbuf_hdr_t) ||
            buf_len < sizeof(lc_mbuf_hdr_t) + hdr->length) {
            LC_KER_CNT_INC(lc_msg_rcv_err);
            LCPD_LOG(LOG_ERR, "The message consume failed, nrecv=%d\n",buf_len);
            return -1;
        }
        LCPD_LOG(LOG_DEBUG, "The message in magic=%x type=%d len=%d\n",
                  hdr->magic, hdr->type, hdr->length);

    }

    LC_KER_CNT_INC(lc_valid_msg_in);
    msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
    if (!msg) {
        LC_KER_CNT_INC(lc_buff_alloc_error);
        return -1;
    }

    if (is_new_hdr) {
        hdr = (lc_mbuf_hdr_t *)&(msg->mbuf);

        hdr->type = LC_MSG_NOTHING;
        if (new_hdr.sender == HEADER__MODULE__VMCLOUDADAPTER) {
            hdr->magic = MSG_MG_VCD2KER;
            hdr->type = LC_MSG_DUMMY;
        } else if (new_hdr.sender == HEADER__MODULE__TALKER) {
            hdr->magic = MSG_MG_UI2KER;
        } else {
            LCPD_LOG(LOG_ERR, "Header is invalid, can not parse pb message "
                    "from sender=%d now.\n", new_hdr.sender);
        }
        /*hdr->type = new_hdr.type;*/
        hdr->length = new_hdr.length;
        hdr->seq = new_hdr.seq;

        memcpy(hdr + 1, buf + pb_hdr_len, hdr->length);
    } else {
        memcpy(&msg->mbuf, buf, sizeof(lc_mbuf_hdr_t) + hdr->length);
    }

    pthread_mutex_lock(&lc_msg_mutex);
    LC_KER_CNT_INC(lc_msg_enqueue);
    lc_msgq_eq(&lc_data_queue, msg);
    pthread_cond_signal(&lc_msg_cond);
    pthread_mutex_unlock(&lc_msg_mutex);

    return 0;
}

void lc_agent_msg_process(int fd)
{
    char buffer[MAX_RCV_BUF_SIZE];
    struct sockaddr_in sa;
    socklen_t sa_len;
    int sock_cli;
    ssize_t nrecv;
    int i, offset;

    lc_mbuf_hdr_t *a_hdr;
    msg_host_report_t *host_reports;
    msg_vm_report_t   *vm_reports;

    lc_msg_t *msg;
    lc_mbuf_hdr_t *hdr;
    struct msg_host_ip_opt *hi_opt;
    struct msg_vm_opt *vm_opt;
    struct msg_vm_opt_start *m_start;

    memset(&sa, 0, sizeof(sa));
    sa_len = sizeof(sa);
    sock_cli = accept(fd, (struct sockaddr *)&sa, &sa_len);

    if (sock_cli < 0) {
        LCPD_LOG(LOG_DEBUG, "The accept() error from %s: %s\n",
                 inet_ntoa(sa.sin_addr),
                strerror(errno));
        return;
    }

    LCPD_LOG(LOG_DEBUG, "Accept socket %d from %s\n",
             sock_cli, inet_ntoa(sa.sin_addr));

    memset(buffer, 0, MAX_RCV_BUF_SIZE);
    nrecv = recv(sock_cli, buffer, MAX_RCV_BUF_SIZE, 0);
    if (nrecv >= MAX_RCV_BUF_SIZE ||
        nrecv < (int)sizeof(lc_mbuf_hdr_t)) {

        LCPD_LOG(LOG_ERR, "Invalid report message size: %zd, %s.\n",
                  nrecv, strerror(errno));
        LC_KER_CNT_INC(lc_invalid_msg_in);
        goto close;
    }
    for (offset = nrecv; offset < MAX_RCV_BUF_SIZE; offset += nrecv) {
        nrecv = recv(sock_cli, (void *)buffer + offset,
                MAX_RCV_BUF_SIZE - offset, 0);
        if (nrecv <= 0) {
            break;
        }
    }

    a_hdr = (lc_mbuf_hdr_t *)buffer;
    a_hdr->magic = ntohl(a_hdr->magic);
    a_hdr->direction = ntohl(a_hdr->direction);
    a_hdr->type = ntohl(a_hdr->type);
    a_hdr->length = ntohl(a_hdr->length);
    a_hdr->seq = ntohl(a_hdr->seq);

    if (!agent_message_sanity_check(a_hdr->magic)) {
        LCPD_LOG(LOG_ERR, "The livecloud version error, version=%x/%x.\n",
                  a_hdr->magic & 0xFFFF0000, AGENT_VERSION_MAGIC);
        LC_KER_CNT_INC(lc_invalid_msg_in);
        goto close;
    }

    if (lc_msgq_len(&lc_data_queue) > MAX_DATAQ_LEN) {
        LCPD_LOG(LOG_ERR, "Drop report msg, magic=%x type=%d len=%d.\n",
                  a_hdr->magic, a_hdr->type, a_hdr->length);
        LC_KER_CNT_INC(lc_thread_msg_drop);
        goto close;
    }

    LCPD_LOG(LOG_DEBUG, "Process report msg, magic=%x type=%d len=%d.\n",
              a_hdr->magic, a_hdr->type, a_hdr->length);

    if (offset < a_hdr->length + (int)sizeof(lc_mbuf_hdr_t)) {
        LCPD_LOG(LOG_ERR, "Invalid report msg, len=%d/%d.\n",
                offset, a_hdr->length + (int)sizeof(lc_mbuf_hdr_t));
        LC_KER_CNT_INC(lc_invalid_msg_in);
        goto close;
    }

    LC_KER_CNT_INC(lc_valid_msg_in);

    if (a_hdr->type == MSG_HOST_REPORT) {
        host_reports = (msg_host_report_t *)(a_hdr + 1);
        msg_host_report_ntoh(host_reports);

        for (i = 0; i < host_reports->n; ++i) {
            if (host_reports->data[i].event != AGENT_REPORT_EVENT_BOOT) {
                LCPD_LOG(LOG_ERR, "Host_%d %s has unknown event %d.\n",
                          i, host_reports->data[i].host_ip,
                        host_reports->data[i].event);
                continue;
            }

            msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
            if (!msg) {
                LC_KER_CNT_INC(lc_buff_alloc_error);
                goto close;
            }
            memset(msg, 0, sizeof(lc_msg_t));

            hdr = &(msg->mbuf.hdr);
            hdr->magic = a_hdr->magic;
            hdr->seq = a_hdr->seq + i;
            hdr->type = LC_MSG_HOST_BOOT;
            hdr->length = sizeof(struct msg_host_ip_opt);

            hi_opt = (struct msg_host_ip_opt *)(hdr + 1);
            snprintf(hi_opt->host_ip, MAX_HOST_ADDRESS_LEN,
                    "%s", inet_ntoa(sa.sin_addr));

            pthread_mutex_lock(&lc_msg_mutex);
            LC_KER_CNT_INC(lc_msg_enqueue);
            lc_msgq_eq(&lc_data_queue, msg);
            pthread_cond_signal(&lc_msg_cond);
            pthread_mutex_unlock(&lc_msg_mutex);

            LCPD_LOG(LOG_DEBUG, "Forward msg, magic=%x type=%d len=%d.\n",
                      hdr->magic, hdr->type, hdr->length);
        }
    } else if (a_hdr->type == MSG_VM_REPORT) {
        vm_reports = (msg_vm_report_t *)(a_hdr + 1);
        msg_vm_report_ntoh(vm_reports);

        for (i = 0; i < vm_reports->n; ++i) {
            if (vm_reports->data[i].event != AGENT_REPORT_EVENT_BOOT &&
                vm_reports->data[i].event != AGENT_REPORT_EVENT_DOWN) {
                LCPD_LOG(LOG_ERR, "vm_%d %s has unknown event %d.\n",
                          i, vm_reports->data[i].name_label,
                        vm_reports->data[i].event);
                continue;
            }

            msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
            if (!msg) {
                LC_KER_CNT_INC(lc_buff_alloc_error);
                goto close;
            }
            memset(msg, 0, sizeof(lc_msg_t));

            hdr = &(msg->mbuf.hdr);
            hdr->magic = MSG_MG_UI2KER;
            hdr->seq = a_hdr->seq + i;
            if (vm_reports->data[i].event == AGENT_REPORT_EVENT_BOOT) {
                hdr->type = LC_MSG_VDEVICE_BOOT;
            } else if (vm_reports->data[i].event == AGENT_REPORT_EVENT_DOWN) {
                hdr->magic = MSG_MG_UI2KER;
                hdr->type = LC_MSG_VDEVICE_DOWN;
            }
            hdr->length = sizeof(struct msg_vm_opt) + sizeof(
                    struct msg_vm_opt_start);

            vm_opt = (struct msg_vm_opt *)(hdr + 1);
            m_start = (struct msg_vm_opt_start *)(vm_opt->data);
            snprintf(vm_opt->vm_name, LC_NAMESIZE,
                    "%s", vm_reports->data[i].name_label);
            snprintf(m_start->vm_server, MAX_HOST_ADDRESS_LEN,
                    "%s", inet_ntoa(sa.sin_addr));

            pthread_mutex_lock(&lc_msg_mutex);
            LC_KER_CNT_INC(lc_msg_enqueue);
            lc_msgq_eq(&lc_data_queue, msg);
            pthread_cond_signal(&lc_msg_cond);
            pthread_mutex_unlock(&lc_msg_mutex);

            LCPD_LOG(LOG_DEBUG, "Forward msg, magic=%x type=%d len=%d.\n",
                      hdr->magic, hdr->type, hdr->length);
        }
    } else {
        LCPD_LOG(LOG_ERR, "The message header type unknown type=%d.\n",
                  a_hdr->type);
        goto close;
    }

close:
    close(sock_cli);
    sock_cli = -1;
    return;
}

int lc_get_app_index(void)
{
    return 1;
}

int lc_bus_init_by_kernel(void)
{
    int res;

    res = lc_bus_init();
    if (res) {
        LCPD_LOG(LOG_ERR, "Init lc_bus failed!\n");
        return -1;
    }

    res = lc_bus_init_consumer(0, LC_BUS_QUEUE_LCPD);
    if (res) {
        LCPD_LOG(LOG_ERR, "Init lc_bus consumer failed!\n");
        lc_bus_exit();
        return -1;
    }

    return 0;
}

int lc_sock_init()
{
    int fd, index, ret;
    int flag = 1;
    struct in_addr addr;

    index = lc_get_app_index();
    if (!index || (index > MAX_APP_NUM)) {
        return -1;
    }

    /* agent listen socket, exit if error occurs */

    lc_sock.sock_agent = -1;
    if (inet_aton(local_ctrl_ip, &addr) == 0) {
        LCPD_LOG(LOG_ERR, "%s: local_ctrl_ip %s inet_aton error!\n", __FUNCTION__, local_ctrl_ip);
        return -1;
    }
    memset(&sa_ker_agent, 0, sizeof(sa_ker_agent));
    sa_ker_agent.sin_family = AF_INET;
    sa_ker_agent.sin_addr.s_addr = addr.s_addr;
    sa_ker_agent.sin_port = htons(AGENT_REPORT_LCPD_PORT);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LCPD_LOG(LOG_ERR, "The ker_agent socket() error %d.\n",fd);
        return -1;
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (ret < 0) {
        LCPD_LOG(LOG_ERR, "The ker_agent setsockopt() error %d.\n",ret);
        close(fd);
        return -1;
    }
    ret = bind(fd, (struct sockaddr *)&sa_ker_agent,
            sizeof(struct sockaddr_in));
    if (ret < 0) {
        LCPD_LOG(LOG_ERR, "The ker_agent bind() error %d.\n", ret);
        close(fd);
        return -1;
    }
    ret = listen(fd, 5);
    if (ret < 0) {
        LCPD_LOG(LOG_ERR, "The ker_agent listen() error %d.\n",ret);
        close(fd);
        return -1;
    }
    lc_sock.sock_agent = fd;

    return 0;
}
void lc_main_process()
{
    int max_fd;
    fd_set fds;
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int lc_kernel_conn_fd = amqp_get_sockfd(*conn);

    max_fd = lc_kernel_conn_fd;
    max_fd = MAX(lc_sock.sock_agent, max_fd);
    ++max_fd;
    LCPD_LOG(LOG_DEBUG, "%d, %d > %d\n",
            lc_kernel_conn_fd, lc_sock.sock_agent, max_fd);
    while (lc_bus_data_in_buffer(conn)) {
        LC_KER_CNT_INC(lc_bus_msg_in);
        if (lc_bus_msg_process(conn) != 0) {
            break;
        }
    }

    while (1) {
        /* in case of bus connection reset */
        conn = lc_bus_get_connection();
        lc_kernel_conn_fd = amqp_get_sockfd(*conn);
        if (lc_kernel_conn_fd + 1 > max_fd) {
            max_fd = lc_kernel_conn_fd + 1;
        }

        FD_ZERO(&fds);
        FD_SET(lc_kernel_conn_fd, &fds);
        FD_SET(lc_sock.sock_agent, &fds);

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {

            LC_KER_CNT_INC(lc_msg_in);

            if (FD_ISSET(lc_kernel_conn_fd, &fds)) {
                do {
                    LC_KER_CNT_INC(lc_bus_msg_in);
                    if (lc_bus_msg_process(conn) != 0) {
                        break;
                    }
                } while (lc_bus_data_in_buffer(conn));
            }

            if (FD_ISSET(lc_sock.sock_agent, &fds)) {
                LC_KER_CNT_INC(lc_agent_msg_in);
                lc_agent_msg_process(lc_sock.sock_agent);
            }
        }
    }
    return;
}

int lc_bus_lcpd_publish_unicast(char *msg, int len, uint32_t queue_id)
{
    return lc_bus_publish_unicast((char *)msg, len, queue_id);
}
