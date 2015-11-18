/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-17
 *
 */

#ifndef _LC_SNF_H
#define _LC_SNF_H

//#define DEBUG_LCSNFD

#include <stdarg.h>

#include "lc_global.h"
#include "agent_msg_host_info.h"
#include "agent_msg_vm_info.h"
#include "agent_msg_interface_info.h"
#include "agent_msg_hwdev_info.h"
#define PROCESS_STACK_SIZE           (20L * 1024L * 1024L)
#define MAX_LOG_MSG_LEN              150

#define VERSION                      "2.3"
#define LC_SNF_SHM_ID                "/tmp/.snf_shm_key"

#define LC_FLAG_DAEMON               0x00000001
#define LC_FLAG_VERSION              0x00000002
#define LC_FLAG_HELP                 0x00000004
#define LC_FLAG_GSTART               0x00000008
#define LC_FLAG_LOGSET               0x00000010

#define SNF_VIF_TRAFFIC_TIMER        60

#define MAX_VM_NAME_LEN              64

#define MAX_VM_PER_HOST              200
#define MAX_HWDEV_PER_HOST           64

#define MAX_VIF_PER_HOST             1600
#define MAIL_ADDR_LEN                530
#define MAX_HOST_NUM                 256
#define MAX_SWITCH_NUM               64

#define MAX_BK_VM_PER_LISTENER       64
#define LC_POOL_TYPE_XEN             1
#define LC_POOL_TYPE_VMWARE          2
#define LC_POOL_TYPE_KVM             3

#define LC_VM_TYPE_VM             1
#define LC_VM_TYPE_GW             2
#define LC_VM_TYPE_VGATEWAY       3

lc_snf_sock_t lc_sock;

typedef struct lc_opt_s {
    u_int32_t     lc_flags;
    u_int32_t     log_level;
} lc_opt_t;

typedef struct lc_snf_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_chk_msg_in;
    atomic_t    lc_snf_data_in;
    atomic_t    lc_snf_data_out;
    atomic_t    lc_snf_data_drop;
    atomic_t    lc_buff_alloc_error;
    atomic_t    lc_snf_thread_create_success;
    atomic_t    lc_snf_thread_create_failed;
} lc_snf_counter_t;

/* queue */

#define MAX_BUFFER_LENGTH (1 << 15)

typedef struct lc_socket_queue_s {
    STAILQ_HEAD(,lc_socket_queue_data_s) qhead;
    u_int32_t enq_cnt;
    u_int32_t deq_cnt;
    u_int32_t len;
} lc_socket_queue_t;

typedef struct lc_socket_queue_data_s {
    char    agent_ip[AGENT_IPV4_ADDR_LEN];
    uint8_t buf[MAX_BUFFER_LENGTH];
    int     size;
    STAILQ_ENTRY(lc_socket_queue_data_s) entries;
} lc_socket_queue_data_t;

lc_socket_queue_t lc_socket_queue;

/* monitor data */

typedef struct monitor_host_s {
    uint32_t    htype;
#define LC_POOL_TYPE_XEN    1
#define LC_POOL_TYPE_VMWARE 2

    uint32_t    stype;
#define LC_POOL_TYPE_LOCAL  1
#define LC_POOL_TYPE_SHARED 2
#define MAX_HOST_NAME_LEN          LC_NAMESIZE
    char        host_name[MAX_HOST_NAME_LEN];
    char        host_address[MAX_HOST_ADDRESS_LEN];
    char        master_address[MAX_HOST_ADDRESS_LEN];
#define MAX_HOST_UUID_LEN          64
    char        host_uuid[MAX_HOST_UUID_LEN];
    char        email_address[MAIL_ADDR_LEN];
    char        email_address_ccs[MAIL_ADDR_LEN];
    char        from[MAIL_ADDR_LEN];
    int         email_enable;
    uint32_t    host_state;
    uint32_t    master_host_state;
    int         error_number;
    int         master_error_number;
    uint32_t    pool_type;
#define MAX_USERNAME_LEN           LC_NAMESIZE
    char        username[MAX_USERNAME_LEN];
#define MAX_PASSWORD_LEN           LC_NAMESIZE
    char        password[MAX_PASSWORD_LEN];
    char        master_username[MAX_USERNAME_LEN];
    char        master_password[MAX_PASSWORD_LEN];
#define LC_HOST_MASTER                1
#define LC_HOST_SLAVE                 2
    int         role;
    int         health_check;
    int         master_health_check;
#define LC_HOST_CPU_INFO       128
    char        cpu_info[LC_HOST_CPU_INFO];
#define LC_HOST_CPU_USAGE      704
    char        cpu_usage[LC_HOST_CPU_USAGE];
    int         cpu_used;
    int         mem_usage;
    int         mem_total;
    int         mem_alloc;
    int         disk_usage;
    int         disk_total;
    int         disk_alloc;
    int         ha_disk_usage;
    int         ha_disk_total;
    int         cr_id;
    int         health_check_failure;
    time_t      host_check_start_time;
    int         host_check_interval;
    int         vm_check_interval;
    int         host_cpu_threshold;
    time_t      host_cpu_start_time;
    int         host_cpu_warning_usage;
    int         host_mem_threshold;
    time_t      host_mem_start_time;
    int         host_mem_warning_usage;
    int         host_disk_threshold;
    time_t      host_disk_start_time;
    int         host_disk_warning_usage;
    int         vm_cpu_threshold;
    int         vm_cpu_warning_usage;
    int         vm_mem_threshold;
    int         vm_mem_warning_usage;
    int         vm_disk_threshold;
    int         vm_disk_warning_usage;
    time_t      stats_time;
}monitor_host_t;

typedef struct sr_disk_info_s {
    int         disk_usage;
    int         disk_total;
    int         disk_alloc;
}sr_disk_info_t;

typedef struct vm_query_info_s {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_label[MAX_VM_UUID_LEN];
    char        vdi_sys_uuid[MAX_VM_UUID_LEN];
    char        vdi_user_uuid[MAX_VM_UUID_LEN];
    uint32_t    vif_id;
    int         pw_state;
    int         cpu_number;
    int         mem_total;
    int         sys_disk;
    int         user_disk;
    uint32_t    vm_id;
    uint32_t    vdc_id;
    int         user_id;
#define LC_VM_IFACES_MAX 7
    uint32_t    all_ifid[LC_VM_IFACES_MAX];
} vm_query_info_t;

typedef struct hwdev_query_info_s {
    char           hwdev_ip[AGENT_IPV4_ADDR_LEN];
    char           community[AGENT_NAME_LEN];
    char           username[AGENT_NAME_LEN];
    char           password[AGENT_NAME_LEN];
} hwdev_query_info_t;

#define LC_PRE_VIF_MAX  6
#define LC_CUR_VIF_MAX  8
#define LC_PUB_VIF_MAX  3
#define LC_L2_MAX  3
#define LC_CTRL_VIF_MAX 1
typedef struct vnet_query_info_s {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_label[MAX_VM_UUID_LEN];
    uint32_t    pub_ifid[LC_PUB_VIF_MAX];
    uint32_t    pvt_ifid[LC_L2_MAX];
    uint32_t    vm_id;
    uint32_t    vdc_id;
    int         user_id;
} vnet_query_info_t;

typedef struct storage_info_s {
    uint32_t    id;
#define MAX_SR_NAME_LEN            LC_NAMESIZE
    char        name_label[MAX_SR_NAME_LEN];
#define MAX_UUID_LEN               64
    char        uuid[MAX_UUID_LEN];
    uint32_t    is_shared;
    uint32_t    disk_total;
    uint32_t    disk_used;
} storage_info_t;

typedef struct host_s {
    uint32_t    id;
    char        host_name[MAX_HOST_NAME_LEN];
    char        host_description[MAX_HOST_NAME_LEN];
    char        host_address[MAX_HOST_ADDRESS_LEN];
    char        master_address[MAX_HOST_ADDRESS_LEN];
    char        peer_address[MAX_HOST_ADDRESS_LEN];
    char        host_uuid[MAX_HOST_UUID_LEN];
    uint32_t    host_flag;
#define LC_HOST_STATE_FREE              0x0001
#define LC_HOST_STATE_INUSE             0x0002
#define LC_HOST_STATE_CONNECTED         0x0004
    uint32_t    host_state;
#define LC_HOST_SINGLE                  0
#define LC_HOST_PEER                    1
#define LC_HOST_HA                      2
    uint32_t    host_service_flag;
    int         error_number;
#define LC_HOST_HTYPE_XEN               1
#define LC_HOST_HTYPE_VMWARE            2
#define LC_HOST_HTYPE_KVM               3
    uint32_t    host_htype;
#define LC_HOST_ROLE_SERVER             1
#define LC_HOST_ROLE_GATEWAY            2
#define LC_HOST_ROLE_NSP                3
    uint32_t    host_role;
    char        username[MAX_USERNAME_LEN];
    char        password[MAX_PASSWORD_LEN];
#define LC_BR_NONE         0
#define LC_BR_PIF          1
#define LC_BR_BOND         2
    uint32_t    if0_type;
    uint32_t    if1_type;
    uint32_t    if2_type;
    char        sr_uuid[MAX_HOST_UUID_LEN];
#define MAX_HOST_SR_NAME_LEN       LC_NAMESIZE
    char        sr_name[MAX_HOST_SR_NAME_LEN];
    uint8_t     max_vm;
    uint8_t     curr_vm;
    uint32_t    pool_index;
#define LC_HOST_MASTER                1
#define LC_HOST_SLAVE                 2
    int         role;
    int         health_check;
    char        cpu_info[LC_HOST_CPU_INFO];
    int         mem_total;
    int         mem_alloc;
    int         mem_usage;
    int         disk_total;
    int         disk_alloc;
    int         vcpu_num;
    int         cr_id;
    time_t      cpu_start_time;
    time_t      disk_start_time;
    time_t      traffic_start_time;
    time_t      check_start_time;
    time_t      stats_time;
    struct host_s    *master;
    uint32_t    rack_id;
    uint32_t    vmware_cport;
    uint32_t    n_storage;
#define LC_MAX_SR_NUM                   8
    storage_info_t   storage[LC_MAX_SR_NUM];
} host_t;

typedef struct storage_connection_s {
    int id;
    char host_address[16];
    int storage_id;
    int active;
}storage_connection_t;

typedef struct storage_s{
    int id;
    char name[64];
    int rack_id;
    char uuid[64];
    int is_shared;
    int disk_total;
    int disk_used;
    int disk_alloc;
}db_storage_t;

typedef struct rack_s {
    int        id;
    int        cluster_id;
} rack_t;

typedef struct cluster_s {
    int        id;
    int        vcdc_id;
} cluster_t;

typedef struct vcdc_s {
    int        id;
    char       vswitch_name[LC_NAMESIZE];
} vcdc_t;

typedef struct vm_info_s {
    uint32_t    vm_type;
#define LC_VM_TYPE_VM   1
#define LC_VM_TYPE_GW   2

    uint32_t    vm_flags;
#define LC_VM_INSTALL_TEMPLATE      0x00000001
#define LC_VM_INSTALL_IMPORT        0x00000002
#define LC_VM_ISOLATE               0x00000004
#define LC_VM_RECOVERY              0x00000008
#define LC_VM_FROZEN_FLAG           0x00000010
#define LC_VM_FORCE_DELETE          0x00000020
#define LC_VM_RETRY                 0x00000040
#define LC_VM_VNC_FLAG              0x00000080
#define LC_VM_CHG_FLAG              0x00000100
#define LC_VM_MIGRATE_FLAG          0x00000200

    uint32_t    vm_os;
#define LC_VM_OS_WINDOWS2008            0x101
#define LC_VM_OS_WINDOWSXP              0x102
#define LC_VM_OS_CENTOS                 0x201

    char        vm_template[LC_NAMESIZE];
#define LC_VM_TEMPLATE_WIN2008          "template-win2008"
#define LC_VM_TEMPLATE_WINXP            "template-winxp"
#define LC_VM_TEMPLATE_CENTOS           "template-centos"
#define LC_VM_TEMPLATE_GATEWAY          "template-gateway"

    char        vm_import[LC_IMAGEPATH];

    int         health_state;
    int         power_state;
    uint32_t    action;
#define LC_VM_ACTION_CLEAR  0
#define LC_VM_ACTION_DONE   1
    /* VM id */
    uint32_t    vnet_id;
    uint32_t    vl2_id;
    uint32_t    vm_id;
    char        vm_opaque_id[LC_NAMESIZE];   // used by vmware

    /* logical VM information */
    uint32_t    pool_id;
    char        vm_name[MAX_VM_NAME_LEN];
    char        vm_label[MAX_VM_NAME_LEN];
    char        vm_uuid[MAX_VM_UUID_LEN];
    uint32_t    vm_state;
    uint32_t    vm_error;
    char        vdi_sys_uuid[MAX_VM_UUID_LEN];
#define MAX_UUID_LEN               64
    char        vdi_sys_sr_uuid[MAX_UUID_LEN];
#define MAX_SR_NAME_LEN            LC_NAMESIZE
    char        vdi_sys_sr_name[MAX_SR_NAME_LEN];
    uint64_t    vdi_sys_size;
    char        vdi_user_uuid[MAX_VM_UUID_LEN];
    char        vdi_user_sr_uuid[MAX_UUID_LEN];
    char        vdi_user_sr_name[MAX_SR_NAME_LEN];
    uint64_t    vdi_user_size;
    uint32_t    tport;
    uint32_t    lport;
    char        vm_lcuuid[MAX_VM_UUID_LEN];

    /* Server information */
    char        host_name[MAX_HOST_NAME_LEN];
    host_t      *hostp;

    /* vm interface information */
    intf_t      pub_vif[LC_PUB_VIF_MAX];

    /* vedge interface information */
    uint32_t    intf_num;
    intf_t      pvt_vif[LC_L2_MAX];

    /* vedge ctrl interface information */
    intf_t      ctrl_vif;

    uint32_t    vrid;

    uint32_t    mem_size;
    uint32_t    vcpu_num;
    time_t      cpu_start_time;
    time_t      disk_start_time;
    time_t      traffic_start_time;
    time_t      check_start_time;
    int         backup_sys_disk;
    int         backup_usr_disk;
#define MAX_VM_PASSWD_LEN            16
    char        vm_passwd[MAX_VM_PASSWD_LEN];

    int         user_id;
    char        email_address[MAIL_ADDR_LEN];
} vm_info_t;

typedef struct lb_listener_s {
    int         id;
    char        name[LC_NAMESIZE];
    char        lcuuid[MAX_VM_UUID_LEN];
    char        lb_lcuuid[MAX_VM_UUID_LEN];
} lb_listener_t;

#define LC_KB (1<<10)
#define LC_MB (1<<20)
#define LC_GB (1<<30)

typedef struct monitor_config_s {
    int         id;
    char        from[MAIL_ADDR_LEN];
    char        email_address[MAIL_ADDR_LEN];
    char        email_address_ccs[MAIL_ADDR_LEN];
    time_t      host_warning_time;
    int         host_warning_interval;
    time_t      vm_warning_time;
    int         vm_warning_interval;
    int         host_cpu_threshold;
    time_t      host_cpu_start_time;
    int         host_cpu_warning_usage;
    int         vm_cpu_threshold;
    time_t      vm_cpu_start_time;
    int         vm_cpu_warning_usage;
    int         vm_disk_threshold;
    time_t      vm_disk_start_time;
    int         vm_disk_warning_usage;
    int         host_network_threshold;
    time_t      host_network_start_time;
    int         host_network_warning_usage;
    int         vm_network_threshold;
    time_t      vm_network_start_time;
    int         vm_network_warning_usage;
    int         host_disk_threshold;
    time_t      host_disk_start_time;
    int         host_disk_warning_usage;
    int         vm_mem_threshold;
    time_t      vm_mem_start_time;
    int         vm_mem_warning_usage;
    int         host_mem_threshold;
    time_t      host_mem_start_time;
    int         host_mem_warning_usage;
} monitor_config_t;

#define SR_NAME_LEN 32
#define SR_NUMBER   3

typedef struct {
    uint32_t    storage_id;
    uint32_t    storage_state;
#define LC_SSR_STATE_NORMAL         0x0
    uint32_t    storage_flag;
#define LC_SSR_FLAG_INUSE           0x1
    uint32_t    storage_htype;
#define LC_SSR_HTYPE_XEN            1
#define LC_SSR_HTYPE_VMWARE         2
    char        sr_name[MAX_SR_NAME_LEN];
    uint32_t    disk_total;
    uint32_t    disk_used;
    uint32_t    rack_id;
} shared_storage_resource_t;

typedef struct vmware_msg_reply_s {
  char *data;
  int  len;
} vmware_msg_reply_t;

typedef struct lc_data_vm_s{
    int                 htype;
    int                 vm_cnt;
    union {
        vm_result_t         *vm;
        vmware_msg_reply_t  vmware_vm;
        lc_msg_t            *vm_ops;
    } u_vm;
} lc_data_vm_t;

#define DPQ_TYPE_HOST_INFO 0
#define DPQ_TYPE_VM_INFO   1

#define LC_SNF_KNOB_MAGIC  0xCAFEBEEE
typedef struct lc_snf_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_snf_counter_t lc_counters;
} lc_snf_sknob_t;

extern lc_snf_sknob_t *lc_snf_knob;

typedef struct host_info_response_s {
    char                      host_addr[AGENT_IPV4_ADDR_LEN];
    msg_host_info_response_t  host_info;
} host_info_response_t;

typedef struct lc_data_host_s{
    int                  htype;
    union {
        host_info_response_t   host;
        vmware_msg_reply_t  vmware_host;
    } u;
} lc_data_host_t;

/* VM adding state */
#define LC_VM_STATE_TMP               0
#define LC_VM_STATE_TO_CREATE         1
#define LC_VM_STATE_COMPLETE          2
#define LC_VM_STATE_TO_START          3
#define LC_VM_STATE_RUNNING           4
#define LC_VM_STATE_TO_SUSPEND        5
#define LC_VM_STATE_SUSPENDED         6
#define LC_VM_STATE_TO_RESUME         7
#define LC_VM_STATE_TO_STOP           8
#define LC_VM_STATE_STOPPED           9
#define LC_VM_STATE_MODIFY            10
#define LC_VM_STATE_ERROR             11
#define LC_VM_STATE_TO_DESTROY        12
#define LC_VM_STATE_DESTROYED         13
#define LC_VM_STATE_TO_MIGRATE        14
#define LC_VM_STATE_UNDEFINED         15

/* VEDGE adding state */
#define LC_VEDGE_STATE_TMP               0
#define LC_VEDGE_STATE_TO_CREATE         1
#define LC_VEDGE_STATE_COMPLETE          2
#define LC_VEDGE_STATE_ERROR             3
#define LC_VEDGE_STATE_MODIFY            4
#define LC_VEDGE_STATE_TO_DESTROY        5
#define LC_VEDGE_STATE_TO_START          6
#define LC_VEDGE_STATE_RUNNING           7
#define LC_VEDGE_STATE_TO_STOP           8
#define LC_VEDGE_STATE_STOPPED           9
#define LC_VEDGE_STATE_DESTROYED         10
#define LC_VEDGE_STATE_TO_MIGRATE        11

#define MONITOR_EMAIL_ADDRESS                  6000
#define MONITOR_HOST_CPU_DURATION              6001
#define MONITOR_HOST_CPU_WARNING_USAGE         6002
#define MONITOR_HOST_MEM_DURATION              6003
#define MONITOR_HOST_MEM_WARNING_USAGE         6004
#define MONITOR_HOST_NETWORK_DURATION          6005
#define MONITOR_HOST_NETWORK_WARNING_USAGE     6006
#define MONITOR_HOST_WARNING_INTERVAL          6007
#define MONITOR_VM_WARNING_INTERVAL            6008
#define MONITOR_VM_NETWORK_DURATION            6009
#define MONITOR_VM_NETWORK_WARNING_USAGE       6010
#define MONITOR_VM_DISK_DURATION               6011
#define MONITOR_VM_DISK_WARNING_USAGE          6012
#define MONITOR_VM_CPU_DURATION                6013
#define MONITOR_VM_CPU_WARNING_USAGE           6014
#define MONITOR_VM_MEM_DURATION                6015
#define MONITOR_VM_MEM_WARNING_USAGE           6016
#define MONITOR_HOST_DISK_DURATION             6017
#define MONITOR_HOST_DISK_WARNING_USAGE        6018

#define LC_MONITOR_CONFIG_START_ID 6000
#define LC_MONITOR_CONFIG_END_ID 6018

/*
XEN vm state:
0: Haltd;
1: Paused;
2: Running;
3: Suspended
4: Undefined
*/

#define VMWARE_HOST_STATE_CONNECTED 3

/* VM errno */
#define LC_VM_ERRNO_TIMEOUT              0x00000001
#define LC_VM_ERRNO_DISK_ADD             0x00000002
#define LC_VM_ERRNO_DISK_DEL             0x00000004
#define LC_VM_ERRNO_DISK_SYS_RESIZE      0x00000008
#define LC_VM_ERRNO_DISK_USER_RESIZE     0x00000010
#define LC_VM_ERRNO_CPU_RESIZE           0x00000020
#define LC_VM_ERRNO_MEM_RESIZE           0x00000040
#define LC_VM_ERRNO_DB_ERROR             0x00000080
#define LC_VM_ERRNO_POOL_ERROR           0x00000100
#define LC_VM_ERRNO_NO_IMPORT_VM         0x00000200
#define LC_VM_ERRNO_VM_EXISTS            0x00000400
#define LC_VM_ERRNO_NO_TEMPLATE          0x00000800
#define LC_VM_ERRNO_IMPORT_OP_FAILED     0x00001000
#define LC_VM_ERRNO_INSTALL_OP_FAILED    0x00002000
#define LC_VM_ERRNO_MIGRATE_FAILED       0x00004000
#define LC_VM_ERRNO_IMPORT_VM_STATE_ERR  0x00008000
#define LC_VM_ERRNO_NO_DISK              0x00010000
#define LC_VM_ERRNO_BW_MODIFY            0x00020000
#define LC_VM_ERRNO_NO_MEMORY            0x00040000
#define LC_VM_ERRNO_NO_CPU               0x00080000
#define LC_VM_ERRNO_NO_RESOURCE          0x00100000
#define LC_VM_ERRNO_URL_ERROR            0x00200000
#define LC_VM_ERRNO_CREATE_ERROR         0x00400000
#define LC_VM_ERRNO_DELETE_ERROR         0x00800000
#define LC_VM_ERRNO_START_ERROR          0x01000000
#define LC_VM_ERRNO_STOP_ERROR           0x02000000
#define LC_VM_ERRNO_NOEXISTENCE          0x04000000

/* VEDGE errno */
#define LC_VEDGE_ERRNO_TIMEOUT           0x00000001
#define LC_VEDGE_ERRNO_DB_ERROR          0x00000080
#define LC_VEDGE_ERRNO_POOL_ERROR        0x00000100
#define LC_VEDGE_ERRNO_NO_IMPORT_VM      0x00000200
#define LC_VEDGE_ERRNO_VM_EXISTS         0x00000400
#define LC_VEDGE_ERRNO_NO_TEMPLATE       0x00000800
#define LC_VEDGE_ERRNO_IMPORT_OP_FAILED  0x00001000
#define LC_VEDGE_ERRNO_INSTALL_OP_FAILED 0x00002000
#define LC_VEDGE_ERRNO_MIGRATE_FAILED    0x00004000
#define LC_VEDGE_ERRNO_IMPORT_STATE_ERR  0x00008000
#define LC_VEDGE_ERRNO_NO_DISK           0x00010000
#define LC_VEDGE_ERRNO_NO_MEMORY         0x00020000
#define LC_VEDGE_ERRNO_NO_CPU            0x00040000
#define LC_VEDGE_ERRNO_URL_ERROR         0x00080000
#define LC_VEDGE_ERRNO_CREATE_ERROR      0x00100000
#define LC_VEDGE_ERRNO_DELETE_ERROR      0x00200000
#define LC_VEDGE_ERRNO_START_ERROR       0x00400000
#define LC_VEDGE_ERRNO_STOP_ERROR        0x00800000
#define LC_VEDGE_ERRNO_NOEXISTENCE       0x01000000

#define LC_VM_THIRD_HW_FLAG              0x00004000
#define LC_VEDGE_THIRD_HW_FLAG           0x00002000

#define MAX_VM_CPU_USAGE_LEN         80
typedef struct vcd_vm_stats_s {
    char        vm_name[MAX_VM_NAME_LEN];

    uint32_t    vm_state;
    char        cpu_usage[MAX_VM_CPU_USAGE_LEN];
} vcd_vm_stats_t;

int lc_get_vm_current_state(char *state, int type);
int lc_monitor_get_vm_res_usg_fields(vm_result_t *pvm, monitor_host_t *phost, vm_info_t *vm_info, int htype);

int lcs_host_response_parse(char *pdata,
                            char *host_addr,
                            int htype,
                            int data_len);

#define LCS_VIF_NUM_XEN AGENT_VM_MAX_VIF
#define LCS_VIF_NUM_VMWARE 1

#define LCS_MAX_RACK_NUM 32

typedef struct lcsnf_domain_s {
    char lcuuid[64];
    char master_ctrl_ip[16];
    char local_ctrl_ip[16];
    char mysql_master_ip[16];
}lcsnf_domain_t;
extern lcsnf_domain_t lcsnf_domain;

#define LCS_CONFIG_FILE         "/usr/local/livecloud/conf/livecloud.conf"
#define LCS_SIGN                "lcsnf"
#define LCS_KEY_DOMAIN_LCUUID   "domain.lcuuid"
#define LCS_GLOBAL_SIGN         "global"
#define LCS_KEY_MASTER_CTRL_IP  "master_ctrl_ip"
#define LCS_KEY_LOCAL_CTRL_IP   "local_ctrl_ip"
#define LCS_KEY_MYSQL_MASTER_IP "mysql_master_ip"
#define LCS_DOMAIN_LCUUID_DEFAULT      "00000000-0000-0000-0000-000000000000"

#endif /*_LC_SNF_H */
