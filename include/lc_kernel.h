#ifndef _LC_KERNEL_H
#define _LC_KERNEL_H

#include <stdarg.h>

#include "lc_global.h"

#define VERSION "2.3"
#define LC_MAX_THREAD_NUM            160
#define LC_DEFAULT_THREAD_NUM        1
#define MAX_FILE_NAME_LEN            200
#define IP_LEN                       16
#define UUID_LEN                     64
#define MAX_NR_NUM                   100
#define VLAN_RANGE_LEN               64

#define MAX_HOST_NUM                 128
#define MAX_VNET_NUM                 128

#define LC_EXEC_CMD_LEN              512
#define MAX_LOG_MSG_LEN              150
#define MAIL_ADDR_LEN                530

#define LC_LCPD_POSTMAN_NO_FLAG      0
#define LC_LCPD_POSTMAN_NOTIFY_FLAG  1

#define IP_MAX_NUM                   120
#define MAX_WANS_NUM                 8         /*ifindex 1-8*/
#define MAX_LANS_NUM                 30        /*ifindex 10-39*/


#define LC_VM_LABEL_LEN              64


#define LC_KER_SHM_ID               "/tmp/.ker_shm_key"
#define LC_LCPD_SYS_LEVEL_SHM_ID    "/usr/local/livecloud/conf/sql_init_cmd" /*added by kzs for system log level global variable*/

#define LCC_SYS_RUNNING_MODE_UNKNOWN         0
#define LCC_SYS_RUNNING_MODE_INDEPENDENT     1
#define LCC_SYS_RUNNING_MODE_HIERARCHICAL    2

#define LC_FLAG_THREAD               0x00000001
#define LC_FLAG_DAEMON               0x00000002
#define LC_FLAG_CONFIG               0x00000004
#define LC_FLAG_VERSION              0x00000008
#define LC_FLAG_HELP                 0x00000010
#define LC_FLAG_FOREGROUND           0x00000020
#define LC_FLAG_DEBUG                0x00000040
#define LC_FLAG_GRACEFUL_RESTART     0x00000080
#define LC_FLAG_LOGLEVEL             0x00000100
#define LC_FLAG_SERVER_VMWARE        0x00000200
#define LC_FLAG_SERVER_XEN           0x00000400
#define LC_FLAG_CONTROL_MODE_LCM     0x00002000
#define LC_FLAG_CONTROL_MODE_LCC     0x00004000

#define LC_POOL_TYPE_XEN             1
#define LC_POOL_TYPE_VMWARE          2
#define LC_POOL_TYPE_KVM             3
#define LC_HOST_HTYPE_XEN            1
#define LC_HOST_HTYPE_VMWARE         2
#define LC_HOST_HTYPE_KVM            3

#define LC_VM_TYPE_VM                1
#define LC_VM_TYPE_GW                2
#define LC_VM_TYPE_VGATEWAY          3
#define LC_VM_INSTALL_TEMPLATE             0x00000001
#define LC_VM_INSTALL_IMPORT               0x00000002
#define LC_VM_ISOLATE                      0x00000004
#define LC_VM_RECOVERY                     0x00000008
#define LC_VM_FROZEN_FLAG                  0x00000010
#define LC_VM_FORCE_DELETE                 0x00000020
#define LC_VM_RETRY                        0x00000040
#define LC_VM_VNC_FLAG                     0x00000080
#define LC_VM_CHG_FLAG                     0x00000100
#define LC_VM_MIGRATE_FLAG                 0x00000200
#define LC_VM_THIRD_HW_FLAG                0x00004000
#define LC_VM_TALK_WITH_CONTROLLER_FLAG    0x00008000

#define LC_VM_HAFLAG_SINGLE         0x1
#define LC_VM_HAFLAG_HA             0x2
#define LC_VM_HASTATE_MASTER        0x1
#define LC_VM_HASTATE_BACKUP        0x2
#define LC_VM_HASTATE_FAULT         0x3
#define LC_VM_HASTATE_UNKOWN        0x4
#define LC_VM_FLAG_ISOLATE          0x4

/* ROLE: setting as talker */
#define LC_ROLE_GATEWAY             0x7
#define LC_ROLE_VALVE               0xb

#define LC_VNET_ADD_SECU_FLAG       0x00000800
#define LC_VNET_DEL_SECU_FLAG       0x00001000
#define LC_VNET_THIRD_HW_FLAG       0x00002000

#define LC_VMWAF_WEBCONF_WAITING_ADD_FLAG    0x01
#define LC_VMWAF_WEBCONF_WAITING_DEL_FLAG    0x02
#define LC_VMWAF_PROXY_MAX           10
#define LC_VMWAF_CONF_STATE_ENABLE         1
#define LC_VMWAF_CONF_STATE_DISABLE        2
#define LC_VMWAF_CONF_STATE_EXCEPTION      3

#define LC_VIF_ERRNO_NETWORK               2

#define MASK2STRING(x,maskstr,masknum) \
    do{\
       (x) = ~0;\
       (x) = ((x) << (32-(masknum)));\
       sprintf(maskstr, "%d.%d.%d.%d", \
                ((x) >> 24) & 0xff, \
                ((x) >> 16) & 0xff, \
                ((x) >> 8) & 0xff, \
                (x) & 0xff);\
    }while(0)

extern int LCPD_LOG_DEFAULT_LEVEL;
extern int *LCPD_LOG_LEVEL_P;

extern char* LCPD_LOGLEVEL[8];
#define LCPD_LOG(level, format, ...)  \
    if (level <= *LCPD_LOG_LEVEL_P) { \
        syslog(level, "[tid=%lu] %s %s: " format, \
                pthread_self(), LCPD_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }


lc_msg_queue_t lc_data_queue;
lc_ker_sock_t lc_sock;

struct sockaddr_un sa_ker_rcv_drv;
struct sockaddr_un sa_ker_rcv_chk;
struct sockaddr_un sa_ker_rcv_ctl;
struct sockaddr_un sa_ker_snd_drv;
struct sockaddr_un sa_ker_snd_chk;
struct sockaddr_un sa_ker_snd_vm_drv;
struct sockaddr_un sa_app_rcv_lcm;
struct sockaddr_in sa_ker_vcd;
struct sockaddr_in sa_ker_agent;

typedef struct lc_opt_ {
    u_int32_t              lc_flags;
    u_int32_t              thread_num;
    volatile u_int32_t     log_level;
    volatile u_int32_t     log_on;
    char                   config_file[MAX_FILE_NAME_LEN];
}lc_opt_t;

typedef struct lc_ker_counter_s {
    atomic_t    lc_msg_in;
    atomic_t    lc_bus_msg_in;
    atomic_t    lc_drv_msg_in;
    atomic_t    lc_app_msg_in;
    atomic_t    lc_chk_msg_in;
    atomic_t    lc_ctl_msg_in;
    atomic_t    lc_vcd_msg_in;
    atomic_t    lc_agent_msg_in;
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
} lc_ker_counter_t;

#define LC_KER_KNOB_MAGIC  0xCAFEBEEF
typedef struct lc_ker_sknob_s {
    uint32_t magic;
    lc_opt_t lc_opt;
    lc_ker_counter_t lc_counters;
} lc_ker_sknob_t;

extern lc_ker_sknob_t *lc_ker_knob;

static inline void LC_LOG(int level, char *format, ...)
{
    va_list ap;
    char buffer[MAX_LOG_BUF_SIZE] = {0};

    if (lc_ker_knob->lc_opt.log_on) {
        if (level <= lc_ker_knob->lc_opt.log_level) {
            sprintf(buffer, "[tid=%lu] ", pthread_self());
            strcat(buffer, format);
            va_start(ap, format);
            vsyslog(level, buffer, ap);
            va_end(ap);
        }
    }
}

typedef struct network_resource_s {
    uint32_t    id;
    uint32_t    state;
#define LC_NR_STATE_TEMPPUSH            0
#define LC_NR_STATE_PUSHING             1
#define LC_NR_STATE_PULLING             2
#define LC_NR_STATE_MODIFYING           3
#define LC_NR_STATE_COMPLETE            4
#define LC_NR_STATE_EXCEPTION           5
#define LC_NR_STATE_TEMPPULL            6
#define LC_NR_STATE_PULLED              7
    uint32_t    err;

    uint32_t    type;
    uint32_t    portnum;
    uint32_t    poolid;
    char        ip[IP_LEN];
    uint32_t    netdeviceid;
    char        tunnel_ip[IP_LEN];
} network_resource_t;

typedef struct {
    u32         id;
#define LC_BR_NONE         0
#define LC_BR_PIF          1
#define LC_BR_BOND         2
    u32         if0_type;
    u32         if1_type;
    u32         if2_type;
    char        peer_address[MAX_HOST_ADDRESS_LEN];
#define LC_HOST_SINGLE                  0
#define LC_HOST_PEER                    1
#define LC_HOST_HA                      2
    uint32_t    host_service_flag;
#define LC_HOST_ROLE_SERVER             1
#define LC_HOST_ROLE_GATEWAY            2
#define LC_HOST_ROLE_NSP                3
    uint32_t    host_role;
    u32         rackid;
    char        ip[MAX_HOST_ADDRESS_LEN];
    char        uplink_ip[MAX_HOST_ADDRESS_LEN];
    char        uplink_netmask[MAX_HOST_ADDRESS_LEN];
    char        uplink_gateway[MAX_HOST_ADDRESS_LEN];
    u32         type;
    char        domain[UUID_LEN];
} host_t;

typedef struct {
    u32         cr_id;
    char        host_address[MAX_HOST_ADDRESS_LEN];
    int         email_enable;
    char        from[MAIL_ADDR_LEN];
    char        email_address[MAIL_ADDR_LEN];
    char        email_address_ccs[MAIL_ADDR_LEN];
} nsp_host_t;

typedef struct {
    u32         id;
    u32         device_id;
#define IPMI_HOST_DEV              1
#define IPMI_HW_DEV                2
    u32         device_type;
    char        ip[MAX_HOST_ADDRESS_LEN];
#define MAX_USERNAME_LEN           LC_NAMESIZE
    char        username[MAX_USERNAME_LEN];
#define MAX_PASSWORD_LEN           LC_NAMESIZE
    char        password[MAX_PASSWORD_LEN];
} ipmi_t;

typedef struct {
    u32         id;
    u32         type;
    char        ofs_port[LC_NAMESIZE];
    u32         ofs_id;
    char        name[LC_NAMESIZE];
    char        mac[LC_NAMESIZE];
    char        host_ip[MAX_GW_ADDR_LEN];
} host_port_config_t;


typedef struct {
    uint32_t    id;
    uint32_t    rack;
    char        ranges[VLAN_RANGE_LEN];
} vlan_t;

typedef struct {
    char address[IP_LEN];
    char netmask[IP_LEN];
} ips_info_t;

typedef struct {
    uint64_t minbandwidth;
    uint64_t maxbandwidth;
}qos_info_t;

typedef struct {
    uint32_t    routerid;
    uint32_t    state;
    uint32_t    ispid;
    uint32_t    ifindex;
    uint32_t    vlantag;
    char        mac[LC_NAMESIZE];
    char        gateway[IP_LEN];
    ips_info_t  ips[IP_MAX_NUM];
    qos_info_t  qos;
    qos_info_t  broadcast_qos;
} vinterface_info_t;


#define LC_KER_CNT_INC(c) \
    atomic_inc(&lc_ker_knob->lc_counters.c)

#define LC_KER_CNT_DEC(c) \
    atomic_dec(&lc_ker_knob->lc_counters.c)

#define LC_KER_CNT_ADD(i, c) \
    atomic_add(i &lc_ker_knob->lc_counters.c)

#define LC_KER_CNT_SUB(i, c) \
    atomic_sub(i &lc_ker_knob->lc_counters.c)

#define LC_KER_CONFIG_FILE         "/usr/local/livecloud/conf/livecloud.conf"
#define LC_KER_GLOBAL_SIGN         "global"
#define LC_KER_KEY_MASTER_CTRL_IP  "master_ctrl_ip"
#define LC_KER_KEY_LOCAL_CTRL_IP   "local_ctrl_ip"
extern char master_ctrl_ip[];
extern char local_ctrl_ip[];

void dev_init(void);
int dev_psw_add(u64 datapath_id, struct ipv4_addr mgmt_ip);
int dev_psw_del_by_dp(u64 *dpid);

void map_init(void);
int map_lp_del(struct logical_port_id *lpid);

int lc_sock_init(void);
int lc_bus_init_by_kernel(void);
int application_msg_handler(struct lc_msg_head *m_head);
int kernel_msg_handler(struct lc_msg_head *m_head);
int lc_send_msg(lc_mbuf_t *msg);
int lc_publish_msg(lc_mbuf_t *msg);
void lc_main_process();
int lc_bus_lcpd_publish_unicast(char *msg, int len, uint32_t queue_id);

int lc_network_notify(uint32_t id, int type, char *server);
int lc_torswitch_boot(struct lc_msg_head *m_head);
int vm_vport_config(int interface_id, char *acl_action, int callback_flag);
int request_kernel_attach_interface(struct lc_msg_head *m_head, int callback_flag);
int request_kernel_add_hwdevice(struct lc_msg_head *m_head);
int request_kernel_migrate_vgateway(struct lc_msg_head *m_head);
int request_kernel_migrate_valve(struct lc_msg_head *m_head);
int nsp_vgws_migrate(vnet_t *vnet, char *dst_nsp_ip);
int is_server_mode_xen(uint32_t vdevice_type, uint32_t vdevice_id);
#endif
