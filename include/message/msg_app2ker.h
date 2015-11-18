#ifndef _MESSAGE_MSG_APP2KER_H
#define _MESSAGE_MSG_APP2KER_H

#include "framework/types.h"

struct msg_req_addr_reply {
    struct service_port_id tsp_id;

    u16 pkt_len;
    u16 reserve;
    u8 pkt_data[PACKET_BUFFER];
};

struct msg_req_conn_accept {
    struct service_port_id ssp_id;
    struct service_port_id dsp_id;

    u64 datapath_id;
    u16 in_port;
    u32 in_sw_buf_id;
};

struct msg_pkt_out_hdr {
    struct service_port_id tsp_id;
    u16 pkt_len;
};
struct msg_bootp_msg_relay {
    struct msg_pkt_out_hdr hdr;
    char data[1500];
};

#endif /* _MESSAGE_MSG_APP2KER_H */
