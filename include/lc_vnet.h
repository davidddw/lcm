#ifndef _LC_VNET_H
#define _LC_VNET_H
#include "lc_global.h"

#define VERSION "2.3"

#if 0
lc_msg_queue_t lc_data_queue;
lc_app_sock_t lc_sock;

//struct sockaddr_un sa_drv_rcv_ker;
struct sockaddr_un sa_app_rcv_ker;
struct sockaddr_un sa_app_snd_ker;
struct sockaddr_un sa_app_rcv_lcm;
struct sockaddr_un sa_app_snd_lcm;


typedef struct lc_opt_ {
    u_int32_t     lc_flags;
    u_int32_t     thread_num;
    char          config_file[MAX_FILE_NAME_LEN];
}lc_opt_t;

#endif
typedef struct lc_vnet_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_ker_msg_in;
    atomic_t    lc_lcm_msg_in;
    atomic_t    lc_valid_msg_in;
    atomic_t    lc_invalid_msg_in;
    atomic_t    lc_msg_rcv_err;
    atomic_t    lc_msg_enqueue;
    atomic_t    lc_remain_buf_left;
    atomic_t    lc_thread_deq_msg1;
    atomic_t    lc_thread_deq_msg2;
    atomic_t    lc_thread_free_msg;
    atomic_t    lc_thread_msg_drop;
    atomic_t    lc_buff_alloc_error;
} lc_vnet_counter_t;

#define LC_VNET_CNT_INC(c) \
    atomic_inc(&lc_vnet_knob->lc_counters.c)

#define LC_VNET_CNT_DEC(c) \
    atomic_dec(&lc_vnet_knob->lc_counters.c)

#define LC_VNET_CNT_ADD(i, c) \
    atomic_add(i, &lc_vnet_knob->lc_counters.c)

#define LC_VNET_CNT_SUB(i, c) \
    atomic_sub(i, &lc_vnet_knob->lc_counters.c)

#endif
