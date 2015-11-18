/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-09
 *
 */

#ifndef __LCS_UTILS_H__
#define __LCS_UTILS_H__

#include <syslog.h>
#include "lc_snf.h"
#include "lc_postman.h"

#define LOG_BUF_LEN    256


extern char *SNFLOGLEVEL[8];
#define SNF_SYSLOG(level, format, ...)  \
    if (level <= lc_snf_knob->lc_opt.log_level) { \
        syslog(level, "[tid=%lu] %s %s: "format, \
        pthread_self(), SNFLOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__); \
    }

extern amqp_connection_state_t lc_snf_conn;

int lc_bus_lcsnf_publish_unicast(char *msg, int len, uint32_t queue_id);
int send_email_through_postman(int rtype, int etype, monitor_host_t *hostp, vm_info_t *vm_info, char *email_msg, int level);
int lcs_get_domain_from_config();
#endif
