/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_agexec_msg.h
 * Author Name: Xiang Yang
 * Date: 2013-3-27
 *
 * Description: Header for lc_platform to agexec messages
 *
 */

#ifndef _LC_AGEXEC_MSG_H
#define _LC_AGEXEC_MSG_H

#include "agexec.h"

#if 0X1C280000 != AGEXEC_VERSION_MAGIC
#error "agexec version not correct (../xcp_agent/agexec.h AGEXEC_VERSION_MAGIC)"
#endif

#define AGEXEC_TRUE    0
#define AGEXEC_FALSE   -1
int lc_agexec_exec(agexec_msg_t *p_req, agexec_msg_t **pp_res, char *ip);

#define LC_AGEXEC_LEN_SINGLE_OVSBR_MSG                                        \
    (sizeof(agexec_msg_t) + sizeof(agexec_br_t))

#define LC_AGEXEC_LEN_SINGLE_OVSFLOW_MSG                                      \
    (sizeof(agexec_msg_t) + sizeof(agexec_flow_t))

#define LC_AGEXEC_LEN_SINGLE_OVSPORT_MSG                                      \
    (sizeof(agexec_msg_t) + sizeof(agexec_port_t))

#define LC_AGEXEC_LEN_SINGLE_OVSVIF_MSG                                       \
    (sizeof(agexec_msg_t) + sizeof(agexec_vif_t))

#define LC_AGEXEC_LEN_SINGLE_OVSPI_MSG                                        \
    (sizeof(agexec_msg_t) + sizeof(agexec_port_t) + sizeof(agexec_vif_t))

#define LC_AGEXEC_LEN_OVS_RESULT_LINE      512


#endif /* _LC_AGEXEC_MSG_H */
