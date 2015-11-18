#ifndef _LC_MON_H
#define _LC_MON_H

#include <stdarg.h>

#include "lc_global.h"
#include <time.h>
#include "agent_msg_host_info.h"
#include "agent_msg_vm_info.h"
#include "agent_msg_interface_info.h"

#define VERSION "2.3"
#define LC_MAX_THREAD_NUM            18
#define LC_DEFAULT_THREAD_NUM        2
#define MAX_FILE_NAME_LEN            128
#define MAX_VM_NAME_LEN              64
#define MAX_VM_CPU_USAGE_LEN         80
#define MAX_SYS_COMMAND_LEN          1024
#define MAX_LOG_MSG_LEN              150

#define LC_MON_SHM_ID                "/tmp/.mon_shm_key"
#define LC_MON_SYS_LEVEL_SHM_ID      "/tmp/.mon_shm_sys_level_key" /*added by kzs for system log level global variable*/

#define LC_FLAG_THREAD               0x00000001
#define LC_FLAG_DAEMON               0x00000002
#define LC_FLAG_CONFIG               0x00000004
#define LC_FLAG_VERSION              0x00000008
#define LC_FLAG_HELP                 0x00000010
#define LC_FLAG_FOREGROUND           0x00000020
#define LC_FLAG_GSTART               0x00000040
#define LC_FLAG_LOGSET               0x00000080

#define LC_EXEC_CMD_LEN              512

#define VM_TYPE_HOST                 1
#define VM_TYPE_GATEWAY              2

#define MON_RESOURCE_TIMER           30
#define MON_VM_RESOURCE_TIMER        60

#define LC_MON_PTHREAD_NUM_3         3
#define LC_MON_PTHREAD_NUM_8         8
#define LC_MON_PTHREAD_NUM_16        16

#define LC_MON_TSK_REQ_PTHREAD_1     1
#define LC_MON_NFS_CHECK_PTHREAD_1   1
#define LC_MON_HA_CHECK_PTHREAD_2    2
#define LC_MON_BK_VM_CHECK_PTHREAD_3 3
#define LC_MON_TSK_REQ_PTHREAD_2     2

#define LC_MON_NSP_PTHREAD_1         1
#define LC_MON_HOST_PTHREAD_2        2

#define LC_MON_RADOS_PTHREAD_1       1

#define LC_MON_VM_PTHREAD_6          6

#define LC_POOL_TYPE_UNKNOWN         0
#define LC_POOL_TYPE_XEN             1
#define LC_POOL_TYPE_VMWARE          2
#define LC_POOL_TYPE_KVM             3

#define LC_VMWAF_STATE_RUNING      1
#define LC_VMWAF_STATE_EXCEPTION   2
#define LC_VMWAF_STATE_HELTED      3
#define LC_VMWAF_STATE_UNKNOWN     4
lc_dpq_queue_t lc_data_queue;
lc_mon_sock_t lc_sock;
/*
struct sockaddr_un sa_mon_rcv_chk;
struct sockaddr_un sa_mon_snd_chk;
*/
struct sockaddr_in sa_mon_vcd;

#define DPQ_TYPE_HOST_INFO 0
#define DPQ_TYPE_VM_INFO   1

#define DPQ_TYPE_VM_CHECK  0

#define HOST_QSIZE 10
#define VM_QSIZE   200

typedef struct lc_opt_ {
    u_int32_t              lc_flags;
    u_int32_t              thread_num;
    volatile u_int32_t     log_level;
    volatile u_int32_t     log_on;
    char                   config_file[MAX_FILE_NAME_LEN];
}lc_opt_t;

typedef struct lc_mon_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_chk_msg_in;
    atomic_t    lc_mon_data_in;
    atomic_t    lc_mon_data_out;
    atomic_t    lc_mon_data_enqueue;
    atomic_t    lc_mon_data_dequeue;
    atomic_t    lc_mon_data_drop;
    atomic_t    lc_buff_alloc_error;
    atomic_t    lc_mon_thread_create;
    atomic_t    lc_mon_thread_create_success;
    atomic_t    lc_mon_thread_create_failed;
} lc_mon_counter_t;

#define LC_MON_KNOB_MAGIC  0xCAFEBEEF
typedef struct lc_mon_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_mon_counter_t lc_counters;
} lc_mon_sknob_t;

typedef struct host_info_response_s {
    char                      host_addr[AGENT_IPV4_ADDR_LEN];
    msg_host_info_response_t  host_info;
} host_info_response_t;

typedef struct vmware_msg_reply_s {
  char *data;
  int  len;
} vmware_msg_reply_t;

typedef int (*sk_data_handler)(char *data, char *addr, int qsize, int offset, int htype);
typedef int (*dpq_handler)(void *pdata, int htype);
typedef struct dpq_obj_s {
    int              type;
    int              qsize;
    int              element_len;
    sk_data_handler  data_func;
    dpq_handler      dpq_func;
} dpq_obj_t;

typedef struct lc_dpq_host_queue_s {
    STAILQ_HEAD(,lc_data_host_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_host_queue_t;

typedef struct lc_dpq_vm_queue_s {
    STAILQ_HEAD(,lc_data_vm_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_vm_queue_t;

typedef struct lc_dpq_nfs_chk_queue_s {
    STAILQ_HEAD(,lc_data_nfs_chk_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_nfs_chk_queue_t;
lc_dpq_nfs_chk_queue_t lc_nfs_chk_data_queue;

typedef struct lc_dpq_vgw_ha_queue_s {
    STAILQ_HEAD(,lc_data_vgw_ha_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_vgw_ha_queue_t;
lc_dpq_vgw_ha_queue_t lc_vgw_ha_data_queue;

typedef struct lc_dpq_bk_vm_health_queue_s {
    STAILQ_HEAD(,lc_data_bk_vm_health_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t dp_cnt;
} lc_dpq_bk_vm_health_queue_t;
lc_dpq_bk_vm_health_queue_t lc_bk_vm_health_data_queue;

typedef struct lc_data_host_s{
    int                  htype;
    union {
        host_info_response_t   host;
        vmware_msg_reply_t  vmware_host;
    } u;
    STAILQ_ENTRY(lc_data_host_s) entries;
} lc_data_host_t;

typedef struct lc_data_vm_s{
    int                 htype;
    char                host_addr[AGENT_IPV4_ADDR_LEN];
    int                 vm_cnt;
    union {
        vm_result_t         *vm;
        vmware_msg_reply_t  vmware_vm;
        lc_msg_t            *vm_ops;
    } u_vm;
    STAILQ_ENTRY(lc_data_vm_s) entries;
} lc_data_vm_t;

typedef struct lc_data_vgw_ha_s{
    char        vgw_ctl_addr[AGENT_IPV4_ADDR_LEN];
    char        label[MAX_VM_UUID_LEN];
    uint32_t    ha_state;
    uint32_t    id;
    STAILQ_ENTRY(lc_data_vgw_ha_s) entries;
} lc_data_vgw_ha_t;

typedef struct lc_data_nfs_chk_s{
    char        host_addr[AGENT_IPV4_ADDR_LEN];
    char        server_ip[MAX_HOST_ADDRESS_LEN];
    char        vm_template[LC_NAMESIZE];
    uint32_t    type;
    uint32_t    htype;
    uint32_t    state;
    STAILQ_ENTRY(lc_data_nfs_chk_s) entries;
} lc_data_nfs_chk_t;

typedef struct lc_data_bk_vm_health_s{
    char        lb_server[AGENT_IPV4_ADDR_LEN];
    char        lb_ctl_addr[AGENT_IPV4_ADDR_LEN];
    char        lb_lcuuid[MAX_VM_UUID_LEN];
    char        lb_label[LC_NAMESIZE];
    STAILQ_ENTRY(lc_data_bk_vm_health_s) entries;
} lc_data_bk_vm_health_t;

extern lc_mon_sknob_t *lc_mon_knob;
extern dpq_obj_t dpq_q_xen_msg[];
extern dpq_obj_t dpq_q_vmware_msg[];
extern dpq_obj_t dpq_q_vm_check_msg[];

extern char *LOGLEVEL[8];

extern int *LCMON_LOG_LEVEL_P;

#define LCMON_LOG(level, format, ...)  \
    if (level <= *LCMON_LOG_LEVEL_P) { \
        syslog(level, "[tid=%lu] %s %s: " format, \
                pthread_self(), LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

static inline void LC_LOG(int level, char *format, ...)
{
    va_list ap;
    char buffer[MAX_LOG_BUF_SIZE] = {0};

    if (lc_mon_knob->lc_opt.log_on) {
        if (level <= lc_mon_knob->lc_opt.log_level) {
            sprintf(buffer, "(tid=%lu) ", pthread_self());
            strcat(buffer, format);
            va_start(ap, format);
            vsyslog(level, buffer, ap);
            va_end(ap);
        }
    }
}
#define LC_MON_CNT_INC(c) \
    atomic_inc(&lc_mon_knob->lc_counters.c)

#define LC_MON_CNT_DEC(c) \
    atomic_dec(&lc_mon_knob->lc_counters.c)

#define LC_MON_CNT_ADD(i, c) \
    atomic_add(i, &lc_mon_knob->lc_counters.c)

#define LC_MON_CNT_SUB(i, c) \
    atomic_sub(i, &lc_mon_knob->lc_counters.c)

typedef struct lcmon_domain_s {
    char lcuuid[64];
    char local_ctrl_ip[16];
    char mysql_master_ip[16];
}lcmon_domain_t;
extern lcmon_domain_t lcmon_domain;

struct config {
    int ceph_rados_monitor_enabled;
    int ceph_switch_threshold;
    char ceph_cluster_name[LC_NAMESIZE];
    char ceph_user_name[LC_NAMESIZE];
    char ceph_conf_file[LC_NAMESIZE];
};
extern struct config lcmon_config;

#define LC_MON_CONFIG_FILE         "/usr/local/livecloud/conf/livecloud.conf"
#define LC_MON_SIGN                "lcmon"
#define LC_MON_KEY_DOMAIN_LCUUID   "domain.lcuuid"
#define LC_MON_GLOBAL_SIGN         "global"
#define LC_MON_KEY_MASTER_CTRL_IP  "master_ctrl_ip"
#define LC_MON_KEY_LOCAL_CTRL_IP   "local_ctrl_ip"
#define LC_MON_KEY_MYSQL_MASTER_IP "mysql_master_ip"
#define LC_MON_DOMAIN_LCUUID_DEFAULT      "00000000-0000-0000-0000-000000000000"

#define LC_MON_KEY_CEPH_RADOS_MONITOR     "ceph_rados_monitor"
#define LC_MON_KEY_CEPH_SWITCH_THRESHOLD  "ceph_switch_threshold"
#define LC_MON_KEY_CEPH_CLUSTER_NAME      "ceph_cluster_name"
#define LC_MON_KEY_CEPH_USER_NAME         "ceph_user_name"
#define LC_MON_KEY_CEPH_CONF_FILE         "ceph_conf_file"

void xen_common_init(void);
int lc_sock_init();
void lc_timer_init(void);
int lc_xen_test_init();
int lc_db_init(char *host, char *user, char *passwd,
               char *dbname, char *dbport, char *version);
void lc_main_process();
int lc_bus_lcmond_publish_unicast(char *msg, int len, uint32_t queue_id);
int lc_send_msg(lc_mbuf_t *msg);
int lc_mon_msg_handler(intptr_t data_ptr);
int lc_mon_nfs_chk_handler(lc_data_nfs_chk_t *pdata);
int lc_mon_vgw_ha_handler(lc_data_vgw_ha_t *pdata);
int lc_mon_bk_vm_health_handler(lc_data_bk_vm_health_t *pdata);
void dump_buffer(void *data, int count);

int lc_monitor_check_top_half();

int host_dpq_handler(void *pdata, int htype);
int vm_dpq_handler(void *pdata, int htype);
int vm_dpq_ops_check_handler(void *pdata, int htype);
int get_data_sock_fd();

int lc_mon_init_bus();
int lc_publish_msg(lc_mbuf_t *msg);
#endif /*_LC_MON_H */
