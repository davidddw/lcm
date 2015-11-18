#ifndef _LC_DB_H
#define _LC_DB_H

#include <pthread.h>
#include "mysql/mysql.h"
#include "lc_global.h"

#define LC_DB_BUF_LEN                     1024
#define LC_DB_INTEGER_LEN                 16
#define LC_DB_IP_LEN                      4

#define MAX_POOL_DB_BUFFER                256
#define MAX_SERVER_DB_BUFFER              512
#define MAX_VNET_DB_BUFFER                512
#define MAX_STORAGE_DB_BUFFER             512
#define MAX_RES_USAGE_DB_BUFFER           2048

#define LC_DB_SERVER_UPDATE_CPU           1
#define LC_DB_SERVER_UPDATE_MEM           2
#define LC_DB_SERVER_UPDATE_DISK          3
#define LC_DB_SERVER_UPDATE_BW_TOTAL      4
#define LC_DB_SERVER_UPDATE_CPU_USAGE     6
#define LC_DB_SERVER_UPDATE_MEM_USAGE     7
#define LC_DB_SERVER_UPDATE_DISK_USAGE    8
#define LC_DB_SERVER_UPDATE_BW_USAGE      9

#define LC_DB_VNET_UPDATE_PUB_INTF        1
#define LC_DB_VNET_UPDATE_SUB_INTF1       2
#define LC_DB_VNET_UPDATE_SUB_INTF2       3
#define LC_DB_VNET_UPDATE_SUB_INTF3       4
#define LC_DB_VNET_UPDATE_SUB_INTF4       5
#define LC_DB_VNET_UPDATE_SUB_INTF5       6

/* DB load error code */

#define LC_DB_OK                          0
#define LC_DB_COMMON_ERROR                1
#define LC_DB_EMPTY_SET                   2
#define LC_DB_HOST_NOT_FOUND              3
#define LC_DB_HOST_FOUND                  0
#define LC_DB_VM_NOT_FOUND                5
#define LC_DB_VM_FOUND                    0
#define LC_DB_POOL_NOT_FOUND              7
#define LC_DB_POOL_FOUND                  0
#define LC_DB_VEDGE_NOT_FOUND             9
#define LC_DB_VEDGE_FOUND                 0
#define LC_DB_VIF_NOT_FOUND               11
#define LC_DB_VIF_FOUND                   0
#define LC_DB_STORAGE_NOT_FOUND           13
#define LC_DB_STORAGE_FOUND               0
#define LC_DB_VL2_NOT_FOUND               13
#define LC_DB_VL2_FOUND                   0
#define LC_DB_SNAPSHOT_NOT_FOUND          17
#define LC_DB_SNAPSHOT_FOUND              0
#define LC_DB_BACKUP_NOT_FOUND            19
#define LC_DB_BACKUP_FOUND                0
#define LC_DB_VLAN_NOT_FOUND              21
#define LC_DB_VLAN_FOUND                  0
#define LC_DB_VCDC_NOT_FOUND              23
#define LC_DB_VCDC_FOUND                  0
#define LC_DB_VMWAF_NOT_FOUND             24
#define LC_DB_VMWAF_FOUND                 0
#define LC_DB_WEB_CONF_NOT_FOUND          25
#define LC_DB_WEB_CONF_FOUND              0
#define LC_DB_PORT_MAP_NOT_FOUND          26
#define LC_DB_PORT_MAP_FOUND              0
#define LC_DB_VNET_NOT_FOUND              27
#define LC_DB_VNET_FOUND                  0
#define LC_DB_THIRD_DEVICE_NOT_FOUND      28
#define LC_DB_THIRD_DEVICE_FOUND          0

/* syslog columns */
#define LC_SYSLOG_ACTION_CREATE_VM        "Create VM"
#define LC_SYSLOG_ACTION_DELETE_VM        "Delete VM"
#define LC_SYSLOG_ACTION_START_VM         "Start VM"
#define LC_SYSLOG_ACTION_STOP_VM          "Stop VM"
#define LC_SYSLOG_ACTION_ATTACH_VIF       "Attach VIF"
#define LC_SYSLOG_ACTION_DETACH_VIF       "Detach VIF"
#define LC_SYSLOG_ACTION_MODIFY_VM        "Modify VM"
#define LC_SYSLOG_ACTION_ISOLATE_VM       "Isolate VM"
#define LC_SYSLOG_ACTION_RELEASE_VM       "Release VM"
#define LC_SYSLOG_ACTION_RECOVERY_VM      "Recovery VM"
#define LC_SYSLOG_ACTION_CREATE_VM_SS     "Create snapshot"
#define LC_SYSLOG_ACTION_DELETE_VM_SS     "Delete snapshot"
#define LC_SYSLOG_ACTION_BOOT_VM          "Boot VM"
#define LC_SYSLOG_ACTION_DOWN_VM          "Down VM"
#define LC_SYSLOG_ACTION_OP_VM            "Start/Stop VM"
#define LC_SYSLOG_ACTION_CREATE_VGW       "Create VGW"
#define LC_SYSLOG_ACTION_DELETE_VGW       "Delete VGW"
#define LC_SYSLOG_ACTION_DELETE_VGATEWAY  "Delete VGATEWAY"
#define LC_SYSLOG_ACTION_DELETE_VALVE     "Delete VALVE"
#define LC_SYSLOG_ACTION_START_VGW        "Start VGW"
#define LC_SYSLOG_ACTION_STOP_VGW         "Stop VGW"
#define LC_SYSLOG_ACTION_MODIFY_VGW       "Modify VGW"
#define LC_SYSLOG_ACTION_SWITCH_VGW       "Switch VGW"
#define LC_SYSLOG_ACTION_CONNECT_VNC      "Connect VNC"
#define LC_SYSLOG_ACTION_BOOT_VGW         "Boot VGW"
#define LC_SYSLOG_ACTION_DOWN_VGW         "Down VGW"
#define LC_SYSLOG_ACTION_ERR              "Failed"
#define LC_SYSLOG_ACTION_WARNING          "Warning"
#define LC_SYSLOG_ACTION_CREATE_VMWAF     "Add vmwaf config"
#define LC_SYSLOG_ACTION_DELETE_VMWAF     "Delete vmwaf config"
#define LC_SYSLOG_ACTION_ATTACH_HWDEV     "Attach HWDev"
#define LC_SYSLOG_ACTION_DETACH_HWDEV     "Detach HWDev"
#define LC_SYSLOG_OBJECTTYPE_CLOUD        "cloud"
#define LC_SYSLOG_OBJECTTYPE_VM           "vm"
#define LC_SYSLOG_OBJECTTYPE_VGW          "vgw"
#define LC_SYSLOG_OBJECTTYPE_VGATEWAY     "vgateway"
#define LC_SYSLOG_OBJECTTYPE_VALVE        "valve"
#define LC_SYSLOG_OBJECTTYPE_VMWAF        "vmwaf"
#define LC_SYSLOG_OBJECTTYPE_HOST         "host"
#define LC_SYSLOG_OBJECTTYPE_HWDEV        "hwdev"
#define LC_SYSLOG_LEVEL_ERR               LOG_ERR
#define LC_SYSLOG_LEVEL_WARNING           LOG_WARNING
#define LC_SYSLOG_LEVEL_INFO              LOG_INFO

/* Resource pool */

#define LC_DB_POOL_COL_STR     "id, name, description, flag"
#define LC_DB_SERVER_COL_STR   "ip, user_name, user_passwd, pool"
#define LC_DB_VNET_COL_STR     "id, name, vl2_num, tport, lport"
#define LC_DB_VL2_COL_STR      "id, name, port_num, vnet"
#define LC_DB_VM_COL_STR       "id, name, state, os, launch_server, if_bandw, vl2, vnet"
#define LC_DB_SNAPSHOT_COL_STR "name, vl2, vnet, vm, description, serverip"
#define LC_DB_STORAGE_COL_STR  "name, rack_id, uuid, is_shared, disk_total, disk_used"
#define LC_DB_SC_COL_STR       "host_address, storage_id"
#define LC_DB_SSR_COL_STR      "state, flag, htype, sr_name, disk_total, rackid"
#define LC_DB_XVA_COL_STR      "name, vl2, vnet, vm, description, serverip, path"
#define LC_DB_HOST_USG_COL_STR "cr_id, host_name_lable, cpu_info, cpu_usage, mem_total, mem_usage, disk_total, disk_usage"
#define LC_DB_VM_USG_COL_STR "vm_id, vm_name_lable, cpu_info, cpu_usage, mem_total, mem_usage, system_disk_total, system_disk_usage, user_disk_total, user_disk_usage"
#define LC_DB_LOG_COL_STR      "module, action, loginfo, level"
#define LC_DB_SYSLOG_COL_STR   "module, action, objecttype, objectid, objectname, loginfo, level"

/* DB columns */

#define LC_DB_POOL_COLUMNS_LIST        "id, name, description"
#define LC_DB_SERVER_COLUMNS_LIST      "ip, pool, state, description, user_name, user_passwd"
#define LC_DB_VL2_COLUMNS_LIST         "vnet, id, name, port_num"
#define LC_DB_VM_COLUMNS_LIST          "vnet, vl2, id, name, state, os, launch_server, if_name, if_status, if_bandw, if_ofport"

#define TABLE_INSERT                   "insert into %s_v%s(%s) values(%s);"
#define TABLE_DELETE                   "delete from %s_v%s where %s;"
#define TABLE_UPDATE                   "update %s_v%s set %s=%s where %s;"
#define TABLE_MULTI_UPDATE             "update %s_v%s set %s where %s;"
#define TABLE_DUMP                     "select %s from %s_v%s;"
#define TABLE_LOAD                     "select %s from %s_v%s where %s;"
#define TABLE_LOAD_ORDER_BY_ID         "select %s from %s_v%s where %s order by id;"
#define TABLE_LOAD_ORDER               "select %s from %s_v%s order by %s;"
#define TABLE_LOAD_WHERE_ORDER         "select %s from %s_v%s where %s order by %s;"
#define TABLE_LOAD_DISTINCT_WHERE      "select distinct %s from %s_v%s where %s order by %s;"
#define TABLE_LOAD_INNER_JOIN          "select %s from %s_v%s table1 inner join " \
                                       "%s_v%s table2 on %s where %s"
#define TABLE_SELECT_COUNT             "select count(%s) from %s_v%s where %s;"
#define TABLE_SELECT_COUNT_DISTINCT    "select count(distinct %s) from %s_v%s where %s;"

struct db_config {
    char *host;
    char *user;
    char *passwd;
    char *dbname;
    char *dbport;
    char *version;
};

struct thread_mysql {
    pthread_t tid;
    MYSQL *conn_handle;
};

extern struct db_config db_cfg;
extern uint32_t db_vm_ctrl_ip_min;
extern uint32_t db_vm_ctrl_ip_max;
extern uint32_t db_vm_ctrl_ip_mask;
extern int *DB_LOG_LEVEL_P;
extern char* DBLOGLEVEL[8];
#define LC_DB_LOG_LEVEL_SHM_ID      "/tmp/.db_shm_sys_level_key" /*added by kzs for system log level global variable*/

#define LC_DB_LOG(level, format, ...)  \
 if (level <= *DB_LOG_LEVEL_P) { \
        syslog(level, "[tid=%lu] %s %s: " format, \
                pthread_self(), DBLOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
}

int lc_db_init(char *host, char *user, char *passwd,
               char *dbname, char *dbport, char *version);
int lc_db_thread_init(void);
int lc_db_thread_term(void);
int handle_lookup(pthread_t tid);
typedef int (*db_process)(void *db_info, char *field, char *value);
typedef int (*sql_process)(MYSQL_RES *result, void *traffic);

int lc_db_exec_sql(char *sql_query, sql_process fp, void *traffic);
int lc_db_table_insert(char *table_name, char *column_list, char *value_list);
int lc_db_table_delete(char *table_name, char *condition);
int lc_db_table_update(char *table_name, char *column, char *value, char *condition);
int lc_db_table_multi_update(char *table_name, char *columns_update, char *condition);
int lc_db_table_dump(char *table_name, char *column_list);
int lc_db_table_count(char *table_name, char *column, char *condition, int *count);
int lc_db_table_load(void *info, char *table_name, char *column_list,
        char *condition, db_process fp);
int lc_db_table_inner_join_load(void *info, char *column_list,
        char *table1, char *table2, char *join_condition,
        char *where_condition, db_process fp);
int lc_db_table_load_all(void *info, char *table_name, char *column_list,
        char *condition, int offset, int *data_cnt, db_process fp);
int lc_db_table_loadn_all(void *info, char *table_name, char *column_list,
        char *condition, int offset, int *data_cnt, int max_cnt, db_process fp);
int lc_db_table_loadn(void *info, int info_size, int info_len,
        char *table_name, char *column_list, char *condition, db_process fp);
int lc_db_table_loadn_order_by_id(void *info, int info_size, int info_len,
                                  char *table_name, char *column_list,
                                  char *condition, db_process fp);
int lc_db_table_load_by_server(void *info, char *table_name, char *column_list,
        char *condition, int offset, int *vm_num, int max, db_process fp);
int lc_db_table_load_inner(void *info, char *column_list, char *table1, char *table2,
               char *join_on, char *condition, int offset, int *data_cnt, int max, db_process fp);
int atomic_lc_db_get_available_vlantag(
        uint32_t *p_vlantag, uint32_t rackid, int min, int max);
int lc_db_get_available_vmware_console_port();
int lc_db_get_all_ips_of_if(int vdc_id, int if_index, int *isp_id, char *ips, int ips_size);

int lc_db_ping_mysql(MYSQL *conn_handle);

int handle_master_lookup(pthread_t tid);
int lc_db_master_init(char *host, char *user, char *passwd,
        char *dbname, char *dbport, char *version);
int lc_db_master_thread_init(void);
int lc_db_master_thread_term(void);
int lc_db_master_term(void);
int lc_db_master_table_insert(char *table_name, char *column_list,
        char *value_list);
int lc_db_master_table_update(char *table_name, char *column, char *value,
        char *condition);
int lc_db_master_table_multi_update(char *table_name, char *columns_update, char *condition);
#endif /*_LC_DB_H */
