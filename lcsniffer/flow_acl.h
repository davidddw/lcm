/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-29
 *
 */

#ifndef __FLOW_ACL_H__
#define __FLOW_ACL_H__


#include <inttypes.h>
#include <stddef.h>
#include <linux/types.h>

#define ETH_ALEN  6

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef FIELD_SIZEOF
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#endif

#if 0
typedef uint64_t __bitwise__ __be64;
typedef uint32_t __bitwise__ __be32;
typedef uint16_t __bitwise__ __be16;
typedef uint8_t  __bitwise__ __be8;
#endif

/* Warnning: currently, user_acl* must be the same as kernel_acl* */

struct user_acl_key {
    u16    phy_in_port;  /* Input switch port (or DP_MAX_PORTS). */
    u8     eth_src[ETH_ALEN];  /* Ethernet source address. */
    u8     eth_dst[ETH_ALEN];  /* Ethernet destination address. */
    __be16 eth_tci;   /* 0 if no VLAN, VLAN_TAG_PRESENT set otherwise. */
    __be16 eth_type;  /* Ethernet frame type. */
    u8     ip_proto;  /* IP protocol or lower 8 bits of ARP opcode. */
    union {
        struct {
            __be32 src;  /* IP source address. */
            __be32 dst;  /* IP destination address. */
        } ipv4;
        struct {
            struct in6_addr src;  /* IPv6 source address. */
            struct in6_addr dst;  /* IPv6 destination address. */
        } ipv6;
    };
    __be16 tp_src;    /* TCP/UDP source port. */
    __be16 tp_dst;    /* TCP/UDP destination port. */
};

struct user_acl_key_wildcard {
    union {
        struct {
            u32 fields;
            u8 eth_dst_type;
            u8 ip_src_prefix_len;
            u8 ip_dst_prefix_len;
        } mini_wc;
        u64 hash;
    };
};

enum ETH_ADDR_TYPES {
    ETH_ADDR_UNICAST = 0,
    ETH_ADDR_BROADCAST = 1,
    ETH_ADDR_MULTICAST = 2,

    NUM_ETH_ADDR_TYPES,
};

enum ACL_KEY_FIELDS {
    AKF_PHY_IN_PORT = 0,
    AKF_ETH_SRC,
    AKF_ETH_DST,
    AKF_ETH_TCI,
    AKF_ETH_TYPE,
    AKF_IP_PROTO,
    AKF_IPV4_SRC,
    AKF_IPV4_DST,
    AKF_IPV6_SRC,
    AKF_IPV6_DST,
    AKF_TP_SRC,
    AKF_TP_DST,

    NUM_ACL_KEY_FIELDS,
};

static const u8 ACL_KEY_FIELD_OFFSET[NUM_ACL_KEY_FIELDS] = {
    [ AKF_PHY_IN_PORT ] = offsetof(struct user_acl_key, phy_in_port),
    [ AKF_ETH_SRC     ] = offsetof(struct user_acl_key, eth_src),
    [ AKF_ETH_DST     ] = offsetof(struct user_acl_key, eth_dst),
    [ AKF_ETH_TCI     ] = offsetof(struct user_acl_key, eth_tci),
    [ AKF_ETH_TYPE    ] = offsetof(struct user_acl_key, eth_type),
    [ AKF_IP_PROTO    ] = offsetof(struct user_acl_key, ip_proto),
    [ AKF_IPV4_SRC    ] = offsetof(struct user_acl_key, ipv4.src),
    [ AKF_IPV4_DST    ] = offsetof(struct user_acl_key, ipv4.dst),
    [ AKF_IPV6_SRC    ] = offsetof(struct user_acl_key, ipv6.src),
    [ AKF_IPV6_DST    ] = offsetof(struct user_acl_key, ipv6.dst),
    [ AKF_TP_SRC      ] = offsetof(struct user_acl_key, tp_src),
    [ AKF_TP_DST      ] = offsetof(struct user_acl_key, tp_dst),
};

static const u8 ACL_KEY_FIELD_SIZEOF[NUM_ACL_KEY_FIELDS] = {
    [ AKF_PHY_IN_PORT ] = FIELD_SIZEOF(struct user_acl_key, phy_in_port),
    [ AKF_ETH_SRC     ] = FIELD_SIZEOF(struct user_acl_key, eth_src),
    [ AKF_ETH_DST     ] = FIELD_SIZEOF(struct user_acl_key, eth_dst),
    [ AKF_ETH_TCI     ] = FIELD_SIZEOF(struct user_acl_key, eth_tci),
    [ AKF_ETH_TYPE    ] = FIELD_SIZEOF(struct user_acl_key, eth_type),
    [ AKF_IP_PROTO    ] = FIELD_SIZEOF(struct user_acl_key, ip_proto),
    [ AKF_IPV4_SRC    ] = FIELD_SIZEOF(struct user_acl_key, ipv4.src),
    [ AKF_IPV4_DST    ] = FIELD_SIZEOF(struct user_acl_key, ipv4.dst),
    [ AKF_IPV6_SRC    ] = FIELD_SIZEOF(struct user_acl_key, ipv6.src),
    [ AKF_IPV6_DST    ] = FIELD_SIZEOF(struct user_acl_key, ipv6.dst),
    [ AKF_TP_SRC      ] = FIELD_SIZEOF(struct user_acl_key, tp_src),
    [ AKF_TP_DST      ] = FIELD_SIZEOF(struct user_acl_key, tp_dst),
};

enum {
    ACL_CLOSE     = 0x0,
    ACL_OPEN      = 0x1,

    ACL_IN_USER   = 0x1,
    ACL_IN_KERN   = 0x2,

    ACL_FLOW_NONE = 0x00000000,
    ACL_FLOW_ANY  = 0x000FFFFF,
    ACL_FLOW_IP   = 0x00000001,
    ACL_FLOW_IPv6 = 0x00000002,
    ACL_FLOW_TCP  = 0x00000004,
    ACL_FLOW_UDP  = 0x00000008,
    ACL_FLOW_ICMP = 0x00000010,
    ACL_FLOW_ARP  = 0x00000020,
    ACL_FLOW_TLSP = 0x00000040,
    ACL_FLOW_GRE  = 0x00000080,

    ACL_DEBUG     = 0x00100000,
    ACL_RESTART   = 0x00200000,
    ACL_RESET     = 0x00400000,
    ACL_RSYNC     = 0x00800000,
    ACL_ISOLATE   = 0x01000000,
    ACL_RELEASE   = 0x02000000,
};

#define DFI_VERSION_MAGIC  0X1C310000
#define DFI_VERSION        "3.1"
#define __DFI_VERSION__    \
    "DFI v" DFI_VERSION "(" TOSTRING(DFI_VERSION_MAGIC) ") [" __DATE__ " " __TIME__ "]"

struct user_acl {
    u32 magic;
    u32 state;
    u32 space;
    u32 option;
    struct user_acl_key key;
    struct user_acl_key_wildcard key_wc;
};

/* Example:
 *
 * uint8_t mac[ETH_ADDR_LEN];
 *    [...]
 * printf("The Ethernet address is "ETH_ADDR_FMT"\n", ETH_ADDR_ARGS(mac));
 *
 */
#define ETH_ADDR_FMT                                                    \
    "%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8
#define ETH_ADDR_ARGS(ea)                                   \
    (ea)[0], (ea)[1], (ea)[2], (ea)[3], (ea)[4], (ea)[5]

/* Example:
 *
 * char *string = "1 00:11:22:33:44:55 2";
 * uint8_t mac[ETH_ADDR_LEN];
 * int a, b;
 *
 * if (sscanf(string, "%d"ETH_ADDR_SCAN_FMT"%d",
 *     &a, ETH_ADDR_SCAN_ARGS(mac), &b) == 1 + ETH_ADDR_SCAN_COUNT + 1) {
 *     ...
 * }
 */
#define ETH_ADDR_SCAN_FMT "%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8":%"SCNx8
#define ETH_ADDR_SCAN_ARGS(ea) \
        &(ea)[0], &(ea)[1], &(ea)[2], &(ea)[3], &(ea)[4], &(ea)[5]
#define ETH_ADDR_SCAN_COUNT 6

/* The "(void) (ip)[0]" below has no effect on the value, since it's the first
 * argument of a comma expression, but it makes sure that 'ip' is a pointer.
 * This is useful since a common mistake is to pass an integer instead of a
 * pointer to IP_ARGS. */
#define IP_FMT "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8
#define IP_ARGS(ip)                             \
        ((void) (ip)[0], ((uint8_t *) ip)[0]),  \
        ((uint8_t *) ip)[1],                    \
        ((uint8_t *) ip)[2],                    \
        ((uint8_t *) ip)[3]

/* Example:
 *
 * char *string = "1 33.44.55.66 2";
 * __be32 ip;
 * int a, b;
 *
 * if (sscanf(string, "%d"IP_SCAN_FMT"%d",
 *     &a, IP_SCAN_ARGS(&ip), &b) == 1 + IP_SCAN_COUNT + 1) {
 *     ...
 * }
 */
#define IP_SCAN_FMT "%"SCNu8".%"SCNu8".%"SCNu8".%"SCNu8
#define IP_SCAN_ARGS(ip)                                    \
        ((void) (__be32) *(ip), &((uint8_t *) ip)[0]),    \
        &((uint8_t *) ip)[1],                               \
        &((uint8_t *) ip)[2],                               \
        &((uint8_t *) ip)[3]
#define IP_SCAN_COUNT 4

/* Example:
 *
 * char *string = "1 ::1 2";
 * char ipv6_s[IPV6_SCAN_LEN + 1];
 * struct in6_addr ipv6;
 *
 * if (sscanf(string, "%d"IPV6_SCAN_FMT"%d", &a, ipv6_s, &b) == 3
 *     && inet_pton(AF_INET6, ipv6_s, &ipv6) == 1) {
 *     ...
 * }
 */
#define IPV6_SCAN_FMT "%46[0123456789abcdefABCDEF:.]"
#define IPV6_SCAN_LEN 46


#endif /* __FLOW_ACL_H__ */
