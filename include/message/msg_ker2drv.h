#ifndef _MESSAGE_MSG_KER2DRV_H
#define _MESSAGE_MSG_KER2DRV_H

#include "framework/types.h"

#define MAX_ACTION 5
#define ACTION_LEN 16
#define DEF_IDLE_TIMEOUT 30
#define LC_FLOW_PERMANENT 0

enum {
    LC_ACT_OUTPUT = 0,

    LC_ACT_SET_DL_SRC = 4,
    LC_ACT_SET_DL_DST,

    LC_ACT_SET_NW_SRC,
    LC_ACT_SET_NW_DST
};

enum {
    LC_WC_IN_PORT = 1 << 0,

    LC_WC_DL_SRC  = 1 << 2,
    LC_WC_DL_DST  = 1 << 3,

    LC_WC_NW_SRC_SHIFT = 8,
    LC_WC_NW_SRC_BITS = 6,
    LC_WC_NW_SRC_MASK = ((1 << LC_WC_NW_SRC_BITS) - 1) << LC_WC_NW_SRC_SHIFT,
    LC_WC_NW_SRC_ALL = 32 << LC_WC_NW_SRC_SHIFT,

    LC_WC_NW_DST_SHIFT = 14,
    LC_WC_NW_DST_BITS = 6,
    LC_WC_NW_DST_MASK = ((1 << LC_WC_NW_DST_BITS) - 1) << LC_WC_NW_DST_SHIFT,
    LC_WC_NW_DST_ALL = 32 << LC_WC_NW_DST_SHIFT,

    LC_WC_ALL = ((1 << 22) - 1)
};

struct lc_action_head {
    u16 type;
    u16 len;
};

struct lc_action_output {
    u16 type;
    u16 len;
    u16 port;
    u16 max_len;
};

struct lc_action_dl_addr {
    u16 type;
    u16 len;
    struct ether_addr dl_addr;
    u8 pad[6];
};

struct lc_action_nw_addr {
    u16 type;
    u16 len;
    struct ipv4_addr nw_addr;
};

struct flow_match {
    /* 32bit word0 */
    u32 wildcards;

    /* 32bit word1, word2 */
    u16 in_port;
    struct ether_addr dl_src;

    /* 32bit word3, word4 */
    struct ether_addr dl_dst;
    u16 dl_vlan;

    /* 32bit word5 */
    u8 dl_vlan_pcp;
    u8 pad1[1];
    u16 dl_type;

    /* 32bit word6 */
    u8 nw_tos;
    u8 nw_proto;
    u8 pad2[2];

    /* 32bit word7 */
    struct ipv4_addr nw_src;

    /* 32bit word8 */
    struct ipv4_addr nw_dst;

    /* 32bit word9 */
    u16 tp_src;
    u16 tp_dst;
};

struct msg_req_sw_send_pkt {
    u64 datapath_id;
    u16 out_port;
    u32 out_sw_buf_id;

    u16 pkt_len;
    u8 pkt_data[PACKET_BUFFER];
};

struct msg_req_sw_set_flow {
    u64 datapath_id;
    u16 in_port;
    u32 in_sw_buf_id;

    u16 idle_timeout;
    u16 hard_timeout;
    struct flow_match pattern;
    u32 action_num;
    u8 actions[MAX_ACTION][ACTION_LEN];
};

struct msg_req_vm_alloc {
};

struct msg_req_vm_free {
};

struct msg_req_vm_power_on {
};

struct msg_req_vm_power_off {
};

#endif /* _MESSAGE_MSG_KER2DRV_H */
