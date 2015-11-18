/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Author Name: Xiang Yang
 * Date: 2013-9-22
 * Ref: http://www.rabbitmq.com/amqp-0-9-1-reference.html
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>

#ifdef WITH_SSL
#include <amqp_ssl_socket.h>
#endif
#include <amqp_tcp_socket.h>

#include "lc_bus.h"

static int lc_bus_reset();
static amqp_connection_state_t *lc_bus_get_connection_atomic();
static int init_amqp_connection(amqp_connection_state_t *conn);
static int exit_amqp_connection();
static int exit_amqp_connection_atomic();
static int lc_bus_init_consumer_atomic(amqp_connection_state_t *conn,
        uint32_t flag, uint32_t queue_id);
static int lc_bus_consume_message_atomic(amqp_connection_state_t *conn,
        uint32_t flag, amqp_envelope_t *envelope,
        amqp_frame_t *frame, amqp_message_t *message, char *buf, int buf_len);
static int lc_bus_exchange_init(amqp_connection_state_t *conn, uint32_t exchange_id);
static int lc_bus_queue_init(amqp_connection_state_t *conn, uint32_t queue_id);
static int lc_bus_binding_init(amqp_connection_state_t *conn, uint32_t binding_id);
static const char *amqp_server_exception_string(const amqp_rpc_reply_t *r);
static const char *amqp_rpc_reply_string(const amqp_rpc_reply_t *r);

#define MINV(a, b) ((a) < (b) ? (a) : (b))
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LB_LOG_LEVEL_HEADER        "LOG_LEVEL: "
int LB_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *LB_LOG_LEVEL_P = &LB_LOG_DEFAULT_LEVEL;

char* LB_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define LB_SYSLOG(level, format, ...)  \
	if(level <= *LB_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), LB_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
		}

#define RECV_TIMEOUT 3

static char *lc_bus_exchange[][2] = { /* exchange name, exchange type */
    [LC_BUS_EXCHANGE_DIRECT    ] = {"lce.direct",    "direct"},
    [LC_BUS_EXCHANGE_FANOUT    ] = {"lce.fanout",    "fanout"},
    [LC_BUS_EXCHANGE_LCMD      ] = {"lce.lcmd",      "topic"},
    [LC_BUS_EXCHANGE_VMDRIVER  ] = {"lce.vmdriver",  "topic" },
    [LC_BUS_EXCHANGE_VMADAPTER ] = {"lce.vmadapter", "topic" },
    [LC_BUS_EXCHANGE_LCPD      ] = {"lce.lcpd",      "topic" },
    [LC_BUS_EXCHANGE_LCMOND    ] = {"lce.lcmond",    "topic" },
    [LC_BUS_EXCHANGE_LCSNFD    ] = {"lce.lcsnfd",    "topic" },
    [LC_BUS_EXCHANGE_POSTMAND  ] = {"lce.postman",   "topic" },
    [LC_BUS_EXCHANGE_LCVWA     ] = {"lce.lcvwa",     "topic" },
    [LC_BUS_EXCHANGE_LCRMD     ] = {"lce.lcrmd",     "topic" },
    [LC_BUS_EXCHANGE_TALKER    ] = {"lce.talker",    "topic" },
    [LC_BUS_EXCHANGE_PMADAPTER ] = {"lce.pmadapter", "topic" },
    [LC_BUS_EXCHANGE_MNTNCT    ] = {"lce.mntnct",    "topic" },
};

static char *lc_bus_queue[] = { /* queue name */
    [LC_BUS_QUEUE_LCMD      ] = "lcq.lcmd",
    [LC_BUS_QUEUE_VMDRIVER  ] = "lcq.vmdriver",
    [LC_BUS_QUEUE_VMADAPTER ] = "lcq.vmadapter",
    [LC_BUS_QUEUE_LCPD      ] = "lcq.lcpd",
    [LC_BUS_QUEUE_LCMOND    ] = "lcq.lcmond",
    [LC_BUS_QUEUE_LCSNFD    ] = "lcq.lcsnfd",
    [LC_BUS_QUEUE_POSTMAND  ] = "lcq.postman",
    [LC_BUS_QUEUE_LCVWA     ] = "lcq.lcvwa",
    [LC_BUS_QUEUE_LCRMD     ] = "lcq.lcrmd",
    [LC_BUS_QUEUE_TALKER    ] = "lcq.talker",
    [LC_BUS_QUEUE_PMADAPTER ] = "lcq.pmadapter",
    [LC_BUS_QUEUE_MNTNCT    ] = "lcq.mntnct",
};

static char *lc_bus_binding_key[] = {
    [LC_BUS_BINDING_DIRECT_LCMD      ] = "lcq.lcmd",
    [LC_BUS_BINDING_DIRECT_VMDRIVER  ] = "lcq.vmdriver",
    [LC_BUS_BINDING_DIRECT_VMADAPTER ] = "lcq.vmadapter",
    [LC_BUS_BINDING_DIRECT_LCPD      ] = "lcq.lcpd",
    [LC_BUS_BINDING_DIRECT_LCMOND    ] = "lcq.lcmond",
    [LC_BUS_BINDING_DIRECT_LCSNFD    ] = "lcq.lcsnfd",
    [LC_BUS_BINDING_DIRECT_POSTMAND  ] = "lcq.postman",
    [LC_BUS_BINDING_DIRECT_LCVWA     ] = "lcq.lcvwa",
    [LC_BUS_BINDING_DIRECT_LCRMD     ] = "lcq.lcrmd",
    [LC_BUS_BINDING_DIRECT_TALKER    ] = "lcq.talker",
    [LC_BUS_BINDING_DIRECT_PMADAPTER ] = "lcq.pmadapter",
    [LC_BUS_BINDING_DIRECT_MNTNCT    ] = "lcq.mntnct",
};

static int lc_bus_binding[][2] = { /* exchange id, queue id */
    [LC_BUS_BINDING_DIRECT_LCMD      ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCMD, },
    [LC_BUS_BINDING_DIRECT_VMDRIVER  ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_VMDRIVER, },
    [LC_BUS_BINDING_DIRECT_VMADAPTER ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_VMADAPTER, },
    [LC_BUS_BINDING_DIRECT_LCPD      ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCPD, },
    [LC_BUS_BINDING_DIRECT_LCMOND    ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCMOND, },
    [LC_BUS_BINDING_DIRECT_LCSNFD    ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCSNFD, },
    [LC_BUS_BINDING_DIRECT_POSTMAND  ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_POSTMAND, },
    [LC_BUS_BINDING_DIRECT_LCVWA     ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCVWA, },
    [LC_BUS_BINDING_DIRECT_LCRMD     ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_LCRMD, },
    [LC_BUS_BINDING_DIRECT_TALKER    ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_TALKER, },
    [LC_BUS_BINDING_DIRECT_PMADAPTER ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_PMADAPTER, },
    [LC_BUS_BINDING_DIRECT_MNTNCT    ] = {
        LC_BUS_EXCHANGE_DIRECT, LC_BUS_QUEUE_MNTNCT, },

};

#define LC_BUS_CHANNEL  1
static pthread_mutex_t bus_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t connection_tid[LC_BUS_MAX_CHANNEL];
amqp_connection_state_t lc_conn[LC_BUS_MAX_CONNECTION];

int lc_bus_init()
{
    uint32_t i;
    int res = LC_BUS_OK;
    amqp_connection_state_t *conn = NULL;

    conn = lc_bus_get_connection();
    if (!conn) {
        return LC_BUS_ERR;
    }

    for (i = 0; i < NUM_LC_BUS_EXCHANGE; ++i) {
        res += lc_bus_exchange_init(conn, i);
    }
    for (i = 0; i < NUM_LC_BUS_QUEUE; ++i) {
        res += lc_bus_queue_init(conn, i);
    }
    for (i = 0; i < NUM_LC_BUS_BINDING; ++i) {
        res += lc_bus_binding_init(conn, i);
    }

    return res;
}

int lc_bus_thread_init()
{
    amqp_connection_state_t *conn = lc_bus_get_connection();
    if (!conn) {
        return LC_BUS_ERR;
    }
    return LC_BUS_OK;
}

int lc_bus_exit()
{
    return exit_amqp_connection();
}

int lc_bus_publish_unicast(const char *buf, int buf_len, uint32_t queue_id)
{
    amqp_connection_state_t *conn = NULL;
    amqp_bytes_t body = { len: buf_len, bytes: (void *)buf };
    amqp_basic_properties_t props;
    int res = 0;

    if (!buf || queue_id >= NUM_LC_BUS_QUEUE) {
        return LC_BUS_ERR;
    }

    conn = lc_bus_get_connection();
    if (!conn) {
        return LC_BUS_ERR;
    }

    memset(&props, 0, sizeof props);
    props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.delivery_mode = 2; /* persistent delivery mode */

    res = amqp_basic_publish(
            *conn,
            LC_BUS_CHANNEL,
            amqp_cstring_bytes(lc_bus_exchange[LC_BUS_EXCHANGE_DIRECT][0]),
            amqp_cstring_bytes(lc_bus_queue[queue_id]),
            0, /* mandatory */
            0, /* immediate */
            &props,
            body
            );
    if (res) {
        LB_SYSLOG(LOG_ERR, "failed channel=%u queue=%u ret=%d\n",
                LC_BUS_CHANNEL, queue_id, res);
        return LC_BUS_ERR;
    } else {
        LB_SYSLOG(LOG_INFO, "succeed channel=%u queue=%u\n",
                LC_BUS_CHANNEL, queue_id);
    }

    return LC_BUS_OK;
}

int lc_bus_publish_broadcast(const char *buf, int buf_len)
{
    /* TODO */
    return LC_BUS_OK;
}

int lc_bus_publish_multicast(const char *buf, int buf_len)
{
    /* TODO */
    return LC_BUS_OK;
}

static int consumer_queueids[NUM_LC_BUS_QUEUE];

static int lc_bus_init_consumer_atomic(amqp_connection_state_t *conn,
        uint32_t flag, uint32_t queue_id)
{
    amqp_basic_consume_ok_t *res_consume = NULL;
    amqp_rpc_reply_t ret;

    if (queue_id >= NUM_LC_BUS_QUEUE) {
        return LC_BUS_ERR;
    }

#if 0
    amqp_basic_qos_ok_t *res_qos = NULL;

    if (count > 0 && count <= 65535 &&
        !(res_qos = amqp_basic_qos(
            *conn,
            1,
            0,
            count,
            0
            ))) {
        die_rpc(amqp_get_rpc_reply(*conn), "basic.qos");
        return LC_BUS_ERR;
    }
#endif

    res_consume = amqp_basic_consume(
            *conn,
            LC_BUS_CHANNEL,
            amqp_cstring_bytes(lc_bus_queue[queue_id]),
            amqp_empty_bytes, /* consumer_tag */
            0,                /* no_local */
            (flag & LC_BUS_CONSUME_WITH_ACK) ? 0 : 1, /* no_ack */
            0,                /* exclusive */
            amqp_empty_table  /* arguments */
            );
    if (!res_consume) {
        ret = amqp_get_rpc_reply(*conn);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            LB_SYSLOG(LOG_ERR, "[%s] failed, channel=%u %s\n",
                    lc_bus_queue[queue_id], LC_BUS_CHANNEL,
                    amqp_rpc_reply_string(&ret));
            return LC_BUS_ERR;
        }
    }

    consumer_queueids[queue_id] = 1;
    LB_SYSLOG(LOG_INFO, "[%s] succeed channel=%u.\n",
            lc_bus_queue[queue_id], LC_BUS_CHANNEL);
    return LC_BUS_OK;
}

int lc_bus_init_consumer(uint32_t flag, uint32_t queue_id)
{
    int ret = 0;
    amqp_connection_state_t *conn = NULL;

    conn = lc_bus_get_connection();
    if (!conn) {
        return LC_BUS_ERR;
    }

    pthread_mutex_lock(&bus_mutex);
    ret = lc_bus_init_consumer_atomic(conn, flag, queue_id);
    pthread_mutex_unlock(&bus_mutex);

    return ret;
}

int lc_bus_data_in_buffer(amqp_connection_state_t *conn)
{
    return amqp_frames_enqueued(*conn) ||
           amqp_data_in_buffer(*conn);
}

static int lc_bus_consume_message_atomic(amqp_connection_state_t *conn,
        uint32_t flag, amqp_envelope_t *envelope,
        amqp_frame_t *frame, amqp_message_t *message, char *buf, int buf_len)
{
    static struct timeval timeout = { tv_sec: RECV_TIMEOUT, tv_usec: 0 };
    size_t body_remaining = 0, offset = 0;
    int res = LC_BUS_OK;
    amqp_basic_deliver_t *deliver = NULL;

    if (!envelope || !frame || !message) {
        return 0;
    }

    amqp_maybe_release_buffers(*conn);
#if 0
    amqp_consume_message(*conn, envelope, &timeout, 0);
#endif

    res = amqp_simple_wait_frame_noblock(*conn, frame, &timeout);
    if (res) {
        LB_SYSLOG(LOG_ERR, "waiting for method frame, ret=%d.\n", res);
        if (res != AMQP_STATUS_TIMEOUT) {
            return -1;
        }
        return 0;
    }
    if (frame->frame_type != AMQP_FRAME_METHOD ||
        frame->payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
        LB_SYSLOG(LOG_WARNING, "got frame type 0x%X (expect AMQP_FRAME_METHOD "
                "0x%X) method 0x%X (expect AMQP_BASIC_DELIVER_METHOD 0x%X), "
                "ignore this message.\n", frame->frame_type, AMQP_FRAME_METHOD,
                frame->payload.method.id, AMQP_BASIC_DELIVER_METHOD);
        return 0;
    }
    LB_SYSLOG(LOG_INFO, "got frame type 0x%X method 0x%X.\n",
            frame->frame_type, frame->payload.method.id);
    deliver = (amqp_basic_deliver_t *)frame->payload.method.decoded;

    res = amqp_simple_wait_frame_noblock(*conn, frame, &timeout);
    if (res) {
        LB_SYSLOG(LOG_ERR, "waiting for header frame, ret=%d\n", res);
        if (res != AMQP_STATUS_TIMEOUT) {
            return -1;
        }
        return 0;
    }
    if (frame->frame_type != AMQP_FRAME_HEADER) {
        LB_SYSLOG(LOG_ERR, "got frame type 0x%X (expect "
                "AMQP_FRAME_HEADER 0x%X).\n",
                frame->frame_type, AMQP_FRAME_HEADER);
        return 0;
    }
    body_remaining = frame->payload.properties.body_size;
    LB_SYSLOG(LOG_INFO, "got frame type 0x%X (AMQP_FRAME_HEADER), "
            "AMQP_FRAME_BODY len %zd\n",
            frame->frame_type, body_remaining);

    while (body_remaining) {
        res = amqp_simple_wait_frame_noblock(*conn, frame, &timeout);
        if (res) {
            LB_SYSLOG(LOG_ERR, "waiting for body frame, ret=%d\n", res);
            if (res != AMQP_STATUS_TIMEOUT) {
                return -1;
            }
            return 0;
        }
        if (frame->frame_type != AMQP_FRAME_BODY) {
            LB_SYSLOG(LOG_ERR, "expected header, got frame type 0x%X\n",
                    frame->frame_type);
            return 0;
        }
        LB_SYSLOG(LOG_DEBUG, "got body len %zd\n",
                frame->payload.body_fragment.len);

        memcpy(buf + offset, frame->payload.body_fragment.bytes,
                MINV(buf_len - offset, frame->payload.body_fragment.len));
        if (buf_len - offset < frame->payload.body_fragment.len) {
            offset = buf_len;
        } else {
            offset += frame->payload.body_fragment.len;
        }
        body_remaining -= frame->payload.body_fragment.len;
    }

    if (flag & LC_BUS_CONSUME_WITH_ACK) {
        res = amqp_basic_ack(
                    *conn,
                    LC_BUS_CHANNEL,
                    deliver->delivery_tag,
                    0  /* multiple */
                    );
        if (res) {
            LB_SYSLOG(LOG_ERR, "basic ack, channel=%u ret=%d\n",
                    LC_BUS_CHANNEL, res);
            return 0;
        }
    }

    if (buf_len == offset) {
        /* buffer is not enough */
        return 0;
    }
    return offset;
}

int lc_bus_consume_message(amqp_connection_state_t *conn,
        uint32_t flag, amqp_envelope_t *envelope,
        amqp_frame_t *frame, amqp_message_t *message,
        char *buf, int buf_len)
{
    int ret = 0, len = 0;

    pthread_mutex_lock(&bus_mutex);
    ret = lc_bus_consume_message_atomic(conn, flag, envelope, frame, message,
            buf, buf_len);
    pthread_mutex_unlock(&bus_mutex);

    if (ret == -1) {
        LB_SYSLOG(LOG_WARNING, "reconnect rabbitmq now.\n");
        lc_bus_reset();
        len = 0;
    } else {
        len = ret;
    }

    return len;
}

/* static funcs */

static amqp_connection_state_t *lc_bus_get_connection_atomic()
{
    pthread_t t = pthread_self();
    amqp_rpc_reply_t ret;
    int res;
    int i;

    for (i = 0; i < LC_BUS_MAX_CONNECTION; ++i) {
        if (!connection_tid[i] || pthread_equal(connection_tid[i], t)) {
            break;
        }
    }
    if (i < LC_BUS_MAX_CHANNEL) {
        if (!connection_tid[i]) {
            res = init_amqp_connection(lc_conn + i);
            if (res == LC_BUS_OK) {
                LB_SYSLOG(LOG_INFO, "init_amqp_connection ret=%d\n", res);
            } else {
                LB_SYSLOG(LOG_ERR, "init_amqp_connection ret=%d\n", res);
                return NULL;
            }

            if (!amqp_channel_open(lc_conn[i], LC_BUS_CHANNEL)) {
                ret = amqp_get_rpc_reply(lc_conn[i]);
                if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
                    LB_SYSLOG(LOG_ERR, "error in opening channel %u, %s\n",
                            LC_BUS_CHANNEL, amqp_rpc_reply_string(&ret));
                    return NULL;
                }
            }

            connection_tid[i] = t;
        }
    } else {
        LB_SYSLOG(LOG_ERR, "connection is not enough, maximal=%u.\n",
                LC_BUS_MAX_CONNECTION);
        return NULL;
    }

    return lc_conn + i;
}

amqp_connection_state_t *lc_bus_get_connection()
{
    amqp_connection_state_t *conn = NULL;
    pthread_mutex_lock(&bus_mutex);
    conn = lc_bus_get_connection_atomic();
    pthread_mutex_unlock(&bus_mutex);
    return conn;
}

static int lc_bus_exchange_init(amqp_connection_state_t *conn, uint32_t exchange_id)
{
    amqp_exchange_declare_ok_t *res = NULL;
    amqp_rpc_reply_t ret;

    if (exchange_id >= NUM_LC_BUS_EXCHANGE) {
        return LC_BUS_ERR;
    }

    res = amqp_exchange_declare(
            *conn,
            LC_BUS_CHANNEL,
            amqp_cstring_bytes(lc_bus_exchange[exchange_id][0]),
            amqp_cstring_bytes(lc_bus_exchange[exchange_id][1]),
            0,               /* passive */
            1,               /* durable */
            0,
            0,
            amqp_empty_table /* arguments */
            );
    if (!res) {
        ret = amqp_get_rpc_reply(*conn);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            LB_SYSLOG(LOG_ERR, "[%s,%s] failed, channel=%u %s\n",
                    lc_bus_exchange[exchange_id][0],
                    lc_bus_exchange[exchange_id][1],
                    LC_BUS_CHANNEL, amqp_rpc_reply_string(&ret));
            return LC_BUS_ERR;
        }
    }

    LB_SYSLOG(LOG_INFO, "[%s,%s] succeed, channel=%u.\n",
            lc_bus_exchange[exchange_id][0],
            lc_bus_exchange[exchange_id][1], LC_BUS_CHANNEL);
    return LC_BUS_OK;
}

static int lc_bus_queue_init(amqp_connection_state_t *conn, uint32_t queue_id)
{
    amqp_queue_declare_ok_t *res = NULL;
    amqp_rpc_reply_t ret;

    if (queue_id >= NUM_LC_BUS_QUEUE) {
        return LC_BUS_ERR;
    }

    res = amqp_queue_declare(
            *conn,
            LC_BUS_CHANNEL,
            amqp_cstring_bytes(lc_bus_queue[queue_id]),
            0,               /* passive */
            1,               /* durable */
            0,               /* exclusive */
            0,               /* auto_delete */
            amqp_empty_table /* arguments */
            );
    if (!res) {
        ret = amqp_get_rpc_reply(*conn);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            LB_SYSLOG(LOG_ERR, "[%s] failed, channel=%u %s\n",
                    lc_bus_queue[queue_id], LC_BUS_CHANNEL,
                    amqp_rpc_reply_string(&ret));
            return LC_BUS_ERR;
        }
    }

    LB_SYSLOG(LOG_INFO, "[%s] succeed channel=%u.\n",
            lc_bus_queue[queue_id], LC_BUS_CHANNEL);
    return LC_BUS_OK;
}

static int lc_bus_binding_init(amqp_connection_state_t *conn, uint32_t binding_id)
{
    uint32_t exchange_id, queue_id;
    amqp_queue_bind_ok_t *res = NULL;
    amqp_rpc_reply_t ret;

    if (binding_id >= NUM_LC_BUS_BINDING) {
        return LC_BUS_ERR;
    }

    exchange_id = lc_bus_binding[binding_id][0];
    queue_id = lc_bus_binding[binding_id][1];
    res = amqp_queue_bind(
            *conn,
            LC_BUS_CHANNEL,
            amqp_cstring_bytes(lc_bus_queue[queue_id]),
            amqp_cstring_bytes(lc_bus_exchange[exchange_id][0]),
            amqp_cstring_bytes(lc_bus_binding_key[binding_id]),
            amqp_empty_table /* arguments */
            );
    if (!res) {
        ret = amqp_get_rpc_reply(*conn);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            LB_SYSLOG(LOG_ERR, "[%s,%s,%s] failed, channel=%u %s\n",
                    lc_bus_exchange[exchange_id][0], lc_bus_queue[queue_id],
                    lc_bus_binding_key[binding_id],
                    LC_BUS_CHANNEL, amqp_rpc_reply_string(&ret));
            return LC_BUS_ERR;
        }
    }

    LB_SYSLOG(LOG_INFO, "[%s,%s,%s] succeed channel=%u.\n",
            lc_bus_exchange[exchange_id][0], lc_bus_queue[queue_id],
            lc_bus_binding_key[binding_id], LC_BUS_CHANNEL);
    return LC_BUS_OK;
}


static int init_amqp_connection(amqp_connection_state_t *conn)
{
    int status;
    amqp_socket_t *socket = NULL;
    struct amqp_connection_info ci;
    amqp_rpc_reply_t ret;

    ci.user = "guest";
    ci.password = "guest";
    ci.host = "localhost";
    ci.vhost = "/";
    ci.port = 20001;
    ci.ssl = 0;
    *conn = amqp_new_connection();

    if (ci.ssl) {
#ifdef WITH_SSL
        socket = amqp_ssl_socket_new(*conn);
        if (!socket) {
            LB_SYSLOG(LOG_ERR, "creating SSL/TLS socket\n");
            return LC_BUS_ERR;
        }
        if (amqp_cacert) {
            amqp_ssl_socket_set_cacert(socket, amqp_cacert);
        }
        if (amqp_key) {
            amqp_ssl_socket_set_key(socket, amqp_cert, amqp_key);
        }
#else
        LB_SYSLOG(LOG_ERR, "librabbitmq was not built with SSL/TLS support\n");
        return LC_BUS_ERR;
#endif
    } else {
        socket = amqp_tcp_socket_new(*conn);
        if (!socket) {
            LB_SYSLOG(LOG_ERR, "creating TCP socket (out of memory)\n");
            return LC_BUS_ERR;
        }
    }

    status = amqp_socket_open(socket, ci.host, ci.port);
    if (status) {
        LB_SYSLOG(LOG_ERR, "opening socket to %s:%d, status=%d, errno=%s.\n",
                ci.host, ci.port, status, strerror(errno));
        return LC_BUS_ERR;
    }
    ret = amqp_login(
                *conn,
                ci.vhost,
                LC_BUS_MAX_CHANNEL, /* channel max */
                LC_BUS_MAX_FRAME_SIZE,
                0,      /* heartbeat */
                AMQP_SASL_METHOD_PLAIN,
                ci.user,
                ci.password
                );
    if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
        LB_SYSLOG(LOG_ERR, "error in logging AMQP server, %s\n",
                amqp_rpc_reply_string(&ret));
        return LC_BUS_ERR;
    }

    return LC_BUS_OK;
}

static int exit_amqp_connection_atomic()
{
    int res = LC_BUS_OK;
    amqp_rpc_reply_t ret;
    int i;

    for (i = 0; i < LC_BUS_MAX_CONNECTION; ++i) {
        if (connection_tid[i]) {
            ret = amqp_channel_close(lc_conn[i],
                    LC_BUS_CHANNEL, AMQP_REPLY_SUCCESS);
            if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
                LB_SYSLOG(LOG_ERR, "error in closing channel: %s\n",
                        amqp_rpc_reply_string(&ret));
            }

            ret = amqp_connection_close(lc_conn[i], AMQP_REPLY_SUCCESS);
            if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
                LB_SYSLOG(LOG_ERR, "error in closing connection, %s\n",
                        amqp_rpc_reply_string(&ret));
            }

            res = amqp_destroy_connection(lc_conn[i]);
            if (res) {
                LB_SYSLOG(LOG_ERR, "error in destroy connection ret=%d\n", res);
            }

            connection_tid[i] = 0;
        }
    }

    return LC_BUS_OK;
}

static int exit_amqp_connection()
{
    int ret = 0;
    pthread_mutex_lock(&bus_mutex);
    ret = exit_amqp_connection_atomic();
    pthread_mutex_unlock(&bus_mutex);
    return ret;
}

#define RECONNECT_MIN_INTERVAL 8
#define RECONNECT_MAX_INTERVAL 64

static int lc_bus_reset()
{
    int i, inv = RECONNECT_MIN_INTERVAL;
    int ret = LC_BUS_OK;
    amqp_connection_state_t *conn = NULL;

    pthread_mutex_lock(&bus_mutex);
    for (;;) {
        exit_amqp_connection_atomic();
        conn = lc_bus_get_connection_atomic();
        if (!conn) {
            return LC_BUS_ERR;
        }
        for (i = 0; i < NUM_LC_BUS_QUEUE && ret == LC_BUS_OK; ++i) {
            if (consumer_queueids[i]) {
                ret = lc_bus_init_consumer_atomic(conn, 0, i);
                break;
            }
        }
        if (ret == LC_BUS_OK) {
            LB_SYSLOG(LOG_INFO, "reconnect to rabbitmq-server.\n");
            break;
        }

        LB_SYSLOG(LOG_ERR, "failed to connect rabbitmq-server, "
                "will try again after %d seconds.\n", inv);
        sleep(inv);
        if (inv < RECONNECT_MAX_INTERVAL) {
            inv <<= 1;
        }
    }
    pthread_mutex_unlock(&bus_mutex);

    return ret;
}

static const char *amqp_server_exception_string(const amqp_rpc_reply_t *r)
{
    int res;
    static char s[512];

    switch (r->reply.id) {
    case AMQP_CONNECTION_CLOSE_METHOD: {
        amqp_connection_close_t *m
            = (amqp_connection_close_t *)r->reply.decoded;
        res = snprintf(s, sizeof(s),
                "server connection error %d, message: %.*s",
                m->reply_code, (int)m->reply_text.len,
                (char *)m->reply_text.bytes);
        break;
    }

    case AMQP_CHANNEL_CLOSE_METHOD: {
        amqp_channel_close_t *m
            = (amqp_channel_close_t *)r->reply.decoded;
        res = snprintf(s, sizeof(s),
                "server channel error %d, message: %.*s",
                m->reply_code, (int)m->reply_text.len,
                (char *)m->reply_text.bytes);
        break;
    }

    default:
        res = snprintf(s, sizeof(s),
                "unknown server error, method id 0x%08X",
                r->reply.id);
        break;
    }

    return res >= 0 ? s : NULL;
}

static const char *amqp_rpc_reply_string(const amqp_rpc_reply_t *r)
{
    if (!r) {
        return "";
    }

    switch (r->reply_type) {
    case AMQP_RESPONSE_NORMAL:
        return "normal response";

    case AMQP_RESPONSE_NONE:
        return "missing RPC reply type";

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        return amqp_error_string2(r->library_error);

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        return amqp_server_exception_string(r);

    default:
        abort();
    }

    return "";
}

