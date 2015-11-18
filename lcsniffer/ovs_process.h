/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-03-08
 *
 */

#ifndef __OVS_PROCESS_H__
#define __OVS_PROCESS_H__

#include "ovs_stdinc.h"

#define OVS_FLOW_TCP_FIN    0x01
#define OVS_FLOW_TCP_SYN    0x02
#define OVS_FLOW_TCP_RST    0x04
#define OVS_FLOW_TCP_ACK    0x10

struct user_mem {
    u8 *addr;
    u32 off;
    u32 size;
};

struct data_eth {
    u8     src[6];
    u8     dst[6];
    u16    vlantag;
    u16    type;
};

struct data_key {
    u32 datapath;
    u32 src_addr;
    u32 dst_addr;
    u16 src_port;
    u16 dst_port;
    u8  protocol;
    u8  direction;
    struct data_eth eth;
};

struct data_stat {
    u64 packet_count;
    u64 byte_count;
    u64 duration;
    u32 used;
    u8  flag;
};

struct bucket_item {
    struct bucket_item *next;
    u64 deadline;
    u64 timer;
    u16 sequence;
    struct data_key key;
    struct data_stat stat;
    //struct data_eth eth;
    u8  mac[6];
};

enum protocols {
    L3_IP   = 0x0008,
    L3_ARP  = 0x0608,
    L3_IPv6 = 0xDD86,

    L4_TCP  = 6,
    L4_UDP  = 17,
    L4_ICMP = 1,
    L4_TLSP = 56,
    L4_GRE  = 47
};

enum deadlines {
    ANY_TIMEOUT = 30,
    TCP_SYN_TIMEOUT = 5,
    TCP_SHORT_TIMEOUT = 180,
    TCP_LONG_TIMEOUT = 1800,
    UDP_TIMEOUT = 300,
    ICMP_TIMEOUT = 60,
    PROMPT = 3
};

int  ovs_timer_open(void);
void ovs_timer_close(void);
int  ovs_data_process(void *data, u32 size, __be32 datapath);

#endif /* __OVS_PROCESS_H__ */
