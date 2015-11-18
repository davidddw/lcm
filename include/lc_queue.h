#ifndef _LC_QUEUE_H
#define _LC_QUEUE_H

lc_msg_t *lc_msgq_dq(lc_msg_queue_t *msgq);
void lc_msgq_eq(lc_msg_queue_t *msgq, lc_msg_t *msg);
void lc_msgq_init(lc_msg_queue_t *msgq);

int lc_is_msgq_empty(lc_msg_queue_t *msgq);
int lc_msgq_len(lc_msg_queue_t *msgq);

lc_data_t *lc_dpq_dq(lc_dpq_queue_t *dpq);
void lc_dpq_eq(lc_dpq_queue_t *dpq, lc_data_t *pdata);
void lc_dpq_init(lc_dpq_queue_t *dpq);
int lc_is_dpq_empty(lc_dpq_queue_t *msgq);
int lc_dpq_len(lc_dpq_queue_t *msgq);

lc_pb_msg_t *lc_pb_msgq_dq(lc_pb_msg_queue_t *msgq);
void lc_pb_msgq_eq(lc_pb_msg_queue_t *msgq, lc_pb_msg_t *msg);
void lc_pb_msgq_init(lc_pb_msg_queue_t *msgq);
int lc_is_pb_msgq_empty(lc_pb_msg_queue_t *msgq);
int lc_pb_msgq_len(lc_pb_msg_queue_t *msgq);
#endif /*_LC_QUEUE_H */
