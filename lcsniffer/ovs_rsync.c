/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-02-28
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <time.h>

#include "lc_snf.h"
#include "lc_db.h"
#include "lcs_utils.h"
#include "ovs_rsync.h"
#include "lcs_usage_db.h"

static int  socket_ctrl = -1;

static struct sockaddr_in sa_data;

#define MAX_CR_PER_CTRL    512
#define LC_CR_HTYPE_XEN    1
#define LC_CR_HTYPE_KVM    3
#define CR_LOAD_INTERVAL   60
static struct sockaddr_in sa_host_list[MAX_CR_PER_CTRL];
time_t host_load_time = 0;

int create_dfi_ctrl_socket(void)
{
    socket_ctrl = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ctrl < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: socket() error\n", __FUNCTION__);
        return -EACCES;
    }

    SNF_SYSLOG(LOG_INFO, "%s() succeeded (ctrl plane socket = %d)\n",
            __FUNCTION__, socket_ctrl);
    return 0;
}

int create_dfi_data_socket(void)
{
    int rv;
    struct in_addr addr;

    if (inet_aton(lcsnf_domain.local_ctrl_ip, &addr) == 0) {
        rv = -EACCES;
        goto err;
    }
    memset(&sa_data, 0, sizeof(struct sockaddr_in));
    sa_data.sin_family = AF_INET;
    sa_data.sin_addr.s_addr = addr.s_addr;
    sa_data.sin_port = htons(DFI_DATA_PORT);
    lc_sock.sock_rcv_dfi = socket(AF_INET, SOCK_DGRAM, 0);
    if (lc_sock.sock_rcv_dfi < 0) {
        rv = -EACCES;
        goto err;
    }
    rv = bind(lc_sock.sock_rcv_dfi,
            (struct sockaddr *)&sa_data, sizeof(struct sockaddr_in));
    if (rv < 0) {
        rv = -EPERM;
        goto err_socket;
    }
    SNF_SYSLOG(LOG_INFO, "%s() succeeded (data plane socket = %d)\n",
            __FUNCTION__, lc_sock.sock_rcv_dfi);
    return 0;

err_socket:
    lc_sock.sock_rcv_dfi = -1;
err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

void exit_dfi_ctrl_socket(void)
{
    close(socket_ctrl);
    socket_ctrl = -1;
    SNF_SYSLOG(LOG_INFO, "%s() succeeded\n", __FUNCTION__);
    return;
}

void exit_dfi_data_socket(void)
{
    close(lc_sock.sock_rcv_dfi);
    lc_sock.sock_rcv_dfi = -1;
    SNF_SYSLOG(LOG_INFO, "%s() succeeded\n", __FUNCTION__);
    return;
}

static int ctrl_plane_message_compose(struct message *msg, const void *ctrl)
{
    msg->magic = MSG_MG_ACL;
    memcpy(msg->data, ctrl, msg->size);
    msg->magic = htonl(msg->magic);
    msg->type  = htonl(msg->type);
    msg->size  = htonl(msg->size);
    msg->seq   = htonl(msg->seq);
    return 0;
}

static int data_plane_message_resolve(struct message *msg, void *data)
{
    msg->magic = ntohl(msg->magic);
    msg->type  = ntohl(msg->type);
    msg->size  = ntohl(msg->size);
    msg->seq   = ntohl(msg->seq);
    switch (msg->magic) {
       case MSG_MG_DFI:
           if (msg->type == MSG_TP_DATA_REPLY) {
               memcpy(data, msg->data, msg->size);
           } else {
               return -EINVAL;
           }
           break;
       default:
           return -ESRCH;
    }
    return 0;
}

static void register_openvswitch(__be32 *datapath, struct in_addr *sin_addr) {
    *datapath = sin_addr->s_addr;
}

static int ctrl_plane_message_send(struct sockaddr_in *host_addr,
        const void *ctrl,
        u32 seq,
        u32 size,
        u32 type)
{
    struct message msg;
    int nsend, rv;
    msg.type = type;
    msg.size = size;
    msg.seq = seq;
    rv = ctrl_plane_message_compose(&msg, ctrl);
    if (rv < 0) {
        rv = -EINVAL;
        goto err;
    }
    nsend = sendto(socket_ctrl, &msg, 16 + size, 0,
            (struct sockaddr *)host_addr, sizeof(struct sockaddr));
    if (nsend < 0) {
        SNF_SYSLOG(LOG_ERR, "%s to %s failed"
                " (send data size = %d, magic = 0x%08x, sequence = %02u)\n",
                __FUNCTION__, inet_ntoa(host_addr->sin_addr),
                nsend, ntohl(msg.magic), ntohl(msg.seq));
        rv = -EIO;
        goto err;
    }

    SNF_SYSLOG(LOG_INFO, "%s to %s succeeded"
            " (send data size = %d, magic = 0x%08x, sequence = %02u)\n",
            __FUNCTION__, inet_ntoa(host_addr->sin_addr),
            nsend, ntohl(msg.magic), ntohl(msg.seq));
    return 0;

err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

static int data_plane_message_recv(void *data, u32 *size, __be32 *datapath)
{
    struct message msg;
    int nrecv, rv;
    struct sockaddr_in sa_extern;
    socklen_t len = sizeof(struct sockaddr_in);
    memset(&msg, 0, sizeof(struct message));
    memset(&sa_extern, 0, len);
    nrecv = recvfrom(lc_sock.sock_rcv_dfi, &msg, sizeof(struct message), 0,
            (struct sockaddr *)&sa_extern, &len);
    if (nrecv < 0) {
        rv = -EIO;
        goto err;
    }
    register_openvswitch(datapath, &sa_extern.sin_addr);
    rv = data_plane_message_resolve(&msg, data);
    if (rv < 0) {
        rv = -EINVAL;
        goto err;
    }
    *size = msg.size;
    SNF_SYSLOG(LOG_INFO, "succeeded"
            " (recv data size = %d, magic = 0x%08x, sequence = %02u, datapath = %s)\n",
            nrecv, msg.magic, msg.seq, inet_ntoa(sa_extern.sin_addr));
    return 0;

err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

static int host_device_process(void *host_info, char *field, char *value)
{
    struct sockaddr_in *phost = (struct sockaddr_in *)host_info;

    if (strcmp(field, "ip") == 0) {
        inet_aton(value, &(phost->sin_addr));
    }

    return 0;
}

static int load_host_device_list()
{
    int i, ret = 0;
    time_t now = time(NULL);
    char condition[LC_DB_BUF_LEN] = {0};

    if (now - host_load_time > CR_LOAD_INTERVAL) {

        host_load_time = now;
        memset(&sa_host_list, 0, sizeof(sa_host_list));

        snprintf(condition, LC_DB_BUF_LEN,
                "(htype=%d or htype=%d) and domain='%s'",
                LC_CR_HTYPE_XEN, LC_CR_HTYPE_KVM, lcsnf_domain.lcuuid);
        ret = lc_db_table_loadn(
                sa_host_list, sizeof(struct sockaddr_in), MAX_CR_PER_CTRL,
                "host_device", "ip",
                condition, host_device_process);

        if (ret && ret != LC_DB_EMPTY_SET) {
            SNF_SYSLOG(LOG_ERR, "%s() load host failed (ret=%d).\n",
                    __FUNCTION__, ret);
            return -1;
        }

        for (i = 0; i < MAX_CR_PER_CTRL && sa_host_list[i].sin_addr.s_addr; ++i) {
            sa_host_list[i].sin_family = AF_INET;
            sa_host_list[i].sin_port = htons(DFI_CTRL_PORT);
        }
    }

    return 0;
}

int rsync_dfi_ctrl_message(void *ctrl, u32 size, u32 type)
{
    int i, ret = 0;

    if (size > MAX_MSG_SIZE) return -EFBIG;

    if (load_host_device_list()) {
        return -1;
    }

    for (i = 0; i < MAX_CR_PER_CTRL && sa_host_list[i].sin_addr.s_addr; ++i) {
        ret += ctrl_plane_message_send(sa_host_list + i, ctrl, 1, size, type);
    }

    return ret;
}

int rsync_dfi_data_message(const struct data_message_handler *ops)
{
    if (ops == NULL) return -EINVAL;
    void *data;
    u32 size = 0;
    __be32 datapath = 0;
    ops->timer_open();
    for (;;) {
        if (!(data = malloc(MAX_MSG_SIZE))) {
            continue;
        }
        memset(data, 0, MAX_MSG_SIZE);
        if (data_plane_message_recv(data, &size, &datapath) < 0) {
            continue;
        }
        ops->data_process(data, size, datapath);
    }
    ops->timer_close();
    return 0;
}
