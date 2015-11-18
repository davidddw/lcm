/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-04
 */
#ifndef _LC_RM_DB_H
#define _LC_RM_DB_H

#include "lc_netcomp.h"

#define LC_RM_ACTION_COL_STR            "actiontype, objecttype, objectid, "\
                                        "vcpu_num, mem_size, sys_disk_size, "\
                                        "usr_disk_size, ha_disk_size, "\
                                        "alloctype"

#define TABLE_VM                        "vm"
#define TABLE_VM_ID                     "id"
#define TABLE_VM_STATE                  "state"
#define TABLE_VM_STATE_TEMP             0
#define TABLE_VM_ERRNO                  "errno"
#define TABLE_VM_ERRNO_NULL             0x0
#define TABLE_VM_FLAG_THIRD_HW          0x4000

#define TABLE_VGW                       "vnet"
#define TABLE_VGW_ID                    "id"
#define TABLE_VGW_STATE                 "state"
#define TABLE_VGW_STATE_RUN             7
#define TABLE_VGW_ERRNO                 "errno"
#define TABLE_VGW_ERRNO_NULL            0x0
#define TABLE_VGW_FLAG_THIRD_HW         0x2000
#define TABLE_VGW_CPU_NUM               2
#define TABLE_VGW_MEM_SIZE              1024
#define TABLE_VGW_DISK_SIZE             30
#define TABLE_VGW_GW_LAUNCH_SERVER     "gw_launch_server"
#define TABLE_VGW_GW_POOLID            "gw_poolid"

#define TABLE_ACTION                    "action"
#define TABLE_ACTION_ID                 "id"
#define TABLE_ACTION_OBJECTID           "objectid"
#define TABLE_ACTION_OBJECTTYPE         "objecttype"
#define TABLE_ACTION_OBJECTTYPE_VM      "vm"
#define TABLE_ACTION_OBJECTTYPE_VGW     "vgw"
#define TABLE_ACTION_TYPE               "actiontype"
#define TABLE_ACTION_TYPE_CREATE        "create"
#define TABLE_ACTION_TYPE_MODIFY        "modify"
#define TABLE_ACTION_TYPE_REPLACE       "replace"
#define TABLE_ACTION_TYPE_SNAPSHOT      "snapshot"
#define TABLE_ACTION_VCPU_NUM           "vcpu_num"
#define TABLE_ACTION_MEM_SIZE           "mem_size"
#define TABLE_ACTION_SYS_DISK_SIZE      "sys_disk_size"
#define TABLE_ACTION_USR_DISK_SIZE      "usr_disk_size"
#define TABLE_ACTION_HA_DISK_SIZE       "ha_disk_size"
#define TABLE_ACTION_ALLOCTYPE          "alloctype"

#define TABLE_CR                        "compute_resource"
#define TABLE_CR_IP                     "ip"
#define TABLE_CR_TYPE                   "type"
#define TABLE_CR_TYPE_SERVER            1
#define TABLE_CR_TYPE_GATEWAY           2
#define TABLE_CR_TYPE_NSP               3
#define TABLE_CR_STATE                  "state"
#define TABLE_CR_STATE_COMPLETE         4

#define TABLE_CR_CPU_ALLOC              "cpu_alloc"
#define TABLE_CR_MEM_ALLOC              "mem_alloc"

#define TABLE_SR_CON                    "storage_connection"
#define TABLE_SR_CON_SRID               "storage_id"
#define TABLE_SR_CON_HOST               "host_address"
#define TABLE_SR_CON_ACTIVE             "active"
#define TABLE_SR_CON_ACTIVE_Y           1
#define TABLE_SR_CON_ACTIVE_N           0

#define TABLE_SR                        "storage"
#define TABLE_SR_ID                     "id"
#define TABLE_SR_DISK_ALLOC             "disk_alloc"

#define TABLE_HA_SR_CON                 "ha_storage_connection"
#define TABLE_HA_SR_CON_SRID            "storage_id"
#define TABLE_HA_SR_CON_HOST            "host_address"
#define TABLE_HA_SR_CON_ACTIVE          "active"
#define TABLE_HA_SR_CON_ACTIVE_Y        1
#define TABLE_HA_SR_CON_ACTIVE_N        0

#define TABLE_HA_SR                     "ha_storage"
#define TABLE_HA_SR_ID                  "id"
#define TABLE_HA_SR_DISK_ALLOC          "disk_alloc"

#define TABLE_VDISK                     "vdisk"
#define TABLE_VDISK_ID                  "id"
#define TABLE_VDISK_VMID                "vmid"
#define TABLE_VDISK_SIZE                "size"
#define TABLE_VDISK_USRDEVICE           "userdevice"

typedef struct rm_vm_s {
    uint32_t      vm_id;
    char          vm_label[LC_NAMESIZE];
    uint32_t      vm_errno;
    uint32_t      vm_flag;
    uint32_t      vl2_id;
    uint32_t      vdc_id;
    uint32_t      vcpu_num;
    uint32_t      mem_size;
    uint32_t      vdi_sys_size;
    uint32_t      vdi_usr_size;
    uint32_t      vdi_vdisk_size;
    char          server[MAX_HOST_ADDRESS_LEN];
} rm_vm_t;

typedef struct rm_vgw_s {
    uint32_t      vgw_id;
    char          vgw_label[LC_NAMESIZE];
    uint32_t      vgw_errno;
    uint32_t      vgw_flag;
    uint32_t      vcpu_num;
    uint32_t      mem_size;
    uint32_t      vdi_sys_size;
    char          server[MAX_HOST_ADDRESS_LEN];
} rm_vgw_t;

typedef struct rm_snapshot_s {
    uint32_t      vm_id;
    uint32_t      snapshot_id;
} rm_snapshot_t;

typedef struct rm_action_s {
    uint32_t      id;
    char          action[LC_NAMESIZE];
    char          obtype[LC_NAMESIZE];
    uint32_t      obtid;
    uint32_t      vcpu_num;
    uint32_t      mem_size;
    uint32_t      sys_disk_size;
    uint32_t      usr_disk_size;
    uint32_t      ha_disk_size;
    uint32_t      alloc_type;
} rm_action_t;

typedef struct rm_pool_s {
    uint32_t      pool_id;
    uint32_t      pool_state;
    uint32_t      pool_type;
    uint32_t      pool_ctype;
    uint32_t      pool_stype;
} rm_pool_t;

typedef struct rm_cr_s {
    uint32_t      cr_id;
    uint32_t      cr_state;
    uint32_t      cr_type;
    uint32_t      pool_id;
    uint32_t      cpu_alloc;
    uint32_t      mem_alloc;
    char          cr_ip[16];
} rm_cr_t;


typedef struct rm_sr_con_s {
    uint32_t      sr_id;
} rm_sr_con_t;

typedef struct rm_sr_s {
    uint32_t      sr_id;
    uint32_t      disk_alloc;
} rm_sr_t;

typedef struct rm_vdisk_s {
    uint32_t      vdisk_id;
    uint32_t      vdisk_size;
    uint32_t      userdevice;
} rm_vdisk_t;

int lc_rmdb_action_insert(rm_action_t *action);
int lc_rmdb_vm_load(rm_vm_t* vm, char *column_list, char *condition);
int lc_rmdb_snapshot_load(rm_snapshot_t* snapshot, char *column_list, char *condition);
int lc_rmdb_vgw_load(rm_vgw_t* vm, char *column_list, char *condition);
int lc_rmdb_cr_get_num(int *cr_num, char *condition);
int lc_rmdb_cr_load_all(rm_cr_t* crinfo, char *column_list,
                        int offset, int *num, char *condition);
int lc_rmdb_vgw_get_num(int *vgw_num, char *condition);
int lc_rmdb_pool_load(rm_pool_t* pool, char *column_list, char *condition);
int lc_rmdb_cr_load(rm_cr_t* cr, char *column_list, char *condition);
int lc_rmdb_sr_load(rm_sr_t* sr, char *column_list, char *condition);
int lc_rmdb_sr_con_load(rm_sr_con_t* cr, char *column_list, char *condition);
int lc_rmdb_ha_sr_load(rm_sr_t* sr, char *column_list, char *condition);
int lc_rmdb_ha_sr_con_load(rm_sr_con_t* sr, char *column_list, char *condition);
int lc_rmdb_vdisk_load(rm_vdisk_t* vdisk, char *column_list, char *condition);
int lc_rmdb_action_load(rm_action_t* action, char *column_list, char *condition);
int lc_rmdb_vm_update(char *column, char *value, char *condition);
int lc_rmdb_vgw_update(char *column, char *value, char *condition);
int lc_rmdb_cr_update(char *column, char *value, char *condition);
int lc_rmdb_sr_update(char *column, char *value, char *condition);
int lc_rmdb_ha_sr_update(char *column, char *value, char *condition);
int lc_rmdb_vm_multi_update(char *columns_update, char *condition);
int lc_rmdb_vgw_multi_update(char *columns_update, char *condition);
int lc_rmdb_cr_multi_update(char *columns_update, char *condition);
int lc_rmdb_action_delete(char *condition);
#endif
