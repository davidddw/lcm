#ifndef _LC_HEALTHCHECK_H
#define _LC_HEALTHCHECK_H

#include <stdarg.h>

#include "lc_global.h"

#define VERSION "2.3"
#define MAX_FILE_NAME_LEN            128

#define LC_CHK_SHM_ID               "/tmp/.chk_shm_key"

//#define LC_FLAG_THREAD               0x00000001
#define LC_FLAG_DAEMON               0x00000002
//#define LC_FLAG_CONFIG               0x00000004
#define LC_FLAG_VERSION              0x00000008
#define LC_FLAG_HELP                 0x00000010
#define LC_FLAG_FOREGROUND           0x00000020
#define LC_FLAG_INTERVAL             0x00000040
#define LC_FLAG_THRESHOLD            0x00000080
#define LC_FLAG_LOGLEVEL             0x00000100

#define LC_KILL_ALL                  "killall lcmd lcpd vmdriver lcmond"
#define LC_START_LCMD                "/usr/local/livecloud/bin/lcmd -l log"
#define LC_START_LCPD                "/usr/local/livecloud/bin/lcpd -t 1 -d -l 7"
#define LC_START_VMDRIVER            "/usr/local/livecloud/bin/vmdriver -t 1 -d -l 7"
#define LC_START_MONITOR             "/usr/local/livecloud/bin/lcmond -t 2 -d -l 7"

lc_msg_queue_t lc_data_queue;
lc_chk_sock_t lc_sock;

struct sockaddr_un sa_chk_snd_lcm;
struct sockaddr_un sa_chk_snd_ker;
struct sockaddr_un sa_chk_snd_drv;
struct sockaddr_un sa_chk_snd_mon;
struct sockaddr_un sa_chk_rcv;

typedef struct lc_opt_s {
    u_int32_t     lc_flags;
    u_int32_t     log_level;
    u_int32_t     check_interval;
    u_int32_t     fail_threshold;
    char          config_file[MAX_FILE_NAME_LEN];
} lc_opt_t;

typedef struct lc_chk_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_thread_msg_drop;
    atomic_t    lc_remain_buf_left;
    atomic_t    lc_msg_rcv_err;
    atomic_t    lc_invalid_msg_in;
    atomic_t    lc_valid_msg_in;
    atomic_t    lc_buff_alloc_error;
    atomic_t    lc_msg_enqueue;
} lc_chk_counter_t;

typedef struct lc_heartbeat_counter_s {
    atomic_t    lcm;
    atomic_t    kernel;
    atomic_t    vmdriver;
    atomic_t    monitor;
} lc_heartbeat_counter_t;

typedef struct lc_chk_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_chk_counter_t lc_counters;
    lc_heartbeat_counter_t hb_counters;
} lc_chk_sknob_t;

extern lc_chk_sknob_t *lc_chk_knob;

static inline void LC_LOG(int level, char *format, ...)
{
    va_list ap;
    char buffer[MAX_LOG_BUF_SIZE] = {0};

    if (level > lc_chk_knob->lc_opt.log_level) {
        return;
    }
    sprintf(buffer, "(tid=%lu) ", pthread_self());
    strcat(buffer, format);
    va_start(ap, format);
    vsyslog(level, buffer, ap);
    va_end(ap);
}

#define LC_CHK_CNT_INC(c) \
    atomic_inc(&lc_chk_knob->lc_counters.c)

#define LC_CHK_CNT_DEC(c) \
    atomic_dec(&lc_chk_knob->lc_counters.c)

#define LC_CHK_CNT_ADD(i, c) \
    atomic_add(i, &lc_chk_knob->lc_counters.c)

#define LC_CHK_CNT_SUB(i, c) \
    atomic_sub(i, &lc_chk_knob->lc_counters.c)

#define LC_HB_CNT_INC(c) \
    atomic_inc(&lc_chk_knob->hb_counters.c)

#define LC_HB_CNT_ADD(i, c) \
    atomic_add(i, &lc_chk_knob->hb_counters.c)

#define LC_HB_CNT_RST(c) \
    atomic_set(&lc_chk_knob->hb_counters.c, 0)

#define LC_HB_CNT_GET(c) \
    (lc_chk_knob->hb_counters.c.counter)

#endif

int lc_sock_init();
void lc_main_process();
int lc_send_lcm_heartbeat();
int lc_send_kernel_heartbeat();
int lc_send_vmdriver_heartbeat();
int lc_send_monitor_heartbeat();
int chk_msg_handler(struct lc_msg_head *m_head);
