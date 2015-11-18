/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-06
 */

#include "lc_rm.h"
#include "lc_rm_db.h"

void lc_rm_save_vm_action(rm_vm_t *vm, char *actiontype, int alloctype)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    rm_cr_t     cr     = {0};
    rm_sr_con_t sr_con = {0};
    rm_sr_t     sr     = {0};
    rm_sr_t     hasr   = {0};
    rm_action_t action = {0};

    RM_LOG(LOG_DEBUG, "%s vm:%u", __FUNCTION__, vm->vm_id);

    /* Insert action */
    memset(&action, 0, sizeof(rm_action_t));
    strncpy(action.action, actiontype, LC_NAMESIZE - 1);
    strncpy(action.obtype, TABLE_ACTION_OBJECTTYPE_VM, LC_NAMESIZE - 1);
    action.obtid         = vm->vm_id;
    action.vcpu_num      = vm->vcpu_num;
    action.mem_size      = vm->mem_size;
    action.sys_disk_size = vm->vdi_sys_size;
    action.usr_disk_size = vm->vdi_usr_size;
    action.ha_disk_size  = vm->vdi_vdisk_size;
    action.alloc_type    = alloctype;
    if (lc_rmdb_action_insert(&action)) {
        RM_LOG(LOG_ERR, "Insert action vm:%u to DB failed", vm->vm_id);
    }

    /* Update storage resource */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
             TABLE_SR_CON_HOST, vm->server,
             TABLE_SR_CON_ACTIVE, TABLE_SR_CON_ACTIVE_Y);
    memset(&sr_con, 0, sizeof(rm_sr_con_t));
    if (lc_rmdb_sr_con_load(&sr_con, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s storage_connection in DB",
               vm->server);
    }

    memset(&sr, 0, sizeof(rm_sr_t));
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_SR_ID, sr_con.sr_id);
    if (lc_rmdb_sr_load(&sr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s active storage in DB",
               vm->server);
    } else {
        sr.disk_alloc += vm->vdi_sys_size;
        sr.disk_alloc += vm->vdi_usr_size;
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%d", sr.disk_alloc);
        lc_rmdb_sr_update(TABLE_SR_DISK_ALLOC, set, where);
    }

    /* Update compute resource */
    memset(&cr, 0, sizeof(rm_cr_t));
    snprintf(where, LC_NAMEDES, "%s='%s'", TABLE_CR_IP, vm->server);
    if (lc_rmdb_cr_load(&cr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s compute_resource in DB",
               vm->server);
    } else {
        cr.cpu_alloc += vm->vcpu_num;
        cr.mem_alloc += vm->mem_size;
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s=%d",
                 TABLE_CR_CPU_ALLOC, cr.cpu_alloc,
                 TABLE_CR_MEM_ALLOC, cr.mem_alloc);
        lc_rmdb_cr_multi_update(set, where);
    }

    /* Update ha storage resource */
    if (action.ha_disk_size) {
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
                 TABLE_HA_SR_CON_HOST, vm->server,
                 TABLE_HA_SR_CON_ACTIVE, TABLE_HA_SR_CON_ACTIVE_Y);
        memset(&sr_con, 0, sizeof(rm_sr_con_t));
        if (lc_rmdb_ha_sr_con_load(&sr_con, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find server: %s ha_storage_connection in DB",
                   vm->server);
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_HA_SR_ID, sr_con.sr_id);
        memset(&hasr, 0, sizeof(rm_sr_t));
        if (lc_rmdb_ha_sr_load(&sr, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find server: %s active ha_storage in DB",
                   vm->server);
        } else {
            hasr.disk_alloc += vm->vdi_vdisk_size;
            memset(set, 0, LC_NAMEDES);
            snprintf(set, LC_NAMEDES, "%d", hasr.disk_alloc);
            lc_rmdb_ha_sr_update(TABLE_HA_SR_DISK_ALLOC, set, where);
        }
    }
}

void lc_rm_save_vgateway_action(rm_vgw_t *vgw, char *actiontype, int alloctype)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    rm_cr_t     cr     = {0};
    rm_action_t action = {0};

    RM_LOG(LOG_DEBUG, "%s vgw:%u", __FUNCTION__, vgw->vgw_id);

    /* Insert action */
    memset(&action, 0, sizeof(rm_action_t));
    strncpy(action.action, actiontype, LC_NAMESIZE - 1);
    strncpy(action.obtype, TABLE_ACTION_OBJECTTYPE_VGW, LC_NAMESIZE - 1);
    action.obtid         = vgw->vgw_id;
    action.vcpu_num      = TABLE_VGW_CPU_NUM;
    action.mem_size      = TABLE_VGW_MEM_SIZE;
    action.sys_disk_size = TABLE_VGW_DISK_SIZE;
    action.alloc_type    = alloctype;
    if (lc_rmdb_action_insert(&action)) {
        RM_LOG(LOG_ERR, "Insert action vgw:%u to DB failed", vgw->vgw_id);
    }

    /* Update compute resource */
    memset(&cr, 0, sizeof(rm_cr_t));
    snprintf(where, LC_NAMEDES, "%s='%s'", TABLE_CR_IP, vgw->server);
    if (lc_rmdb_cr_load(&cr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s compute_resource in DB",
               vgw->server);
    } else {
        cr.cpu_alloc += TABLE_VGW_CPU_NUM;
        cr.mem_alloc += TABLE_VGW_MEM_SIZE;
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s=%d",
                 TABLE_CR_CPU_ALLOC, cr.cpu_alloc,
                 TABLE_CR_MEM_ALLOC, cr.mem_alloc);
        lc_rmdb_cr_multi_update(set, where);
    }
}

void lc_rm_save_vgw_action(rm_vgw_t *vgw, char *actiontype, int alloctype)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    rm_cr_t     cr     = {0};
    rm_sr_con_t sr_con = {0};
    rm_sr_t     sr     = {0};
    rm_action_t action = {0};

    RM_LOG(LOG_DEBUG, "%s vgw:%u", __FUNCTION__, vgw->vgw_id);

    /* Insert action */
    memset(&action, 0, sizeof(rm_action_t));
    strncpy(action.action, actiontype, LC_NAMESIZE - 1);
    strncpy(action.obtype, TABLE_ACTION_OBJECTTYPE_VGW, LC_NAMESIZE - 1);
    action.obtid         = vgw->vgw_id;
    action.vcpu_num      = TABLE_VGW_CPU_NUM;
    action.mem_size      = TABLE_VGW_MEM_SIZE;
    action.sys_disk_size = TABLE_VGW_DISK_SIZE;
    action.alloc_type    = alloctype;
    if (lc_rmdb_action_insert(&action)) {
        RM_LOG(LOG_ERR, "Insert action vgw:%u to DB failed", vgw->vgw_id);
    }

    /* Update storage resource */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
             TABLE_SR_CON_HOST, vgw->server,
             TABLE_SR_CON_ACTIVE, TABLE_SR_CON_ACTIVE_Y);
    memset(&sr_con, 0, sizeof(rm_sr_con_t));
    if (lc_rmdb_sr_con_load(&sr_con, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s storage_connection in DB",
               vgw->server);
    }

    memset(&sr, 0, sizeof(rm_sr_t));
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_SR_ID, sr_con.sr_id);
    if (lc_rmdb_sr_load(&sr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s active storage in DB",
               vgw->server);
    } else {
        sr.disk_alloc += TABLE_VGW_DISK_SIZE;
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%d", sr.disk_alloc);
        lc_rmdb_sr_update(TABLE_SR_DISK_ALLOC, set, where);
    }

    /* Update compute resource */
    memset(&cr, 0, sizeof(rm_cr_t));
    snprintf(where, LC_NAMEDES, "%s='%s'", TABLE_CR_IP, vgw->server);
    if (lc_rmdb_cr_load(&cr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s compute_resource in DB",
               vgw->server);
    } else {
        cr.cpu_alloc += TABLE_VGW_CPU_NUM;
        cr.mem_alloc += TABLE_VGW_MEM_SIZE;
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s=%d",
                 TABLE_CR_CPU_ALLOC, cr.cpu_alloc,
                 TABLE_CR_MEM_ALLOC, cr.mem_alloc);
        lc_rmdb_cr_multi_update(set, where);
    }
}

void lc_rm_del_vm_action(rm_vm_t *vm, rm_action_t *action)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    rm_cr_t     cr     = {0};
    rm_sr_con_t sr_con = {0};
    rm_sr_t     sr     = {0};
    rm_sr_t     hasr   = {0};

    RM_LOG(LOG_DEBUG, " %s vm:%d\n", __FUNCTION__, vm->vm_id);

    /* Update storage resource */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
             TABLE_SR_CON_HOST, vm->server,
             TABLE_SR_CON_ACTIVE, TABLE_SR_CON_ACTIVE_Y);
    memset(&sr_con, 0, sizeof(rm_sr_con_t));
    if (lc_rmdb_sr_con_load(&sr_con, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s storage_connection in DB",
               vm->server);
    }

    memset(&sr, 0, sizeof(rm_sr_t));
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_SR_ID, sr_con.sr_id);
    if (lc_rmdb_sr_load(&sr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s active storage in DB",
               vm->server);
    } else {

        if (0 <= (int)(sr.disk_alloc - action->sys_disk_size)) {
            sr.disk_alloc -= action->sys_disk_size;
        } else {
            sr.disk_alloc  = 0;
            RM_LOG(LOG_ERR, "disk info: used:%u, action:%u error",
                   sr.disk_alloc, action->sys_disk_size);
        }
        if (0 <= (int)(sr.disk_alloc - action->usr_disk_size)) {
            sr.disk_alloc -= action->usr_disk_size;
        } else {
            sr.disk_alloc  = 0;
            RM_LOG(LOG_ERR, "disk info: used:%u, action:%u error",
                   sr.disk_alloc, action->usr_disk_size);
        }
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%d", sr.disk_alloc);
        lc_rmdb_sr_update(TABLE_SR_DISK_ALLOC, set, where);
    }

    /* Update compute resource */
    memset(&cr, 0, sizeof(rm_cr_t));
    snprintf(where, LC_NAMEDES, "%s='%s'", TABLE_CR_IP, vm->server);
    if (lc_rmdb_cr_load(&cr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s compute_resource in DB",
               vm->server);
    } else {

        if (0 <= (int)(cr.cpu_alloc - action->vcpu_num)) {
            cr.cpu_alloc -= action->vcpu_num;
        } else {
            cr.cpu_alloc  = 0;
            RM_LOG(LOG_ERR, "cpu info: used:%u, action:%u error",
                   cr.cpu_alloc, action->vcpu_num);
        }
        if (0 <= (int)(cr.mem_alloc - action->mem_size)) {
            cr.mem_alloc -= action->mem_size;
        } else {
            cr.mem_alloc  = 0;
            RM_LOG(LOG_ERR, "mem info: used:%u, action:%u error",
                   cr.mem_alloc, action->mem_size);
        }
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s=%d",
                 TABLE_CR_CPU_ALLOC, cr.cpu_alloc,
                 TABLE_CR_MEM_ALLOC, cr.mem_alloc);
        lc_rmdb_cr_multi_update(set, where);
    }

    /* Update ha storage resource */
    if (action->ha_disk_size) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
                 TABLE_HA_SR_CON_HOST, vm->server,
                 TABLE_HA_SR_CON_ACTIVE, TABLE_HA_SR_CON_ACTIVE_Y);
        memset(&sr_con, 0, sizeof(rm_sr_con_t));
        if (lc_rmdb_ha_sr_con_load(&sr_con, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find server: %s ha_storage_connection in DB",
                   vm->server);
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_HA_SR_ID, sr_con.sr_id);
        memset(&hasr, 0, sizeof(rm_sr_t));
        if (lc_rmdb_ha_sr_load(&hasr, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find server: %s active ha_storage in DB",
                   vm->server);
        } else {
            if (0 <= (int)(hasr.disk_alloc - action->ha_disk_size)) {
                hasr.disk_alloc -= action->ha_disk_size;
            } else {
                hasr.disk_alloc  = 0;
                RM_LOG(LOG_ERR, "vdisk info: used:%u, action:%u error",
                       hasr.disk_alloc, action->ha_disk_size);
            }
            memset(set, 0, LC_NAMEDES);
            snprintf(set, LC_NAMEDES, "%d", hasr.disk_alloc);
            lc_rmdb_ha_sr_update(TABLE_HA_SR_DISK_ALLOC, set, where);
        }
    }

    /* delete action from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d",TABLE_ACTION_ID, action->id);
    lc_rmdb_action_delete(where);
}

void lc_rm_del_vgw_action(rm_vgw_t *vgw, rm_action_t *action)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    rm_cr_t     cr     = {0};
    rm_sr_con_t sr_con = {0};
    rm_sr_t     sr     = {0};

    RM_LOG(LOG_DEBUG, "%s vgw:%u", __FUNCTION__, vgw->vgw_id);

    /* Update storage resource */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
             TABLE_SR_CON_HOST, vgw->server,
             TABLE_SR_CON_ACTIVE, TABLE_SR_CON_ACTIVE_Y);
    memset(&sr_con, 0, sizeof(rm_sr_con_t));
    if (lc_rmdb_sr_con_load(&sr_con, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s storage_connection in DB",
               vgw->server);
    }

    memset(&sr, 0, sizeof(rm_sr_t));
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_SR_ID, sr_con.sr_id);
    if (lc_rmdb_sr_load(&sr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s active storage in DB",
               vgw->server);
    } else {
        if (0 <= (int)(sr.disk_alloc - action->sys_disk_size)) {
            sr.disk_alloc -= action->sys_disk_size;
        } else {
            sr.disk_alloc  = 0;
            RM_LOG(LOG_ERR, "disk info: used:%u, action:%u error",
                   sr.disk_alloc, action->sys_disk_size);
        }
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%d", sr.disk_alloc);
        lc_rmdb_sr_update(TABLE_SR_DISK_ALLOC, set, where);
    }

    /* Update compute resource */
    memset(&cr, 0, sizeof(rm_cr_t));
    snprintf(where, LC_NAMEDES, "%s='%s'", TABLE_CR_IP, vgw->server);
    if (lc_rmdb_cr_load(&cr, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find server: %s compute_resource in DB",
               vgw->server);
    } else {

        if (0 <= (int)(cr.cpu_alloc - action->vcpu_num)) {
            cr.cpu_alloc -= action->vcpu_num;
        } else {
            cr.cpu_alloc  = 0;
            RM_LOG(LOG_ERR, "cpu info: used:%u, action:%u error",
                   cr.cpu_alloc, action->vcpu_num);
        }
        if (0 <= (int)(cr.mem_alloc - action->mem_size)) {
            cr.mem_alloc -= action->mem_size;
        } else {
            cr.mem_alloc  = 0;
            RM_LOG(LOG_ERR, "mem info: used:%u, action:%u error",
                   cr.mem_alloc, action->mem_size);
        }
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s=%d",
                 TABLE_CR_CPU_ALLOC, cr.cpu_alloc,
                 TABLE_CR_MEM_ALLOC, cr.mem_alloc);
        lc_rmdb_cr_multi_update(set, where);
    }

    /* delete action from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d",TABLE_ACTION_ID, action->id);
    lc_rmdb_action_delete(where);
}
