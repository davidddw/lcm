#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "../include/lc_global.h"

lc_msg_t *lc_msgq_dq(lc_msg_queue_t *msgq)
{
    lc_msg_t *msg = NULL;

    if (STAILQ_EMPTY(&msgq->qhead)) {
        return NULL;
    }

    msg = STAILQ_FIRST(&msgq->qhead);
    STAILQ_REMOVE_HEAD(&msgq->qhead, entries);
    msgq->deq_cnt++;
    msgq->msg_cnt--;
    return msg;
}

void lc_msgq_eq(lc_msg_queue_t *msgq, lc_msg_t *msg)
{
    STAILQ_INSERT_TAIL(&msgq->qhead, msg, entries);
    msgq->enq_cnt++;
    msgq->msg_cnt++;
    return;
}

void lc_msgq_init(lc_msg_queue_t *msgq)
{
    STAILQ_INIT(&msgq->qhead);
    msgq->enq_cnt = 0;
    msgq->deq_cnt = 0;
    msgq->msg_cnt = 0;
    return;
}

int lc_is_msgq_empty(lc_msg_queue_t *msgq)
{
    if (STAILQ_EMPTY(&msgq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_msgq_len(lc_msg_queue_t *msgq)
{
    return msgq->msg_cnt;
}

lc_pb_msg_t *lc_pb_msgq_dq(lc_pb_msg_queue_t *msgq)
{
    lc_pb_msg_t *msg = NULL;

    if (STAILQ_EMPTY(&msgq->qhead)) {
        return NULL;
    }

    msg = STAILQ_FIRST(&msgq->qhead);
    STAILQ_REMOVE_HEAD(&msgq->qhead, entries);
    msgq->deq_cnt++;
    msgq->msg_cnt--;
    return msg;
}

void lc_pb_msgq_eq(lc_pb_msg_queue_t *msgq, lc_pb_msg_t *msg)
{
    STAILQ_INSERT_TAIL(&msgq->qhead, msg, entries);
    msgq->enq_cnt++;
    msgq->msg_cnt++;
    return;
}

void lc_pb_msgq_init(lc_pb_msg_queue_t *msgq)
{
    STAILQ_INIT(&msgq->qhead);
    msgq->enq_cnt = 0;
    msgq->deq_cnt = 0;
    msgq->msg_cnt = 0;
    return;
}

int lc_is_pb_msgq_empty(lc_pb_msg_queue_t *msgq)
{
    if (STAILQ_EMPTY(&msgq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_pb_msgq_len(lc_pb_msg_queue_t *msgq)
{
    return msgq->msg_cnt;
}

#define LC_MSGQ_FOREACH(msg, msgq) \
    STAILQ_FOREACH(msg, msgq->qhead, entries)

lc_data_t *lc_dpq_dq(lc_dpq_queue_t *dpq)
{
    lc_data_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

void lc_dpq_eq(lc_dpq_queue_t *dpq, lc_data_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_dpq_init(lc_dpq_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

int lc_is_dpq_empty(lc_dpq_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_dpq_len(lc_dpq_queue_t *dpq)
{
    return dpq->dp_cnt;
}

#define LC_DPQ_FOREACH(pdata, dpq) \
    STAILQ_FOREACH(pdata, dpq->qhead, entries)

#if 0
lc_msg_queue_t data_queue;
int main()
{
    lc_msg_t *msg;
    lc_msgq_init(&data_queue);
    int i = 0;

    for (i = 0; i < 10000; i++) {
        msg = (lc_msg_t *)malloc(sizeof(lc_msg_t));
        msg->mbuf.hdr.type = 1;
        msg->mbuf.hdr.magic = 0x1234;
        lc_msgq_eq(&data_queue, msg);
    }

    for (i = 0; i < 10000; i++) {
        msg = (lc_msg_t *)lc_msgq_dq(&data_queue);
//        printf("xxx %d %x\n", msg->mbuf.type, msg->mbuf.magic);
        free(msg);
    }


    if (lc_is_msgq_empty(&data_queue)) {
        printf("queue is empty\n");
    } else {
        printf("queue not empty\n");
    }

    printf("data_queue. enq = %d \n", data_queue.enq_cnt);
    printf("data_queue. dnq = %d \n", data_queue.deq_cnt);

}

#endif
