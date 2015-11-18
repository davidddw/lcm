#ifndef _LC_DB_STORAGE_H
#define _LC_DB_STORAGE_H

#include "message/msg_lcm2dri.h"
#include "vm/vm_host.h"

int lc_storagedb_snapshot_insert (struct msg_request_vm_snapshot* vm, char* sname, char* serverip);
int lc_storagedb_backup_insert (struct msg_request_vm_backup* vm, char* backup_name, char* serverip, char* path);

int lc_vmdb_storage_resource_load(storage_resource_t *sr, char *ip);
int lc_vmdb_storage_resource_delete(char *sr_uuid);
int lc_vmdb_storage_resource_state_update(int state, int id);
int lc_vmdb_storage_resource_errno_update(int err, int id);
int lc_vmdb_storage_resource_sr_name_update(char *sr_name, int id);
int lc_vmdb_storage_resource_sr_uuid_update(char *sr_uuid, int id);
int lc_vmdb_storage_resource_sr_backup_name_update(char *sr_name, int id);
int lc_vmdb_storage_resource_sr_backup_uuid_update(char *sr_uuid, int id);
int lc_vmdb_storage_resource_load_by_id(storage_resource_t *sr, int id);
int lc_vmdb_storage_resource_load_by_ip(storage_resource_t *sr, char *ip);
int lc_vmdb_shared_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *sr_name);

int lc_storagedb_shared_storage_resource_check(char *sr_name, int rack_id);
int lc_storagedb_shared_storage_resource_load(shared_storage_resource_t *ssr, char *sr_name, int rack_id);
int lc_storagedb_shared_storage_resource_insert(shared_storage_resource_t *ssr);

int lc_storagedb_host_device_add_storage(host_t *host, storage_info_t *storage);
int lc_storagedb_host_device_del_storage(host_t *host);
int lc_storagedb_host_device_get_storage_uuid(char *uuid, uint32_t len,
        char *address, char *sr_name);
int lc_hastoragedb_host_device_get_storage_uuid(char *uuid, uint32_t len,
        char *address, char *sr_name);
int lc_get_template_info(template_os_info_t *template_info, char *template_os);
int lc_get_remote_template_info(template_os_info_t *template_info, char *template_os);
int lc_storagedb_host_device_add_ha_storage(host_t *host, storage_info_t *storage);
int lc_storagedb_host_device_del_ha_storage(host_t *host);
int lc_storagedb_master_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *uuid);
int lc_storagedb_master_sr_usage_update_by_name(int disk_total, int disk_usage, int disk_allocated, char *name, int rackid);
int lc_storagedb_master_ha_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *uuid);
int lc_storagedb_master_ha_sr_usage_update_by_name(int disk_total, int disk_usage, int disk_allocated, char *name, int rackid);
int lc_update_master_template_os_state_by_ip(int state, char *ip);
int lc_update_master_template_os_state(int state, char *template_os);

#endif
