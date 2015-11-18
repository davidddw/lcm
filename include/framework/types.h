#ifndef _FRAMEWORK_TYPES_H
#define _FRAMEWORK_TYPES_H

#include <endian.h>

#include "framework/basic_types.h"

#define ETHER_ADDR_LEN 6
#define IPV4_ADDR_LEN 4

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806

#define IPV4_PROTO_ICMP  1
#define IPV4_PROTO_TCP   6
#define IPV4_PROTO_UDP  17

/* the highest two bits are reserved */
#define UUMAC_ESW_BITS 14
#define UUMAC_ASW_BITS 6
#define UUMAC_IF_BITS  6
#define UUMAC_VNET_BITS 16
#define UUMAC_VL2_BITS 4

#define UUMAC_ESW_SHIFT 32
#define UUMAC_ASW_SHIFT 26
#define UUMAC_IF_SHIFT  20
#define UUMAC_VNET_SHIFT 4
#define UUMAC_VL2_SHIFT 0

#define UUMAC_ESW_MASK ((1ULL << UUMAC_ESW_BITS) - 1)
#define UUMAC_ASW_MASK ((1ULL << UUMAC_ASW_BITS) - 1)
#define UUMAC_IF_MASK ((1ULL << UUMAC_IF_BITS) - 1)
#define UUMAC_VNET_MASK ((1ULL << UUMAC_VNET_BITS) - 1)
#define UUMAC_VL2_MASK ((1ULL << UUMAC_VL2_BITS) - 1)

#define PACKET_BUFFER ((1 << 10) + (1 << 9))

#define MAX_SWITCH_PORT_NAME_LEN 16
#define MAX_SWITCH_PORT_NUM (1 << UUMAC_IF_BITS)

struct ether_addr {
    u8 octet[ETHER_ADDR_LEN];
};

struct ipv4_addr {
    u8 octet[IPV4_ADDR_LEN];
};

struct ether_header {
    u8  dhost[ETHER_ADDR_LEN];
    u8  shost[ETHER_ADDR_LEN];
    u16 type_len;
};

enum {
    ARPHRD_ETHER = 1
};

enum {
    ARPOP_REQUEST  = 1,
    ARPOP_REPLY    = 2,
    ARPOP_RREQUEST = 3,
    ARPOP_RREPLY   = 4
};

struct arp_header {
    u16 hrd;
    u16 pro;
    u8  hln;
    u8  pln;
    u16 op;
};

struct ether_arp_payload {
    struct arp_header header;
    u8 sha[ETHER_ADDR_LEN];
    u8 sip[IPV4_ADDR_LEN];
    u8 tha[ETHER_ADDR_LEN];
    u8 tip[IPV4_ADDR_LEN];
};

struct ip_header {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    u8 ip_hl:4;
    u8 ip_v:4;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    u8 ip_v:4;
    u8 ip_hl:4;
#endif
    u8 ip_tos;
    u16 ip_len;

    u16 ip_id;
    u16 ip_off;

    u8 ip_ttl;
    u8 ip_p;
    u16 ip_sum;

    struct ipv4_addr ip_src;
    struct ipv4_addr ip_dst;
};

struct physical_port_id {
    u32 rack_id;
    u32 slot_id;
    u32 if_id;
};

struct service_port_id {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
};

struct logical_port_id {
    u32 esw_id;
    u32 asw_id;
    u32 if_id;
};

struct physical_port {
    struct physical_port_id id;
    union {
        u32 octet;
        struct {
            u32 is_valid :1;
            u32 is_access :1;
        } s;
    } flag;
};

struct service_port {
    struct service_port_id id;
    union {
        u32 octet;
        struct {
            u32 is_valid :1;
        } s;
    } flag;
    struct ipv4_addr ip_addr;
    struct ipv4_addr ip_mask;
    struct ether_addr mac_addr;
};

struct logical_port {
    struct logical_port_id id;
    union {
        u32 octet;
        struct {
            u32 is_valid :1;
        } s;
    } flag;
    struct physical_port pp;
    struct service_port sp;
    struct ether_addr uumac;
};

#endif /* _FRAMEWORK_TYPES_H */
