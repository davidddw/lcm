/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Author Name: Xiang Yang
 * Date: 2013-9-22
 */

#ifndef _LC_BUS_H
#define _LC_BUS_H

#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>


#define LC_BUS_OK      0
#define LC_BUS_ERR     -1

#define LC_BUS_MAX_FRAME_SIZE  65536
#define LC_BUS_MAX_CHANNEL     256
#define LC_BUS_MAX_CONNECTION  32

enum LC_BUS_EXCHANGE {
    LC_BUS_EXCHANGE_DIRECT = 0,
    LC_BUS_EXCHANGE_FANOUT,
    LC_BUS_EXCHANGE_LCMD,
    LC_BUS_EXCHANGE_VMDRIVER,
    LC_BUS_EXCHANGE_VMADAPTER,
    LC_BUS_EXCHANGE_LCPD,
    LC_BUS_EXCHANGE_LCMOND,
    LC_BUS_EXCHANGE_LCSNFD,
    LC_BUS_EXCHANGE_POSTMAND,
    LC_BUS_EXCHANGE_LCVWA,
    LC_BUS_EXCHANGE_LCRMD,
    LC_BUS_EXCHANGE_TALKER,
    LC_BUS_EXCHANGE_PMADAPTER,
    LC_BUS_EXCHANGE_MNTNCT,

    NUM_LC_BUS_EXCHANGE,
};

enum LC_BUS_QUEUE {
    LC_BUS_QUEUE_LCMD = 0,
    LC_BUS_QUEUE_VMDRIVER,
    LC_BUS_QUEUE_VMADAPTER,
    LC_BUS_QUEUE_LCPD,
    LC_BUS_QUEUE_LCMOND,
    LC_BUS_QUEUE_LCSNFD,
    LC_BUS_QUEUE_POSTMAND,
    LC_BUS_QUEUE_LCVWA,
    LC_BUS_QUEUE_LCRMD,
    LC_BUS_QUEUE_TALKER,
    LC_BUS_QUEUE_PMADAPTER,
    LC_BUS_QUEUE_MNTNCT,

    NUM_LC_BUS_QUEUE,
};

enum LC_BUS_BINDING {
    LC_BUS_BINDING_DIRECT_LCMD = 0,
    LC_BUS_BINDING_DIRECT_VMDRIVER,
    LC_BUS_BINDING_DIRECT_VMADAPTER,
    LC_BUS_BINDING_DIRECT_LCPD,
    LC_BUS_BINDING_DIRECT_LCMOND,
    LC_BUS_BINDING_DIRECT_LCSNFD,
    LC_BUS_BINDING_DIRECT_POSTMAND,
    LC_BUS_BINDING_DIRECT_LCVWA,
    LC_BUS_BINDING_DIRECT_LCRMD,
    LC_BUS_BINDING_DIRECT_TALKER,
    LC_BUS_BINDING_DIRECT_PMADAPTER,
    LC_BUS_BINDING_DIRECT_MNTNCT,

    NUM_LC_BUS_BINDING,
};


int lc_bus_init();
int lc_bus_exit();
amqp_connection_state_t *lc_bus_get_connection();

int lc_bus_publish_unicast(const char *buf, int buf_len, uint32_t queue_id);

enum LC_BUS_CONSUME_FLAG {
    LC_BUS_CONSUME_WITH_ACK = 0x01,
};
int lc_bus_init_consumer(uint32_t flag, uint32_t queue_id);
int lc_bus_thread_init();
int lc_bus_data_in_buffer(amqp_connection_state_t *conn);
int lc_bus_consume_message(amqp_connection_state_t *conn,
        uint32_t flag, amqp_envelope_t *envelope,
        amqp_frame_t *frame, amqp_message_t *message,
        char *buf, int buf_len);


#endif /* _LC_BUS_H */
