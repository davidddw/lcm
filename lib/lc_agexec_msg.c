/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_agexec_msg.c
 * Author Name: Xiang Yang
 * Date: 2012-3-22
 *
 * Description: Send and receive Agexec messages
 *
 */

#include <arpa/inet.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>

#include "agexec.h"
#include "agexec_inner.h"
#include "lc_agexec_msg.h"

#define RECV_TIMEOUT      60

int AGEXEC_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *AGEXEC_LOG_LEVEL_P = &AGEXEC_LOG_DEFAULT_LEVEL;

char *AGEXEC_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define AGEXEC_LOG(level, format, ...)  \
    if(level <= *AGEXEC_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), AGEXEC_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

static int lc_agexec_sock_init(int *p_sock, char *ip)
{
    struct sockaddr_in sa_data;
    int ret;

    if (!p_sock || !ip) {
        return AGEXEC_FALSE;
    }

    memset(&sa_data, 0, sizeof(sa_data));
    sa_data.sin_family = AF_INET;
    inet_aton(ip, &(sa_data.sin_addr));
    sa_data.sin_port = htons(AGEXEC_CTRL_PORT);

    *p_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*p_sock < 0) {
        AGEXEC_LOG(LOG_ERR, "%s: socket() error\n", __FUNCTION__);
        return AGEXEC_FALSE;
    }

    ret = connect(*p_sock, (struct sockaddr *)&sa_data,
            sizeof(struct sockaddr_in));
    if (ret < 0) {
        AGEXEC_LOG(LOG_ERR, "%s: connect() error\n", __FUNCTION__);
        close(*p_sock);
        *p_sock = -1;
        return AGEXEC_FALSE;
    }

    return AGEXEC_TRUE;
}

static int lc_agexec_sock_recv(int sock, agexec_msg_t **pp_res)
{
    struct sockaddr_in sa_src;
    socklen_t addr_len;

    agexec_msg_hdr_t *hdr = NULL;

    char buf[AGEXEC_LEN_RECV_BUF];
    ssize_t nrecv = 0, offset = 0, len = 0;
    struct timeval timeout = { tv_sec: RECV_TIMEOUT, tv_usec: 0 };

    if (!pp_res) {
        return AGEXEC_FALSE;
    }
    *pp_res = NULL;

    memset(buf, 0, AGEXEC_LEN_RECV_BUF);
    memset(&sa_src, 0, sizeof(sa_src));
    addr_len = sizeof(sa_src);

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
            (char *)&timeout, sizeof(struct timeval));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
            (char *)&timeout, sizeof(struct timeval));

    nrecv = recvfrom(sock, buf, AGEXEC_LEN_RECV_BUF, 0,
            (struct sockaddr *)&sa_src, &addr_len);
    if (nrecv >= AGEXEC_LEN_RECV_BUF || nrecv < (int)sizeof(agexec_msg_hdr_t)) {
        AGEXEC_LOG(LOG_ERR, "%s: Invalid message size: %zd, errno=%d(%s), "
                "agexec version may not correct, my version is %x.\n",
                __FUNCTION__, nrecv, errno, strerror(errno), AGEXEC_ACTION_MAGIC);
        return AGEXEC_FALSE;
    }
    hdr = (agexec_msg_hdr_t *)buf;
    agexec_msg_hdr_ntoh(hdr);

    if (!agexec_message_sanity_check(hdr)) {
        AGEXEC_LOG(LOG_ERR, "%s: agexec version error: agexec version of %s is %x, "
                "my version is %x\n", __FUNCTION__, inet_ntoa(sa_src.sin_addr),
                hdr->action_type & 0xFFFF0000, AGEXEC_ACTION_MAGIC);
        return AGEXEC_FALSE;
    }

    len = hdr->length + sizeof(agexec_msg_t);
    *pp_res = (agexec_msg_t *)malloc(len);
    if (*pp_res == NULL) {
        AGEXEC_LOG(LOG_ERR, "%s: can not alloc memory for pp_res\n", __FUNCTION__);
        return AGEXEC_FALSE;
    }
    memcpy(*pp_res, buf, nrecv);

    for (offset = nrecv; offset < len; offset += nrecv) {
        nrecv = recv(sock, (void *)(*pp_res) + offset, len - offset, 0);
        if (nrecv <= 0) {
            break;
        }
    }

    AGEXEC_LOG(LOG_DEBUG, "%s: recv: len=%u/%zd/%zd seq=%u "
            "action_type=%x object_type=%u err_code=%u[%s] from %s\n",
            __FUNCTION__, hdr->length, len, offset, hdr->seq,
            hdr->action_type, hdr->object_type, hdr->err_code,
            (hdr->err_code == AGEXEC_ERR_SUCCESS || offset < len)
              ? "" : (char *)((*pp_res)->data),
            inet_ntoa(sa_src.sin_addr));

    if (offset < len) {
        return AGEXEC_FALSE;
    }
    return AGEXEC_TRUE;
}

static int lc_agexec_sock_close(int sock)
{
    if (close(sock) < 0) {
        AGEXEC_LOG(LOG_ERR, "%s: close() error\n", __FUNCTION__);
        return AGEXEC_FALSE;
    }

    return AGEXEC_TRUE;
}

static int lc_agexec_request(agexec_msg_t *p_req,
        agexec_msg_t **pp_res, char *ip)
{
    int sock = -1, len;
    int nsend;

    if (!p_req || !pp_res || !ip || lc_agexec_sock_init(&sock, ip)) {
        return AGEXEC_FALSE;
    }

    len = sizeof(agexec_msg_t) + p_req->hdr.length;
    AGEXEC_LOG(LOG_DEBUG, "%s: send: len=%u/%d seq=%u action_type=%x "
            "object_type=%u err_code=%u\n", __FUNCTION__,
            p_req->hdr.length, len, p_req->hdr.seq, p_req->hdr.action_type,
            p_req->hdr.object_type, p_req->hdr.err_code);

    agexec_msg_hdr_hton(&(p_req->hdr));
    nsend = send(sock, p_req, len, 0);
    agexec_msg_hdr_ntoh(&(p_req->hdr));
    if (nsend <= 0) {
        AGEXEC_LOG(LOG_ERR, "%s: send() error: %s\n", __FUNCTION__, strerror(errno));

        lc_agexec_sock_close(sock);
        return AGEXEC_FALSE;
    }

    if (p_req->hdr.flag & AGEXEC_FLG_ASYNC_MSG) {
        goto finish;
    }

    if (lc_agexec_sock_recv(sock, pp_res)) {
        lc_agexec_sock_close(sock);
        return AGEXEC_FALSE;
    }

finish:
    lc_agexec_sock_close(sock);
    return AGEXEC_TRUE;
}

/* exec */

int lc_agexec_exec(agexec_msg_t *p_req, agexec_msg_t **pp_res, char *ip)
{
    int err;

    if (!p_req || !pp_res || !ip) {
        return AGEXEC_FALSE;
    }

    *pp_res = NULL;

    switch(p_req->hdr.object_type) {
    case AGEXEC_OBJ_BR:
        agexec_br_hton(p_req);
        break;

    case AGEXEC_OBJ_PO:
        agexec_port_hton(p_req);
        break;

    case AGEXEC_OBJ_IF:
        agexec_vif_hton(p_req);
        break;

    case AGEXEC_OBJ_FL:
        agexec_flow_hton(p_req);
        break;

    case AGEXEC_OBJ_VM:
        agexec_vm_hton(p_req);
        break;

    case AGEXEC_OBJ_HO:
        agexec_host_hton(p_req);
        break;

    case AGEXEC_OBJ_ERRMSG:
        break;
    case AGEXEC_OBJ_PIF:
        break;

    default:
        return AGEXEC_FALSE;
        break;
    }

    err = lc_agexec_request(p_req, pp_res, ip);

    /* recover p_req for reuse */

    switch(p_req->hdr.object_type) {
    case AGEXEC_OBJ_BR:
        agexec_br_ntoh(p_req);
        break;

    case AGEXEC_OBJ_PO:
        agexec_port_ntoh(p_req);
        break;

    case AGEXEC_OBJ_IF:
        agexec_vif_ntoh(p_req);
        break;

    case AGEXEC_OBJ_FL:
        agexec_flow_ntoh(p_req);
        break;

    case AGEXEC_OBJ_VM:
        agexec_vm_ntoh(p_req);
        break;

    case AGEXEC_OBJ_HO:
        agexec_host_ntoh(p_req);
        break;

    case AGEXEC_OBJ_ERRMSG:
        break;
    case AGEXEC_OBJ_PIF:
        break;
    default:
        return AGEXEC_FALSE;
        break;
    }

    /* return */

    if (err) {
        return AGEXEC_FALSE;
    }

    if (*pp_res) {
        switch((*pp_res)->hdr.object_type) {

        case AGEXEC_OBJ_BR:
            agexec_br_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_PO:
            agexec_port_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_IF:
            agexec_vif_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_FL:
            agexec_flow_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_VM:
            agexec_vm_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_HO:
            agexec_host_ntoh(*pp_res);
            break;

        case AGEXEC_OBJ_ERRMSG:
            break;
        case AGEXEC_OBJ_PIF:
            break;

        default:
            return AGEXEC_FALSE;
            break;
        }
    }

    return AGEXEC_TRUE;
}
