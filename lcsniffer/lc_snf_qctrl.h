/*
 * Copyright 2012 yunshan network
 * All rights reserved
 * Author:
 * Date:
 */
#ifndef LC_SNIFFER_QUEUE_H
#define LC_SNIFFER_QUEUE_H

void lc_socket_queue_init(lc_socket_queue_t *queue);

void lc_socket_queue_eq(lc_socket_queue_t *queue, lc_socket_queue_data_t *pdata);
lc_socket_queue_data_t *lc_socket_queue_dq(lc_socket_queue_t *queue);

int lc_is_socket_queue_empty(lc_socket_queue_t *queue);
int lc_socket_queue_len(lc_socket_queue_t *queue);

#endif /*LC_SNIFFER_QUEUE_H*/

