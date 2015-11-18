/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_snf_qctrl.c
 * Port from lc_mon_qctrl.c
 * Date: 2015-05-04
 *
 * Description: queue management
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "../include/lc_global.h"
#include "lc_snf.h"

lc_socket_queue_data_t *lc_socket_queue_dq(lc_socket_queue_t *queue)
{
    lc_socket_queue_data_t *pdata = NULL;

    if (STAILQ_EMPTY(&queue->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&queue->qhead);
    STAILQ_REMOVE_HEAD(&queue->qhead, entries);
    queue->deq_cnt++;
    queue->len--;
    return pdata;
}

void lc_socket_queue_eq(lc_socket_queue_t *queue, lc_socket_queue_data_t *pdata)
{
    STAILQ_INSERT_TAIL(&queue->qhead, pdata, entries);
    queue->enq_cnt++;
    queue->len++;
    return;
}

void lc_socket_queue_init(lc_socket_queue_t *queue)
{
    STAILQ_INIT(&queue->qhead);
    queue->enq_cnt = 0;
    queue->deq_cnt = 0;
    queue->len = 0;
    return;
}

int lc_is_socket_queue_empty(lc_socket_queue_t *queue)
{
    if (STAILQ_EMPTY(&queue->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_socket_queue_len(lc_socket_queue_t *queue)
{
    return queue->len;
}
