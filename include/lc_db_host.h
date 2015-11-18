#ifndef _LC_DB_HOST_H
#define _LC_DB_HOST_H

#include "vm/vm_host.h"

int lc_vmdb_host_device_load (host_t *host, char *ip, char *columns);
int lc_vmdb_host_device_load_by_id(host_t *host, int id, char *columns);
int lc_vmdb_host_device_load_all (host_t *host, int offset, int *host_num, int poolid);
int lc_vmdb_host_device_load_all_v2 (host_t *host, int offset, int *host_num, int poolid);
int lc_db_compute_resource_multi_update(char *columns, char *condition);
int lc_vmdb_compute_resource_load (host_t *host, char *ip);
int lc_vmdb_compute_resource_load_by_col (host_t *host, char *ip, char *columns);
int lc_vmdb_network_resource_load(host_t *host, char *ip);
int lc_vmdb_compute_resource_load_all (host_t *host, int offset, int *host_num);
int lc_vmdb_compute_resource_load_all_v2 (host_t *host, int offset, int *host_num);
int lc_vmdb_compute_resource_load_all_v2_by_poolid (host_t *host, int offset,
        int *host_num, int poolid);
int lc_vmdb_compute_resource_load_by_id(host_t *host, int id);
int lc_vmdb_check_host_device_exist (char *ip);

int lc_vmdb_host_device_info_update(host_t *host, char *ip);
int lc_vmdb_host_device_iftype_update(host_t *host, char *ip);
int lc_vmdb_host_device_state_update(int state, char *ip);
int lc_vmdb_host_device_master_update(char *master_ip, char *ip);
int lc_vmdb_host_device_vmware_console_port_update(int cport, char *ip);
int lc_vmdb_host_device_errno_update(int err, char *ip);
int lc_vmdb_host_port_config_insert(char *host_ip,char *eth_name,char *eth_mac);
int lc_vmdb_compute_resource_uuid_update(char *uuid, char *ip);
int lc_vmdb_compute_resource_state_update(int state, char *ip);
int lc_vmdb_compute_resource_disk_update(int disk, char *ip);
int lc_vmdb_compute_resource_memory_update(int memory, char *ip);
int lc_vmdb_compute_resource_errno_update(int err, char *ip);
int lc_vmdb_compute_resource_flag_update(int flag, char *ip);
int lc_vmdb_compute_resource_usage_update(char *cpu_usage, int cpu_used,
                                          int mem_usage, int disk_usage, char *ip);
int lc_vmdb_compute_resource_master_ip_update(char *master_ip, char *ip);

int lc_vmdb_load_compute_resource_state(int *value, char *ip);
int lc_vmdb_vcdc_load_by_rack_id(vcdc_t *vcdc, int rack_id);

int lc_db_master_compute_resource_update(char *column, char *value, char *condition);
int lc_vmdb_compute_resource_load_by_domain (host_t *host, int offset, int *host_num, char *domain);
int lc_vmdb_master_compute_resource_update_health_check(int healthcheck, char *ip);
int lc_vmdb_master_compute_resource_state_multi_update(int state, int error_number, int healthcheck, char *ip);
int lc_vmdb_master_host_device_state_update(int state, char *ip);
int lc_vmdb_master_compute_resource_time_update(time_t cpu_start_time,
                                                time_t disk_start_time,
                                                time_t check_start_time, char *ip);
int lc_vmdb_master_compute_resource_stats_time_update(time_t stats_time, char *ip);
int lc_vmdb_master_compute_resource_state_update(int state, char *ip);
#endif
