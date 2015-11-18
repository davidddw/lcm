#ifndef _MESSAGE_MSG_KER2APP_H
#define _MESSAGE_MSG_KER2APP_H

#include "framework/types.h"
#include "message/msg_drv2ker.h"

struct msg_ntf_se_demand {
    struct service_port_id sp_id;

    u64 datapath_id;
    u16 in_port;
    u16 reserve;
    u32 in_sw_buf_id;

    u32 pkt_len;
    struct packet_snap pkt_snap;
    u8 pkt_data[PACKET_BUFFER];
};

struct msg_rep_app_join {
};

struct msg_rep_app_leave {
};

#endif /* _MESSAGE_MSG_KER2APP_H */
