#include "lc_rm.h"
#include "lc_queue.h"
#include "lc_header.h"

extern pthread_mutex_t lc_msg_mutex;
extern pthread_cond_t lc_msg_cond;

amqp_connection_state_t lc_rm_conn;

int lc_bus_msg_process(amqp_connection_state_t *conn)
{
    lc_pb_msg_t   *msg     = NULL;
    header_t      *msg_hdr = {0};
    int            buf_len = 0;
    char           buf[LC_BUS_BUFFER_SIZE] = {0};
    header_t       hdr = {0};

    int pb_hdr_len = get_message_header_pb_len();
    amqp_envelope_t envelope;
    amqp_frame_t frame;
    amqp_message_t message;

    buf_len = lc_bus_consume_message(conn,
              0, &envelope, &frame, &message, buf, sizeof(buf));

    if (unpack_message_header((uint8_t *)buf, pb_hdr_len, &hdr) == 0 &&
        pb_hdr_len + hdr.length == buf_len) {
        LC_RM_CNT_INC(lc_valid_msg_in);
        RM_LOG(LOG_DEBUG, "Message in sender=%d type=%d len=%d\n",
               hdr.sender, hdr.type, hdr.length);
    } else {
        LC_RM_CNT_INC(lc_msg_rcv_err);
        RM_LOG(LOG_ERR, "Message consume failed, nrecv=%d\n", buf_len);
        return -1;
    }

    msg = (lc_pb_msg_t *)malloc(sizeof(lc_pb_msg_t));
    if (!msg) {
        LC_RM_CNT_INC(lc_buff_alloc_error);
        RM_LOG(LOG_ERR, "Message malloc failed");
        return -1;
    }
    memset(msg, 0, sizeof(lc_pb_msg_t));
    msg_hdr = (header_t *)&(msg->mbuf);
    memcpy(msg_hdr, &hdr, pb_hdr_len);
    memcpy(msg_hdr + 1, buf + pb_hdr_len, hdr.length);

    pthread_mutex_lock(&lc_msg_mutex);
    LC_RM_CNT_INC(lc_msg_enqueue);
    lc_pb_msgq_eq(&lc_data_queue, msg);
    pthread_cond_signal(&lc_msg_cond);
    pthread_mutex_unlock(&lc_msg_mutex);

    return 0;
}

int lc_bus_init_by_rm(void)
{
    int res;

    res = lc_bus_init();
    if (res) {
        RM_LOG(LOG_ERR, "Init lc_bus failed!\n");
        return -1;
    }

    res = lc_bus_init_consumer(0, LC_BUS_QUEUE_LCRMD);
    if (res) {
        RM_LOG(LOG_ERR, "Init lc_bus consumer failed!\n");
        lc_bus_exit();
        return -1;
    }
    return 0;
}

void lc_main_process()
{
    int max_fd;
    fd_set fds;
    amqp_connection_state_t *conn = lc_bus_get_connection();
    int lc_rm_conn_fd = amqp_get_sockfd(*conn);

    max_fd = lc_rm_conn_fd;
    ++max_fd;
    RM_LOG(LOG_DEBUG, "%d > %d\n", lc_rm_conn_fd, max_fd);

    while (lc_bus_data_in_buffer(conn)) {
        LC_RM_CNT_INC(lc_bus_msg_in);
        if (lc_bus_msg_process(conn)) {
            break;
        }
    }

    while (1) {
        /* in case of bus connection reset */
        conn = lc_bus_get_connection();
        lc_rm_conn_fd = amqp_get_sockfd(*conn);
        if (lc_rm_conn_fd + 1 > max_fd) {
            max_fd = lc_rm_conn_fd + 1;
        }

        FD_ZERO(&fds);
        FD_SET(lc_rm_conn_fd, &fds);

        if (select(max_fd, &fds, NULL, NULL, NULL) > 0) {

            if (FD_ISSET(lc_rm_conn_fd, &fds)) {
                do {
                    LC_RM_CNT_INC(lc_bus_msg_in);
                    if (lc_bus_msg_process(conn)) {
                        break;
                    }
                } while (lc_bus_data_in_buffer(conn));
            }
        }
    }
    return;
}

int lc_bus_rm_publish_unicast(char *msg, int len, uint32_t queue_id)
{
    return lc_bus_publish_unicast((char *)msg, len, queue_id);
}
