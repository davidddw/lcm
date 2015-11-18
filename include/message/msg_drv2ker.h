#ifndef _MSESSAGE_MSG_DRV2KER_H
#define _MSESSAGE_MSG_DRV2KER_H

#include "framework/types.h"
#include "lc_netcomp.h"

enum {
    SWITCH_PORT_ADD,
    SWITCH_PORT_DELETE,
    SWITCH_PORT_MODIFY
};

struct switch_port {
    char name[MAX_SWITCH_PORT_NAME_LEN];
    union {
        u32 octet;
        struct {
            u32 is_in_use :1;
        } s;
    } flag;
    u32 id;
    u32 speed;
    struct ether_addr hw_addr;
};

struct packet_snap {
    /* byte 0 - byte 15 */
    struct ether_addr dl_src;
    struct ether_addr dl_dst;
    u16 dl_vlan;
    u8 dl_vlan_pcp;
    union {
        u8 octet;
        struct {
            u8 is_snap_valid :1;
            u8 is_packet_full :1;
        } s;
    } flag;

    /* byte 16 - byte 31 */
    u16 dl_type;
    u8 nw_proto;
    u8 nw_tos;
    struct ipv4_addr nw_src;
    struct ipv4_addr nw_dst;
    u16 tp_src;
    u16 tp_dst;
}; /* 32 bytes */

struct msg_ntf_sw_join {
    u64 datapath_id;
    u32 port_total;
    struct ipv4_addr mgmt_ip;
    struct switch_port ports[MAX_SWITCH_PORT_NUM];
};

struct msg_ntf_sw_leave {
    u64 datapath_id;
};

struct msg_ntf_sw_recv_pkt {
    u64 datapath_id;
    u16 in_port;
    u32 in_sw_buf_id;

    u32 pkt_len;
    struct packet_snap pkt_snap;
    u8 pkt_data[PACKET_BUFFER];
};

struct msg_ntf_sw_port_change {
    u64 datapath_id;
    u32 reason;
    struct switch_port port;
};

struct msg_reply_vm_alloc {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
    
    uint32_t rack_id;
    uint32_t slot_id;
    uint32_t of_port;

    vm_t vm;
};

struct msg_reply_vm_del {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
};

struct msg_reply_vm_start {
    uint32_t result;
    uint32_t code;

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
    
    uint32_t rack_id;
    uint32_t slot_id;
    uint32_t of_port;
};

struct msg_reply_vm_stop {
    uint32_t result;
    uint32_t code;

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
};

struct msg_reply_vm_pause {
    uint32_t result;
    uint32_t code;

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
};

struct msg_reply_vm_resume {
    uint32_t result;
    uint32_t code;

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;

    uint32_t rack_id;
    uint32_t slot_id;
    uint32_t of_port;
};

struct msg_reply_vedge_alloc {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t rack_id;
    uint32_t slot_id;
    uint32_t of_port;     /* This is for public interface */

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;

    vedge_t vedge;
};

struct msg_reply_vedge_del {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    vedge_t vedge;
};

struct msg_reply_vedge_start {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    vedge_t vedge;
};

struct msg_reply_vedge_stop {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    vedge_t vedge;
};

/* VM Disk operations */
struct msg_reply_vm_disk_opt {
    uint32_t result;
    uint32_t code;         /* meaningful if result is failure */

    uint32_t vnet_id;
    uint32_t vl2_id;
    uint32_t vm_id;
};

#endif /* _MESSAGE_MSG_DRV2KER_H */
