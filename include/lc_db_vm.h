#ifndef _LC_DB_VM_H
#define _LC_DB_VM_H

#include "vm/vm_host.h"

int lc_vmdb_vm_update_ip (char *vm_ip, uint32_t vm_id);
int lc_vmdb_vm_update_name (char *vm_name, uint32_t vm_id);
int lc_vmdb_vm_update_label (char *vm_label, uint32_t vm_id);
int lc_vmdb_vm_update_uuid (char *uuid, uint32_t vm_id);
int lc_vmdb_vm_update_vdi_size (uint64_t size, char *size_field, uint32_t vm);
int lc_vmdb_vm_update_vdi_info (char *vdi_info, char *vdi_info_field, uint32_t vm);
int lc_vmdb_vm_update_disk_info(vm_info_t *vm_info, uint32_t vm_id);
int lc_vmdb_vm_update_cpu(int cpu, uint32_t vm_id);
int lc_vmdb_vm_update_memory(int memory, uint32_t vm_id);
int lc_vmdb_vm_update_flag (vm_info_t *vm_info, uint32_t vm_id);
int lc_vmdb_vm_update_flag_by_label(vm_info_t *vm_info, char *vm_label);
int lc_vmdb_vedge_update_time (vm_info_t *vm_info);
int lc_vmdb_vm_update_state (int state, uint32_t vm_id);
int lc_vmdb_vm_update_state_by_label(int state, char *vm_label);
int lc_vmdb_vm_update_action (int action, uint32_t vm_id);
int lc_vmdb_vnet_update_action (int action, uint32_t vm_id);
int lc_vmdb_vm_update_errno (int err_no, uint32_t vm_id);
int lc_vmdb_vm_update_errno_by_label(int err_no, char *vm_label);
int lc_vmdb_vm_update_tport (int tport, uint32_t vm_id);
int lc_vmdb_vm_update_lport (int lport, uint32_t vm_id);
int lc_vmdb_vm_update_opaque_id(char *opaque_id, uint32_t vm_id);
int lc_vmdb_vm_update_opaque_id_by_label(char *opaque_id, char *vm_label);
int lc_vmdb_vm_update_thumbprint_by_label(char *thumbprint, char *vm_label);
int lc_vmdb_vm_update_vm_path_by_label(char *vm_path, char *vm_label);
int lc_vmdb_vm_update_launchserver(char *launch_server, uint32_t vm_id);
int lc_vmdb_vm_update_sr_name(char *sr_name, uint32_t vm_id);
int lc_vmdb_vm_update_sys_sr_uuid(char *sr_name, uint32_t vm_id);
int lc_vmdb_vm_update_user_sr_uuid(char *sr_name, uint32_t vm_id);
int lc_vmdb_vm_load (vm_info_t *vm_info, uint32_t vm_id);
int lc_vmdb_vedge_load_by_import_namelable(vm_info_t *vm_info,char *importvm_lable);
int lc_vmdb_vm_load_by_import_namelable(vm_info_t *vm_info,char *importvm_lable);
int lc_vmdb_vm_load_by_namelable (vm_info_t *vm_info, char *vm_lable);
int lc_vmdb_vm_load_by_uuid (vm_info_t *vm_info, char *vm_uuid);
int lc_vmdb_vedge_load_by_namelable (vm_info_t *vm_info, char *vm_lable);
int lc_vmdb_vedge_load_by_ha(vm_info_t * vm_info, int vdc_id, int vnet_id);
int lc_vmdb_vnet_update_opaque_id(char *opaque_id, uint32_t vnet_id);
int lc_vmdb_vedge_load(vm_info_t *vm_info, uint32_t vnet_id);
int lc_vmdb_vedge_passwd_load(char *vm_passwd_info, uint32_t vnet_id);
int lc_vmdb_check_vm_exist (int vnet_id, int vl2_id, int vm_id);
int lc_vmdb_vnet_update_vedge_server (char *server_addr, uint32_t vnet_id);
int lc_vmdb_vnet_update_vedge_status (uint32_t status, uint32_t vnet_id);
int lc_vmdb_vif_load(intf_t *intf_info, uint32_t vif_id);
int lc_vmdb_vif_load_by_id(intf_t *intf_info, uint32_t vif_index, int vm_type, int vmid);
int lc_vmdb_vif_loadn_by_vm_id(intf_t *intf_info, int max_count, int vmid);
int lc_vmdb_vif_update_state(int state, uint32_t id);
int lc_vmdb_vif_update_ofport(int ofport, uint32_t id);
int lc_vmdb_vif_update_name(char *name, uint32_t id);
int lc_vmdb_vif_update_mac(char *mac, uint32_t id);
int lc_vmdb_vif_update_netmask(uint32_t netmask, uint32_t id);
int lc_vmdb_vif_update_port_id(char *port_id, uint32_t id);
int lc_vmdb_vif_update_pg_id(char *pg_id, uint32_t id);
int lc_vmdb_vif_update_ip(char *ip, uint32_t id);
int lc_vmdb_vedge_delete (uint32_t vnet_id);
int lc_vmdb_vedge_update_state (int state, uint32_t vnet_id);
int lc_vmdb_vedge_update_errno(int err_no, uint32_t vm_id);
int lc_vmdb_vedge_update_tport (int tport, uint32_t vnet_id);
int lc_vmdb_vedge_update_lport (int lport, uint32_t vnet_id);
int lc_vmdb_vedge_update_label(char *vm_name, uint32_t vnet_id);
int lc_vmdb_vedge_update_uuid (char *uuid, uint32_t vnet_id);
int lc_vmdb_vedge_update_flag(uint32_t vm_flags, uint32_t vnet_id);
int lc_vmdb_vedge_update_vrid (int vrid, uint32_t vnet_id);
int lc_db_get_available_tport();
int lc_db_get_available_vedge_tport();
int lc_vmdb_vif_update_vlantag(int vlan, uint32_t id);
int lc_vmdb_vm_snapshot_load(vm_snapshot_info_t *snapshot_info,
                             uint32_t            snapshot_id);
int lc_vmdb_vm_snapshot_update_state(int state, uint32_t snapshot_id);
int lc_vmdb_vm_snapshot_update_state_by_label(int state, char *snapshot_label);
int lc_vmdb_vm_snapshot_update_state_of_to_create_by_label(int state, char *snapshot_label);
int lc_vmdb_vm_snapshot_update_error(int error, uint32_t snapshot_id);
int lc_vmdb_vm_snapshot_update_error_by_label(int error, char *snapshot_label);
int lc_vmdb_vm_snapshot_update_error_of_to_create_by_label(int error, char *snapshot_label);
int lc_vmdb_vm_snapshot_update_label(char *label, uint32_t snapshot_id);
int lc_vmdb_vm_snapshot_update_uuid(char *uuid, uint32_t snapshot_id);
int lc_vmdb_vm_snapshot_load_by_vmid(vm_snapshot_info_t *snapshot_info,
                                     uint32_t            vm_id);
int lc_vmdb_vm_backup_load(vm_backup_info_t *backup_info, uint32_t backup_id);
int lc_vmdb_vm_backup_load_by_vmid(vm_backup_info_t *backup_info, uint32_t vm_id);
int lc_vmdb_vm_backup_update_state(int state, uint32_t backup_id);
int lc_vmdb_vm_backup_update_error(int error, uint32_t backup_id);
int lc_vmdb_vm_backup_update_flag(uint32_t flag, uint32_t backup_id);
int lc_vmdb_vm_backup_update_local_info(char *local_info, char *local_info_field,
                                        uint32_t backup_id);
int lc_vmdb_vm_backup_update_local_vdi_uuid(char *vdi_sys_uuid, char *vdi_user_uuid,
                                      uint32_t backup_id);
int lc_vmdb_vm_backup_update_peer_vdi_uuid(char *vdi_sys_uuid, char *vdi_user_uuid,
                                      uint32_t backup_id);
int lc_vmdb_vm_backup_delete(uint32_t backup_id);

int lc_get_vm_by_server(vm_query_info_t *vm_info, int offset, int *vm_num, char *server, int skip_tmp_state, int max);
int lc_get_vnet_by_server(vnet_query_info_t *vm_info, int offset, int *vnet_num, char *server, int skip_tmp_state, int max);
int lc_get_email_address_from_user_db(char *email, int user_id);

int lc_vmdb_vm_backup_disks_update(int sys_disk, int usr_disk, int id);

int lc_vmdb_vmware_session_ticket_update(char *ticket);
int lc_vmdb_disk_load(disk_t *disk_info, uint32_t vm_id, uint32_t userdevice);
int lc_vmdb_disk_load_by_id(disk_t *disk_info, uint32_t id);
int lc_vmdb_disk_load_all(disk_t *disk_info, int offset, int *cnt, uint32_t vm_id);
int lc_vmdb_disk_update_sr_uuid(char *sr_uuid, uint32_t disk_id);
int lc_vmdb_disk_update_uuid(char *uuid, uint32_t disk_id);
int lc_vmdb_disk_update_size(uint64_t size, uint32_t disk_id);
int lc_vmdb_disk_update_uuid_by_userdevice(char *uuid, uint32_t vm_id, uint32_t userdevice);
int lc_vmdb_disk_update_size_by_userdevice(uint64_t size, uint32_t vm_id, uint32_t userdevice);
int lc_vmdb_disk_update_name_by_userdevice(char *name, uint32_t vm_id, uint32_t userdevice);
int lc_vmdb_disk_delete(uint32_t disk_id);
int lc_vmdb_disk_delete_by_userdevice(uint32_t vm_id, uint32_t userdevice);
int lc_vmdb_disk_delete_by_vm(uint32_t vm_id);
int lc_vmdb_third_device_load(third_device_t *device, uint32_t vm_id, uint32_t flag);
int lc_db_get_vmwaf_by_launch_server(vmwaf_query_info_vm_t *vmwaf_query, char* launch_server);
int lc_db_lb_listener_load(lb_listener_t* listener_info, char *column_list, char *condition);
int lc_vmdb_lb_listener_load_by_name(lb_listener_t *listener_info,
                                     char *listener_lable,
                                     char *lb_lcuuid);
int lc_vmdb_lb_bk_vm_load_by_name(lb_bk_vm_t *bk_vm_info,
                                  char *lb_bk_vm_name,
                                  char *lb_listener_lcuuid,
                                  char *lb_lcuuid);
int lc_db_master_vmwaf_update_state_by_launch_server (int state, char* launch_server);
int lc_vmdb_master_vm_update_state (int state, uint32_t vm_id);
int lc_vmdb_master_vm_update_errno (int err_no, uint32_t vm_id);
int lc_vmdb_master_vedge_update_state (int state, uint32_t vnet_id);
int lc_vmdb_master_vedge_update_errno(int err_no, uint32_t vm_id);
int lc_vmdb_master_vm_update_action (int action, uint32_t vm_id);
int lc_vmdb_master_vnet_update_action (int action, uint32_t vm_id);
int lc_vmdb_master_vm_update_time (vm_info_t *vm_info);
int lc_vmdb_master_vedge_update_time (vm_info_t *vm_info);
int lc_update_master_ha_vnet_hastate(int hastate, char *label);
int lc_update_master_lb_bk_vm_health_state(int health_state, char *lcuuid);
int lc_get_ha_vnet_ctl_ip_by_domain(vgw_ha_query_info_t *vgw_query, int offset, int *ha_vnet_num, int max, char *domain);
int lc_get_lb_ctl_ip_by_domain(lb_query_info_t *lb_query, int offset, int *lb_num, int max, char *domain);
#endif
