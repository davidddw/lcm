#ifndef _LC_RM_H
#define _LC_RM_H

#include <stdarg.h>

#include "lc_global.h"

#define VERSION "2.3"
#define LC_MAX_THREAD_NUM            160
#define LC_DEFAULT_THREAD_NUM        1

#define LC_RM_SHM_ID                 "/tmp/.ker_shm_key"
#define LC_RM_SYS_LEVEL_SHM_ID       "/usr/local/livecloud/conf/sql_init_cmd"

#define LC_FLAG_THREAD               0x00000001
#define LC_FLAG_DAEMON               0x00000002
#define LC_FLAG_CONFIG               0x00000004
#define LC_FLAG_VERSION              0x00000008
#define LC_FLAG_HELP                 0x00000010
#define LC_FLAG_FOREGROUND           0x00000020
#define LC_FLAG_DEBUG                0x00000040
#define LC_FLAG_GRACEFUL_RESTART     0x00000080
#define LC_FLAG_LOGLEVEL             0x00000100

#define LC_VM_IDS_LEN                1024
#define LC_VGW_IDS_LEN               1024
#define LC_POOL_IDS_LEN              1024

extern int RM_LOG_DEFAULT_LEVEL;
extern int *RM_LOG_LEVEL_P;

extern char* RM_LOGLEVEL[8];
#define RM_LOG(level, format, ...)  \
    if (level <= *RM_LOG_LEVEL_P) { \
        syslog(level, "[tid=%lu] %s %s: " format, \
                pthread_self(), RM_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }


lc_pb_msg_queue_t lc_data_queue;

typedef struct lc_opt_ {
    u_int32_t              lc_flags;
    u_int32_t              thread_num;
    volatile u_int32_t     log_level;
    volatile u_int32_t     log_on;
}lc_opt_t;

typedef struct lc_rm_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_bus_msg_in;
    atomic_t    lc_agent_msg_in;
    atomic_t    lc_valid_msg_in;
    atomic_t    lc_invalid_msg_in;
    atomic_t    lc_msg_rcv_err;
    atomic_t    lc_msg_enqueue;
    atomic_t    lc_thread_deq_msg1;
    atomic_t    lc_thread_deq_msg2;
    atomic_t    lc_thread_free_msg;
    atomic_t    lc_thread_msg_drop;
    atomic_t    lc_buff_alloc_error;
} lc_rm_counter_t;

typedef struct lc_rm_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_rm_counter_t lc_counters;
} lc_rm_sknob_t;

extern lc_rm_sknob_t *lc_rm_knob;

#define LC_RM_CNT_INC(c) \
    atomic_inc(&lc_rm_knob->lc_counters.c)

#define LC_RM_CNT_DEC(c) \
    atomic_dec(&lc_rm_knob->lc_counters.c)

#define LC_RM_CNT_ADD(i, c) \
    atomic_add(i &lc_rm_knob->lc_counters.c)

#define LC_RM_CNT_SUB(i, c) \
    atomic_sub(i &lc_rm_knob->lc_counters.c)

int lc_rm_sknob_init();
int lc_bus_init_by_rm(void);
int rm_msg_handler(header_t *m_head);
int lc_publish_msg(lc_mbuf_t *msg);
void lc_main_process();
int lc_bus_rm_publish_unicast(char *msg, int len, uint32_t queue_id);
#endif
