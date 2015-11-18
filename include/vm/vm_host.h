#ifndef _LC_VM_HOST_H
#define _LC_VM_HOST_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef _LC_MONITOR_
#include "lc_mon.h"
#else
#include "lc_vm.h"
#include "../message/msg_vm.h"
#include "../message/msg.h"
#endif
#include "../lc_netcomp.h"
#include "xen/api/xen_common.h"
#include "agent_sizes.h"

#ifdef _LC_MONITOR_
#define LC_VM_HOST_LOG(level, format, ...)  \
    LCMON_LOG(level, format, ##__VA_ARGS__)
#else
#define LC_VM_HOST_LOG(level, format, ...)  \
    VMDRIVER_LOG(level, format, ##__VA_ARGS__)
#endif

#define MAX_VM_PER_HOST            200
#define MAX_VGW_NUM                256
#define MAIL_ADDR_LEN              530
#define MAX_HOST_NUM               256
#define MAX_RPOOL_NUM              MAX_HOST_NUM
#define MAX_LB_NUM                 512

#define MAX_UUID_LEN               64
#define MAX_SR_NAME_LEN            LC_NAMESIZE
#define MAX_VDI_NAME_LEN           128
#define LC_SR_PRIVATE_LEN          256
#define LC_VM_INSTALL_CMD_LEN      256
#define MAX_RPOOL_NAME_LEN         LC_NAMESIZE
#define MAX_RPOOL_HOST_NUM         50

#define MAX_HOST_NAME_LEN          LC_NAMESIZE
#define MAX_HOST_UUID_LEN          64
#define MAX_HOST_SR_NAME_LEN       LC_NAMESIZE
#define MAX_PASSWORD_LEN           LC_NAMESIZE
#define MAX_USERNAME_LEN           LC_NAMESIZE
#define MAX_HOST_URL_LEN           32

#define VM_HOST_FREE               0x00000001
#define VM_HOST_INUSE              0x00000002
#define VM_HOST_CONNECTED          0x00000004

/* Pool mode */
#define LC_POOL_MODE_COMMON        0
#define LC_POOL_MODE_MASTER_SLAVE  1

#define VM_HOST_MASTER             0x10000000
#define VM_HOST_SLAVE              0x20000000

#define LC_VM_CREATE               1
#define LC_VM_QUERY                2

/* Host running state */
#define LC_HOST_STATE_SHUTDOWN      100
#define LC_HOST_STATE_RUNNING       101
#define LC_HOST_STATE_CONNECT_FAIL  102
#define LC_HOST_STATE_UNKNOWN       103


#define LC_HOST_CPU_INFO       128
#define LC_HOST_CPU_USAGE      704

#define TEMP_HOST_NUMBER       200

/* Host adding state */
#define LC_HOST_STATE_TMP             0
#define LC_HOST_STATE_CREATING        1
#define LC_HOST_STATE_COMPLETE        2
#define LC_HOST_STATE_CHANGING        3
#define LC_HOST_STATE_ERROR           4
#define LC_HOST_STATE_MAINTAINED      5

/* Compute Resource state */
#define LC_CR_STATE_TMP               0
#define LC_CR_STATE_TO_JOIN           1
#define LC_CR_STATE_TO_EJECT          2
#define LC_CR_STATE_TO_MODIFY         3
#define LC_CR_STATE_COMPLETE          4
#define LC_CR_STATE_ERROR             5
#define LC_CR_TMP_EJECT               6
#define LC_CR_STATE_EJECTED           7
#define LC_CR_STATE_MAINTAINED        8

/* Storage Resource state */
#define LC_SR_STATE_TMP               0
#define LC_SR_STATE_TO_JOIN           1
#define LC_SR_STATE_TO_EJECT          2
#define LC_SR_STATE_TO_MODIFY         3
#define LC_SR_STATE_COMPLETE          4
#define LC_SR_STATE_ERROR             5
#define LC_SR_STATE_TP_EJECT          6
#define LC_SR_STATE_EJECTED           7

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

/* Snapshot creating state */
#define LC_VM_SNAPSHOT_STATE_TO_CREATE   1
#define LC_VM_SNAPSHOT_STATE_COMPLETE    2
#define LC_VM_SNAPSHOT_STATE_TO_DELETE   3
#define LC_VM_SNAPSHOT_STATE_DELETED     4
#define LC_VM_SNAPSHOT_STATE_TO_RECOVERY 5
#define LC_VM_SNAPSHOT_STATE_ERROR       6

/* VM Backup state */
#define LC_VM_BACKUP_STATE_TO_CREATE     1
#define LC_VM_BACKUP_STATE_COMPLETE      2
#define LC_VM_BACKUP_STATE_TO_DELETE     3
#define LC_VM_BACKUP_STATE_DELETED       4
#define LC_VM_BACKUP_STATE_TO_RECOVERY   5
#define LC_VM_BACKUP_STATE_ERROR         6

/* VM Backup recovery */
#define LC_VM_BACKUP_FLAG_CREATE_LOCAL   1
#define LC_VM_BACKUP_FLAG_CREATE_REMOTE  2
#define LC_VM_BACKUP_FLAG_RESTORE_LOCAL  4
#define LC_VM_BACKUP_FLAG_RESTORE_REMOTE 8

/* VM/VEDGE third party device flag */
#define LC_VM_THIRD_HW_FLAG              0x00004000
#define LC_VEDGE_THIRD_HW_FLAG           0x00002000

#define LC_MB (1<<20)
#define LC_GB (1<<30)

#define LC_VM_STARTUP_ERROR         1
#define LC_NO_ENOUGH_DISK           2

#define MAX_VM_HA_DISK              2

#define HA_DISK_NAME_PREFIX         "ha_"
#define HA_DISK_NAME_PREFIX_LEN     3

typedef struct host_res_usg_s {
    char        host_name[MAX_HOST_NAME_LEN];
    char        host_address[MAX_HOST_ADDRESS_LEN];
    char        host_uuid[MAX_HOST_UUID_LEN];
    char        cpu_info[LC_HOST_CPU_INFO];
    char        cpu_usage[LC_HOST_CPU_USAGE];
    uint32_t    mem_total;
    uint32_t    mem_usage;
    int         disk_total;
    int         disk_usage;
    int         cr_id;
} host_res_usg_t;

typedef struct vm_res_usg_s {
    char		vm_name[MAX_VM_NAME_LEN];
    char		vm_label[MAX_VM_NAME_LEN];
    char		vm_uuid[MAX_VM_UUID_LEN];
    char        cpu_info[LC_HOST_CPU_INFO];
    char        cpu_usage[LC_HOST_CPU_USAGE];
    uint32_t    mem_total;
    uint32_t    mem_usage;
    int         system_disk_total;
    uint64_t    system_disk_usage;
    int         user_disk_total;
    uint64_t    user_disk_usage;
    int         vm_id;
} vm_res_usg_t;

#define LC_MAX_SR_NUM                   8
typedef struct storage_info_s {
    uint32_t    id;
    char        name_label[MAX_SR_NAME_LEN];
    char        uuid[MAX_UUID_LEN];
    uint32_t    is_shared;
    uint32_t    disk_total;
    uint32_t    disk_used;
} storage_info_t;

typedef struct host_s {
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
    int         disk_total;
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
    storage_info_t   storage[LC_MAX_SR_NUM];
    uint32_t    n_hastorage;
    storage_info_t   ha_storage[MAX_VM_HA_DISK];
} host_t;

typedef struct resource_pool_s {
    uint32_t    rpool_index;
    char        rpool_name[MAX_RPOOL_NAME_LEN];
    uint32_t    rpool_flag;
    char        rpool_desc[LC_NAMEDES];
    uint32_t    ctype;
#define LC_POOL_TYPE_XEN    1
#define LC_POOL_TYPE_VMWARE 2
#define LC_POOL_TYPE_KVM    3

    uint32_t    stype;
#define LC_POOL_TYPE_LOCAL  1
#define LC_POOL_TYPE_SHARED 2

    char        master_address[MAX_HOST_ADDRESS_LEN];
} rpool_t;

#ifdef _LC_MONITOR_
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

typedef struct monitor_host_s {
    uint32_t    htype;
#define LC_POOL_TYPE_XEN    1
#define LC_POOL_TYPE_VMWARE 2
#define LC_POOL_TYPE_KVM    3

    uint32_t    stype;
#define LC_POOL_TYPE_LOCAL  1
#define LC_POOL_TYPE_SHARED 2

    uint32_t    type;
#define LC_HOST_ROLE_SERVER  1
#define LC_HOST_ROLE_GATEWAY 2
#define LC_HOST_ROLE_NSP     3
    char        host_name[MAX_HOST_NAME_LEN];
    char        host_address[MAX_HOST_ADDRESS_LEN];
    char        master_address[MAX_HOST_ADDRESS_LEN];
    char        host_uuid[MAX_HOST_UUID_LEN];
    char        email_address[MAIL_ADDR_LEN];
    char        email_address_ccs[MAIL_ADDR_LEN];
    char        from[MAIL_ADDR_LEN];
    int         email_enable;
    uint32_t    host_state;
    uint32_t    master_host_state;
    int         error_number;
    int         master_error_number;
    uint32_t    pool_id;
    char        username[MAX_USERNAME_LEN];
    char        password[MAX_PASSWORD_LEN];
    char        master_username[MAX_USERNAME_LEN];
    char        master_password[MAX_PASSWORD_LEN];
#define LC_HOST_MASTER                1
#define LC_HOST_SLAVE                 2
    int         role;
    int         health_check;
    int         master_health_check;
    char        cpu_info[LC_HOST_CPU_INFO];
    char        cpu_usage[LC_HOST_CPU_USAGE];
    int         cpu_used;
    int         mem_usage;
    int         mem_total;
    int         disk_usage;
    int         disk_total;
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

#endif

typedef struct storage_resource_s {
    uint32_t    storage_id;
    uint32_t    storage_state;
    uint32_t    storage_error;
    char        host_address[MAX_HOST_ADDRESS_LEN];
    uint32_t    storage_type;
    char        private[LC_SR_PRIVATE_LEN];
    char        sr_uuid[MAX_UUID_LEN];
    char        sr_name[MAX_SR_NAME_LEN];
} storage_resource_t;

typedef struct {
    uint32_t    storage_id;
    uint32_t    storage_state;
#define LC_SSR_STATE_NORMAL         0x0
    uint32_t    storage_flag;
#define LC_SSR_FLAG_INUSE           0x1
    uint32_t    storage_htype;
#define LC_SSR_HTYPE_XEN            1
#define LC_SSR_HTYPE_VMWARE         2
#define LC_SSR_HTYPE_KVM            3
    char        sr_name[MAX_SR_NAME_LEN];
    uint32_t    disk_total;
    uint32_t    disk_used;
    uint32_t    rack_id;
} shared_storage_resource_t;

typedef struct {
    uint32_t    id;
    char        vdi_uuid[MAX_VM_UUID_LEN];
    char        vdi_name[MAX_VDI_NAME_LEN];
    char        vdi_sr_uuid[MAX_UUID_LEN];
    char        vdi_sr_name[MAX_SR_NAME_LEN];
    uint64_t    size;
    uint32_t    vmid;
    uint32_t    userdevice;
} disk_t;

typedef struct vm_info_s {
    uint32_t    vm_type;
#define LC_VM_TYPE_VM       1
#define LC_VM_TYPE_GW       2
#define LC_VM_TYPE_VGATEWAY 3

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
#define LC_VM_REPLACE_FLAG          0x00000400
#define LC_VM_HA_DISK_FLAG          0x00000800
#define LC_VM_SET_HADISK_FLAG       0x00001000
#define LC_VM_EXPIRED_FLAG          0x00002000
#define LC_VM_THIRD_HW_FLAG         0x00004000

#define LC_VNET_THIRD_HW_FLAG       0x00002000

    uint32_t    vm_os;
#define LC_VM_OS_WINDOWS2008            0x101
#define LC_VM_OS_WINDOWSXP              0x102
#define LC_VM_OS_CENTOS                 0x201

    uint32_t    vm_template_type;
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
    char        vdi_sys_sr_uuid[MAX_UUID_LEN];
    char        vdi_sys_sr_name[MAX_SR_NAME_LEN];
    uint64_t    vdi_sys_size;
    char        vdi_user_uuid[MAX_VM_UUID_LEN];
    char        vdi_user_sr_uuid[MAX_UUID_LEN];
    char        vdi_user_sr_name[MAX_SR_NAME_LEN];
    uint64_t    vdi_user_size;
    uint32_t    tport;
    uint32_t    lport;

    /* Server information */
    char        host_address[MAX_HOST_NAME_LEN];
    char        host_name_label[MAX_HOST_NAME_LEN];
    host_t      *hostp;

    /* vm interface information */
    intf_t      pub_vif[LC_PUB_VIF_MAX];
#define LC_VM_IFACE_CSTM 3
#define LC_VM_IFACE_MGMT 4
#define LC_VM_IFACE_SERV 5
#define LC_VM_IFACE_CTRL 6
#define LC_VM_IFACES_MAX 7
    intf_t      ifaces[LC_VM_IFACES_MAX];

    /* vedge interface information */
    uint32_t    intf_num;
    intf_t      pvt_vif[LC_L2_MAX];

    /* vedge ctrl interface information */
    intf_t      ctrl_vif;

    /* vedge ha information */
    uint32_t    ha_flag;
#define LC_VM_HAFLAG_SINGLE        0x1
#define LC_VM_HAFLAG_HA            0x2

    uint32_t    ha_state;
#define LC_VM_HASTATE_MASTER       0x1
#define LC_VM_HASTATE_BACKUP       0x2
#define LC_VM_HASTATE_FAULT        0x3
#define LC_VM_HASTATE_UNKOWN       0x4

    uint32_t    ha_switchmode;
#define LC_VM_HAMODE_NO            0x0
#define LC_VM_HAMODE_AUTO          0x1
#define LC_VM_HAMODE_MANUAL        0x2

    uint32_t    vrid;

    uint32_t    mem_size;
    uint32_t    vcpu_num;
    time_t      cpu_start_time;
    time_t      disk_start_time;
    time_t      traffic_start_time;
    time_t      check_start_time;
    int         backup_sys_disk;
    int         backup_usr_disk;
    char        vm_passwd[MAX_VM_PASSWD_LEN];

    int         user_id;
    char        email_address[MAIL_ADDR_LEN];
    char        email_address_ccs[MAIL_ADDR_LEN];
} vm_info_t;

typedef struct lb_listener_s {
    int         id;
    char        name[LC_NAMESIZE];
    char        lcuuid[MAX_VM_UUID_LEN];
    char        lb_lcuuid[MAX_VM_UUID_LEN];
} lb_listener_t;

typedef struct lb_bk_vm_s {
    int         id;
    char        name[LC_NAMESIZE];
    uint32_t    health_state;
#define LC_BK_VM_HEALTH_STATE_DOWN      0x0
#define LC_BK_VM_HEALTH_STATE_UP        0x1
#define LC_BK_VM_HEALTH_STATE_NO_CHECK  0x2
    char        lcuuid[MAX_VM_UUID_LEN];
    char        lb_listener_lcuuid[MAX_VM_UUID_LEN];
    char        lb_lcuuid[MAX_VM_UUID_LEN];
} lb_bk_vm_t;

typedef struct vm_snapshot_info_t {
    uint32_t    snapshot_id;
    char        snapshot_name[MAX_VM_NAME_LEN];
    char        snapshot_label[MAX_VM_NAME_LEN];
    char        snapshot_uuid[MAX_VM_UUID_LEN];
    uint32_t    state;
    uint32_t    error;
    uint32_t    vm_id;
} vm_snapshot_info_t;

typedef struct vm_backup_info_t {
    uint32_t    dbr_id;
    uint32_t    state;
    uint32_t    error;
    uint32_t    vm_id;
    char        vm_label[MAX_VM_NAME_LEN];
    char        resident_server[MAX_HOST_ADDRESS_LEN];
    char        resident_username[MAX_USERNAME_LEN];
    char        resident_passwd[MAX_PASSWORD_LEN];
    char        peer_server[MAX_HOST_ADDRESS_LEN];
    char        peer_username[MAX_USERNAME_LEN];
    char        peer_passwd[MAX_PASSWORD_LEN];
    char        local_sr_uuid[MAX_UUID_LEN];
    char        peer_sr_uuid[MAX_UUID_LEN];
    int         sys_disk;
    int         usr_disk;

    uint32_t    backup_flags;

    char        local_vdi_sys_uuid[MAX_UUID_LEN];
    char        local_vdi_user_uuid[MAX_UUID_LEN];
    char        peer_vdi_sys_uuid[MAX_UUID_LEN];
    char        peer_vdi_user_uuid[MAX_UUID_LEN];
} vm_backup_info_t;

typedef struct third_device_s{
    uint32_t    id;
    uint32_t    vm_id;
    uint32_t    type;
#define LC_THIRD_DEVICE_TYPE_VM   1
#define LC_THIRD_DEVICE_TYPE_GW   2
    char        data_mac[LC_L2_MAX][MAX_VIF_MAC_LEN];
} third_device_t;

typedef struct vm_query_info_s {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_label[MAX_VM_UUID_LEN];
    char        vm_template[LC_NAMESIZE];
    char        vdi_sys_uuid[MAX_VM_UUID_LEN];
    char        vdi_user_uuid[MAX_VM_UUID_LEN];
    uint32_t    pw_state;
    int         cpu_number;
    int         mem_total;
    int         sys_disk;
    int         user_disk;
    uint32_t    vm_id;
} vm_query_info_t;

#define LC_PRE_VIF_MAX  6
#define LC_CUR_VIF_MAX  8
#define LC_PUB_VIF_MAX  3
#define LC_L2_MAX  3
#define LC_CTRL_VIF_MAX 1
typedef struct vnet_query_info_s {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_label[MAX_VM_UUID_LEN];
    char        vm_template[LC_NAMESIZE];
    uint32_t    pub_ifid[LC_PUB_VIF_MAX];
    uint32_t    pvt_ifid[LC_L2_MAX];
    uint32_t    ctrl_ifid;
    uint32_t    vm_id;
    uint32_t    vdc_id;
    uint32_t    pw_state;
} vnet_query_info_t;

typedef struct vgw_ha_query_info_s {
    char        ctl_ip[MAX_HOST_ADDRESS_LEN];
    char        label[MAX_VM_UUID_LEN];
    uint32_t    ha_state;
    uint32_t    vnet_id;
} vgw_ha_query_info_t;

typedef struct lb_query_info_s {
    char        ctl_ip[MAX_HOST_ADDRESS_LEN];
    char        label[LC_NAMESIZE];
    char        lcuuid[MAX_VM_UUID_LEN];
    char        server[MAX_HOST_ADDRESS_LEN];
} lb_query_info_t;

#define LC_DOMAIN_LEN                64
typedef struct vmwaf_query_info_vm_s {
    int         id;
    int         state;
    int         pre_state;
    int         flag;
    int         eventid;
    int         errorno;
    char        label[MAX_VM_UUID_LEN];
    char        description[LC_NAMEDES];
    int         userid;
    int         ctrl_ifid;
    int         wan_if1id;
    int         lan_if1id;
    char        launch_server[MAX_HOST_ADDRESS_LEN];
    int         mem_size;
    int         vcpu_num;
    char        domain[LC_DOMAIN_LEN];
} vmwaf_query_info_vm_t;

#define LC_TEMPLATE_OS_LOCAL     0
#define LC_TEMPLATE_OS_REMOTE    1
#define LC_TEMPLATE_STATE_ON     0
#define LC_TEMPLATE_STATE_OFF    1
#define LC_TEMPLATE_OS_XEN       1
#define LC_TEMPLATE_OS_VMWARE    2
typedef struct template_os_info_s {
    char        server_ip[MAX_HOST_ADDRESS_LEN];
    char        vm_template[LC_NAMESIZE];
    uint32_t    type;
    uint32_t    htype;
    uint32_t    state;
} template_os_info_t;

typedef struct cluster_s {
    int        id;
    int        vcdc_id;
} cluster_t;

typedef struct rack_s {
    int        id;
    int        cluster_id;
} rack_t;

typedef struct vcdc_s {
    int        id;
    char       vswitch_name[LC_NAMESIZE];
} vcdc_t;

#define LC_HOST_CONNECT(host, session)                              \
do {                                                                \
    char url_name[MAX_HOST_URL_LEN];                                \
    memset(url_name, 0, MAX_HOST_URL_LEN);                          \
    sprintf(url_name, "https:%s%s%s/", "/", "/" ,host->host_address);     \
    session = (xen_session *)xen_create_session(url_name, host->username, \
        host->password);                                            \
    LC_VM_HOST_LOG(LOG_DEBUG, "%s: session:%p url=%s user=*** password=***\n",           \
        __FUNCTION__, session, url_name); \
    if (!session) {                                                 \
        LC_VM_HOST_LOG(LOG_ERR, "%s: create xen session failure: (address:%s)\n",\
            __FUNCTION__,host->host_address);                       \
        session = NULL;                                             \
    }                                                                \
    if ((session)&&!lc_host_health_check(host->host_address, session)) {                           \
        xen_session_logout(session);                      \
        session = NULL;                                             \
    }                                                               \
} while (0)

#define LC_HOST_CONNECT_NO_CHECK(host, session)                               \
do {                                                                          \
    char url_name[MAX_HOST_URL_LEN];                                          \
    memset(url_name, 0, MAX_HOST_URL_LEN);                                    \
    sprintf(url_name, "https:%s%s%s/", "/", "/" ,host->host_address);         \
    session = (xen_session *)xen_create_session(url_name, host->username,     \
        host->password);                                                      \
    LC_VM_HOST_LOG(LOG_DEBUG, "%s: session:%p url=%s user=*** password=***\n",        \
        __FUNCTION__, session, url_name);                                     \
} while (0)

#define LC_HOST_FREE(host, session)                  \
do {                                                    \
    if (host && session) {                           \
        xen_session_logout(session);                 \
        session = NULL;                              \
    }                                                \
} while (0)

#ifdef _LC_MONITOR_
int lc_get_vm_from_db_by_uuid (vm_info_t *vm_info, char *vm_uuid);
int lc_get_vm_from_db_by_import_namelable(vm_info_t *vm_info, char *import_name_lable);
int lc_get_vedge_from_db_by_namelable (vm_info_t *vm_info, char *vm_lable);
int lc_get_all_compute_resource (host_t *host_info, int offset, int *host_num);
int lc_get_config_info_from_system_config(monitor_config_t *cfg_info);
int lc_update_info_to_system_config(monitor_config_t *cfg_info);
#endif
int lc_get_vm_from_db_by_namelable (vm_info_t *vm_info, char *vm_lable);

#endif /* _LC_VM_HOST_T */
