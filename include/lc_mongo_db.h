/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-04-07
 *
 */

#ifndef __LC_MONGO_DB_H__
#define __LC_MONGO_DB_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <math.h>

#define SYSLOG_LEVEL LOG_DEBUG

#define MONGO_SYSLOG(level, format, ...) \
    if (level <= SYSLOG_LEVEL) { \
        syslog(level, format, ##__VA_ARGS__); \
    }

#define MONGO_NAME_LEN          64
#define MONGO_CPU_LEN           1024
#define MONGO_IP_LEN            ( 15 + 1 )
#define MONGO_TABLE_NAME_LEN    64
#define MONGO_VGW_VIF_NUM       40
#define MONGO_VM_VIF_NUM        7
#define MONGO_VM_DISK_NUM       16
#define MONGO_UUID_LEN          64
#define MONGO_BK_VM_NUM         64
#define MONGO_SWITCH_PORT_NUM   128
#define OVS_FLOW_BATCH_NUM      256

#ifndef __LC_BR_ID_TYPES__
#define __LC_BR_ID_TYPES__
enum LC_BR_ID_TYPES {
    LC_CTRL_BR_ID = 0,
    LC_DATA_BR_ID = 1,
    LC_ULNK_BR_ID = 2,
    LC_TUNL_BR_ID = 3,

    LC_N_BR_ID
};
#endif

typedef struct mongo_db_conf_s {
    char *host;
    char *port;
    char *user;
    char *passwd;
    char *db;
    char *version;
} mongo_db_conf_t;

typedef struct host_usage_s {
    time_t timestamp;
    int cr_id;
    int cpu_usage_size;
    char *host_name;
    char *cpu_info;
    char *cpu_usage;
    int64_t cpu_total;
    int64_t cpu_allocated;
    int64_t mem_total;
    int64_t mem_usage;
    int64_t disk_total;
    int64_t disk_usage;
} host_usage_t;

typedef struct vm_usage_s {
    uint32_t timestamp;
    int vm_id;
    int cpu_usage_size;
    char *vm_name;
    char *cpu_info;
    char *cpu_usage;
    int64_t mem_total;
    int64_t mem_usage;
    int64_t system_disk_total;
    int64_t system_disk_usage;
    int64_t user_disk_total;
    int64_t user_disk_usage;
} vm_usage_t;

typedef struct traffic_usage_s {
    time_t timestamp;
    int vif_id;
    int unused;
    char *vif_name;
    int64_t rx_bytes;
    int64_t rx_dropped;
    int64_t rx_errors;
    int64_t rx_packets;
    int64_t rx_bps;
    int64_t rx_pps;
    int64_t tx_bytes;
    int64_t tx_dropped;
    int64_t tx_errors;
    int64_t tx_packets;
    int64_t tx_bps;
    int64_t tx_pps;
} traffic_usage_t;

typedef struct hwdev_usage_s {
    int id;
    int userid;
    int cpu_num;
    int dsk_num;
    int mem_size;
    int mem_used;
    time_t curr_time;
    char *hwdev_ip;
    char *hwdev_mac;
    char *sys_uptime;
    char *sys_os;
    char *cpu_type;
    char *cpu_data;
    char * mem_usage;
    char *dsk_info;
    char *hwdev_name;
    char *community;
    char *data_ip;
    char *launch_server;
    char *user_name;
    char *user_passwd;
    char *rack_name;
} hwdev_usage_t;

typedef struct table_index_s {
    char table[MONGO_TABLE_NAME_LEN];
    int  type;
    time_t start_time;
    time_t end_time;
} table_index_t;

typedef struct vgw_server_load_s {
    struct vgw_server_object_s {
        char name[MONGO_NAME_LEN];
        char server_ip[MONGO_IP_LEN];
        int  htype;
    } object;
    time_t timestamp;
    struct {
        char   cpu_usage[MONGO_CPU_LEN];
        double mem_total;
        double mem_usage;
        double disk_total;
        double disk_usage;
        double io_read;
        double io_write;
        double ha_disk_usage;
    } system;
    struct traffic_obj{
        int64_t iftype;
        int64_t rx_bps;
        int64_t tx_bps;
        int64_t rx_pps;
        int64_t tx_pps;
    } traffic[LC_N_BR_ID];
} vgw_server_load_t;
typedef struct traffic_obj traffic_t;
typedef struct vgw_server_object_s vgw_server_object_t;


typedef struct vm_server_load_s {
    uint32_t timestamp;
    struct vm_server_object_s {
        int  htype;
        char name[MONGO_NAME_LEN];
        char server_ip[MONGO_IP_LEN];
    } object;
    struct {
        char   cpu_usage[MONGO_CPU_LEN];
        struct {
            double mem_total;
            double mem_usage;
            double disk_total;
            double disk_usage;
        }dom0;
        struct {
            double mem_total;
            double mem_alloc;
            double mem_usage;
            double disk_total;
            double disk_alloc;
            double disk_usage;
            double io_read;
            double io_write;
        }domu;
        double ha_disk_usage;
    } system;
    struct {
        int64_t iftype;
        int64_t rx_bps;
        int64_t tx_bps;
        int64_t rx_pps;
        int64_t tx_pps;
    } traffic[LC_N_BR_ID];
} vm_server_load_t;
typedef struct vm_server_object_s vm_server_object_t;

typedef struct vgw_load_s {
    struct vgw_object_s {
        int  vgw_id;
    } object;
    time_t timestamp;
    struct vgw_vif_info_s{
        int vif_id;
        int ifindex;
        struct {
            int64_t rx_bps;
            int64_t rx_pps;
            int64_t tx_bps;
            int64_t tx_pps;
        } traffic;
    } vif_info[MONGO_VGW_VIF_NUM];
} vgw_load_t;
typedef struct vgw_object_s vgw_object_t;
typedef struct vgw_vif_info_s vgw_vif_info_t;

typedef struct vm_load_s {
    uint32_t timestamp;
    struct vm_object_s {
        int  vm_id;
    } object;
    struct {
        char cpu_usage[MONGO_CPU_LEN];
        double     mem_usage;
        uint64_t   sys_disk_total;
        uint64_t   sys_disk_usage;
        uint64_t   user_disk_total;
        uint64_t   user_disk_usage;
        double     sys_disk_read_rate;
        double     sys_disk_write_rate;
        double     user_disk_read_rate;
        double     user_disk_write_rate;
    } system;
    struct {
        int vif_id;
        struct {
            int64_t rx_bps;
            int64_t tx_bps;
            int64_t rx_pps;
            int64_t tx_pps;
        } traffic;
    } vif_info[MONGO_VM_VIF_NUM];
    int disk_num;
    struct {
        char device[MONGO_NAME_LEN];
        struct {
            double read_rate;
            double write_rate;
        } io;
    } disk_info[MONGO_VM_DISK_NUM];
} vm_load_t;
typedef struct vm_object_s vm_object_t;

typedef struct flow_load_s {
    time_t timestamp;
    uint64_t packet_count;
    uint64_t byte_count;
    uint64_t duration;
    uint32_t used;
    uint32_t datapath;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t vlantag;
    uint16_t eth_type;
    uint8_t protocol;
    uint8_t direction;
    uint8_t tcp_flags;
    char src_ip[16];
    char dst_ip[16];
    char src_mac[18];
    char dst_mac[18];
} flow_load_t;

typedef struct listener_load_s {
    char   *lcuuid;
    int    lb_id;
    int64_t conn_num;
    time_t timestamp;
    int    bk_vm_num;
    struct {
        char   *name;
        int64_t conn_num;
    } bk_vm[MONGO_BK_VM_NUM];
} listener_load_t;

typedef struct switch_load_s {
    int switch_id;
    char switch_name[MONGO_NAME_LEN];
    char switch_mip[MONGO_IP_LEN];
    time_t timestamp;
    int port_num;
    struct {
        int port_index;
        char port_name[MONGO_NAME_LEN];
        int64_t rx_bps;
        int64_t rx_pps;
        int64_t tx_bps;
        int64_t tx_pps;
    } port[MONGO_SWITCH_PORT_NUM];
} switch_load_t;

typedef enum mongo_app_s {
    MONGO_APP_NONE            = 0,
    MONGO_APP_HOST_USAGE      = 1,
    MONGO_APP_VM_USAGE        = 2,
    MONGO_APP_VGW_USAGE       = 3,
    MONGO_APP_TRAFFIC_USAGE   = 4,
    MONGO_APP_TABLE_INDEX     = 5,
    MONGO_APP_VGW_SERVER_LOAD = 6,
    MONGO_APP_VM_SERVER_LOAD  = 7,
    MONGO_APP_VGW_LOAD        = 8,
    MONGO_APP_VM_LOAD         = 9,
    MONGO_APP_FLOW_LOAD       = 10,
    MONGO_APP_HWDEV_USAGE     = 11,
    MONGO_APP_LISTENER_CONN   = 12,
    MONGO_APP_SWITCH_LOAD     = 13,

    MONGO_APP_NUM,
} mongo_app_t;

typedef int time_grain_t;

int  mongo_db_init(const char *, const char *,
                   const char *, const char *,
                   const char *, const char *,
                   uint32_t);
void mongo_db_exit(void);
int  mongo_db_entry_insert(mongo_app_t, void *);
int mongo_db_entrys_batch_insert(mongo_app_t app, void *entry, int num);
int  mongo_db_table_rename(const char *, const char *);
int  mongo_handler_init(void);
void mongo_handler_exit(void);

#endif /* __LC_MONGO_DB_H__ */
