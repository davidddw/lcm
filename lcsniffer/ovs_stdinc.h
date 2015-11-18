/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-02-28
 *
 */

#ifndef __OVS_STDINC_H__
#define __OVS_STDINC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <syslog.h>
#include <netinet/in.h>

#include "flow_acl.h"

struct ovs_key_ipv4_tunnel {
    __be64 tun_id;
    __be32 ipv4_src;
    __be32 ipv4_dst;
    u16  tun_flags;
    u8   ipv4_tos;
    u8   ipv4_ttl;
} __attribute__ ((packed));

struct flow_key {
#if 0
    struct ovs_key_ipv4_tunnel tun_key;  /* Encapsulating tunnel key. */
    struct {
        u32    priority;   /* Packet QoS priority. */
        u32    skb_mark;   /* SKB mark. */
        u16    in_port;    /* Input switch port (or DP_MAX_PORTS). */
    } phy;
#endif
    struct {
        u8     src[6];     /* Ethernet source address. */
        u8     dst[6];     /* Ethernet destination address. */
        __be16 tci;        /* 0 if no VLAN, VLAN_TAG_PRESENT set otherwise. */
        __be16 type;       /* Ethernet frame type. */
    } eth;
    struct {
        u8     proto;      /* IP protocol or lower 8 bits of ARP opcode. */
#if 0
        u8     tos;        /* IP ToS. */
        u8     ttl;        /* IP TTL/hop limit. */
        u8     frag;       /* One of OVS_FRAG_TYPE_*. */
#endif
    } ip;
    union {
        struct {
            struct {
                __be32 src;    /* IP source address. */
                __be32 dst;    /* IP destination address. */
            } addr;
            union {
                struct {
                    __be16 src;    /* TCP/UDP source port. */
                    __be16 dst;    /* TCP/UDP destination port. */
                } tp;
                struct {
                    u8 sha[6];     /* ARP source hardware address. */
                    u8 tha[6];     /* ARP target hardware address. */
                } arp;
            };
        } ipv4;
        struct {
            struct {
                struct in6_addr src;    /* IPv6 source address. */
                struct in6_addr dst;    /* IPv6 destination address. */
            } addr;
            __be32 label;          /* IPv6 flow label. */
            struct {
                __be16 src;        /* TCP/UDP source port. */
                __be16 dst;        /* TCP/UDP destination port. */
            } tp;
            struct {
                struct in6_addr target;    /* ND target address. */
                u8 sll[6];    /* ND source link layer address. */
                u8 tll[6];    /* ND target link layer address. */
            } nd;
        } ipv6;
    };
} __attribute__ ((packed));

struct flow_format {
    struct flow_key key;
    u64 used;           /* Last used time (in jiffies). */
    u64 packet_count;   /* Number of packets matched. */
    u64 byte_count;     /* Number of bytes matched. */
    u8 tcp_flags;       /* Union of seen TCP flags. */
} __attribute__ ((packed));

#define size_in_block(size) ( (size) - (size) % sizeof(struct flow_format) )

#endif /* __OVS_STDINC_H__ */
