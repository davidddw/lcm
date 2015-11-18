#ifndef _LC_VM_H
#define _LC_VM_H

#include <stdarg.h>

#include "lc_global.h"
#include "message/msg_ker2vmdrv.h"
#include "message/msg_lcm2dri.h"
#include "message/msg_vm.h"

#define VERSION "2.3"
#define LC_MAX_THREAD_NUM            16
#define LC_DEFAULT_THREAD_NUM        1
#define MAX_FILE_NAME_LEN            128
#define MAX_VM_NAME_LEN              64
#define MAX_SYS_COMMAND_LEN          1024
#define CCT                          (+8)
#define VM_EXPORT_XVA_DIR            "/root/nfs"
#define MAX_VM_CPU_USAGE_LEN         80
//#define LC_PHY_IFACE_INT             "xenbr3"
//#define LC_PHY_IFACE_DP              "xenbr0"

#define LC_VM_SHM_ID                 "/tmp/.vm_shm_key"
#define LC_VM_SYS_LEVEL_SHM_ID      "/tmp/.vm_shm_sys_level_key" /*added by kzs for system log level global variable*/

#define LC_FLAG_THREAD               0x00000001
#define LC_FLAG_DAEMON               0x00000002
#define LC_FLAG_CONFIG               0x00000004
#define LC_FLAG_VERSION              0x00000008
#define LC_FLAG_HELP                 0x00000010
#define LC_FLAG_FOREGROUND           0x00000020
#define LC_FLAG_GSTART               0x00000040
#define LC_FLAG_LEARNING             0x00000080
#define LC_FLAG_LOGSET               0x00000100

#define LC_EXEC_CMD_LEN              512
#define LC_VNC_PORT_BASE             15000
#define LC_VNC_PORT_MOD              19999

#define VM_TYPE_HOST                 1
#define VM_TYPE_GATEWAY              2

lc_msg_queue_t lc_data_queue;
lc_vm_sock_t lc_sock;

struct sockaddr_un sa_vm_rcv_ker;
struct sockaddr_un sa_vm_rcv_chk;
struct sockaddr_un sa_vm_snd_ker;
struct sockaddr_un sa_vm_snd_chk;
struct sockaddr_in sa_vm_vcd;
struct sockaddr_in sa_vm_agexec;

typedef struct lc_opt_ {
    u_int32_t              lc_flags;
    u_int32_t              thread_num;
    volatile u_int32_t     log_level;
    volatile u_int32_t     log_on;
    char                   config_file[MAX_FILE_NAME_LEN];
}lc_opt_t;

typedef struct lc_vm_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_bus_msg_in;
    atomic_t    lc_ker_msg_in;
    atomic_t    lc_lcm_msg_in;
    atomic_t    lc_chk_msg_in;
    atomic_t    lc_vcd_msg_in;
    atomic_t    lc_agexec_msg_in;
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

    atomic_t    lc_vm_thread_create;
    atomic_t    lc_vm_thread_create_success;
    atomic_t    lc_vm_thread_create_failed;
} lc_vm_counter_t;

#define LC_KER_KNOB_MAGIC  0xCAFEBEEF
typedef struct lc_vm_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_vm_counter_t lc_counters;
} lc_vm_sknob_t;
extern char* LOGLEVEL[8];

extern int *VMDRIVER_LOG_LEVEL_P;

#define VMDRIVER_LOG(level, format, ...)  \
    if (level <= *VMDRIVER_LOG_LEVEL_P) { \
        syslog(level, "[tid=%lu] %s %s: " format, \
                pthread_self(), LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

extern lc_vm_sknob_t *lc_vm_knob;
extern int *LB_LOG_LEVEL_P;

static inline void LC_LOG(int level, char *format, ...)
{
    va_list ap;
    char buffer[MAX_LOG_BUF_SIZE] = {0};

    if (lc_vm_knob->lc_opt.log_on) {
        if (level <= lc_vm_knob->lc_opt.log_level) {
            sprintf(buffer, "(tid=%lu) ", pthread_self());
            strcat(buffer, format);
            va_start(ap, format);
            vsyslog(level, buffer, ap);
            va_end(ap);
        }
    }
}

#define LC_VM_CNT_INC(c) \
    atomic_inc(&lc_vm_knob->lc_counters.c)

#define LC_VM_CNT_DEC(c) \
    atomic_dec(&lc_vm_knob->lc_counters.c)

#define LC_VM_CNT_ADD(i, c) \
    atomic_add(i, &lc_vm_knob->lc_counters.c)

#define LC_VM_CNT_SUB(i, c) \
    atomic_sub(i, &lc_vm_knob->lc_counters.c)

#define LC_VM_CONFIG_FILE         "/usr/local/livecloud/conf/livecloud.conf"
#define LC_VM_GLOBAL_SIGN         "global"
#define LC_VM_KEY_LOCAL_CTRL_IP   "local_ctrl_ip"
extern char local_ctrl_ip[];

int lc_sock_init();
int lc_bus_init_by_vmdriver(void);
void lc_main_process();
int lc_bus_vmdriver_publish_unicast(char *msg, int len, uint32_t queue_id);
int vm_msg_handler(struct lc_msg_head *m_head);

int lc_send_msg(lc_mbuf_t *msg);
int lc_publish_msg(lc_mbuf_t *msg);

int lc_vm_backup(struct lc_msg_head *m_head);

#endif /*_LC_VM_H */
