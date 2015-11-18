/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_socket.c
 * Date: 2012-11-26
 *
 * Description: socket communication between monitor
 *              and healthcheck daemon
 *
 */

#include <arpa/inet.h>
#include <memory.h>
#include <assert.h>
#include "lc_mon.h"
#include "lc_queue.h"
#include "lc_mon_qctrl.h"
#include "lc_agent_msg.h"
#include "lc_header.h"
#include "cloudmessage.pb-c.h"

#define MAX_BUFFER_LENGTH (1 << 15)
#define AGENT_LCMON_PORT  20002
#define MAX_BACKLOG_VAL   100

pthread_mutex_t lc_dpq_host_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_host_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lc_dpq_vm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_vm_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lc_dpq_vgw_ha_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_vgw_ha_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lc_dpq_bk_vm_health_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_bk_vm_health_cond = PTHREAD_COND_INITIALIZER;

extern lc_dpq_host_queue_t lc_host_data_queue;
extern lc_dpq_vm_queue_t lc_vm_data_queue;

static int host_data_handler(char *data, char *addr, int qsize, int offset, int htype);
static int vm_data_handler(char *data, char *addr, int qsize, int offset, int htype);
static int vm_check_handler(char *data, char *addr, int qsize, int offset, int htype);

dpq_obj_t dpq_q_xen_msg[] = {
{DPQ_TYPE_HOST_INFO, HOST_QSIZE, sizeof(msg_host_info_response_t), host_data_handler, host_dpq_handler},
{DPQ_TYPE_VM_INFO, VM_QSIZE, sizeof(vm_result_t), vm_data_handler, vm_dpq_handler},
};

dpq_obj_t dpq_q_vmware_msg[] = {
{DPQ_TYPE_HOST_INFO, HOST_QSIZE, 0, host_data_handler, host_dpq_handler},
{DPQ_TYPE_VM_INFO, VM_QSIZE, 0, vm_data_handler, vm_dpq_handler},
};

dpq_obj_t dpq_q_vm_check_msg[] = {
{DPQ_TYPE_VM_CHECK, VM_QSIZE, 0, vm_check_handler, vm_dpq_ops_check_handler},
};

int data_sock_fd = -1;

static dpq_obj_t *vmware_msg_handler(char *buf, int len)
{
    Cloudmessage__Message *msg = NULL;
    dpq_obj_t *msg_obj = NULL;

    if (!buf) {
        return NULL;
    }

    msg = cloudmessage__message__unpack(NULL, len, (uint8_t *)buf);
    if (!msg) {
        return NULL;
    }

    if (msg->vm_stats_response) {
        msg_obj = &dpq_q_vmware_msg[DPQ_TYPE_VM_INFO];
    } else if (msg->host_info_response) {
        msg_obj = &dpq_q_vmware_msg[DPQ_TYPE_HOST_INFO];
    }

    cloudmessage__message__free_unpacked(msg, NULL);

    return msg_obj;
}

static void lc_mon_agent_handler(int magic, int type, char *data, char *addr, int len, int htype)
{
    dpq_obj_t *msg_obj = NULL;

    if (magic == MSG_MG_UI2MON) {
        msg_obj = &dpq_q_vm_check_msg[DPQ_TYPE_VM_CHECK];
    } else if(magic == MSG_MG_VCD2MON) {
        msg_obj = vmware_msg_handler(data, len);
        htype = LC_POOL_TYPE_VMWARE;
    } else {
        switch (type) {
          case MSG_HOST_INFO_RESPONSE:
               msg_obj = &dpq_q_xen_msg[DPQ_TYPE_HOST_INFO];
               htype = LC_POOL_TYPE_XEN;
               break;
          case MSG_VM_INFO_RESPONSE:
               msg_obj = &dpq_q_xen_msg[DPQ_TYPE_VM_INFO];
               htype = LC_POOL_TYPE_XEN;
               break;
#if 0
          case LC_MSG_VCD_REPLY_VMD_HOST_STATS:
               msg_obj = &dpq_q_vmware_msg[DPQ_TYPE_HOST_INFO];
               htype = LC_POOL_TYPE_VMWARE;
               break;
          case LC_MSG_VCD_REPLY_VMD_VM_STATS:
               msg_obj = &dpq_q_vmware_msg[DPQ_TYPE_VM_INFO];
               htype = LC_POOL_TYPE_VMWARE;
               break;
#endif
        }
    }
    if (msg_obj) {
        if (0 == msg_obj->element_len) {
            msg_obj->data_func(data, addr, msg_obj->qsize, len, htype);
        } else {
            msg_obj->data_func(data, addr, msg_obj->qsize, msg_obj->element_len, htype);
        }
    }
    return;
}

static int host_data_handler(char *data, char *addr, int qsize, int offset, int htype)
{
    lc_data_host_t *pdata = NULL;

    if (lc_host_dpq_len(&lc_host_data_queue) > qsize) {
        return -1;
    }

    pdata = (lc_data_host_t *)malloc(sizeof(lc_data_host_t));
    if (!pdata) {
        return -1;
    }
    memset(pdata, 0, sizeof(lc_data_host_t));
    if (htype == LC_POOL_TYPE_XEN) {
        memcpy((char *)&(pdata->u.host.host_info), data, offset);
        strcpy(pdata->u.host.host_addr, addr);
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        pdata->u.vmware_host.data = (char *)malloc(offset);
        if (NULL == pdata->u.vmware_host.data) {
            free(pdata);
            return -1;
        }
        memset(pdata->u.vmware_host.data, 0, offset);
        memcpy((char *)(pdata->u.vmware_host.data), data, offset);
        pdata->u.vmware_host.len = offset;
    }
    pdata->htype = htype;
    pthread_mutex_lock(&lc_dpq_host_mutex);
    lc_host_dpq_eq(&lc_host_data_queue, pdata);
    pthread_cond_signal(&lc_dpq_host_cond);
    pthread_mutex_unlock(&lc_dpq_host_mutex);

    return 0;
}

static int vm_data_handler(char *data, char *addr, int qsize, int offset, int htype)
{
    int count = 0;
    lc_data_vm_t *pdata = NULL;
    msg_vm_info_response_t *vm_info = NULL;

    if (lc_vm_dpq_len(&lc_vm_data_queue) > qsize) {
        return -1;
    }
    pdata = (lc_data_vm_t *)malloc(sizeof(lc_data_vm_t));
    if (!pdata) {
        return -1;
    }
    memset(pdata, 0, sizeof(lc_data_vm_t));
    if (htype == LC_POOL_TYPE_XEN) {
        vm_info = (msg_vm_info_response_t *)data;
        count = ntohl(vm_info->n);
        pdata->u_vm.vm = (vm_result_t *)malloc(count * offset);
        if (!pdata->u_vm.vm) {
            free(pdata);
            return -1;
        }
        memset((char *)pdata->u_vm.vm, 0, count * offset);
        memcpy((char *)pdata->u_vm.vm, (char *)&vm_info->data[0], offset * count);
        strcpy(pdata->host_addr, addr);
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        pdata->u_vm.vmware_vm.data = (char *)malloc(offset);
        if (NULL == pdata->u_vm.vmware_vm.data) {
            free(pdata);
            return -1;
        }
        memcpy((char *)(pdata->u_vm.vmware_vm.data), data, offset);
        pdata->u_vm.vmware_vm.len = offset;
    }
    pdata->vm_cnt = count;
    pdata->htype = htype;

    pthread_mutex_lock(&lc_dpq_vm_mutex);
    lc_vm_dpq_eq(&lc_vm_data_queue, pdata);
    pthread_cond_signal(&lc_dpq_vm_cond);
    pthread_mutex_unlock(&lc_dpq_vm_mutex);
    return 0;
#if 0
    int i, count;
    msg_vm_info_response_t *vm_info = (msg_vm_info_response_t *)data;
    count = ntohl(vm_info->n);
    for (i = 0; i < count; i++) {
        lc_data_vm_t *pdata = NULL;

        if (lc_vm_dpq_len(&lc_vm_data_queue) > qsize) {
            return -1;
        }

        pdata = (lc_data_vm_t *)malloc(sizeof(lc_data_vm_t));
        if (!pdata) {
            return -1;
        }
        memset(pdata, 0, sizeof(lc_data_vm_t));
        memcpy((char *)&pdata->vm, (char *)&vm_info->data[i], offset);

        pthread_mutex_lock(&lc_dpq_vm_mutex);
        lc_vm_dpq_eq(&lc_vm_data_queue, pdata);
        pthread_cond_signal(&lc_dpq_vm_cond);
        pthread_mutex_unlock(&lc_dpq_vm_mutex);
    }
    return 0;
#endif
}

static int vm_check_handler(char *data, char *addr, int qsize, int offset, int htype)
{
    int count = 0;
    lc_data_vm_t *pdata = NULL;

    if (lc_vm_dpq_len(&lc_vm_data_queue) > qsize) {
        return -1;
    }
    pdata = (lc_data_vm_t *)malloc(sizeof(lc_data_vm_t));
    if (!pdata) {
        return -1;
    }
    memset(pdata, 0, sizeof(lc_data_vm_t));
    pdata->u_vm.vm_ops = (lc_msg_t *)malloc(sizeof(lc_msg_t));
    if (!pdata->u_vm.vm_ops) {
        free(pdata);
        return -1;
    }
    memset((char *)pdata->u_vm.vm_ops, 0, sizeof(lc_mbuf_hdr_t) + offset);
    memcpy((char *)pdata->u_vm.vm_ops, data, sizeof(lc_mbuf_hdr_t) + offset);
    pdata->vm_cnt = count;
    pdata->htype = htype;

    pthread_mutex_lock(&lc_dpq_vm_mutex);
    lc_vm_dpq_eq(&lc_vm_data_queue, pdata);
    pthread_cond_signal(&lc_dpq_vm_cond);
    pthread_mutex_unlock(&lc_dpq_vm_mutex);
    return 0;
}

int get_data_sock_fd()
{
    if (data_sock_fd > 0) {
        return data_sock_fd;
    } else {
        return -1;
    }
}

/*create TCP socket as data session*/
static int lc_create_data_socket()
{
    int servfd, opt = 1;
    struct sockaddr_in servaddr;
    struct in_addr addr;

    data_sock_fd = -1;
    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LCMON_LOG(LOG_INFO, "%s create socket error!\n", __FUNCTION__);
        return -1;
    }
    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(AGENT_LCMON_PORT);
    if (inet_aton(lcmon_domain.local_ctrl_ip, &addr) == 0) {
        LCMON_LOG(LOG_INFO, "%s: local_ctrl_ip %s inet_aton error!\n", __FUNCTION__, lcmon_domain.local_ctrl_ip);
        close(servfd);
        return -1;
    }
    servaddr.sin_addr.s_addr = addr.s_addr;
    if (bind(servfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        LCMON_LOG(LOG_INFO, "%s bind to %d failure %s!\n",
                __FUNCTION__, AGENT_LCMON_PORT, strerror(errno));
        close(servfd);
        return -1;
    }

    if (listen(servfd, MAX_BACKLOG_VAL) < 0) {
        LCMON_LOG(LOG_INFO, "%s call listen failure!\n", __FUNCTION__);
        close(servfd);
        return -1;
    }

    data_sock_fd = servfd;
    return servfd;
}

int lc_send_hb_message()
{
    lc_mbuf_t msg;

    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic = MSG_MG_MON2CHK;
    msg.hdr.direction = MSG_DIR_MON2CHK;
    msg.hdr.type = LC_MSG_MON_REPLY_CHK_HEARTBEAT;
    msg.hdr.length = 0;

    lc_send_msg(&msg);

    return 0;
}

static void lc_data_msg_process(int fd, int htype)
{
    int sk_fd = -1, n;
    char *peer_ip = NULL;
    struct sockaddr_in sk_addr;
    char buf[MAX_BUFFER_LENGTH];
    char buf_data[MAX_RCV_BUF_SIZE];
    struct sockaddr_storage sa;
    socklen_t sa_len;
    lc_mbuf_hdr_t *hdr = NULL;
    socklen_t length = sizeof (sk_addr);
    int offset = 0;

    if (htype == LC_POOL_TYPE_XEN) {
        sk_fd = accept(fd, (struct sockaddr *)&sk_addr, &length);
        if (sk_fd < 0) {
            LCMON_LOG(LOG_INFO, " error comes when call accept!%s",strerror(errno));
            return;
        }

        peer_ip = inet_ntoa(sk_addr.sin_addr);
        memset(buf, 0, MAX_BUFFER_LENGTH);

        if ((n = recv(sk_fd, buf, MAX_BUFFER_LENGTH, 0)) > 0) {
            hdr = (lc_mbuf_hdr_t *)buf;
            hdr->magic = ntohl(hdr->magic);
            hdr->direction = ntohl(hdr->direction);
            hdr->type = ntohl(hdr->type);
            hdr->length = ntohl(hdr->length);
            hdr->seq = ntohl(hdr->seq);
            offset += n;
            while(offset < hdr->length + sizeof(hdr)) {
                if ((n = recv(sk_fd, buf + offset, MAX_BUFFER_LENGTH - offset, 0)) > 0) {
                     offset += n;
                } else {
                    break;
                }
            }
            LCMON_LOG(LOG_DEBUG, "%s[%d]: buf recv length:%d, data length:%d",
                             __FUNCTION__, hdr->type, offset, hdr->length);
            lc_mon_agent_handler(hdr->magic, hdr->type, (char *)(hdr + 1), peer_ip, hdr->length, htype);
        }
        close(sk_fd);
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        memset(buf_data, 0, MAX_RCV_BUF_SIZE);
        sa_len = sizeof(sa);
        if ((n = recvfrom(fd, buf_data, MAX_RCV_BUF_SIZE, 0,
                         (struct sockaddr *)&sa, &sa_len)) > 0) {
            hdr = (lc_mbuf_hdr_t *)buf_data;
            hdr->magic = ntohl(hdr->magic);
            hdr->direction = ntohl(hdr->direction);
            hdr->type = ntohl(hdr->type);
            hdr->length = ntohl(hdr->length);
            hdr->seq = ntohl(hdr->seq);
            offset += n;
            while(offset < hdr->length + sizeof(hdr)) {
                if ((n = recvfrom(fd, buf_data + offset, MAX_RCV_BUF_SIZE - offset, 0,
                                              (struct sockaddr *)&sa, &sa_len)) > 0) {
                     offset += n;
                } else {
                    break;
                }
            }
            LCMON_LOG(LOG_DEBUG, "%s[%d]: buf recv length:%d, data length:%d",
                             __FUNCTION__, hdr->type, offset, hdr->length);
            lc_mon_agent_handler(hdr->magic, hdr->type, (char *)(hdr + 1), peer_ip, hdr->length, htype);
        }
    }
    return;
}

void lc_hb_msg_process(int fd)
{
    char buffer[sizeof(lc_mbuf_t)];
    struct sockaddr_storage sa;
    socklen_t sa_len;
    int nrecv;
    struct lc_msg_head *msg = (struct lc_msg_head *) buffer;

    sa_len = sizeof(sa);
    memset(buffer, 0, sizeof(lc_mbuf_t));
    nrecv = recvfrom(fd, buffer, sizeof(lc_mbuf_t), 0,
        (struct sockaddr *)&sa, &sa_len);

    if (nrecv < 0) {
        return;
    }

    if (!is_lc_msg(msg)) {
        return;
    }

    if (!is_msg_to_mon(msg)) {
        return;
    }

    if (msg->type == LC_MSG_CHK_REQUEST_MON_HEARTBEAT) {
        lc_send_hb_message();
    }
}

int lc_sock_init()
{
    lc_sock.sock_rcv_agent = lc_create_data_socket();
    if (lc_sock.sock_rcv_agent < 0) {
        return -1;
    }

    /* vmware daemon socket init */
    lc_sock.sock_rcv_vcd = -1;
#if 0
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        LCMON_LOG(LOG_ERR, "%s create vcd socket error.\n", __FUNCTION__);
    } else {
        memset(&sa_mon_vcd, 0, sizeof(sa_mon_vcd));
        sa_mon_vcd.sin_family = AF_INET;
        sa_mon_vcd.sin_port = htons(LC_VCD_PORT);
        sa_mon_vcd.sin_addr.s_addr = inet_addr(LC_VCD_ADDRESS);
        if (connect(fd, (struct sockaddr*)&sa_mon_vcd, sizeof(sa_mon_vcd)) == -1) {
            LCMON_LOG(LOG_ERR, "%s connect vcd socket error.\n", __FUNCTION__);
        } else {
            lc_sock.sock_rcv_vcd = fd;
        }
    }
#endif
    return 0;
}

int lc_mon_init_bus()
{
    int res;

    res = lc_bus_init();
    if (res) {
        LCMON_LOG(LOG_ERR, "init lc_bus failed");
        return -1;
    }

    res = lc_bus_init_consumer(0, LC_BUS_QUEUE_LCMOND);
    if (res) {
        LCMON_LOG(LOG_ERR, "init lc_bus consumer failed");
        lc_bus_exit();
        return -1;
    }

    return 0;
}

int lc_bus_msg_process(amqp_connection_state_t *conn)
{
    lc_mbuf_hdr_t *hdr = NULL;
    int htype = LC_POOL_TYPE_UNKNOWN;
    amqp_envelope_t envelope;
    amqp_frame_t frame;
    amqp_message_t message;
    char buf[LC_BUS_BUFFER_SIZE] = {0};
    int buf_len;

    header_t new_hdr;
    int is_new_hdr = 0;
    int pb_hdr_len = get_message_header_pb_len();

    buf_len = lc_bus_consume_message(conn,
            0, &envelope, &frame, &message, buf, sizeof(buf));
    if (buf_len >= pb_hdr_len &&
            unpack_message_header((uint8_t *)buf, pb_hdr_len, &new_hdr) == 0 &&
            pb_hdr_len + new_hdr.length == buf_len) {
        /* this is a protobuf message */
        is_new_hdr = 1;
        LCMON_LOG(LOG_DEBUG, "%s: msg in sender=%d type=%d len=%d\n",
                __FUNCTION__, new_hdr.sender, new_hdr.type, new_hdr.length);

    } else {
        hdr = (lc_mbuf_hdr_t *)buf;

        if (buf_len < sizeof(lc_mbuf_hdr_t) ||
            buf_len < sizeof(lc_mbuf_hdr_t) + hdr->length) {
            LCMON_LOG(LOG_ERR, "%s: msg consume failed, nrecv=%d\n",
                    __FUNCTION__, buf_len);
            return -1;
        }
    }
    if (is_new_hdr) {
        lc_mon_agent_handler(MSG_MG_VCD2MON, 0, (char *)(buf + pb_hdr_len), NULL, new_hdr.length, htype);
    } else {
        hdr->magic = hdr->magic;
        hdr->type = hdr->type;
        hdr->length = hdr->length;
        hdr->seq = hdr->seq;
        lc_mon_agent_handler(hdr->magic, hdr->type, (char *)hdr, NULL, hdr->length, htype);
        LCMON_LOG(LOG_DEBUG, "%s: msg in magic=%x type=%d len=%d\n",
                __FUNCTION__, hdr->magic, hdr->type, hdr->length);
    }

    return 0;
}

void lc_main_process()
{
    int max_fd = 1;
    fd_set fds;
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int lc_mon_conn_fd = amqp_get_sockfd(*conn);

    max_fd = lc_mon_conn_fd;
    max_fd = MAX(lc_sock.sock_rcv_agent, max_fd);
    max_fd = MAX(lc_sock.sock_rcv_chk, max_fd);
    ++max_fd;
    LCMON_LOG(LOG_DEBUG, "%s: sock_fd %d, %d, %d", __FUNCTION__,
                          lc_mon_conn_fd, lc_sock.sock_rcv_agent,
                                           lc_sock.sock_rcv_chk);

    while (lc_bus_data_in_buffer(conn)) {
        if (lc_bus_msg_process(conn)) {
            break;
        }
    }

    while (1) {
        /* in case of bus connection reset */
        conn = lc_bus_get_connection();
        lc_mon_conn_fd = amqp_get_sockfd(*conn);
        if (lc_mon_conn_fd + 1 > max_fd) {
            max_fd = lc_mon_conn_fd + 1;
        }

        FD_ZERO(&fds);
        FD_SET(lc_mon_conn_fd, &fds);
        FD_SET(lc_sock.sock_rcv_agent, &fds);

        if (lc_sock.sock_rcv_chk > 0) {
            FD_SET(lc_sock.sock_rcv_chk, &fds);
        }

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {
            LC_MON_CNT_INC(lc_msg_in);

            if (FD_ISSET(lc_mon_conn_fd, &fds)) {
                do {
                    if (lc_bus_msg_process(conn)) {
                        break;
                    }
                } while (lc_bus_data_in_buffer(conn));
            }

            if (FD_ISSET(lc_sock.sock_rcv_agent, &fds)) {
                lc_data_msg_process(lc_sock.sock_rcv_agent, LC_POOL_TYPE_XEN);
            }

            if (lc_sock.sock_rcv_chk > 0 && FD_ISSET(lc_sock.sock_rcv_chk, &fds)) {
                LC_MON_CNT_INC(lc_chk_msg_in);
                lc_hb_msg_process(lc_sock.sock_rcv_chk);
            }
#if 0
            if (lc_sock.sock_rcv_vcd > 0 && FD_ISSET(lc_sock.sock_rcv_vcd, &fds)) {
                lc_data_msg_process(lc_sock.sock_rcv_vcd, LC_POOL_TYPE_VMWARE);
            }
#endif
        }
    }

    return;
}

int lc_bus_lcmond_publish_unicast(char *msg, int len, uint32_t queue_id)
{
    return lc_bus_publish_unicast((char *)msg, len, queue_id);
}
