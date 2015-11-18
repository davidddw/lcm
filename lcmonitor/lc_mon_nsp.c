/*
 * Copyright (c) 2012-2014 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_nsp.c
 * Date: 2014-04-29
 *
 * Description: get nsp state by heartbeat
 *
 *
 */

#define _GNU_SOURCE
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <libxml/parser.h>
#include <curl/curl.h>

#include "lc_mon_nsp.h"
#include "lc_xen_monitor.h"
#include "lc_xen.h"
#include "lc_utils.h"
#include "vm/vm_host.h"
#include "lc_db_host.h"
#include "lc_db_pool.h"
#include "postman.pb-c.h"
#include "lc_mon.h"

#define NSP_STATE_MON_INTERVAL 30
#define NSP_CLEAR_CMD   \
    "/usr/local/livegate/script/router.sh delete all_routers 2>&1 >/dev/null;" \
    "echo $?"
#define NSP_RELOAD_CMD  \
    "echo '0 0' > /tmp/uptime;" \
    "ps x | grep talker.py | grep -v grep | awk '{print $1}' | xargs kill -9;" \
    "echo $?"

int lc_nsp_get_host_state(host_t *host)
{
    int  cnt = 0;
    char cmd[LC_EXEC_CMD_LEN];
    FILE *stream  = NULL;

    if ((NULL == host) || (NULL == host->host_address)) {
        LCMON_LOG(LOG_ERR, "Input Para is NULL\n");
        goto error;
    }

    /* SSH */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "sshpass -p %s ssh -o ConnectTimeout=1 %s@%s pwd | grep -c %s",
            host->password, host->username, host->host_address, host->username);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        LCMON_LOG(LOG_ERR, "stream is NULL.\n");
        goto error;
    }
    fscanf(stream, "%d", &cnt);
    pclose(stream);

    if (cnt > 0) {
        return LC_CR_STATE_COMPLETE;
    };

    /* PING */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "ping -w 1 -i 0.001 -c 100 %s -q | grep -Eo \"[0-9]+ received\"",
            host->host_address);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        LCMON_LOG(LOG_ERR, "stream is NULL.\n");
        goto error;
    }
    fscanf(stream, "%d received", &cnt);
    pclose(stream);

    if (cnt > 0) {
        return LC_CR_STATE_COMPLETE;
    }

error:
    return LC_CR_STATE_ERROR;
}

static void lc_nsp_cluster_ha_notify(host_t *host, uint32_t pool_index)
{
    lc_mbuf_t msg;
    struct msg_nsp_opt *p;
    int len;

    len = sizeof(struct lc_msg_head) + sizeof(struct msg_nsp_opt);

    memset(&msg, 0, sizeof msg);
    msg.hdr.magic = MSG_MG_MON2KER;
    msg.hdr.type = LC_MSG_NSP_DOWN;
    msg.hdr.length = sizeof(struct msg_nsp_opt);

    p = (struct msg_nsp_opt *)msg.data;
    memcpy(p->host_address, host->host_address, sizeof(host->host_address));
    p->host_state = host->host_state;
    p->pool_index = pool_index;

    lc_bus_lcmond_publish_unicast((char *)&msg, len, LC_BUS_QUEUE_LCPD);

    LCMON_LOG(LOG_INFO, "send nsp down message!");
}

static void lc_nsp_cluster_ha_email(host_t *host, rpool_t *pool, char *info)
{
    char msg[MAX_LOG_MSG_LEN] = {0};
    monitor_host_t *pmon_host = (monitor_host_t *)malloc(sizeof(monitor_host_t));
    if (NULL == pmon_host) {
        LCMON_LOG(LOG_ERR, "%s: monitor host alloc failed", __FUNCTION__);
        return;
    }
    memset(pmon_host, 0 , sizeof(monitor_host_t));
    pmon_host->htype = pool->ctype;
    pmon_host->stype = pool->stype;
    pmon_host->pool_id = pool->rpool_index;
    pmon_host->role = host->role;
    pmon_host->type = host->host_role;
    pmon_host->host_state = host->host_state;
    pmon_host->error_number = host->error_number;
    pmon_host->health_check = host->health_check;
    pmon_host->stats_time = host->stats_time;

    strcpy(pmon_host->host_address, host->host_address);
    strcpy(pmon_host->username, host->username);
    strcpy(pmon_host->password, host->password);
    pmon_host->cr_id = host->cr_id;
    lc_mon_get_config_info(pmon_host, 1);

    memset(msg, 0, sizeof(msg));
    snprintf(msg, MAX_LOG_MSG_LEN, "NSP server %s is abnormal, %s",
            pmon_host->host_address, info);
    send_nsp_fail_email(POSTMAN__RESOURCE__HOST, pmon_host, NULL, msg,
            LC_SYSLOG_LEVEL_WARNING, LC_MONITOR_POSTMAN_NO_FLAG);
}

int lc_nsp_cluster_ha_activate(host_t *host, rpool_t *pool)
{
    lc_nsp_cluster_ha_notify(host, pool->rpool_index);

    return 0;
}

int lc_nsp_cluster_ha_resume(host_t *host, int reload)
{
    int  res = 0;
    char cmd[LC_EXEC_CMD_LEN];
    FILE *stream  = NULL;

    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "sshpass -p %s ssh -o ConnectTimeout=9 %s@%s \"%s\"",
            host->password, host->username, host->host_address, NSP_CLEAR_CMD);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        LCMON_LOG(LOG_ERR, "stream is NULL.\n");
        goto error;
    }
    fscanf(stream, "%d", &res);
    pclose(stream);

    if (res) {
        return -1;
    };

    if (reload) {
        memset(cmd, 0, LC_EXEC_CMD_LEN);
        sprintf(cmd, "sshpass -p %s ssh -o ConnectTimeout=9 %s@%s \"%s\"",
                host->password, host->username, host->host_address, NSP_RELOAD_CMD);
        stream = popen(cmd, "r");
        if (NULL == stream) {
            LCMON_LOG(LOG_ERR, "stream is NULL.\n");
            goto error;
        }
        fscanf(stream, "%d", &res);
        pclose(stream);

        if (res) {
            return -1;
        };
    }

    return 0;

error:
    return -1;
}

int lc_nsp_state_monitor()
{
    rpool_t pool_set[MAX_RPOOL_NUM], *mon_pool;
    host_t host_set[MAX_HOST_NUM], *mon_host;
    uint32_t mon_state;
    int pool_num, host_num, down_host_num, up_host_num, code, i, j, k;
    int resume_host_num, normal_host_num;

    while (1) {
        /* timer(); */
        sleep(NSP_STATE_MON_INTERVAL);

        memset(pool_set, 0, sizeof(rpool_t) * MAX_RPOOL_NUM);
        code = lc_vmdb_pool_load_all_v2_by_type_domain(pool_set,
                sizeof(rpool_t), &pool_num,
                LC_POOL_TYPE_KVM, lcmon_domain.lcuuid);

        if (code != LC_DB_POOL_FOUND) {
            continue;
        }
        if (pool_num > MAX_RPOOL_NUM) {
            LCMON_LOG(LOG_WARNING, "The pool number is %d, "
                    "exceeding the max pool number %d",
                    pool_num, MAX_RPOOL_NUM);
            pool_num = MAX_RPOOL_NUM;
        }

        for (i=0; i<pool_num; i++) {
            mon_pool = &pool_set[i];
            memset(host_set, 0, sizeof(host_t) * MAX_HOST_NUM);
            code = lc_vmdb_compute_resource_load_all_v2_by_poolid(host_set,
                    sizeof(host_t), &host_num, mon_pool->rpool_index);

            if (code != LC_DB_HOST_FOUND) {
                continue;
            }
            if (host_num > MAX_HOST_NUM) {
                LCMON_LOG(LOG_WARNING, "The host number is %d, "
                        "exceeding the max host number %d",
                        host_num, MAX_HOST_NUM);
                host_num = MAX_HOST_NUM;
            }

            resume_host_num = 0;
            normal_host_num = 0;
            for (j=0; j<host_num; j++) {
                mon_host = &host_set[j];
                mon_state = lc_nsp_get_host_state(mon_host);

                if (mon_state == LC_CR_STATE_COMPLETE &&
                        mon_host->host_state == LC_CR_STATE_ERROR) {

                    mon_host->host_state = -2;
                    resume_host_num++;

                    LCMON_LOG(LOG_INFO, "NSP server %s is %s!", mon_host->host_address,
                            (mon_state == LC_CR_STATE_COMPLETE) ? "UP" : "DOWN");
                    continue;
                }

                if (mon_state == LC_CR_STATE_ERROR &&
                        mon_host->host_state == LC_CR_STATE_COMPLETE) {

                    lc_vmdb_master_compute_resource_state_update(LC_CR_STATE_ERROR,
                            mon_host->host_address);

                    mon_host->host_state = -1;

                    LCMON_LOG(LOG_INFO, "NSP server %s is %s!", mon_host->host_address,
                            (mon_state == LC_CR_STATE_COMPLETE) ? "UP" : "DOWN");
                    continue;
                }

                if (mon_state == LC_CR_STATE_COMPLETE &&
                        mon_host->host_state == LC_CR_STATE_COMPLETE) {

                    normal_host_num++;
                }
            }

            /* If there are NSP hosts recovered from DOWN, take further actions */
            if (resume_host_num > 0) {
                for (j=0; j<host_num; j++) {
                    mon_host = &host_set[j];
                    /* Find out currently recovered NSP hosts A */
                    if (mon_host->host_state == -2) {
                        /* If there are UP NSP hosts B which are also UP during last check,
                         * then it can be deduced that the vgateways of hosts A have been
                         * migrated to normal hosts B after A are regarded as DOWN last time.
                         *
                         * In other words, when A recovered from DOWN this time, A's vgateways
                         * must be deleted if exist to avoid conflict with the new vgateways
                         * in B.
                         */
                        if (normal_host_num > 0) {
                            lc_nsp_cluster_ha_resume(mon_host, 0);
                        /* If there are multiple hosts A and no host B, then use talker to
                         * reconstruct all vgateways in hosts A, because it is hard to judge
                         * whether there are duplicate vgateways in each NSP of hosts A.
                         */
                        } else if (resume_host_num > 1) {
                            lc_nsp_cluster_ha_resume(mon_host, 1);
                        }
                        lc_vmdb_master_compute_resource_state_update(LC_CR_STATE_COMPLETE,
                                mon_host->host_address);
                        mon_host->host_state = LC_CR_STATE_COMPLETE;
                    }
                }
            }

            down_host_num = 0;
            up_host_num = 0;
            k = 0;
            for (j=0; j<host_num; j++) {
                mon_host = &host_set[j];
                if (mon_host->host_state == -1) {
                    k = j;
                    down_host_num++;
                } else if (mon_host->host_state == LC_CR_STATE_COMPLETE) {
                    up_host_num++;
                }
            }

            if (down_host_num == 0) {
                continue;
            } else if (down_host_num == 1) {
                mon_host = &host_set[k];

                if (up_host_num == 0) {
                    lc_nsp_cluster_ha_email(mon_host, mon_pool,
                            "but no normal NSP server exists in the pool, "
                            "corresponding vgateways cannot be migrated");
                    continue;
                }

                mon_host->host_state = LC_CR_STATE_ERROR;
                lc_nsp_cluster_ha_email(mon_host, mon_pool,
                        "corresponding vgateways will be migrated");

                mon_host->host_state = LC_CR_STATE_COMPLETE;
                lc_nsp_cluster_ha_activate(mon_host, mon_pool);
            } else {
                if (up_host_num == 0) {
                    LCMON_LOG(LOG_INFO, "ALARM: all the NSP servers are down together, "
                            "which is regarded as the problem of the controller, "
                            "rather than the NSP servers, thus don't take HA action!");
                } else {
                    for (j=0; j<host_num; j++) {
                        mon_host = &host_set[j];

                        if (mon_host->host_state == -1) {
                            mon_host->host_state = LC_CR_STATE_ERROR;
                            lc_nsp_cluster_ha_email(mon_host, mon_pool,
                                    "corresponding vgateways will be migrated");

                            mon_host->host_state = LC_CR_STATE_COMPLETE;
                            lc_nsp_cluster_ha_activate(mon_host, mon_pool);
                        }
                    } /* end for (j=0; j<host_num; j++) */
                } /* end if (up_host_num == 0) */
            } /* end if (down_host_num == 0) */
        } /* end for (i=0; i<pool_num; i++) */
    } /* end while (1) */

    return 0;
}
