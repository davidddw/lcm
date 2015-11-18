#include <arpa/inet.h>
#include <memory.h>
#include "lc_vm.h"
#include "lc_queue.h"
#include "agexec.h"

#include "lc_header.h"

extern pthread_mutex_t lc_msg_mutex;
extern pthread_cond_t lc_msg_cond;

char remain_buf[sizeof(lc_msg_t)];
int remain_buf_len = 0;
int remain_buf_flag = 0;

int create_socket(struct sockaddr_un *sa_addr)
{
    int fd_server;
    int val;

    fd_server = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd_server < 0) {
        VMDRIVER_LOG(LOG_ERR, "Call socket() function error!\n");
        return -1;
    }

    val = bind(fd_server, (struct sockaddr *)sa_addr, sizeof(struct sockaddr_un));
    if (val < 0) {
        VMDRIVER_LOG(LOG_ERR, "call socket() function error!\n");
        return -2;
    }

    return fd_server;
}

int lc_vm_msg_sanity_check(lc_mbuf_hdr_t *hdr)
{
    if (hdr &&
        ((hdr->magic == MSG_MG_KER2DRV) ||
         (hdr->magic == MSG_MG_UI2DRV) ||
         (hdr->magic == MSG_MG_VCD2DRV))) {
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
        VMDRIVER_LOG(LOG_DEBUG, "Message in sender=%d type=%d len=%d\n",
                new_hdr.sender, new_hdr.type, new_hdr.length);

    } else {
        hdr = (lc_mbuf_hdr_t *)buf;
        if (buf_len < sizeof(lc_mbuf_hdr_t) ||
            buf_len < sizeof(lc_mbuf_hdr_t) + hdr->length) {
            LC_VM_CNT_INC(lc_msg_rcv_err);
            VMDRIVER_LOG(LOG_ERR, "Message consume failed, nrecv=%d\n", buf_len);
            return -1;
        }
        VMDRIVER_LOG(LOG_DEBUG, "Message in magic=%x type=%d len=%d\n",
                      hdr->magic, hdr->type, hdr->length);
    }

    LC_VM_CNT_INC(lc_valid_msg_in);
    msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
    if (!msg) {
        LC_VM_CNT_INC(lc_buff_alloc_error);
        return -1;
    }

    if (is_new_hdr) {
        hdr = (lc_mbuf_hdr_t *)&(msg->mbuf);

        hdr->type = LC_MSG_NOTHING;
        if (new_hdr.sender == HEADER__MODULE__VMCLOUDADAPTER) {
            hdr->magic = MSG_MG_VCD2DRV;
            hdr->type = LC_MSG_DUMMY;
        } else if (new_hdr.sender == HEADER__MODULE__TALKER) {
            hdr->magic = MSG_MG_UI2DRV;
        } else {
            VMDRIVER_LOG(LOG_ERR, "Header is invalid, can not parse pb message "
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
    LC_VM_CNT_INC(lc_msg_enqueue);
    lc_msgq_eq(&lc_data_queue, msg);
    pthread_cond_signal(&lc_msg_cond);
    pthread_mutex_unlock(&lc_msg_mutex);

    return 0;
}

void lc_msg_process(int fd)
{
    char buffer[MAX_RCV_BUF_SIZE];
    struct sockaddr_storage sa;
    socklen_t sa_len;
    ssize_t nrecv;
    int buffer_left;
    int offset = 0;
    lc_msg_t *msg;
    lc_mbuf_hdr_t *hdr;
    char *p = buffer;
    int buff_full = 0;

    sa_len = sizeof(sa);
    memset(buffer, 0, MAX_RCV_BUF_SIZE);

    if (remain_buf_flag) {
        memcpy(p, remain_buf, remain_buf_len);
        offset = remain_buf_len;

        remain_buf_len = 0;
        remain_buf_flag = 0;

    }
    nrecv = recvfrom(fd, p + offset, MAX_RCV_BUF_SIZE - offset, 0,
            (struct sockaddr *)&sa, &sa_len);

    if (lc_msgq_len(&lc_data_queue) > MAX_DATAQ_LEN) {
        LC_VM_CNT_INC(lc_thread_msg_drop);
        return;
    }

    if (nrecv == (MAX_RCV_BUF_SIZE - offset)) {
        LC_VM_CNT_INC(lc_remain_buf_left);
        buff_full = 1;
    }

    if (nrecv < sizeof(lc_mbuf_hdr_t)) {
        LC_VM_CNT_INC(lc_msg_rcv_err);
        return;
    } else {
        buffer_left = nrecv + offset;
        remain_buf_flag = 0;
        while (buffer_left) {
            if (buff_full && (buffer_left < sizeof(lc_msg_t))) {
                LC_VM_CNT_INC(lc_remain_buf_left);
                memcpy(remain_buf, p, buffer_left);
                remain_buf_len = buffer_left;
                remain_buf_flag = 1;
                return;
            }
            hdr = (lc_mbuf_hdr_t *)p;

            VMDRIVER_LOG(LOG_DEBUG, "magic=%x type=%d len=%d\n",
                    hdr->magic, hdr->type, hdr->length);

            if ((buffer_left < sizeof(lc_mbuf_hdr_t)) ||
                    !lc_vm_msg_sanity_check(hdr)) {
                LC_VM_CNT_INC(lc_invalid_msg_in);
                return;
            }
            if (buffer_left < (hdr->length + sizeof(lc_mbuf_hdr_t))) {
                LC_VM_CNT_INC(lc_invalid_msg_in);
                return;
            }

            LC_VM_CNT_INC(lc_valid_msg_in);
            msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));

            if (!msg) {
                LC_VM_CNT_INC(lc_buff_alloc_error);
                return;
            }

            VMDRIVER_LOG(LOG_DEBUG, "Buffer left=%d \n", buffer_left);
            memcpy(&msg->mbuf, p, sizeof(lc_mbuf_hdr_t) + hdr->length);

            buffer_left -= (sizeof(lc_mbuf_hdr_t) + hdr->length);
            p += (sizeof(lc_mbuf_hdr_t) + hdr->length);

            pthread_mutex_lock(&lc_msg_mutex);
            LC_VM_CNT_INC(lc_msg_enqueue);
            lc_msgq_eq(&lc_data_queue, msg);
            pthread_cond_signal(&lc_msg_cond);
            pthread_mutex_unlock(&lc_msg_mutex);
        }
    }
    return;
}

void lc_agexec_msg_process(int fd)
{
    int sock_cli;
    char buffer[MAX_RCV_BUF_SIZE];
    struct sockaddr_in sa;
    socklen_t sa_len;
    ssize_t nrecv;
    int offset;
    lc_msg_t *msg;
    lc_mbuf_hdr_t *hdr;
    agexec_msg_hdr_t *a_hdr;
    int len;

    memset(&sa, 0, sizeof(sa));
    sa_len = sizeof(sa);
    sock_cli = accept(fd, (struct sockaddr *)&sa, &sa_len);

    if (sock_cli < 0) {
        VMDRIVER_LOG(LOG_DEBUG, "The accept() error from %s: %s\n",inet_ntoa(sa.sin_addr),
                strerror(errno));
        return;
    }

    VMDRIVER_LOG(LOG_DEBUG, "Accept socket %d from %s\n",
                 sock_cli, inet_ntoa(sa.sin_addr));

    memset(buffer, 0, MAX_RCV_BUF_SIZE);
    nrecv = recv(sock_cli, buffer, MAX_RCV_BUF_SIZE, 0);
    if (nrecv >= MAX_RCV_BUF_SIZE ||
        nrecv < (int)sizeof(agexec_msg_hdr_t)) {

        VMDRIVER_LOG(LOG_ERR, "Invalid message size: %zd, %s.\n",
                      nrecv, strerror(errno));
        LC_VM_CNT_INC(lc_invalid_msg_in);
        close(sock_cli);
        sock_cli = -1;
        return;
    }
    for (offset = nrecv; offset < MAX_RCV_BUF_SIZE; offset += nrecv) {
        nrecv = recv(sock_cli, (void *)buffer + offset,
                MAX_RCV_BUF_SIZE - offset, 0);
        if (nrecv <= 0) {
            break;
        }
    }

    a_hdr = (agexec_msg_hdr_t *)buffer;
    agexec_msg_hdr_ntoh(a_hdr);

    if (!agexec_message_sanity_check(a_hdr)) {
        VMDRIVER_LOG(LOG_ERR, "The livecloud version error, version=%x/%x\n",
                     a_hdr->action_type & 0xFFFF0000,AGEXEC_ACTION_MAGIC);
        LC_VM_CNT_INC(lc_invalid_msg_in);
        close(sock_cli);
        sock_cli = -1;
        return;
    }

    if (lc_msgq_len(&lc_data_queue) > MAX_DATAQ_LEN) {
        LC_VM_CNT_INC(lc_thread_msg_drop);
        close(sock_cli);
        sock_cli = -1;
        return;
    }

    LC_VM_CNT_INC(lc_valid_msg_in);

    len = a_hdr->length + sizeof(lc_mbuf_hdr_t);
    msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
    if (!msg) {
        LC_VM_CNT_INC(lc_buff_alloc_error);
        close(sock_cli);
        sock_cli = -1;
        return;
    }
    memset(msg, 0, sizeof(lc_msg_t));

    hdr = &(msg->mbuf.hdr);
    hdr->magic = MSG_MG_UI2DRV;
    hdr->seq = a_hdr->seq;
    hdr->length = a_hdr->length;
    if (a_hdr->object_type == AGEXEC_OBJ_VM) {
        switch (a_hdr->action_type) {
        case  AGEXEC_ACTION_ADD:
            hdr->type = LC_MSG_VM_ADD_RESUME;
            break;
        case  AGEXEC_ACTION_START:
            hdr->type = LC_MSG_VM_START_RESUME;
            break;
        case  AGEXEC_ACTION_STOP:
            hdr->type = LC_MSG_VM_STOP_RESUME;
            break;
        case  AGEXEC_ACTION_DEL:
            hdr->type = LC_MSG_VM_DEL_RESUME;
            break;
        }
        agexec_vm_ntoh((agexec_msg_t *)a_hdr);
        memcpy(msg->mbuf.data, a_hdr + 1, a_hdr->length);

        VMDRIVER_LOG(LOG_DEBUG, "magic=%x type=%d len=%d\n",
                     hdr->magic, hdr->type, hdr->length);

        pthread_mutex_lock(&lc_msg_mutex);
        LC_VM_CNT_INC(lc_msg_enqueue);
        lc_msgq_eq(&lc_data_queue, msg);
        pthread_cond_signal(&lc_msg_cond);
        pthread_mutex_unlock(&lc_msg_mutex);
    }

    close(sock_cli);
    sock_cli = -1;
    return;
}

int lc_bus_init_by_vmdriver()
{
    int res;

    res = lc_bus_init();
    if (res) {
        VMDRIVER_LOG(LOG_ERR, "The lc_bus_init()  failed, res=%d\n",res);
        return -1;
    }

    res = lc_bus_init_consumer(0, LC_BUS_QUEUE_VMDRIVER);
    if (res) {
        VMDRIVER_LOG(LOG_ERR, "Theinit lc_bus_init_consumer() failed,res=%d\n",res);
        lc_bus_exit();
        return -1;
    }

    return 0;
}

int lc_sock_init()
{
    int fd, flag = 1, ret;
    struct in_addr addr;

    /* agexec listen socket, exit if error occurs */

    lc_sock.sock_agexec = -1;
    if (inet_aton(local_ctrl_ip, &addr) == 0) {
        VMDRIVER_LOG(LOG_ERR, "%s: local_ctrl_ip %s inet_aton error!\n", __FUNCTION__, local_ctrl_ip);
        return -1;
    }
    memset(&sa_vm_agexec, 0, sizeof(sa_vm_agexec));
    sa_vm_agexec.sin_family = AF_INET;
    sa_vm_agexec.sin_addr.s_addr = addr.s_addr;
    sa_vm_agexec.sin_port = htons(VMDRIVER_AGEXEC_PORT);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
       VMDRIVER_LOG(LOG_ERR, "The vm_agexec socket() error %d.\n",fd);
        return -1;
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (ret < 0) {
       VMDRIVER_LOG(LOG_ERR, "The vm_agexec setsockopt() error %d.\n",ret);
        close(fd);
        return -1;
    }
    ret = bind(fd, (struct sockaddr *)&sa_vm_agexec,
            sizeof(struct sockaddr_in));
    if (ret < 0) {
        VMDRIVER_LOG(LOG_ERR, "The vm_agexec bind() error %d.\n",ret);
        close(fd);
        return -1;
    }
    ret = listen(fd, 5);
    if (ret < 0) {
        VMDRIVER_LOG(LOG_ERR, "The vm_agexec listen() error %d.\n", ret);
        close(fd);
        return -1;
    }
    lc_sock.sock_agexec = fd;

    return 0;
}

void lc_main_process()
{
    int max_fd;
    fd_set fds;
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int lc_vmdriver_conn_fd = amqp_get_sockfd(*conn);

    max_fd = lc_vmdriver_conn_fd;
    max_fd = MAX(lc_sock.sock_agexec, max_fd);
    ++max_fd;
    VMDRIVER_LOG(LOG_DEBUG, "sock_fd %d, %d -> %d\n",
                lc_vmdriver_conn_fd, lc_sock.sock_agexec, max_fd);

    while (lc_bus_data_in_buffer(conn)) {
        LC_VM_CNT_INC(lc_bus_msg_in);
        if (lc_bus_msg_process(conn)) {
            break;
        }
    }

    while (1) {
        /* in case of bus connection reset */
        conn = lc_bus_get_connection();
        lc_vmdriver_conn_fd = amqp_get_sockfd(*conn);
        if (lc_vmdriver_conn_fd + 1 > max_fd) {
            max_fd = lc_vmdriver_conn_fd + 1;
        }

        FD_ZERO(&fds);
        FD_SET(lc_vmdriver_conn_fd, &fds);
        FD_SET(lc_sock.sock_agexec, &fds);

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {
            LC_VM_CNT_INC(lc_msg_in);

            if (FD_ISSET(lc_vmdriver_conn_fd, &fds)) {
                do {
                    LC_VM_CNT_INC(lc_bus_msg_in);
                    if (lc_bus_msg_process(conn)) {
                        break;
                    }
                } while (lc_bus_data_in_buffer(conn));
            }

            if (FD_ISSET(lc_sock.sock_agexec, &fds)) {
                LC_VM_CNT_INC(lc_agexec_msg_in);
                lc_agexec_msg_process(lc_sock.sock_agexec);
            }
        }
    }

    return;
}

int lc_bus_vmdriver_publish_unicast(char *msg, int len, uint32_t queue_id)
{
    return lc_bus_publish_unicast((char *)msg, len, queue_id);
}
