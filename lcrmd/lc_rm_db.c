/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-04
 */

#include <string.h>
#include <stdlib.h>
#include "mysql.h"
#include "lc_db.h"
#include "lc_rm_db.h"

extern MYSQL *conn_handle;

static int rm_vm_process(void *rm_vm, char *field, char *value)
{
    rm_vm_t *pvm = (rm_vm_t *)rm_vm;

    if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "errno") == 0) {
        pvm->vm_errno = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        pvm->vm_flag = atoi(value);
    } else if (strcmp(field, "vl2id") == 0) {
        pvm->vl2_id = atoi(value);
    } else if (strcmp(field, "vnetid") == 0) {
        pvm->vdc_id = atoi(value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        pvm->vcpu_num = atoi(value);
    } else if (strcmp(field, "mem_size") == 0) {
        pvm->mem_size = atoi(value);
    } else if (strcmp(field, "vdi_sys_size") == 0) {
        pvm->vdi_sys_size = atoi(value);
    } else if (strcmp(field, "vdi_user_size") == 0) {
        pvm->vdi_usr_size = atoi(value);
    } else if (strcmp(field, "launch_server") == 0) {
        strcpy(pvm->server, value);
    }

    return 0;
}

static int rm_vgw_process(void *rm_vgw, char *field, char *value)
{
    rm_vgw_t *pvgw = (rm_vgw_t *)rm_vgw;

    if (strcmp(field, "id") == 0) {
        pvgw->vgw_id = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvgw->vgw_label, value);
    } else if (strcmp(field, "errno") == 0) {
        pvgw->vgw_errno = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        pvgw->vgw_flag = atoi(value);
    } else if (strcmp(field, "gw_launch_server") == 0) {
        strcpy(pvgw->server, value);
    }
    pvgw->vcpu_num = TABLE_VGW_CPU_NUM;
    pvgw->mem_size = TABLE_VGW_MEM_SIZE;
    pvgw->vdi_sys_size = TABLE_VGW_DISK_SIZE;

    return 0;
}

static int rm_snapshot_process(void *rm_snapshot, char *field, char *value)
{
    rm_snapshot_t *psnapshot = (rm_snapshot_t *)rm_snapshot;

    if (strcmp(field, "id") == 0) {
        psnapshot->snapshot_id = atoi(value);
    } else if (strcmp(field, "vm_id") == 0) {
        psnapshot->vm_id = atoi(value);
    }

    return 0;
}

static int rm_pool_process(void *rm_pool, char *field, char *value)
{
    rm_pool_t *pool = (rm_pool_t *)rm_pool;

    if (strcmp(field, "id") == 0) {
        pool->pool_id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pool->pool_state = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        pool->pool_type = atoi(value);
    } else if (strcmp(field, "ctype") == 0) {
        pool->pool_ctype = atoi(value);
    } else if (strcmp(field, "stype") == 0) {
        pool->pool_stype = atoi(value);
    }

    return 0;
}
static int rm_cr_process(void *rm_cr, char *field, char *value)
{
    rm_cr_t *cr = (rm_cr_t *)rm_cr;

    if (strcmp(field, "id") == 0) {
        cr->cr_id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        cr->cr_state = atoi(value);
    } else if (strcmp(field, "poolid") == 0) {
        cr->pool_id = atoi(value);
    } else if (strcmp(field, "cpu_alloc") == 0) {
        cr->cpu_alloc = atoi(value);
    } else if (strcmp(field, "mem_alloc") == 0) {
        cr->mem_alloc = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        cr->cr_type = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(cr->cr_ip, value);
    }

    return 0;
}

static int rm_sr_process(void *rm_sr, char *field, char *value)
{
    rm_sr_t *sr = (rm_sr_t *)rm_sr;

    if (strcmp(field, "id") == 0) {
        sr->sr_id = atoi(value);
    } else if (strcmp(field, "disk_alloc") == 0) {
        sr->disk_alloc = atoi(value);
    }
    return 0;
}

static int rm_sr_con_process(void *rm_sr_con, char *field, char *value)
{
    rm_sr_con_t *sr_con = (rm_sr_con_t *)rm_sr_con;

    if (strcmp(field, "storage_id") == 0) {
        sr_con->sr_id = atoi(value);
    }
    return 0;
}

static int rm_vdisk_process(void *rm_vdisk, char *field, char *value)
{
    rm_vdisk_t *vdisk = (rm_vdisk_t *)rm_vdisk;

    if (strcmp(field, "id") == 0) {
        vdisk->vdisk_id = atoi(value);
    } else if (strcmp(field, "size") == 0) {
        vdisk->vdisk_size = atoi(value);
    } else if (strcmp(field, "userdevice") == 0) {
        vdisk->userdevice = atoi(value);
    }

    return 0;
}

static int rm_action_process(void *rm_action, char *field, char *value)
{
    rm_action_t *action = (rm_action_t *)rm_action;

    if (strcmp(field, "id") == 0) {
        action->id = atoi(value);
    } else if (strcmp(field, "objectid") == 0) {
        action->obtid = atoi(value);
    } else if (strcmp(field, "actiontype") == 0) {
        strncpy(action->action, value, LC_NAMESIZE - 1);
    } else if (strcmp(field, "objecttype") == 0) {
        strncpy(action->obtype, value, LC_NAMESIZE - 1);
    } else if (strcmp(field, "vcpu_num") == 0) {
        action->vcpu_num = atoi(value);
    } else if (strcmp(field, "mem_size") == 0) {
        action->mem_size = atoi(value);
    } else if (strcmp(field, "sys_disk_size") == 0) {
        action->sys_disk_size = atoi(value);
    } else if (strcmp(field, "usr_disk_size") == 0) {
        action->usr_disk_size = atoi(value);
    } else if (strcmp(field, "ha_disk_size") == 0) {
        action->ha_disk_size = atoi(value);
    }

    return 0;
}

int lc_rmdb_action_insert(rm_action_t *action)
{
    char values[LC_NAMEDES];

    memset(values, 0, LC_NAMEDES);
    snprintf(values, LC_NAMEDES, "\"%s\", \"%s\", %d, %d, %d, %d, %d, %d, %d",
             action->action, action->obtype, action->obtid, action->vcpu_num,
             action->mem_size, action->sys_disk_size, action->usr_disk_size,
             action->ha_disk_size, action->alloc_type);

    return lc_db_table_insert("action", LC_RM_ACTION_COL_STR, values);
}

int lc_rmdb_vm_load(rm_vm_t* vm, char *column_list, char *condition)
{
    return lc_db_table_load(vm, "vm", column_list, condition, rm_vm_process);
}

int lc_rmdb_vgw_load(rm_vgw_t* vgw, char *column_list, char *condition)
{
    return lc_db_table_load(vgw, "vnet", column_list, condition, rm_vgw_process);
}

int lc_rmdb_snapshot_load(rm_snapshot_t* snapshot, char *column_list, char *condition)
{
    return lc_db_table_load(snapshot, "vm_snapshot", column_list, condition, rm_snapshot_process);
}

int lc_rmdb_pool_load(rm_pool_t* pool, char *column_list, char *condition)
{
    return lc_db_table_load(pool, "pool", column_list, condition, rm_pool_process);
}

int lc_rmdb_cr_load(rm_cr_t* cr, char *column_list, char *condition)
{
    return lc_db_table_load(cr, "compute_resource", column_list, condition, rm_cr_process);
}

int lc_rmdb_sr_load(rm_sr_t* sr, char *column_list, char *condition)
{
    return lc_db_table_load(sr, "storage", column_list, condition, rm_sr_process);
}

int lc_rmdb_sr_con_load(rm_sr_con_t* sr, char *column_list, char *condition)
{
    return lc_db_table_load(sr, "storage_connection", column_list, condition, rm_sr_con_process);
}

int lc_rmdb_ha_sr_load(rm_sr_t* sr, char *column_list, char *condition)
{
    return lc_db_table_load(sr, "ha_storage", column_list, condition, rm_sr_process);
}

int lc_rmdb_ha_sr_con_load(rm_sr_con_t* sr, char *column_list, char *condition)
{
    return lc_db_table_load(sr, "ha_storage_connection", column_list, condition, rm_sr_con_process);
}

int lc_rmdb_vdisk_load(rm_vdisk_t* vdisk, char *column_list, char *condition)
{
    return lc_db_table_load(vdisk, "vdisk", column_list, condition, rm_vdisk_process);
}

int lc_rmdb_action_load(rm_action_t* action, char *column_list, char *condition)
{
    return lc_db_table_load(action, "action", column_list, condition, rm_action_process);
}

int lc_rmdb_vm_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vm", column, value, condition);
}

int lc_rmdb_vgw_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vnet", column, value, condition);
}

int lc_rmdb_cr_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("compute_resource", column, value, condition);
}

int lc_rmdb_sr_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("storage", column, value, condition);
}

int lc_rmdb_ha_sr_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("ha_storage", column, value, condition);
}

int lc_rmdb_vm_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vm", columns_update, condition);
}

int lc_rmdb_vgw_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vnet", columns_update, condition);
}

int lc_rmdb_cr_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("compute_resource", columns_update, condition);
}

int lc_rmdb_action_delete(char *condition)
{
    return lc_db_table_delete("action", condition);
}

int lc_rmdb_cr_get_num(int *cr_num, char *condition)
{
    return lc_db_table_count("compute_resource", "*", condition, cr_num);
}

int lc_rmdb_cr_load_all(rm_cr_t* crinfo, char *column_list,
                        int offset, int *num, char *condition)
{
    return lc_db_table_load_all(crinfo, "compute_resource",
                                column_list, condition,
                                offset, num, rm_cr_process);
}

int lc_rmdb_vgw_get_num(int *vgw_num, char *condition)
{
    return lc_db_table_count("vnet", "*", condition, vgw_num);
}

