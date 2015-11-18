#include <inttypes.h>
#include "message/msg.h"

bool is_lc_msg(struct lc_msg_head *m_head)
{
    if (!m_head) {
        return false;
    }

    return (m_head->magic & 0xFFFF0000) == (MSG_MG_DRV2KER & 0xFFFF0000);
}

bool is_msg_to_drv(struct lc_msg_head *m_head)
{
    return m_head && (
           m_head->magic == MSG_MG_KER2DRV
        || m_head->magic == MSG_MG_UI2DRV
        || m_head->magic == MSG_MG_VCD2DRV
        || m_head->magic == MSG_MG_MON2DRV
        || m_head->magic == MSG_MG_CHK2DRV);
}

bool is_msg_to_ker(struct lc_msg_head *m_head)
{
    return m_head && (
           m_head->magic == MSG_MG_DRV2KER
        || m_head->magic == MSG_MG_CHK2KER
        || m_head->magic == MSG_MG_APP2KER
        || m_head->magic == MSG_MG_VCD2KER
        || m_head->magic == MSG_MG_MON2KER
        || m_head->magic == MSG_MG_UI2KER
        || m_head->magic == MSG_MG_KER2APP
        || m_head->magic == MSG_MG_UI2APP
        || m_head->magic == MSG_MG_VWA2KER);
}

bool is_msg_to_rm(struct lc_msg_head *m_head)
{
    return m_head && (
           m_head->magic == MSG_MG_UI2RM
        || m_head->magic == MSG_MG_DRV2RM);
}

bool is_msg_to_ui(struct lc_msg_head *m_head)
{
    return m_head && (
           m_head->magic == MSG_MG_APP2UI
        || m_head->magic == MSG_MG_CHK2UI);
}

bool is_msg_to_chk(struct lc_msg_head *m_head)
{
    return m_head && (
           m_head->magic == MSG_MG_UI2CHK
        || m_head->magic == MSG_MG_KER2CHK
        || m_head->magic == MSG_MG_DRV2CHK
        || m_head->magic == MSG_MG_MON2CHK
        || m_head->magic == MSG_MG_SNF2CHK);
}

bool is_msg_to_mon(struct lc_msg_head *m_head)
{
    return m_head && m_head->magic == MSG_MG_CHK2MON;
}

bool is_msg_to_3rd(struct lc_msg_head *m_head)
{
    return m_head && m_head->magic == MSG_MG_UI2VWA;
}

bool is_msg_to_snf(struct lc_msg_head *m_head)
{
    return m_head && m_head->magic == MSG_MG_CHK2SNF;
}

void dump_lc_msg_head(FILE *out_fd,
        struct lc_msg_head *m_head)
{
    u32 i = 0;

    if (out_fd == NULL || m_head == NULL) {
        return;
    }

    fprintf(out_fd, "\n");
    fprintf(out_fd, "magic = 0x%08x, in ASCII: ", m_head->magic);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    for (i = sizeof(m_head->magic); i > 0; i--) {
        fprintf(out_fd, "%c", *((char *)&m_head->magic + i - 1));
    }
#elif __BYTE_ORDER == __BIG_ENDIAN
    for (i = 0; i < sizeof(m_head->magic); i++) {
        fprintf(out_fd, "%c", *((char *)&m_head->magic + i));
    }
#endif
    fprintf(out_fd, "\n");
    fprintf(out_fd, "direction = 0x%08x\n", m_head->direction);
    fprintf(out_fd, "type = %d\n", m_head->type);
    fprintf(out_fd, "length = %d\n", m_head->length);
    fprintf(out_fd, "\n");

    return;
}

void dump_msg_ntf_sw_join(FILE *out_fd,
        struct msg_ntf_sw_join *sw_join)
{
    u32 i = 0;
    u32 j = 0;

    if (out_fd == NULL || sw_join == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_join->datapath_id);

    fprintf(out_fd, "port_total = %d\n",
            sw_join->port_total);

    fprintf(out_fd, "mgmt_ip = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", sw_join->mgmt_ip.octet[i]);
    }
    fprintf(out_fd, "\n");

    for (i = 0; i < sw_join->port_total; i++) {
        fprintf(out_fd, "port[%d].name = %s\n", i, sw_join->ports[i].name);
        fprintf(out_fd, "port[%d].flag.octet = 0x%08x\n", i, sw_join->ports[i].flag.octet);
        fprintf(out_fd, "port[%d].id = %d\n", i, sw_join->ports[i].id);
        fprintf(out_fd, "port[%d].speed = %d\n", i, sw_join->ports[i].speed);
        fprintf(out_fd, "port[%d].hw_addr = ", i);
        for (j = 0; j < ETHER_ADDR_LEN; j++) {
            fprintf(out_fd, "%02x:", sw_join->ports[i].hw_addr.octet[j]);
        }
        fprintf(out_fd, "\n");
    }

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_ntf_sw_leave(FILE *out_fd,
        struct msg_ntf_sw_leave *sw_leave)
{
    if (out_fd == NULL || sw_leave == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_leave->datapath_id);

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_ntf_sw_recv_pkt(FILE *out_fd,
        struct msg_ntf_sw_recv_pkt *sw_recv_pkt)
{
    u32 i = 0;

    if (out_fd == NULL || sw_recv_pkt == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_recv_pkt->datapath_id);

    fprintf(out_fd, "in_port = %d\n",
            sw_recv_pkt->in_port);

    fprintf(out_fd, "in_sw_buf_id = 0x%08x\n",
            sw_recv_pkt->in_sw_buf_id);

    fprintf(out_fd, "pkt_len = %d\n",
            sw_recv_pkt->pkt_len);

    fprintf(out_fd, "pkt_snap.dl_src = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", sw_recv_pkt->pkt_snap.dl_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.dl_dst = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", sw_recv_pkt->pkt_snap.dl_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.dl_type = %04x\n", sw_recv_pkt->pkt_snap.dl_type);

    fprintf(out_fd, "pkt_snap.nw_src = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", sw_recv_pkt->pkt_snap.nw_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.nw_dst = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", sw_recv_pkt->pkt_snap.nw_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.nw_proto = 0x%02x\n",
            sw_recv_pkt->pkt_snap.nw_proto);

    fprintf(out_fd, "pkt_snap.tp_src = 0x%04x\n",
            sw_recv_pkt->pkt_snap.tp_src);

    fprintf(out_fd, "pkt_snap.tp_dst = 0x%04x\n",
            sw_recv_pkt->pkt_snap.tp_dst);

    fprintf(out_fd, "pkt_snap.flag.s.is_snap_valid = %d\n",
            sw_recv_pkt->pkt_snap.flag.s.is_snap_valid);

    fprintf(out_fd, "pkt_snap.flag.s.is_packet_full = %d\n",
            sw_recv_pkt->pkt_snap.flag.s.is_packet_full);

    fprintf(out_fd, "pkt_data:");
    for (i = 0; i < 64; i++) {
        if ((i & 0xf) == 0) {
            fprintf(out_fd, "\n");
        }
        fprintf(out_fd, "%02x ", sw_recv_pkt->pkt_data[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_ntf_sw_port_change(FILE *out_fd,
        struct msg_ntf_sw_port_change *sw_port_change)
{
    u32 i = 0;

    if (out_fd == NULL || sw_port_change == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_port_change->datapath_id);

    fprintf(out_fd, "reason = 0x%04x\n",
            sw_port_change->reason);

    fprintf(out_fd, "port.name = %s\n", sw_port_change->port.name);
    fprintf(out_fd, "port.flag.octet = 0x%08x\n", sw_port_change->port.flag.octet);
    fprintf(out_fd, "port.id = %d\n", sw_port_change->port.id);
    fprintf(out_fd, "port.speed = %d\n", sw_port_change->port.speed);
    fprintf(out_fd, "port.hw_addr = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", sw_port_change->port.hw_addr.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_req_sw_send_pkt(FILE *out_fd,
        struct msg_req_sw_send_pkt *sw_send_pkt)
{
    u32 i = 0;

    if (out_fd == NULL || sw_send_pkt == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_send_pkt->datapath_id);

    fprintf(out_fd, "out_port = %d\n",
            sw_send_pkt->out_port);

    fprintf(out_fd, "out_sw_buf_id = 0x%08x\n",
            sw_send_pkt->out_sw_buf_id);

    fprintf(out_fd, "pkt_len = %d\n",
            sw_send_pkt->pkt_len);

    fprintf(out_fd, "pkt_data:");
    for (i = 0; i < 64; i++) {
        if ((i & 0xf) == 0) {
            fprintf(out_fd, "\n");
        }
        fprintf(out_fd, "%02x ", sw_send_pkt->pkt_data[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_req_sw_set_flow(FILE *out_fd,
        struct msg_req_sw_set_flow *sw_set_flow)
{
    u32 i = 0;
    u32 j = 0;

    if (out_fd == NULL || sw_set_flow == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            sw_set_flow->datapath_id);

    fprintf(out_fd, "in_port = %d\n",
            sw_set_flow->in_port);

    fprintf(out_fd, "in_sw_buf_id = 0x%08x\n",
            sw_set_flow->in_sw_buf_id);

    fprintf(out_fd, "idle_timeout = %d\n",
            sw_set_flow->idle_timeout);

    fprintf(out_fd, "hard_timeout = %d\n",
            sw_set_flow->hard_timeout);

    fprintf(out_fd, "pattern.wildcards = 0x%08x\n",
            sw_set_flow->pattern.wildcards);

    fprintf(out_fd, "pattern.in_port = %d\n",
            sw_set_flow->pattern.in_port);

    fprintf(out_fd, "pattern.dl_src = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", sw_set_flow->pattern.dl_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pattern.dl_dst = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", sw_set_flow->pattern.dl_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pattern.dl_type = 0x%04x\n",
            sw_set_flow->pattern.dl_type);

    fprintf(out_fd, "pattern.nw_src = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", sw_set_flow->pattern.nw_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pattern.nw_dst = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", sw_set_flow->pattern.nw_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pattern.nw_proto = 0x%02x\n",
            sw_set_flow->pattern.nw_proto);

    fprintf(out_fd, "pattern.tp_src = 0x%04x\n",
            sw_set_flow->pattern.tp_src);

    fprintf(out_fd, "pattern.tp_dst = 0x%04x\n",
            sw_set_flow->pattern.tp_dst);

    fprintf(out_fd, "action_num = %d\n",
            sw_set_flow->action_num);

    fprintf(out_fd, "actions:\n");
    for (i = 0; i < MAX_ACTION; i++) {
        for (j = 0; j < ACTION_LEN; j++) {
            fprintf(out_fd, "%02x ", sw_set_flow->actions[i][j]);
        }
        fprintf(out_fd, "\n");
    }

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_ntf_se_demand(FILE *out_fd,
        struct msg_ntf_se_demand *se_demand)
{
    u32 i = 0;

    if (out_fd == NULL || se_demand == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "sp_id.vnet_id = 0x%d\n",
            se_demand->sp_id.vnet_id);

    fprintf(out_fd, "sp_id.vl2_id = 0x%d\n",
            se_demand->sp_id.vl2_id);

    fprintf(out_fd, "sp_id.vm_id = 0x%d\n",
            se_demand->sp_id.vm_id );

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            se_demand->datapath_id);

    fprintf(out_fd, "in_port = %d\n",
            se_demand->in_port);

    fprintf(out_fd, "in_sw_buf_id = 0x%08x\n",
            se_demand->in_sw_buf_id);

    fprintf(out_fd, "pkt_len = %d\n",
            se_demand->pkt_len);

    fprintf(out_fd, "pkt_snap.dl_src = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", se_demand->pkt_snap.dl_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.dl_dst = ");
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        fprintf(out_fd, "%02x:", se_demand->pkt_snap.dl_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.dl_type = %04x\n", se_demand->pkt_snap.dl_type);

    fprintf(out_fd, "pkt_snap.nw_src = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", se_demand->pkt_snap.nw_src.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.nw_dst = ");
    for (i = 0; i < IPV4_ADDR_LEN; i++) {
        fprintf(out_fd, "%d.", se_demand->pkt_snap.nw_dst.octet[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "pkt_snap.nw_proto = 0x%02x\n",
            se_demand->pkt_snap.nw_proto);

    fprintf(out_fd, "pkt_snap.tp_src = 0x%04x\n",
            se_demand->pkt_snap.tp_src);

    fprintf(out_fd, "pkt_snap.tp_dst = 0x%04x\n",
            se_demand->pkt_snap.tp_dst);

    fprintf(out_fd, "pkt_snap.flag.s.is_snap_valid = %d\n",
            se_demand->pkt_snap.flag.s.is_snap_valid);

    fprintf(out_fd, "pkt_snap.flag.s.is_packet_full = %d\n",
            se_demand->pkt_snap.flag.s.is_packet_full);

    fprintf(out_fd, "pkt_data:");
    for (i = 0; i < 64; i++) {
        if ((i & 0xf) == 0) {
            fprintf(out_fd, "\n");
        }
        fprintf(out_fd, "%02x ", se_demand->pkt_data[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_req_addr_reply(FILE *out_fd,
        struct msg_req_addr_reply *addr_reply)
{
    u32 i = 0;

    if (out_fd == NULL || addr_reply == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "tsp_id.vnet_id = 0x%d\n",
            addr_reply->tsp_id.vnet_id);

    fprintf(out_fd, "tsp_id.vl2_id = 0x%d\n",
            addr_reply->tsp_id.vl2_id);

    fprintf(out_fd, "tsp_id.vm_id = 0x%d\n",
            addr_reply->tsp_id.vm_id);

    fprintf(out_fd, "pkt_len = %d\n",
            addr_reply->pkt_len);

    fprintf(out_fd, "pkt_data:");
    for (i = 0; i < 64; i++) {
        if ((i & 0xf) == 0) {
            fprintf(out_fd, "\n");
        }
        fprintf(out_fd, "%02x ", addr_reply->pkt_data[i]);
    }
    fprintf(out_fd, "\n");

    fprintf(out_fd, "\n");

    return;
}

void dump_msg_req_conn_accept(FILE *out_fd,
        struct msg_req_conn_accept *conn_accept)
{
    if (out_fd == NULL || conn_accept == NULL) {
        return;
    }

    fprintf(out_fd, "\n");

    fprintf(out_fd, "ssp_id.vnet_id = 0x%d\n",
            conn_accept->ssp_id.vnet_id);

    fprintf(out_fd, "ssp_id.vl2_id = 0x%d\n",
            conn_accept->ssp_id.vl2_id);

    fprintf(out_fd, "ssp_id.vm_id = 0x%d\n",
            conn_accept->ssp_id.vm_id);

    fprintf(out_fd, "dsp_id.vnet_id = 0x%d\n",
            conn_accept->dsp_id.vnet_id);

    fprintf(out_fd, "dsp_id.vl2_id = 0x%d\n",
            conn_accept->dsp_id.vl2_id);

    fprintf(out_fd, "dsp_id.vm_id = 0x%d\n",
            conn_accept->dsp_id.vm_id);

    fprintf(out_fd, "datapath_id = 0x%016"PRIx64"\n",
            conn_accept->datapath_id);

    fprintf(out_fd, "in_port = %d\n",
            conn_accept->in_port);

    fprintf(out_fd, "in_sw_buf_id = 0x%08x\n",
            conn_accept->in_sw_buf_id);

    fprintf(out_fd, "\n");

    return;
}

