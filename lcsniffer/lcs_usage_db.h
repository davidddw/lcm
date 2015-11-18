/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2013-05-08
 *
 */
#ifndef _LCS_USAGE_DB_H
#define _LCS_USAGE_DB_H


#include <inttypes.h>
#include "lc_agent_msg.h"
#include "lc_netcomp.h"
#include "lc_snf.h"

#define TRAFFIC_VINTF_BUF_SIZE    128
#define MAX_HOST_ADDRESS_LEN      20
#define LC_DOMAIN_LEN 64

typedef struct vmwaf_query_info_lcs_s {
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
} vmwaf_query_info_lcs_t;

typedef struct host_device_s {
    uint32_t   id;
    char       ip[MAX_HOST_ADDRESS_LEN];
    char       name[LC_NAMESIZE];
#define LC_POOL_HTYPE_XEN         1
#define LC_POOL_HTYPE_VMWARE      2
#define LC_POOL_HTYPE_KVM         3
    uint32_t   htype;
#define LC_SERVER_TYPE_VM         1
#define LC_SERVER_TYPE_VGW        2
#define LC_SERVER_TYPE_NSP        3
    uint32_t   type;
} host_device_t;

typedef struct lcg_device_s {
    uint32_t   id;
    char       ip[MAX_HOST_ADDRESS_LEN];
    char       name[LC_NAMESIZE];
    #define LC_POOL_HTYPE_XEN         1
    #define LC_POOL_HTYPE_VMWARE      2
    #define LC_POOL_HTYPE_KVM         3
    uint32_t   htype;
    #define LC_SERVER_TYPE_VM         1
    #define LC_SERVER_TYPE_VGW        2
    #define LC_SERVER_TYPE_NSP        3
    uint32_t   type;
    uint32_t   rackid;
} lcg_device_t;

typedef struct mong_vm_s {
    char        vm_label[MAX_VM_UUID_LEN];
    uint32_t    vm_id;
    uint32_t    vdc_id;
    int         user_id;
    int         htype;
    uint32_t    type;
    struct pub_if_s {
        uint32_t vif_id;
        int64_t tcp_conn_cps;
        int64_t udp_conn_cps;
    } pub_if[LC_VM_IFACES_MAX];
} mongo_vm_t;

typedef struct mongo_host_vm_s {
    host_device_t host;
    int64_t tcp_conn_cps;
    uint32_t      nvm;
    mongo_vm_t  vms[MAX_VM_PER_HOST];
} mongo_host_vm_t;

typedef struct {
    int cr_id;
    char cpu_usage[LC_HOST_CPU_USAGE];
    int64_t cpu_num;
    int64_t mem_total;
    int64_t mem_usage;
    struct {
        int64_t iftype;
        int64_t rx_bps;
        int64_t tx_bps;
        int64_t rx_pps;
        int64_t tx_pps;
    }traffic[LC_N_BR_ID];
} nsp_host_stat_t;

typedef struct {
    int vgw_id;
    char cpu_usage[LC_HOST_CPU_USAGE];
    int64_t mem_total;
    int64_t mem_usage;
    struct {
        int vif_id;
        int64_t traffic_rx_bps;
        int64_t traffic_rx_pps;
        int64_t traffic_tx_bps;
        int64_t traffic_tx_pps;
    } vif_info[AGENT_VM_MAX_VIF];
} nsp_vgw_stat_t;

typedef struct {
    int vif_id;
    int64_t rx_bytes;
    int64_t rx_packets;
    int64_t rx_bps;
    int64_t rx_pps;
    int64_t tx_bytes;
    int64_t tx_packets;
    int64_t tx_bps;
    int64_t tx_pps;
} nsp_traffic_stat_t;

typedef struct lb_listener_stat_s {
    char     name[LC_NAMESIZE];
    int64_t  conn_num;
    char     lb_label[LC_NAMESIZE];
    int      bk_vm_num;
    struct {
        char     name[LC_NAMESIZE];
        int64_t  conn_num;
    } bk_vm[MAX_BK_VM_PER_LISTENER];
} lb_listener_stat_t;

typedef struct network_device_s {
    uint32_t   id;
    char       mip[MAX_HOST_ADDRESS_LEN];
    char       name[LC_NAMESIZE];
} network_device_t;

int lcs_usage_db_host_loadn(host_device_t *host_info,
        uint32_t hostid_offset, int buf_size);
int lcs_usage_db_host_loadn_by_type(host_device_t *host_infos,
        uint32_t hostid_offset, int buf_size, int type);
int lcs_usage_db_switch_loadn(network_device_t *switch_infos,
        uint32_t switchid_offset, int buf_size);
int lcs_usage_db_lcg_loadn(lcg_device_t *lcg_infos,uint32_t lcg_offset, int buf_size);

int lcs_traffic_db_vif_load_by_hostip(agent_interface_id_t *vintf_info,
        int buf_size, char *hostip);
int lcs_traffic_db_vif_load_by_vm(agent_interface_id_t *vintf_info,
        int type, int vmid);

int lc_get_vm_by_server(vm_query_info_t *vm_info, int offset, int *vm_num, char *server, int skip_tmp_state, int max);
int lc_get_hwdev_by_server (hwdev_query_info_t *hwdev_query, int offset, int *hwdev_num, char *server, int max);
int lc_get_vnet_by_server(vnet_query_info_t *vm_info, int offset, int *vnet_num, char *server, int skip_tmp_state, int max);
int lc_mon_get_config_info(monitor_config_t *pcfg,
                              monitor_host_t *pmon_host, int is_host);
int lc_get_config_info_from_system_config(monitor_config_t *cfg_info);
int lc_vmdb_compute_resource_usage_update(char *cpu_usage, int cpu_used, int mem_usage,
                                          int disk_usage, int ha_disk_usage, char *ip);
int lc_vmdb_compute_resource_usage_update_v2(char *cpu_usage, int cpu_used,
        int mem_total, int mem_usage, int disk_usage, int ha_disk_usage, char *ip);
int lc_storagedb_shared_storage_resource_load(shared_storage_resource_t *ssr, char *sr_name, int rack_id);
int lc_db_shared_storage_resource_load(shared_storage_resource_t *sr_info, char *column_list, char *condition);

int lc_vmdb_compute_resource_load (host_t *host, char *ip);
int lc_get_vm_from_db_by_namelable(vm_info_t *vm_info, char *name_lable);
int lc_vmdb_vm_load_by_namelable(vm_info_t *vm_info,char *vm_lable);
int lc_get_vm_from_db_by_id(vm_info_t *vm_info, int id);
int lc_get_vedge_from_db_by_namelable(vm_info_t *vm_info, char *name_lable);
int lc_vmdb_vedge_load_by_namelable(vm_info_t * vm_info,char *vm_lable);
int lc_get_vedge_from_db_by_id(vm_info_t *vm_info, int id);
int lc_get_vedge_from_db_by_str(vm_info_t *vm_info, char *str);
int lc_get_email_address_from_user_db(char *email, int user_id);
int lc_db_vm_load(vm_info_t* vm_info, char *column_list, char *condition);
int lc_db_vedge_load(vm_info_t* vm_info, char *column_list, char *condition);
int get_email_address(void *email, char *field, char *value);
int vm_process(void *vm_info, char *field, char *value);
int vedge_process(void *vm_info, char *field, char *value);
int lc_vmdb_host_device_load (host_t *host, char *ip, char *columns);
int lc_vmdb_host_device_load_by_id(host_t *host, int id, char *columns);
int lc_vmdb_host_device_usage_load(host_device_t *host, char *ip, char *columns);
int lcs_db_vcdc_load_by_host_id(vcdc_t *vcdc, int host_id);
int lcs_db_get_vmwaf_by_launch_server(vmwaf_query_info_lcs_t *vmwaf_query, char* launch_server);
int lc_vmdb_vinterface_load_by_type_and_id(intf_t *intf_info,
        int devicetype, int deviceid, int ifindex);
int lc_vmdb_lb_load_by_namelable(vm_info_t *vm_info,char *lb_lable);
int lb_listener_process(void *listener_info, char *field, char *value);
int lc_db_lb_listener_load(lb_listener_t* listener_info, char *column_list, char *condition);
int lc_vmdb_lb_listener_load_by_name(lb_listener_t *listener_info,
                                     char *listener_lable,
                                     char *lb_lcuuid);
int lc_get_vifid_by_mac(int *vifid, char *mac);
int lc_vmdb_storage_connection_get_num_by_ip(int *conn_num, char *ip);
int lc_vmdb_storage_connection_load_all(storage_connection_t *storage_connection, char *ip, int *host_num);
int lc_vmdb_storage_load(db_storage_t* storage, int id);
int lcs_usage_db_domain_rackid_loadn(int *rackid_infos,
        uint32_t rackid_size, int rackid_len, char *domain);

#endif /* _LCS_TRAFFIC_DB_H */
