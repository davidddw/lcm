/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_worker.c
 * Date: 2012-11-26
 *
 * Description: get info from db table
 *
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xen/api/xen_all.h>

#include "vm/vm_host.h"
#include "lc_db.h"
#include "lc_queue.h"
#include "lc_db_vm.h"
#include "lc_db_host.h"
#include "lc_db_pool.h"
#include "lc_xen_monitor.h"
#include "lc_mon.h"

static int system_config_process(void *cfg_info, char *field, char *value)
{
    monitor_config_t *pcfg = (monitor_config_t *)cfg_info;
    if (strcmp(field, "id") == 0) {
        pcfg->id = atoi(value);
    } else if (strcmp(field, "value") == 0) {
        switch (pcfg->id) {
        case MONITOR_HOST_CPU_DURATION:
             pcfg->host_cpu_threshold = atoi(value);
             break;
        case MONITOR_HOST_CPU_WARNING_USAGE:
             pcfg->host_cpu_warning_usage = atoi(value);
             break;
        case MONITOR_VM_CPU_DURATION:
             pcfg->vm_cpu_threshold = atoi(value);
             break;
        case MONITOR_VM_CPU_WARNING_USAGE:
             pcfg->vm_cpu_warning_usage = atoi(value);
             break;
        case MONITOR_HOST_WARNING_INTERVAL:
             pcfg->host_warning_interval = atoi(value);
             break;
        case MONITOR_VM_WARNING_INTERVAL:
             pcfg->vm_warning_interval = atoi(value);
             break;
        case MONITOR_EMAIL_ADDRESS:
             strcpy(pcfg->email_address, value);
             break;
        case MONITOR_HOST_NETWORK_DURATION:
             pcfg->host_network_threshold = atoi(value);
             break;
        case MONITOR_HOST_NETWORK_WARNING_USAGE:
             pcfg->host_network_warning_usage = atoi(value);
             break;
        case MONITOR_VM_NETWORK_DURATION:
             pcfg->vm_network_threshold = atoi(value);
             break;
        case MONITOR_VM_NETWORK_WARNING_USAGE:
             pcfg->vm_network_warning_usage = atoi(value);
             break;
        case MONITOR_VM_DISK_DURATION:
             pcfg->vm_disk_threshold = atoi(value);
             break;
        case MONITOR_VM_DISK_WARNING_USAGE:
             pcfg->vm_disk_warning_usage = atoi(value);
             break;
        case MONITOR_HOST_MEM_DURATION:
             pcfg->host_mem_threshold = atoi(value);
             break;
        case MONITOR_HOST_MEM_WARNING_USAGE:
             pcfg->host_mem_warning_usage = atoi(value);
             break;
        case MONITOR_VM_MEM_DURATION:
             pcfg->vm_mem_threshold = atoi(value);
             break;
        case MONITOR_VM_MEM_WARNING_USAGE:
             pcfg->vm_mem_warning_usage = atoi(value);
             break;
        case MONITOR_HOST_DISK_DURATION:
             pcfg->host_disk_threshold = atoi(value);
             break;
        case MONITOR_HOST_DISK_WARNING_USAGE:
             pcfg->host_disk_warning_usage = atoi(value);
             break;
        }
    }
    return 0;
}

static int email_address_process(void *cfg_info, char *field, char *value)
{
    monitor_config_t *pcfg = (monitor_config_t *)cfg_info;
    if (strcmp(field, "email") == 0) {
        strcpy(pcfg->email_address, value);
    }else if (strcmp(field, "ccs") == 0) {
        strcpy(pcfg->email_address_ccs, value);
    }
    return 0;
}

#if 0
static int update_item_to_config(int id, char *field, time_t value)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "%ld", value);
    if (lc_db_table_update("sys_configuration", field, buffer, condition) != 0) {
        LCMON_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}
#endif

int lc_mon_msg_handler(intptr_t data_ptr)
{
    lc_monitor_check_bottom_half(data_ptr);
    return 0;
}

int lc_get_vm_from_db_by_uuid(vm_info_t *vm_info, char *vm_uuid)
{
    int code;

    code = lc_vmdb_vm_load_by_uuid(vm_info, vm_uuid);

    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s uuid %s not found\n", __FUNCTION__, vm_uuid);
    }

    return code;
}

int lc_get_vm_from_db_by_import_namelable(vm_info_t *vm_info, char *import_name_lable)
{
    int code;

    code = lc_vmdb_vm_load_by_import_namelable(vm_info, import_name_lable);

    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s vm%s not imported\n", __FUNCTION__, import_name_lable);
    }

    return code;
}

int lc_get_vm_from_db_by_namelable(vm_info_t *vm_info, char *name_lable)
{
    int code;

    code = lc_vmdb_vm_load_by_namelable(vm_info, name_lable);

    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s vm%s not found\n", __FUNCTION__, name_lable);
    }

    return code;
}

int lc_get_vedge_from_db_by_namelable(vm_info_t *vm_info, char *name_lable)
{
    int code;

    code = lc_vmdb_vedge_load_by_namelable(vm_info, name_lable);

    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s vm%s not found\n", __FUNCTION__, name_lable);
    }

    return code;
}

int lc_get_listener_from_db_by_name(lb_listener_t *listener,
                                    char *name, char *lb_lcuuid)
{
    int code;

    code = lc_vmdb_lb_listener_load_by_name(listener, name, lb_lcuuid);

    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s lb %s listener %s not found\n",
               __FUNCTION__, lb_lcuuid, name);
    }

    return code;
}

int lc_get_all_compute_resource (host_t *host_info, int offset, int *host_num)
{
    int code;

    code = lc_vmdb_compute_resource_load_by_domain(host_info, offset, host_num, lcmon_domain.lcuuid);

    if (code == LC_DB_HOST_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s host not found\n", __FUNCTION__);
    }

    return code;
}

int lc_get_compute_resource (host_t *host_info, char *ip)
{
    int code;

    if (!ip) {
        return LC_DB_COMMON_ERROR;
    }

    code = lc_vmdb_compute_resource_load(host_info, ip);

    if (code == LC_DB_HOST_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s host %s not found\n", __FUNCTION__, ip);
    }

    return code;
}

int lc_get_pool_by_index (rpool_t *pool_info, uint32_t pool_index)
{
    int code;

    if (pool_index <= 0) {
        return -1;
    }

    code = lc_vmdb_pool_load(pool_info, pool_index);

    if (code == LC_DB_HOST_NOT_FOUND) {
        LCMON_LOG(LOG_ERR, "%s pool %d not found\n", __FUNCTION__, pool_index);
    }

    return code;
}

int lc_get_config_info_from_system_config(monitor_config_t *cfg_info)
{
    char condition[LC_DB_BUF_LEN];
    int result;
    int id = 0;
    for (id = LC_MONITOR_CONFIG_START_ID; id <= LC_MONITOR_CONFIG_END_ID; id ++) {

         memset(condition, 0, LC_DB_BUF_LEN);
         sprintf(condition, "id=%d", id);
         result = lc_db_table_load((void *)cfg_info, "sys_configuration", "*", condition, system_config_process);
         if (result == LC_DB_COMMON_ERROR) {
             LCMON_LOG(LOG_ERR, "%s: error=%d", __FUNCTION__, result);
             return result;
         } else if (result == LC_DB_EMPTY_SET) {
             LCMON_LOG(LOG_ERR, "%s: system config table is NULL", __FUNCTION__);
             return -1;
         }
    }
    id = 1;
    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    memset(cfg_info->email_address, 0, MAIL_ADDR_LEN);
    result = lc_db_table_load((void *)cfg_info, "fdb_user", "email", condition, email_address_process);
    if (result == LC_DB_COMMON_ERROR) {
        LCMON_LOG(LOG_ERR, "%s: get email address error", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LCMON_LOG(LOG_ERR, "%s: system config table is NULL", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_update_info_to_system_config(monitor_config_t *cfg_info)
{
#if 0
    update_item_to_config(LC_MONITOR_CONFIG_UPDATE_START_ID, "value", cfg_info->host_warning_time);
    update_item_to_config(LC_MONITOR_CONFIG_UPDATE_START_ID + 1, "value", cfg_info->vm_warning_time);
    update_item_to_config(LC_MONITOR_CONFIG_UPDATE_START_ID + 2, "value", cfg_info->host_cpu_start_time);
    update_item_to_config(LC_MONITOR_CONFIG_UPDATE_START_ID + 3, "value", cfg_info->vm_cpu_start_time);
    update_item_to_config(LC_MONITOR_CONFIG_UPDATE_START_ID + 4, "value", cfg_info->vm_disk_start_time);
#endif
    return 0;
}
