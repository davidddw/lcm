/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-09
 *
 */

#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

#include "lc_snf.h"
#include "lcs_utils.h"
#include "lcs_socket.h"
#include "ovs_rsync.h"

#define AGENT_LCSNF_PORT  20007
#define MAX_BACKLOG_VAL   100
#define APIPATH "/tmp/.api2lcsnf"

static int create_agent_data_socket(void);
static void exit_agent_data_socket(void);
static int create_api_socket(void);
static void exit_api_socket(void);

int lcsnf_sock_init(void)
{
    int ret = 0;

    ret += create_agent_data_socket();
    ret += create_dfi_data_socket();
    ret += create_dfi_ctrl_socket();
    ret += create_api_socket();

    return ret < 0 ? -1 : 0;
}

int lcsnf_sock_exit(void)
{
    exit_agent_data_socket();

    exit_dfi_ctrl_socket();
    exit_dfi_data_socket();
    exit_api_socket();

    return 0;
}

static int create_agent_data_socket(void)
{
    int opt = 1;
    struct sockaddr_in servaddr;
    struct in_addr addr;

    if ((lc_sock.sock_rcv_agent = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: create socket error!\n", __FUNCTION__);
        lc_sock.sock_rcv_agent = -1;
        return -1;
    }

    setsockopt(lc_sock.sock_rcv_agent, SOL_SOCKET,
            SO_REUSEADDR, (void*)&opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(AGENT_LCSNF_PORT);
    if (inet_aton(lcsnf_domain.local_ctrl_ip, &addr) == 0) {
        SNF_SYSLOG(LOG_INFO, "%s: local_ctrl_ip %s inet_aton error!\n", __FUNCTION__, lcsnf_domain.local_ctrl_ip);
        close(lc_sock.sock_rcv_agent);
        lc_sock.sock_rcv_agent = -1;
        return -1;
    }
    servaddr.sin_addr.s_addr = addr.s_addr;

    if (bind(lc_sock.sock_rcv_agent,
                (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: bind to %d failure %s!\n",
                __FUNCTION__, AGENT_LCSNF_PORT, strerror(errno));
        close(lc_sock.sock_rcv_agent);
        lc_sock.sock_rcv_agent = -1;
        return -1;
    }

    if (listen(lc_sock.sock_rcv_agent, MAX_BACKLOG_VAL) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: call listen failure!\n", __FUNCTION__);
        close(lc_sock.sock_rcv_agent);
        lc_sock.sock_rcv_agent = -1;
        return -1;
    }

    return 0;
}

static int create_api_socket(void)
{
    struct sockaddr_un servaddr;

    if ((lc_sock.sock_rcv_api = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: create socket error!\n", __FUNCTION__);
        lc_sock.sock_rcv_api = -1;
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_UNIX;
    strncpy(servaddr.sun_path, APIPATH, sizeof(servaddr.sun_path)-1);
    unlink(APIPATH);

    if (bind(lc_sock.sock_rcv_api,
                (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: bind to %s failure %s!\n",
                __FUNCTION__, APIPATH, strerror(errno));
        close(lc_sock.sock_rcv_api);
        lc_sock.sock_rcv_api = -1;
        return -1;
    }
    chmod(APIPATH, 666);

    if (listen(lc_sock.sock_rcv_api, MAX_BACKLOG_VAL) < 0) {
        SNF_SYSLOG(LOG_INFO, "%s: call listen failure!\n", __FUNCTION__);
        close(lc_sock.sock_rcv_api);
        lc_sock.sock_rcv_api = -1;
        unlink(APIPATH);
        return -1;
    }

    return 0;
}

static void exit_agent_data_socket(void)
{
    close(lc_sock.sock_rcv_agent);
    lc_sock.sock_rcv_agent = -1;
    SNF_SYSLOG(LOG_INFO, "%s() succeeded\n", __FUNCTION__);
}

static void exit_api_socket(void)
{
    close(lc_sock.sock_rcv_api);
    lc_sock.sock_rcv_api = -1;
    SNF_SYSLOG(LOG_INFO, "%s() succeeded\n", __FUNCTION__);
}

