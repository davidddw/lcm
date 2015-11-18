/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-03-08
 *
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "lc_snf.h"
#include "lc_db.h"
#include "lcs_utils.h"
#include "jhash.h"
#include "lcs_list.h"
#include "ovs_rsync.h"
#include "ovs_process.h"
#include "lc_mongo_db.h"
#include "lcs_usage_db.h"

extern void ovs_plugin_app_at_header(void *item);
extern void ovs_plugin_app_at_tailer(void *item);

#define QUEUE_SIZE         65536
#define ASYN_THREAD_NUM    8
#define ASYN_BUSY_INTERVAL 1  // us
#define ASYN_FREE_INTERVAL 1  // s

#define TIMEOUT   ( TCP_SYN_TIMEOUT + 1 )
#define HEARTBEAT 60
#define BW 12
#define BN ( 1 << (32-BW) )
#define MAX_COUNT_TO_PROCESS \
    ( (u32)((1 << 31) / sizeof(struct bucket_item)) )  // 2GB

#define __swap(x,y) { (x) ^= (y); (y) ^= (x); (x) ^= (y); }

int debug_lcsnfd = 0, debug_dfi = 0;

typedef struct {
    struct flow_format *data;
    u32 size;
    __be32 datapath;
} pkt_t;

typedef struct {
    pkt_t pkt[QUEUE_SIZE];
    pthread_mutex_t mutex;
    int n_head;
    int num;
} queue_t;

static struct bucket_item *BUCKET[BN];
static u8 rcu;

static queue_t pkt_queue;
static u32 count;

static pthread_t tid[ASYN_THREAD_NUM + 1];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define LCSNFD_MAC_LEN 18

static int mac_cmp(void *val1, void *val2);
static void mac_del(void *val);

list_t g_mac_list = {.head=NULL, .count=0, .cmp=mac_cmp, .del=mac_del};

typedef struct {
    char mac[LCSNFD_MAC_LEN];
    uint32_t id;
}vif_node_t;

static int mac_cmp(void *val1, void *val2)
{
    vif_node_t *vif1 = (vif_node_t *)val1;
    vif_node_t *vif2 = (vif_node_t *)val2;

    return strcmp(vif1->mac, vif2->mac);
}

static void mac_del(void *val)
{
    free(val);
}

static void ovs_data_debug(void *data, u32 size)
{
    struct flow_format *flow;
    u32 pos = 0;
    do {
        flow = (struct flow_format *)(data + pos);
        pos += sizeof(struct flow_format);
        if (flow->key.eth.type == L3_IP) {
            SNF_SYSLOG(LOG_DEBUG,
#if __WORDSIZE == 64
                    "DEBUG: [used:%lu][pkt:%lu][byte:%lu]"
#else
                    "DEBUG: [used:%"PRIu64"][pkt:%"PRIu64"][byte:%"PRIu64"]"
#endif
                    "[proto:%hhu][flag:0x%02x][0x%04x][%u.%u.%u.%u->%u.%u.%u.%u][%u->%u]\n",
                    flow->used, flow->packet_count,
                    flow->byte_count, flow->key.ip.proto, flow->tcp_flags, ntohs(flow->key.eth.type),
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<24))>>24,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<16))>>16,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<8))>>8,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<0))>>0,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<24))>>24,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<16))>>16,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<8))>>8,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<0))>>0,
                    ntohs(flow->key.ipv4.tp.src), ntohs(flow->key.ipv4.tp.dst));
            continue;
        }
        if (flow->key.eth.type == L3_ARP) {
            SNF_SYSLOG(LOG_DEBUG,
#if __WORDSIZE == 64
                    "DEBUG: [used:%lu][pkt:%lu][byte:%lu]"
#else
                    "DEBUG: [used:%"PRIu64"][pkt:%"PRIu64"][byte:%"PRIu64"]"
#endif
                    "[0x%04x][%u.%u.%u.%u->%u.%u.%u.%u]"
                    "[%02x:%02x:%02x:%02x:%02x:%02x->%02x:%02x:%02x:%02x:%02x:%02x]\n",
                    flow->used, flow->packet_count,
                    flow->byte_count, ntohs(flow->key.eth.type),
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<24))>>24,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<16))>>16,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<8))>>8,
                    (ntohl(flow->key.ipv4.addr.src)&(0xFF<<0))>>0,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<24))>>24,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<16))>>16,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<8))>>8,
                    (ntohl(flow->key.ipv4.addr.dst)&(0xFF<<0))>>0,
                    flow->key.eth.src[0], flow->key.eth.src[1], flow->key.eth.src[2],
                    flow->key.eth.src[3], flow->key.eth.src[4], flow->key.eth.src[5],
                    flow->key.eth.dst[0], flow->key.eth.dst[1], flow->key.eth.dst[2],
                    flow->key.eth.dst[3], flow->key.eth.dst[4], flow->key.eth.dst[5]);
            continue;
        }
        if (flow->key.eth.type == L3_IPv6) {
            SNF_SYSLOG(LOG_DEBUG,
#if __WORDSIZE == 64
                    "DEBUG: [used:%lu][pkt:%lu][byte:%lu]"
#else
                    "DEBUG: [used:%"PRIu64"][pkt:%"PRIu64"][byte:%"PRIu64"]"
#endif
                    "[proto:%hhu][flag:0x%02x][0x%04x]"
                    "[%02x:%02x:%02x:%02x:%02x:%02x->%02x:%02x:%02x:%02x:%02x:%02x]\n",
                    flow->used, flow->packet_count,
                    flow->byte_count, flow->key.ip.proto, flow->tcp_flags, ntohs(flow->key.eth.type),
                    flow->key.eth.src[0], flow->key.eth.src[1], flow->key.eth.src[2],
                    flow->key.eth.src[3], flow->key.eth.src[4], flow->key.eth.src[5],
                    flow->key.eth.dst[0], flow->key.eth.dst[1], flow->key.eth.dst[2],
                    flow->key.eth.dst[3], flow->key.eth.dst[4], flow->key.eth.dst[5]);
            continue;
        }
    } while (pos < size);
    SNF_SYSLOG(LOG_DEBUG, "%s() display data of size %u/%u ended\n", __FUNCTION__, pos, size);
}

static int insert_item(struct bucket_item **item, struct data_key *key, struct flow_format *flow)
{
    struct timeval start;
    int rv;
    if (count > MAX_COUNT_TO_PROCESS) {
        rv = -ENOMEM;
        goto err;
    }
    *item = (struct bucket_item *)malloc(sizeof(struct bucket_item));
    if (*item == NULL) {
        rv = -ENOMEM;
        goto err;
    }
    memset(*item, 0, sizeof(struct bucket_item));
    memcpy(&(*item)->key, key, sizeof(struct data_key));
    memcpy(&(*item)->mac, (key->direction == 0) ? flow->key.eth.src : flow->key.eth.dst, 6);
    switch (key->protocol) {
        case L4_TCP:
            if ((flow->tcp_flags & OVS_FLOW_TCP_SYN) != 0
                   && flow->packet_count < 3) {
                (*item)->deadline = TCP_SYN_TIMEOUT;
                break;
            }
            if (flow->key.ipv4.tp.dst == 80) {
                (*item)->deadline = TCP_SHORT_TIMEOUT;
                break;
            }
            (*item)->deadline = TCP_LONG_TIMEOUT;
            break;
        case L4_UDP:
            (*item)->deadline = UDP_TIMEOUT;
            break;
        case L4_ICMP:
            (*item)->deadline = ICMP_TIMEOUT;
            break;
        default:
            (*item)->deadline = ANY_TIMEOUT;
            break;
    }
    (*item)->sequence = 1;
    (*item)->stat.used = (u32)flow->used;
    (*item)->stat.flag |= flow->tcp_flags;
    (*item)->stat.packet_count = flow->packet_count;
    (*item)->stat.byte_count = flow->byte_count;
    gettimeofday(&start, NULL);
    (*item)->stat.duration = 0;
    (*item)->timer = (u64)start.tv_sec;
    (*item)->next = NULL;
    count++;
    return 0;

err:
    return rv;
}

static void update_item(struct bucket_item *item, struct flow_format *flow)
{
    struct timeval cont;
    if (item->sequence == 1 && item->stat.packet_count < 3
            && (item->stat.flag & OVS_FLOW_TCP_SYN) != 0
            && (flow->tcp_flags & OVS_FLOW_TCP_SYN) == 0) {
        if (flow->key.ipv4.tp.dst == 80) {
            item->deadline = TCP_SHORT_TIMEOUT;
        } else {
            item->deadline = TCP_LONG_TIMEOUT;
        }
    }
    item->sequence += 1;
    item->stat.used = (u32)flow->used;
    item->stat.flag |= flow->tcp_flags;
    item->stat.packet_count += flow->packet_count;
    item->stat.byte_count += flow->byte_count;
    gettimeofday(&cont, NULL);
    item->stat.duration += ((u64)cont.tv_sec - item->timer);
    item->timer = (u64)cont.tv_sec;
}

static void change_item(struct bucket_item *item, struct flow_format *flow)
{
    struct timeval cont;
    item->deadline = PROMPT;
    if ((flow->tcp_flags & OVS_FLOW_TCP_RST) == 0) {
        item->deadline = ANY_TIMEOUT;
        item->sequence += 1;
        item->stat.used = (u32)flow->used;
        item->stat.byte_count += flow->byte_count;
    }
    item->stat.flag |= flow->tcp_flags;
    item->stat.packet_count += flow->packet_count;
    gettimeofday(&cont, NULL);
    item->stat.duration += ((u64)cont.tv_sec - item->timer);
    item->timer = (u64)cont.tv_sec;
}

static int is_valid_flow(char *src_mac, char *dst_mac)
{
    list_node_t *listnode;
    vif_node_t *node;
    int vifid = 0;
    int res = 0, flag = 0;

    listnode = listnode_search(&g_mac_list, src_mac);
    if (!listnode) {
        node = (vif_node_t *)malloc(sizeof(vif_node_t));
        memset(node, 0, sizeof(vif_node_t));
        snprintf(node->mac, LCSNFD_MAC_LEN, "%s", src_mac);
        listnode_add(&g_mac_list, node);
        res = lc_get_vifid_by_mac(&vifid, src_mac);
        if (!res) {
            node->id = vifid;
            flag = 1;
        }
    } else {
        node = listnode->data;
        flag = node->id;
    }

    if (!flag) {
        listnode = listnode_search(&g_mac_list, dst_mac);
        if (!listnode) {
            node = (vif_node_t *)malloc(sizeof(vif_node_t));
            memset(node, 0, sizeof(vif_node_t));
            snprintf(node->mac, LCSNFD_MAC_LEN, "%s", dst_mac);
            listnode_add(&g_mac_list, node);
            res = lc_get_vifid_by_mac(&vifid, dst_mac);
            if (!res) {
                node->id = vifid;
                flag = 1;
            }
        } else {
            node = listnode->data;
            flag = node->id;
        }
    }

    list_aged(&g_mac_list);
    return flag;
}

static int ovs_get_flow_info(struct bucket_item *item, flow_load_t *flow)
{
    uint32_t ip;
    struct timeval cont;

    memset(flow, 0, sizeof(flow_load_t));
    gettimeofday(&cont, NULL);

    flow->timestamp = (u64)cont.tv_sec - item->stat.duration;
    flow->packet_count = item->stat.packet_count;
    flow->byte_count = item->stat.byte_count;
    flow->duration = item->stat.duration;
    flow->used = item->stat.used;
    flow->datapath = item->key.datapath;
    flow->src_port = item->key.src_port;
    flow->dst_port = item->key.dst_port;
    flow->vlantag = item->key.eth.vlantag;
    flow->eth_type = item->key.eth.type;

    flow->protocol = item->key.protocol;
    flow->direction = item->key.direction;
    flow->tcp_flags = item->stat.flag;
    ip = htonl(item->key.src_addr);
    inet_ntop(AF_INET, (void *)&ip, flow->src_ip, sizeof(flow->src_ip));
    ip = htonl(item->key.dst_addr);
    inet_ntop(AF_INET, (void *)&ip, flow->dst_ip, sizeof(flow->dst_ip));

    snprintf(flow->src_mac, sizeof(flow->src_mac), "%02x:%02x:%02x:%02x:%02x:%02x", item->key.eth.src[0],
            item->key.eth.src[1], item->key.eth.src[2], item->key.eth.src[3], item->key.eth.src[4], item->key.eth.src[5]);
    snprintf(flow->dst_mac, sizeof(flow->dst_mac), "%02x:%02x:%02x:%02x:%02x:%02x", item->key.eth.dst[0],
            item->key.eth.dst[1], item->key.eth.dst[2], item->key.eth.dst[3], item->key.eth.dst[4], item->key.eth.dst[5]);

    if (!is_valid_flow(flow->src_mac, flow->dst_mac)) {
        //SNF_SYSLOG(LOG_INFO, "%s() ignore flow. mac:(%s, %s)\n", __FUNCTION__, flow->src_mac, flow->dst_mac);
        return 1;
    }
    return 0;
}

static int delete_item(struct bucket_item *item, flow_load_t *flow)
{
    int ret;
    ret = ovs_get_flow_info(item, flow);
    free(item);
    count--;
    return ret;
}

static int keycmp(struct data_key *x, struct data_key *y)
{
    return ((x->src_port != y->src_port)
            || (x->src_addr != y->src_addr)
            || (x->dst_port != y->dst_port)
            || (x->dst_addr != y->dst_addr)
            || (x->protocol != y->protocol)
            || (x->datapath != y->datapath));
}

static int bucket_insert(u32 index, struct data_key *key, struct flow_format *flow)
{
    struct bucket_item *item = BUCKET[index];
    int rv;
    if (item == NULL) {
        rv = insert_item(&BUCKET[index], key, flow);
        if (rv < 0) {
            goto err;
        }
        if (debug_lcsnfd) SNF_SYSLOG(LOG_DEBUG,
                "[I]protocol %u, packet_count %"PRIu64", deadline %"PRIu64", curr %lx, next %lx\n",
                BUCKET[index]->key.protocol,
                BUCKET[index]->stat.packet_count,
                BUCKET[index]->deadline,
                (unsigned long)BUCKET[index],
                (unsigned long)(BUCKET[index]->next));
        return 0;
    }
    do {
        if (keycmp(key, &item->key) == 0) {
            if ((flow->tcp_flags & OVS_FLOW_TCP_SYN) == 0
                    || ((flow->tcp_flags & OVS_FLOW_TCP_SYN) != 0
                    &&  (flow->tcp_flags & OVS_FLOW_TCP_ACK) != 0)) {
                update_item(item, flow);
            } else {
                if (key->direction != item->key.direction) {
                    key->direction = 0xFF;
                }
            }
            if (debug_lcsnfd) SNF_SYSLOG(LOG_DEBUG,
                    "[U]protocol %u, packet_count %"PRIu64", deadline %"PRIu64", curr %lx, next %lx\n",
                    item->key.protocol,
                    item->stat.packet_count,
                    item->deadline,
                    (unsigned long)item,
                    (unsigned long)(item->next));
            return 0;
        }
        item = item->next;
    } while (item != NULL);
    item = BUCKET[index];
    rv = insert_item(&BUCKET[index], key, flow);
    if (rv < 0) {
        goto err;
    }
    BUCKET[index]->next = item;
    if (debug_lcsnfd) SNF_SYSLOG(LOG_DEBUG,
            "[A]protocol %u, packet_count %"PRIu64", deadline %"PRIu64", curr %lx, next %lx\n",
            BUCKET[index]->key.protocol,
            BUCKET[index]->stat.packet_count,
            BUCKET[index]->deadline,
            (unsigned long)BUCKET[index],
            (unsigned long)(BUCKET[index]->next));
    return 0;

err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

static void bucket_delete(u32 index, struct data_key *key, struct flow_format *flow)
{
    struct bucket_item *item = BUCKET[index];
    if (item == NULL) {
        return;
    }
    do {
        if (keycmp(key, &item->key) == 0) {
            change_item(item, flow);
            if (debug_lcsnfd) SNF_SYSLOG(LOG_DEBUG,
                    "[C]protocol %u, packet_count %"PRIu64", deadline %"PRIu64", curr %lx, next %lx\n",
                    item->key.protocol,
                    item->stat.packet_count,
                    item->deadline,
                    (unsigned long)item,
                    (unsigned long)(item->next));
            return;
        }
        item = item->next;
    } while (item != NULL);
}

#define size_of_key(x) ( (size_t)(&((x)->direction)) - (size_t)(x) )
static void ovs_data_insert(__be32 datapath, struct flow_format *flows, u32 num)
{
    struct data_key key;
    u32 i, index;
    for (i=0; i<num; i++) {
        if (flows[i].key.eth.type == L3_IP) {
            key.datapath = datapath;
            key.src_addr = ntohl(flows[i].key.ipv4.addr.src);
            key.dst_addr = ntohl(flows[i].key.ipv4.addr.dst);
            key.src_port = ntohs(flows[i].key.ipv4.tp.src);
            key.dst_port = ntohs(flows[i].key.ipv4.tp.dst);
            key.protocol = flows[i].key.ip.proto;
            key.eth.vlantag = ntohs(flows[i].key.eth.tci);
            key.eth.type = ntohs(flows[i].key.eth.type);
            if (key.src_addr < key.dst_addr) {
                __swap(key.src_addr, key.dst_addr);
                __swap(key.src_port, key.dst_port);
                key.direction = 1;
                memcpy(key.eth.src, flows[i].key.eth.dst, 6);
                memcpy(key.eth.dst, flows[i].key.eth.src, 6);
            }else {
                key.direction = 0;
                memcpy(key.eth.src, flows[i].key.eth.src, 6);
                memcpy(key.eth.dst, flows[i].key.eth.dst, 6);
            }

            index = jhash(&key, size_of_key(&key), 0) >> BW;
            if (((flows[i].tcp_flags & OVS_FLOW_TCP_FIN) == 0)
                    && ((flows[i].tcp_flags & OVS_FLOW_TCP_RST) == 0)) {
                bucket_insert(index, &key, &flows[i]);
                continue;
            }
            if ((flows[i].tcp_flags & OVS_FLOW_TCP_SYN) != 0) {
                bucket_insert(index, &key, &flows[i]);
                if (key.direction == 0xFF) {
                    bucket_delete(index, &key, &flows[i]);
                }
                continue;
            }
            bucket_delete(index, &key, &flows[i]);
        }
    }
}

static u32 ovs_data_delete(u32 pos)
{
    u32 i, j = 0;
    struct timeval timer;
    struct bucket_item *item, *jtem;
    gettimeofday(&timer, NULL);
    flow_load_t *flow;
    int rv = 0;

    flow = malloc(sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
    memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
    /* iterates bucket list, save completed and outdated flows to db */
    for (i=pos; i<BN; i++) {
        if (rcu) {
            return i;
        }
        jtem = BUCKET[i];
        item = jtem;
        if (item != NULL) {
            do {
                if (debug_lcsnfd) SNF_SYSLOG(LOG_DEBUG,
                        "[D]protocol %u, packet_count %"PRIu64", deadline %"PRIu64", time %"PRIu64", curr %lx, next %lx\n",
                        item->key.protocol,
                        item->stat.packet_count,
                        item->deadline,
                        ((u64)timer.tv_sec - item->timer),
                        (unsigned long)item,
                        (unsigned long)item->next);
                if (((u64)timer.tv_sec - item->timer) >= item->deadline) {
                    if (item == BUCKET[i]) {
                        BUCKET[i] = BUCKET[i]->next;
                        jtem = BUCKET[i];
                        if (delete_item(item, &flow[j]) == 0) {
                            j++;
                        }
                        item = jtem;
                    } else {
                        jtem->next = item->next;
                        if (delete_item(item, &flow[j]) == 0) {
                            j++;
                        }
                        item = jtem->next;
                    }
                } else {
                    jtem = item;
                    item = jtem->next;
                }

                if (j >= OVS_FLOW_BATCH_NUM) {

                    if ((rv = mongo_db_entrys_batch_insert(MONGO_APP_FLOW_LOAD, flow, j)) < 0 ) {
                        SNF_SYSLOG(LOG_ERR, "LINE %d failed(rv=%d)\n", __LINE__, rv);
                        goto err;
                    }
                    memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
                    j = 0;
                }
            } while (item != NULL);
        }
    }

    if (j > 0) {
        if ((rv = mongo_db_entrys_batch_insert(MONGO_APP_FLOW_LOAD, flow, j)) < 0 ) {
            SNF_SYSLOG(LOG_ERR, "LINE %d failed(rv=%d)\n", __LINE__, rv);
            goto err;
        }
        memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
        j = 0;
    }

err:
    if (flow){
        free(flow);
    }
    return 0;
}

static void ovs_data_release(void)
{
    int rv;
    u32 i, j = 0;
    struct bucket_item *item, *jtem;
    struct flow_format *data;
    flow_load_t *flow;
    flow = malloc(sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);

    memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
    for (i=0; i<BN; i++) {
        item = BUCKET[i];
        if (item != NULL) {
            do {
                jtem = item->next;
                if (delete_item(item, &flow[j]) == 0) {
                    j++;
                }
                item = jtem;
                if (j >= OVS_FLOW_BATCH_NUM){

                    if ((rv = mongo_db_entrys_batch_insert(MONGO_APP_FLOW_LOAD, flow, j)) < 0 ) {
                        SNF_SYSLOG(LOG_ERR, "%s(%d) failed(rv=%d)\n", __FUNCTION__, __LINE__, rv);
                        goto err;
                    }
                    memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
                    j = 0;
                }
            } while (item != NULL);
        }
    }
    if (j > 0) {

        if ((rv = mongo_db_entrys_batch_insert(MONGO_APP_FLOW_LOAD, flow, j)) < 0 ) {
            SNF_SYSLOG(LOG_ERR, "%s(%d) failed(rv=%d)\n", __FUNCTION__, __LINE__, rv);
            goto err;
        }
        memset(flow, 0, sizeof(flow_load_t) * OVS_FLOW_BATCH_NUM);
        j = 0;
    }

err:
    for (i=0; i<QUEUE_SIZE; i++) {
        data = pkt_queue.pkt[i].data;
        if (data) {
            free(data);
        }
    }

    if (flow){
        free(flow);
    }
    return;
}

static int ovs_asyn_process_handler(pkt_t *pkt)
{
    if (rcu == 1 && pthread_mutex_lock(&mutex) == 0) {
        ovs_data_insert(pkt->datapath, pkt->data, pkt->size);
        pthread_mutex_unlock(&mutex);
    }
    return rcu;
}

static int pkt_queue_push(pkt_t *pkt)
{
    int curr;

    if (pkt_queue.num >= QUEUE_SIZE) {
        SNF_SYSLOG(LOG_ERR, "pkt_queue is full, drop packet");
        return -1;
    }

    curr = (pkt_queue.n_head + pkt_queue.num) % QUEUE_SIZE;
    memcpy(&pkt_queue.pkt[curr], pkt, sizeof(pkt_t));

    pkt_queue.num++;
    return 0;
}

static int pkt_queue_pop(pkt_t *pkt)
{
    u32 curr;

    if (pkt_queue.num <= 0) {
        return -1;
    }

    curr = pkt_queue.n_head;
    memcpy(pkt, &pkt_queue.pkt[curr], sizeof(pkt_t));
    pkt_queue.n_head = (curr + 1) % QUEUE_SIZE;

    pkt_queue.num--;
    return 0;
}

static void *ovs_asyn_process(void *trash)
{
    /* deque flows in flow receiving queue */

    int empty;
    pkt_t pkt;
    lc_db_thread_init();

    for (;;) {
        if (pthread_mutex_lock(&pkt_queue.mutex) == 0) {
            empty = pkt_queue_pop(&pkt);
            pthread_mutex_unlock(&pkt_queue.mutex);

            if (!empty && pkt.data) {
                if (debug_lcsnfd) ovs_data_debug(pkt.data, pkt.size);
                pkt.size /= sizeof(struct flow_format);
                ovs_asyn_process_handler(&pkt);
                free(pkt.data);
            }
            if (empty) {
                sleep(ASYN_FREE_INTERVAL);
            }
        }
        usleep(ASYN_BUSY_INTERVAL);
    }

    return NULL;
}

int ovs_data_process(void *data, u32 size, __be32 datapath)
{
    /* enque flows received from dfi agent */

    int rv;
    pkt_t pkt = {
        .data = data,
        .size = size,
        .datapath = datapath,
    };
    if (pthread_mutex_lock(&pkt_queue.mutex) == 0) {
        rv = pkt_queue_push(&pkt);
        pthread_mutex_unlock(&pkt_queue.mutex);
        return rv;
    }
    return -1;
}

static void *ovs_data_timeout(void *trash)
{
    /* save aggregated flows to mongodb */

    struct timeval heartbeat, timeout, cont;
    u32 pos = 0;
    gettimeofday(&heartbeat, NULL);
    gettimeofday(&timeout, NULL);

    lc_db_thread_init();
    if (mongo_handler_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "mongo_handler_init error\n");
        exit(1);
    }
    g_mac_list.timestamp = time(NULL);

    for (;;) {
        if (count) {
            SNF_SYSLOG(LOG_DEBUG, "before aggregate, %u flows in bucket list", count);
        }
        rcu = 0;
        if (pthread_mutex_lock(&mutex) == 0) {
            pos = ovs_data_delete(pos);
            pthread_mutex_unlock(&mutex);
        }
        rcu = 1;
        if (count) {
            SNF_SYSLOG(LOG_DEBUG, "after aggregate, %u flows in bucket list", count);
        }

        /* waiting for flow timeout in bucket list,
         * signal pull request to dfi agent when necessary */
        for (;;) {
            gettimeofday(&cont, NULL);
            if (cont.tv_sec - heartbeat.tv_sec > HEARTBEAT) {
                struct user_acl user_setted_acl = {
                    .magic  = DFI_VERSION_MAGIC,
                    .state  = ACL_OPEN,
                    .space  = ACL_IN_USER,
                    .option = ACL_FLOW_ANY | ACL_RSYNC,
                };
                if (debug_dfi) {
                    user_setted_acl.option |= ACL_DEBUG;
                }
                memset(&user_setted_acl.key, 0, sizeof(user_setted_acl.key));
                memset(&user_setted_acl.key_wc, 0, sizeof(user_setted_acl.key_wc));

                rsync_dfi_ctrl_message(&user_setted_acl,
                        sizeof(struct user_acl), MSG_TP_CTRL_REQUEST);
                gettimeofday(&heartbeat, NULL);
            }
            sleep(1);
            if (cont.tv_sec - timeout.tv_sec > TIMEOUT || (rcu == 0 && pos != 0)) {
                gettimeofday(&timeout, NULL);
                break;
            }
        }
    }

    mongo_handler_exit();
    return NULL;
}

int ovs_timer_open(void)
{
    int i, rv;

    pthread_mutex_init(&pkt_queue.mutex, NULL);

    if (pthread_create(&tid[0], NULL, ovs_data_timeout, NULL) != 0) {
        rv = -ECHILD;
        goto err;
    }
    for (i=1; i<=ASYN_THREAD_NUM; i++) {
        if (pthread_create(&tid[i], NULL, ovs_asyn_process, NULL) != 0) {
            rv = -ECHILD;
            goto err;
        }
    }
    return 0;

err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

void ovs_timer_close(void)
{
    int i;
    for (i=ASYN_THREAD_NUM; i>=0; i--) {
        pthread_join(tid[i], NULL);
    }
    ovs_data_release();
}
