#ifndef _MESSAGE_MSG_APP2UI_H
#define _MESSAGE_MSG_APP2UI_H

#include "msg_app2lcm.h"

struct msg_reg_app {
    u32 aid;
};

struct msg_net_reply_vnet_add {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
};

struct msg_net_reply_vnet_del {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
};

struct msg_net_reply_vnet_change {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
};

struct msg_net_reply_vl2_add {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
    uint32_t vl2_id;
};

struct msg_net_reply_vl2_del {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
    uint32_t vl2_id;
};

struct msg_net_reply_vl2_change {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */
    uint32_t vnet_id;
    uint32_t vl2_id;
};

struct msg_net_reply_vm_add {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_del {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_chg {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_start {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_stop {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_pause {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vm_resume {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_add {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_del {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_chg {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_start {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_stop {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_pause {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_vedge_resume {
    u32 vnet_id;
    u32 result;
    u32 code;
};

struct msg_net_reply_disk_opt {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
};
#endif /* _MESSAGE_MSG_APP2UI_H */
