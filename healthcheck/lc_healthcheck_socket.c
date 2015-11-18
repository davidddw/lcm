#include "lc_healthcheck.h"
#include "lc_queue.h"
#include "message/msg_ker2vmdrv.h"

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
        LC_LOG(LOG_ERR, "call socket() function error!");
        return -1;
    }

    val = bind(fd_server, (struct sockaddr *)sa_addr, sizeof(struct sockaddr_un));
    if (val < 0) {
        LC_LOG(LOG_ERR, "call socket() function error!");
        return -2;
    }

    return fd_server;
}

int lc_sock_init()
{
    int fd;

    memset(&sa_chk_rcv, 0, sizeof(sa_chk_rcv));
    strcpy(sa_chk_rcv.sun_path, SOCK_CHK_RCV);
    sa_chk_rcv.sun_family = AF_UNIX;
    unlink(SOCK_CHK_RCV);

    fd = create_socket(&sa_chk_rcv);
    if (fd < 0) {
        LC_LOG(LOG_ERR, "create socket error for SOCK_KER_RCV_DRV");
        unlink(SOCK_CHK_RCV);
        lc_sock.sock_rcv = -1;
    } else {
        lc_sock.sock_rcv = fd;
    }

    memset(&sa_chk_snd_lcm, 0, sizeof(sa_chk_snd_lcm));
    strcpy(sa_chk_snd_lcm.sun_path, SOCK_LCM_RCV_APP);
    sa_chk_snd_lcm.sun_family = AF_UNIX;

    memset(&sa_chk_snd_ker, 0, sizeof(sa_chk_snd_ker));
    strcpy(sa_chk_snd_ker.sun_path, SOCK_KER_RCV_APP);
    sa_chk_snd_ker.sun_family = AF_UNIX;

    memset(&sa_chk_snd_drv, 0, sizeof(sa_chk_snd_drv));
    strcpy(sa_chk_snd_drv.sun_path, SOCK_DRV_RCV_APP);
    sa_chk_snd_drv.sun_family = AF_UNIX;

    memset(&sa_chk_snd_mon, 0, sizeof(sa_chk_snd_mon));
    strcpy(sa_chk_snd_mon.sun_path, SOCK_MON_RCV_APP);
    sa_chk_snd_mon.sun_family = AF_UNIX;

    return 0;
}

int lc_chk_msg_sanity_check(lc_mbuf_hdr_t *hdr)
{
    if (hdr && 
        ((hdr->magic == MSG_MG_UI2CHK)  ||
         (hdr->magic == MSG_MG_KER2CHK) ||
         (hdr->magic == MSG_MG_DRV2CHK) ||
         (hdr->magic == MSG_MG_MON2CHK))) {
        return 1;
    }
    return 0;
}

int lc_send_msg(lc_mbuf_t *msg)
{
    int nsend, len;
    int ret = 0;

    if (!msg) {
        return -1;
    }

    LC_LOG(LOG_INFO, "%s in dir= %08x type=%d len=%d seq=%d\n", 
               __FUNCTION__, msg->hdr.direction, msg->hdr.type, msg->hdr.length, msg->hdr.seq);
    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.direction) {
        case MSG_DIR_CHK2UI:
            nsend = sendto(lc_sock.sock_rcv, msg, len, 0,
                (struct sockaddr *)&sa_chk_snd_lcm, sizeof(struct sockaddr_un));
            if (nsend < 0) {
                LC_LOG(LOG_INFO, "%s err \n", __FUNCTION__);
                ret = -1;
            }
            break;
        case MSG_DIR_CHK2KER:
            nsend = sendto(lc_sock.sock_rcv, msg, len, 0,
                (struct sockaddr *)&sa_chk_snd_ker, sizeof(struct sockaddr_un));
            if (nsend < 0) {
                LC_LOG(LOG_INFO, "%s err \n", __FUNCTION__);
                ret = -1;
            }
            break;
        case MSG_DIR_CHK2DRV:
            nsend = sendto(lc_sock.sock_rcv, msg, len, 0,
                (struct sockaddr *)&sa_chk_snd_drv, sizeof(struct sockaddr_un));
            if (nsend < 0) {
                LC_LOG(LOG_INFO, "%s err \n", __FUNCTION__);
                ret = -1;
            }
            break;
        case MSG_DIR_CHK2MON:
            nsend = sendto(lc_sock.sock_rcv, msg, len, 0,
                (struct sockaddr *)&sa_chk_snd_mon, sizeof(struct sockaddr_un));
            if (nsend < 0) {
                LC_LOG(LOG_INFO, "%s err \n", __FUNCTION__);
                ret = -1;
            }
            break;
        default:
            return -1;

    }
    return ret;
}

int lc_send_lcm_heartbeat()
{
    int ret;
    lc_mbuf_t msg;

    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic     = MSG_MG_CHK2UI;
    msg.hdr.direction = MSG_DIR_CHK2UI;
    msg.hdr.type      = LC_MSG_CHK_REQUEST_LCM_HEARTBEAT;
    msg.hdr.length    = 0;

    ret = lc_send_msg(&msg);
    LC_HB_CNT_INC(lcm);

    return ret;
}

int lc_send_kernel_heartbeat()
{
    int ret;
    lc_mbuf_t msg;

    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic     = MSG_MG_CHK2KER;
    msg.hdr.direction = MSG_DIR_CHK2KER;
    msg.hdr.type      = LC_MSG_CHK_REQUEST_KER_HEARTBEAT;
    msg.hdr.length    = 0;

    ret = lc_send_msg(&msg);
    LC_HB_CNT_INC(kernel);

    return ret;
}

int lc_send_vmdriver_heartbeat()
{
    int ret;
    lc_mbuf_t msg;

    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic     = MSG_MG_CHK2DRV;
    msg.hdr.direction = MSG_DIR_CHK2DRV;
    msg.hdr.type      = LC_MSG_CHK_REQUEST_DRV_HEARTBEAT;
    msg.hdr.length    = sizeof(struct msg_req_drv_vm_opt);

    ret = lc_send_msg(&msg);
    LC_HB_CNT_INC(vmdriver);

    return ret;
}

int lc_send_monitor_heartbeat()
{
    int ret;
    lc_mbuf_t msg;

    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic     = MSG_MG_CHK2MON;
    msg.hdr.direction = MSG_DIR_CHK2MON;
    msg.hdr.type      = LC_MSG_CHK_REQUEST_MON_HEARTBEAT;
    msg.hdr.length    = sizeof(struct msg_req_drv_vm_opt);

    ret = lc_send_msg(&msg);
    LC_HB_CNT_INC(monitor);

    return ret;
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
        LC_CHK_CNT_INC(lc_thread_msg_drop);
        return;
    }

    if (nrecv == (MAX_RCV_BUF_SIZE - offset)) {
        LC_CHK_CNT_INC(lc_remain_buf_left);
        buff_full = 1;
    }

    if (nrecv < sizeof(lc_mbuf_hdr_t)) {
        LC_CHK_CNT_INC(lc_msg_rcv_err);
        return;
    } else {
        buffer_left = nrecv + offset;
        remain_buf_flag = 0;
        while (buffer_left) {
            if (buff_full && (buffer_left < sizeof(lc_msg_t))) {
                LC_CHK_CNT_INC(lc_remain_buf_left);
                memcpy(remain_buf, p, buffer_left);
                remain_buf_len = buffer_left;
                remain_buf_flag = 1;
                return;
            }
            hdr = (lc_mbuf_hdr_t *)p;

            LC_LOG(LOG_INFO, "%s: type=%d \n", __FUNCTION__, hdr->type);

            if ((buffer_left < sizeof(lc_mbuf_hdr_t)) || 
                    !lc_chk_msg_sanity_check(hdr)) {
                LC_CHK_CNT_INC(lc_invalid_msg_in);
                return;
            }
            if (buffer_left < (hdr->length + sizeof(lc_mbuf_hdr_t))) {
                LC_CHK_CNT_INC(lc_invalid_msg_in);
                return;
            }

            LC_CHK_CNT_INC(lc_valid_msg_in);
            msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));

            if (!msg) {
                LC_CHK_CNT_INC(lc_buff_alloc_error);
                return;
            }

            LC_LOG(LOG_DEBUG, "%s: buffer left=%d \n", __FUNCTION__, buffer_left);
            memcpy(&msg->mbuf, p, sizeof(lc_mbuf_hdr_t) + hdr->length);

            buffer_left -= (sizeof(lc_mbuf_hdr_t) + hdr->length);
            p += (sizeof(lc_mbuf_hdr_t) + hdr->length);

            pthread_mutex_lock(&lc_msg_mutex);
            LC_CHK_CNT_INC(lc_msg_enqueue);
            lc_msgq_eq(&lc_data_queue, msg);
            pthread_cond_signal(&lc_msg_cond);
            pthread_mutex_unlock(&lc_msg_mutex);
        }
    }
    return;
}

void lc_main_process()
{
    int max_fd;
    fd_set fds;

    max_fd = lc_sock.sock_rcv + 1;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(lc_sock.sock_rcv, &fds);
        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {
            LC_CHK_CNT_INC(lc_msg_in);
            if (FD_ISSET(lc_sock.sock_rcv, &fds)) {
                lc_msg_process(lc_sock.sock_rcv);
            }
        }
    }

    return;
}
