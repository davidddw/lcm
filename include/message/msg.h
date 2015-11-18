#ifndef _MESSAGE_MSG_H
#define _MESSAGE_MSG_H

#include <stdio.h>

#include "framework/types.h"
#include "message/msg_drv2ker.h"
#include "message/msg_ker2drv.h"
#include "message/msg_ker2app.h"
#include "message/msg_app2ker.h"
#include "message/msg_app2lcm.h"
#include "message/msg_lcm2app.h"
#include "message/msg_bootp.h"
#include "message/msg_vm.h"
#include "message/msg_host.h"
#include "message/msg_enum.h"
#include "lc_header.h"

#define MAX_MSG_BUFFER (1 << 15)
#define MAX_PATH_LEN 8

struct lc_msg_head {
    u32 magic;
    u32 direction;
    u32 type;
    u32 length;
    u32 seq;
    u32 time;
};

typedef struct lc_msg_head lc_mbuf_hdr_t;

typedef struct lc_mbuf_s {
    lc_mbuf_hdr_t hdr;
    u8  data[MAX_MSG_BUFFER];
} lc_mbuf_t;

typedef struct lc_pb_mbuf_s {
    header_t hdr;
    u8  data[MAX_MSG_BUFFER];
} lc_pb_mbuf_t;

static inline void lc_mbuf_hdr_ntoh(lc_mbuf_hdr_t *p_hdr)
{
    if (p_hdr) {
        p_hdr->magic     = ntohl(p_hdr->magic);
        p_hdr->direction = ntohl(p_hdr->direction);
        p_hdr->type      = ntohl(p_hdr->type);
        p_hdr->length    = ntohl(p_hdr->length);
        p_hdr->seq       = ntohl(p_hdr->seq);
    }
}

static inline void lc_mbuf_hdr_hton(lc_mbuf_hdr_t *p_hdr)
{
    if (p_hdr) {
        p_hdr->magic     = htonl(p_hdr->magic);
        p_hdr->direction = htonl(p_hdr->direction);
        p_hdr->type      = htonl(p_hdr->type);
        p_hdr->length    = htonl(p_hdr->length);
        p_hdr->seq       = htonl(p_hdr->seq);
    }
}

bool is_lc_msg(struct lc_msg_head *m_head);
bool is_msg_to_drv(struct lc_msg_head *m_head);
bool is_msg_to_ker(struct lc_msg_head *m_head);
bool is_msg_to_rm(struct lc_msg_head *m_head);
bool is_msg_to_app(struct lc_msg_head *m_head);
bool is_msg_to_ui(struct lc_msg_head *m_head);
bool is_msg_to_chk(struct lc_msg_head *m_head);
bool is_msg_to_mon(struct lc_msg_head *m_head);
bool is_msg_to_snf(struct lc_msg_head *m_head);
bool is_msg_to_3rd(struct lc_msg_head *m_head);

void dump_lc_msg_head(FILE *out_fd,
        struct lc_msg_head *m_head);
void dump_msg_ntf_sw_join(FILE *out_fd,
        struct msg_ntf_sw_join *sw_join);
void dump_msg_ntf_sw_leave(FILE *out_fd,
        struct msg_ntf_sw_leave *sw_leave);
void dump_msg_ntf_sw_recv_pkt(FILE *out_fd,
        struct msg_ntf_sw_recv_pkt *sw_recv_pkt);
void dump_msg_ntf_sw_port_change(FILE *out_fd,
        struct msg_ntf_sw_port_change *sw_port_change);
void dump_msg_req_sw_send_pkt(FILE *out_fd,
        struct msg_req_sw_send_pkt *sw_send_pkt);
void dump_msg_req_sw_set_flow(FILE *out_fd,
        struct msg_req_sw_set_flow *sw_set_flow);
void dump_msg_ntf_se_demand(FILE *out_fd,
        struct msg_ntf_se_demand *se_demand);
void dump_msg_req_addr_reply(FILE *out_fd,
        struct msg_req_addr_reply *addr_reply);
void dump_msg_req_conn_accept(FILE *out_fd,
        struct msg_req_conn_accept *conn_accept);

int lc_msg_get_path(int type, int now);
int lc_msg_get_rollback_type(int type);
int lc_msg_get_rollback_destination(int *dest, int type, int now);

#endif /* _MESSAGE_MSG_H */
