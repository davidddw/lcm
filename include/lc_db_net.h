#ifndef _LC_DB_NET_H
#define _LC_DB_NET_H

#include "lc_netcomp.h"
#include "lc_kernel.h"

int lc_vmdb_network_resource_load(network_resource_t *nr_info, uint32_t id);
int lc_vmdb_network_resource_load_by_ip(network_resource_t *nr_info, char *ip);
int lc_vmdb_network_resource_load_all(network_resource_t *nr_info, int offset, int *nr_num);

int lc_db_get_sys_running_mode();
int lc_vmdb_vnet_insert(vnet_t *vnet);
int lc_vmdb_vnet_delete(uint32_t vnet_id);
int lc_vmdb_vm_load(vm_t *vm_info, uint32_t id);
int lc_vmdb_vm_load_by_vl2id(vm_t *vm_info, uint32_t vl2_id);
int lc_vmdb_vm_load_by_label(vm_t *vm_info, char *label);
int lc_vmdb_vnet_load(vnet_t *vnet_info, uint32_t vnet_id);
int lc_vmdb_vnet_update_name(char *name, uint32_t vnet_id);
int lc_vmdb_vnet_update_vl2num(uint32_t vl2_num, uint32_t vnet_id);
int lc_vmdb_vl2_insert(vl2_t *vl2, uint32_t vnet_id);
int lc_vmdb_vl2_delete(uint32_t vl2_id);
int lc_vmdb_vl2_load(vl2_t *vl2_info, uint32_t vl2_id);
int lc_vmdb_vl2_update_name(char *vl2_name, uint32_t vnet_id, uint32_t vl2_id);
int lc_vmdb_vl2_update_portnum(int port_num, uint32_t vnet_id, uint32_t vl2_id);
int lc_vmdb_vl2_vlan_insert(uint32_t vl2id, uint32_t rack, uint32_t vlantag);
int lc_vmdb_vl2_vlan_load(uint32_t *p_vlantag, uint32_t vl2id, uint32_t rackid);
int lc_vmdb_vl2_vlan_check_occupy(uint32_t vl2id, uint32_t rackid, uint32_t vlantag);
int lc_vmdb_vl2_vlan_delete_one(uint32_t vl2id, uint32_t rackid);
int lc_vmdb_vl2_vlan_delete_all(uint32_t vl2id);
int lc_vmdb_vl2_update_state(uint32_t state, uint32_t vl2_id);
int lc_vmdb_vl2_update_errno(uint32_t err, uint32_t vl2_id);
int lc_vmdb_vl2_update_net_type(int net_type, uint32_t vl2_id);
int lc_vmdb_vl2_update_pgid(char *pg_id, uint32_t vl2_id);
int lc_vmdb_network_resource_update_state(uint32_t state, uint32_t id);
int lc_vmdb_network_resource_update_errno(uint32_t err, uint32_t id);
int lc_vmdb_vif_load(intf_t *intf_info, uint32_t vif_id);
int lc_vmdb_vif_load_all(intf_t *intf_info, char *column_list, char *condition, int *vif_num);
int lc_vmdb_vif_load_by_vl2_and_device_id(intf_t *intf_info, uint32_t vl2, uint32_t deviceid);
int lc_vmdb_vif_update_state(int state, uint32_t id);
int lc_vmdb_vif_update_port_id(char *port_id, uint32_t id);
int lc_vmdb_vif_update_vlantag(uint32_t vlantag, uint32_t ifid);
int lc_vmdb_vif_update_vlantag_by_vl2id(uint32_t vlantag, uint32_t vl2id);
int lc_vmdb_vif_update_pg_id(char *pg_id, uint32_t id);
int lc_vmdb_host_port_config_load_by_id(host_port_config_t *host_port_config, int id);

int lc_vmdb_get_vm_name_label(int id, char *label);
int lc_vmdb_get_vedge_name_label(int id, char *label);
int lc_vmdb_get_pool_htype(uint32_t poolid);

int lc_vmdb_host_load(host_t *host, char *ip);
int lc_vmdb_host_load_rackid(uint32_t *p_rackid, char *ip);
int lc_vmdb_host_load_hostid(int *p_hostid, char *ip);
int lc_vmdb_compute_resource_load(host_t *host, char *ip);
int lc_vmdb_vlan_range_load_by_rack(vlan_t *vlan, int rack);
int lc_db_alloc_vlantag(uint32_t *p_vlantag,
        uint32_t vl2id, uint32_t rackid, uint32_t vlan_request, uint32_t force);
int lc_db_free_vlantag(uint32_t vl2id, uint32_t rackid);
int lc_db_free_all_vlantag_of_vl2(uint32_t vl2id);
int lc_db_vm_load(vm_t* vm_info, char *column_list, char *condition);
int lc_db_vm_update(char *column, char *value, char *condition);
int lc_db_vif_load(intf_t* intf_info, char *column_list, char *condition);

int lc_vmdb_vnet_load_all_v2_by_server_for_migrate(
        vnet_t *vnet, int offset, int *vnet_num, char *server_addr);
int lc_db_compute_resource_load_all_v2(host_t *host_info, char *column_list,
        char *condition, int offset, int *host_num);
int lc_vmdb_compute_resource_load_all_v2_by_state(host_t *host, int offset,
        int *host_num, int poolid, int state);
int lc_vmdb_host_device_load_all_v2_by_state(host_t *host, int offset,
        int *host_num, int poolid, int state);
int lc_vmdb_vnet_get_num_by_server(int *vnet_num, char *server_addr);
int lc_vmdb_vnet_update_state(int state, uint32_t vnet_id);
int lc_vmdb_vnet_update_server(char *server_addr, uint32_t vnet_id);

int lc_vmdb_compute_resource_state_update(int state, char *ip);

int lc_vmdb_ipmi_load_by_type(ipmi_t *ipmi, int id, int type);
int lc_vmdb_nsp_host_load(nsp_host_t *nsp_host, char *ip);
int lc_db_vif_ip_loadn(intf_ip_t* intf_ip, int len, char *condition);

#endif
