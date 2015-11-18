#include <memory.h>

#include "lc_bus.h"
#include "lc_global.h"
#include "header.pb-c.h"

#define RECV_BUFSIZE 2048

int send_message(lc_mbuf_t* msg)
{
    int len, dst;

    if (!msg) {
        printf("%s: null buffer\n", __FUNCTION__);
        return -1;
    }

    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;

    if ((msg->hdr.magic & 0xFF) == 0x44) {
        /* vmdriver */
        printf("%s: sending %d byte(s) to vmdriver ... ",
                __FUNCTION__, len);
        dst = LC_BUS_QUEUE_VMDRIVER;
    } else if ((msg->hdr.magic & 0xFF) == 0x4B) {
        /* lcpd */
        printf("%s sending %d byte(s) to lcpd ... ",
                __FUNCTION__, len);
        dst = LC_BUS_QUEUE_LCPD;
    } else {
        printf("%s: error magic 0x%8x\n", __FUNCTION__, msg->hdr.magic);
        return -1;
    }

    if (lc_bus_init()) {
        printf("%s: error calling lc_bus_init()\n", __FUNCTION__);
        return -1;
    }
    if (lc_bus_publish_unicast((void *)msg, len, dst)) {
        printf("%s: error calling lc_bus_publish_unicast()\n", __FUNCTION__);
        return -1;
    }
    if (lc_bus_exit()) {
        printf("%s: error calling lc_bus_exit()\n", __FUNCTION__);
        return -1;
    }

    printf("done\n");
    return 0;
}

inline int get_message_header_pb_len(void)
{
    const Header__Header hdr = HEADER__HEADER__INIT;
    const int hdr_len = header__header__get_packed_size(&hdr);

    return hdr_len;
}

int recv_message(uint8_t **body, size_t *len)
{
    int head_len = get_message_header_pb_len();
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int fd = amqp_get_sockfd(*conn);
    int max_fd = fd + 1;
    fd_set fds;
    amqp_envelope_t envelope;
    amqp_frame_t frame;
    amqp_message_t message;
    char buf[RECV_BUFSIZE] = {0};
    int buf_len;
    Header__Header *p_head = 0;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(max_fd, &fds, 0, 0, 0) > 0) {
        buf_len = lc_bus_consume_message(conn, 0, &envelope, &frame, &message,
                                         buf, sizeof(buf));
    } else {
        return -1;
    }
    if (buf_len < head_len) {
        return -1;
    }
    p_head = header__header__unpack(0, head_len, (uint8_t *)buf);
    if (!p_head || head_len + p_head->length != buf_len) {
        return -1;
    }
    *body = calloc(1, p_head->length);
    if (!*body) {
        return -1;
    }
    memcpy(*body, buf + head_len, p_head->length);
    *len = p_head->length;
    header__header__free_unpacked(p_head, 0);

    return 0;
}

void free_message(void *body)
{
    if (body) {
        free(body);
    }
}
