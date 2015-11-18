#include <stdlib.h>
#include <string.h>
#include "lc_rm.h"
#include "lc_rm_db.h"
#include "lc_rm_msg.h"
#include "lc_rm_action.h"
#include "allocation/lc_rm_alloc.h"

pthread_mutex_t vm_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vgw_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static int lc_rm_vm_add_request(Talker__VmAddReq *msg, uint32_t seq);
static int lc_rm_vm_modify_request(Talker__VmModifyReq *msg, uint32_t seq);
static int lc_rm_third_vm_add_request(Talker__ThirdVmAddReq *msg, uint32_t seq);
static int lc_rm_third_vgw_add_request(Talker__ThirdVgwAddReq *msg, uint32_t seq);
static int lc_rm_vm_ops_reply(Talker__VmOpsReply *msg, uint32_t seq);
static int lc_rm_vgateway_add_request(Talker__VgatewayAddReq *msg, uint32_t seq);
static int lc_rm_vm_snapshot_request(Talker__VmCreateSnapshotReq *msg, uint32_t seq);

int rm_msg_handler(header_t *m_head)
{
    int ret = 0;
    Talker__Message *msg = NULL;
    msg = talker__message__unpack(NULL, m_head->length, (uint8_t *)(m_head + 1));

    if (!msg) {
        return -1;
    }

    if (msg->vm_add_req) {
        ret = lc_rm_vm_add_request(msg->vm_add_req, m_head->seq);
    } else if (msg->vm_modify_req) {
        ret = lc_rm_vm_modify_request(msg->vm_modify_req, m_head->seq);
    } else if (msg->third_vm_add_req) {
        ret = lc_rm_third_vm_add_request(msg->third_vm_add_req, m_head->seq);
    } else if (msg->third_vgw_add_req) {
        ret = lc_rm_third_vgw_add_request(msg->third_vgw_add_req, m_head->seq);
    } else if (msg->vm_ops_reply) {
        ret = lc_rm_vm_ops_reply(msg->vm_ops_reply, m_head->seq);
    } else if (msg->vgateway_add_req) {
        ret = lc_rm_vgateway_add_request(msg->vgateway_add_req, m_head->seq);
    } else if (msg->vm_create_snapshot_req) {
        ret = lc_rm_vm_snapshot_request(msg->vm_create_snapshot_req, m_head->seq);
    } else {
        RM_LOG(LOG_ERR, "Unknow msg");
    }

    if (ret) {
        RM_LOG(LOG_ERR, "rm_msg_handler failed");
    }

    talker__message__free_unpacked(msg, NULL);
    return ret;
}

static int lc_rm_vm_add_request(Talker__VmAddReq *msg, uint32_t seq)
{
    int   ret     = 0;
    int   vmid    = 0;
    char *token   = NULL;
    char *saveptr = NULL;
    char  where[LC_NAMEDES];
    char  vm_ids[LC_VM_IDS_LEN];
    rm_vm_t vm;
    msg_vm_add_t msg_vm_add;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "VM add req msg is NULL");
        ret = -1;
        goto error;
    }
    /* Message parsing */
    memset(&msg_vm_add, 0, sizeof(msg_vm_add_t));
    msg_vm_add.alloc_type = msg->alloc_type;
    if (NULL != msg->vm_ids) {
        memcpy(msg_vm_add.vm_ids, msg->vm_ids, LC_VM_IDS_LEN);
    }
    if (NULL != msg->pool_ids) {
        memcpy(msg_vm_add.pool_ids, msg->pool_ids, LC_POOL_IDS_LEN);
    }
    if (NULL != msg->vm_passwd) {
        memcpy(msg_vm_add.vm_passwd, msg->vm_passwd, MAX_VM_PASSWD_LEN);
    } else {
        RM_LOG(LOG_ERR, "VM passwd is NULL");
    }

    pthread_mutex_lock(&vm_alloc_mutex);

    /* Resource assign */
    if (TALKER__ALLOC_TYPE__AUTO == msg_vm_add.alloc_type) {
        alloc_vm_auto(msg_vm_add.vm_ids, msg_vm_add.pool_ids, 0);
    } else {
        alloc_vm_manual(msg_vm_add.vm_ids);
    }

    memset(vm_ids, 0, LC_VM_IDS_LEN);
    memcpy(vm_ids, msg_vm_add.vm_ids, LC_VM_IDS_LEN);
    for (token = strtok_r(vm_ids, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        /* Load vm from db */
        vmid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vm:%d", vmid);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s=%d",
                 TABLE_VM_ID, vmid, TABLE_VM_STATE, TABLE_VM_STATE_TEMP);
        memset(&vm, 0, sizeof(rm_vm_t));
        if (lc_rmdb_vm_load(&vm, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", vmid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vmid;
            msg_ops_reply.type = TALKER__VM_TYPE__VM;
            msg_ops_reply.ops = TALKER__VM_OPS__VM_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }

        if (vm.vm_errno) {
            RM_LOG(LOG_ERR, "Vm:%d resource allocation failed", vmid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vmid;
            msg_ops_reply.type = TALKER__VM_TYPE__VM;
            msg_ops_reply.ops = TALKER__VM_OPS__VM_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }

        /* Save action */
        lc_rm_save_vm_action(&vm, TABLE_ACTION_TYPE_CREATE,
                             msg_vm_add.alloc_type);

        /* Send Msg to vmdriver */
        lc_rm_send_vm_add_msg(&vm, msg_vm_add.vm_passwd, seq);
    }
    pthread_mutex_unlock(&vm_alloc_mutex);

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

static int lc_rm_third_vm_add_request(Talker__ThirdVmAddReq *msg, uint32_t seq)
{
    int   ret     = 0;
    int   vmid    = 0;
    char *token   = NULL;
    char *saveptr = NULL;
    char  where[LC_NAMEDES];
    char  vm_ids[LC_VM_IDS_LEN];
    rm_vm_t vm;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "VM add req msg is NULL");
        ret = -1;
        goto error;
    }

    memset(vm_ids, 0, LC_VM_IDS_LEN);
    if (NULL != msg->vm_ids) {
        memcpy(vm_ids, msg->vm_ids, LC_VM_IDS_LEN);
    }
    for (token = strtok_r(vm_ids, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        /* Load vm from db */
        vmid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vm:%d", vmid);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s=%d",
                 TABLE_VM_ID, vmid, TABLE_VM_STATE, TABLE_VM_STATE_TEMP);
        memset(&vm, 0, sizeof(rm_vm_t));
        if (lc_rmdb_vm_load(&vm, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", vmid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vmid;
            msg_ops_reply.type = TALKER__VM_TYPE__VM;
            msg_ops_reply.ops = TALKER__VM_OPS__VM_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }
        /* Send Msg to vmdriver */
        lc_rm_send_vm_add_msg(&vm, NULL, seq);
    }

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

void lc_rm_alloc_vgateway_auto(char *vgatewayids)
{
    char   *token      = NULL;
    char   *saveptr    = NULL;
    rm_cr_t *crinfo = NULL;
    rm_cr_t crone = {0};
    int    vgatewayid      = 0;
    int    ret = 0, cr_num = 0;
    int    i = 0, vgateway_num = 0, pre_vgateway_num = 0, flag = 0;
    char   where[LC_NAMEDES];
    char   pool_id[LC_NAMEDES];
    char   server[LC_NAMEDES];
    char   data[LC_VGW_IDS_LEN];
    rm_vgw_t vgateway;

    memset(data, 0, LC_VGW_IDS_LEN);
    memcpy(data, vgatewayids, LC_VGW_IDS_LEN);

    for (token = strtok_r(data, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        vgatewayid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vgateway:%d", vgatewayid);
        if (0 == vgatewayid) {
            continue;
        }
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d",
                 TABLE_VGW_ID, vgatewayid);
        memset(&vgateway, 0, sizeof(rm_vgw_t));

        /*get vgateway info*/
        if (lc_rmdb_vgw_load(&vgateway, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vgateway:%d in DB", vgatewayid);
            return;
        }
     }
    /* get cr info */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d and %s=%d",
            TABLE_CR_TYPE, TABLE_CR_TYPE_NSP,
            TABLE_CR_STATE, TABLE_CR_STATE_COMPLETE);
    ret = lc_rmdb_cr_get_num(&cr_num, where);
    if (ret){
        RM_LOG(LOG_ERR, "Cannot get cr num info.");
        return;
    }

    if ( cr_num == 0) {
        RM_LOG(LOG_ERR, "Cr_num is 0.");
        return;
    }
    crinfo= malloc(sizeof(rm_cr_t) * cr_num);
    ret = lc_rmdb_cr_load_all(crinfo, "*", sizeof(rm_cr_t), &cr_num, where);
    if (ret) {
        RM_LOG(LOG_ERR, "Cannot get cr info.");
        return;
    }
    for (i = 0; i< cr_num; i++) {
        crone = crinfo[i];
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s'",
                TABLE_VGW_GW_LAUNCH_SERVER, crone.cr_ip);
        ret = lc_rmdb_vgw_get_num(&vgateway_num, where);
        if (ret){
            RM_LOG(LOG_ERR, "Cannot get vgateway num info on cr:%d.",
                    crone.cr_id);
            vgateway_num = 0;
        }
        if ( i == 0 ) {
            pre_vgateway_num = vgateway_num;
        }
        if (pre_vgateway_num > vgateway_num) {
            flag = i;
            pre_vgateway_num = vgateway_num;
        }

    }
    RM_LOG(LOG_DEBUG, "Vgateway launch to cr_id:%d; cr_ip:%s; cr_poolid:%d",
            crinfo[flag].cr_id, crinfo[flag].cr_ip, crinfo[flag].pool_id);

    /*update vgateway info*/
    for (token = strtok_r(data, "#", &saveptr);
          NULL != token;
          token = strtok_r(NULL, "#", &saveptr)) {

        vgatewayid = atoi(token);
        RM_LOG(LOG_DEBUG, "Update vgateway:%d", vgatewayid);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VGW_ID, vgatewayid);
        memset(&vgateway, 0, sizeof(rm_vgw_t));

        /*update vgateway info*/
        snprintf(server, LC_NAMEDES, "'%s'", crinfo[flag].cr_ip);
        if (lc_rmdb_vgw_update(TABLE_VGW_GW_LAUNCH_SERVER,
                               server, where)) {
            RM_LOG(LOG_ERR, "Update vgateway :%d launch serverfail in DB",
                   vgatewayid);
        }
        snprintf(pool_id, LC_NAMEDES, "%d", crinfo[flag].pool_id);
        if (lc_rmdb_vgw_update(TABLE_VGW_GW_POOLID,
                               pool_id, where)) {
            RM_LOG(LOG_ERR, "Update vgateway:%d fail in DB", vgatewayid);
        }
    }
}

static int lc_rm_vgateway_add_request(Talker__VgatewayAddReq *msg, uint32_t seq)
{
    int   ret     = 0;
    int   vgwid   = 0;
    char *token   = NULL;
    char *saveptr = NULL;
    char  where[LC_NAMEDES];
    char  vgw_ids[LC_VM_IDS_LEN];
    rm_vgw_t vgw;
    msg_vgw_add_t msg_vgw_add;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "Vgateway add req msg is NULL");
        ret = -1;
        goto error;
    }
    /* Message parsing */
    memset(&msg_vgw_add, 0, sizeof(msg_vgw_add_t));
    msg_vgw_add.alloc_type = msg->alloc_type;
    if (NULL != msg->vgw_ids) {
        memcpy(msg_vgw_add.vgw_ids, msg->vgw_ids, LC_VGW_IDS_LEN);
    }

    pthread_mutex_lock(&vgw_alloc_mutex);

    /* Resource assign */
    if (TALKER__ALLOC_TYPE__AUTO == msg_vgw_add.alloc_type) {
        lc_rm_alloc_vgateway_auto(msg_vgw_add.vgw_ids);
    } else {
        alloc_vgateway_manual(msg_vgw_add.vgw_ids);
    }

    memset(vgw_ids, 0, LC_VGW_IDS_LEN);
    memcpy(vgw_ids, msg_vgw_add.vgw_ids, LC_VGW_IDS_LEN);
    for (token = strtok_r(vgw_ids, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        /* Load vgateway from db */
        vgwid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vgw:%d", vgwid);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d",
                 TABLE_VGW_ID, vgwid);
        memset(&vgw, 0, sizeof(rm_vgw_t));
        if (lc_rmdb_vgw_load(&vgw, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vgateway:%d in DB", vgwid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vgwid;
            msg_ops_reply.type = TALKER__VM_TYPE__VGATEWAY;
            msg_ops_reply.ops = TALKER__VGATEWAY_OPS__VGATEWAY_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_vgateway_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }

        if (vgw.vgw_errno) {
            RM_LOG(LOG_ERR, "Vgateway:%d resource allocation failed", vgwid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vgwid;
            msg_ops_reply.type = TALKER__VM_TYPE__VGATEWAY;
            msg_ops_reply.ops = TALKER__VGATEWAY_OPS__VGATEWAY_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_vgateway_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }

        /* Save action */
        lc_rm_save_vgateway_action(&vgw, TABLE_ACTION_TYPE_CREATE,
                              msg_vgw_add.alloc_type);
    }
    pthread_mutex_unlock(&vgw_alloc_mutex);
    if (!ret) {
        memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
        msg_ops_reply.id = vgwid;
        msg_ops_reply.type = TALKER__VM_TYPE__VGATEWAY;
        msg_ops_reply.ops = TALKER__VGATEWAY_OPS__VGATEWAY_CREATE;
        msg_ops_reply.result = TALKER__RESULT__SUCCESS;
        lc_rm_send_vgateway_ops_reply(&msg_ops_reply, seq);
    }
error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

static int lc_rm_third_vgw_add_request(Talker__ThirdVgwAddReq *msg, uint32_t seq)
{
    int   ret     = 0;
    int   vgwid   = 0;
    char *token   = NULL;
    char *saveptr = NULL;
    char  where[LC_NAMEDES];
    char  vgw_ids[LC_VM_IDS_LEN];
    rm_vgw_t vgw;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "Vgw add req msg is NULL");
        ret = -1;
        goto error;
    }

    memset(vgw_ids, 0, LC_VGW_IDS_LEN);
    if (NULL != msg->vgw_ids) {
        memcpy(vgw_ids, msg->vgw_ids, LC_VGW_IDS_LEN);
    }
    for (token = strtok_r(vgw_ids, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        /* Load vgw from db */
        vgwid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vgw:%d", vgwid);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d",
                 TABLE_VGW_ID, vgwid);
        memset(&vgw, 0, sizeof(rm_vgw_t));
        if (lc_rmdb_vgw_load(&vgw, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vgw:%d in DB", vgwid);
            memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
            msg_ops_reply.id = vgwid;
            msg_ops_reply.type = TALKER__VM_TYPE__VGW;
            msg_ops_reply.ops = TALKER__VM_OPS__VM_CREATE;
            msg_ops_reply.result = TALKER__RESULT__FAIL;
            lc_rm_send_ops_reply(&msg_ops_reply, seq);
            ret = -1;
            continue;
        }
        /* Send Msg to vmdriver */
        lc_rm_send_vgw_add_msg(&vgw, seq);
    }

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

static int lc_rm_vm_modify_request(Talker__VmModifyReq *msg, uint32_t seq)
{
    int   ret     = 0;
    char  where[LC_NAMEDES];
    rm_vm_t vm;
    rm_vdisk_t vdisk;
    msg_vm_modify_t msg_vm_modify;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "VM modify msg is NULL");
        ret = -1;
        goto error;
    }
    /* Message parsing */
    memset(&msg_vm_modify, 0, sizeof(msg_vm_modify_t));
    msg_vm_modify.vm_id = msg->vm_id;
    msg_vm_modify.mask = msg->mask;
    if (msg_vm_modify.mask & TALKER__MODIFY_MASK__CPU_RESIZE) {
        msg_vm_modify.vcpu_num = msg->vcpu_num;
    }
    if (msg_vm_modify.mask & TALKER__MODIFY_MASK__MEM_RESIZE) {
        msg_vm_modify.mem_size = msg->mem_size;
    }
    if (msg_vm_modify.mask & TALKER__MODIFY_MASK__DISK_USER_RESIZE) {
        msg_vm_modify.usr_disk_size = msg->usr_disk_size;
    }
    if (msg_vm_modify.mask & TALKER__MODIFY_MASK__HA_DISK) {
        msg_vm_modify.ha_disk_size = msg->ha_disk_size;
    }
    if (msg_vm_modify.mask & TALKER__MODIFY_MASK__VL2_ID) {
        msg_vm_modify.vdc_id = msg->vdc_id;
        msg_vm_modify.vl2_id = msg->vl2_id;
        memcpy(msg_vm_modify.ip, msg->ip, MAX_VM_ADDRESS_LEN - 1);
    }

    pthread_mutex_lock(&vm_alloc_mutex);

    /* Resource assign */
    alloc_vm_modify(&msg_vm_modify);

    RM_LOG(LOG_DEBUG, "Load vm:%d", msg_vm_modify.vm_id);

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VM_ID, msg_vm_modify.vm_id);
    memset(&vm, 0, sizeof(rm_vm_t));
    if (lc_rmdb_vm_load(&vm, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", msg_vm_modify.vm_id);
        pthread_mutex_unlock(&vm_alloc_mutex);
        memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
        msg_ops_reply.id = vm.vm_id;
        msg_ops_reply.type = TALKER__VM_TYPE__VM;
        msg_ops_reply.ops = TALKER__VM_OPS__VM_MODIFY;
        msg_ops_reply.result = TALKER__RESULT__FAIL;
        lc_rm_send_ops_reply(&msg_ops_reply, seq);
        ret = -1;
        goto error;
    }

    if (vm.vm_errno) {
        RM_LOG(LOG_ERR, "Vm:%d resource allocation failed", vm.vm_id);
        pthread_mutex_unlock(&vm_alloc_mutex);
        memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
        msg_ops_reply.id = vm.vm_id;
        msg_ops_reply.type = TALKER__VM_TYPE__VM;
        msg_ops_reply.ops = TALKER__VM_OPS__VM_MODIFY;
        msg_ops_reply.result = TALKER__RESULT__FAIL;
        lc_rm_send_ops_reply(&msg_ops_reply, seq);
        ret = -1;
        goto error;
    }

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VDISK_VMID, msg_vm_modify.vm_id);
    memset(&vdisk, 0, sizeof(rm_vdisk_t));
    if (lc_rmdb_vdisk_load(&vdisk, "*", where)) {
        RM_LOG(LOG_DEBUG, "No vm:%d vdisk in DB", msg_vm_modify.vm_id);
    }

    /* Modify mask for add user disk */
    if (0 == vm.vdi_usr_size) {
        msg_vm_modify.mask &= ~TALKER__MODIFY_MASK__DISK_USER_RESIZE;
        msg_vm_modify.mask |= TALKER__MODIFY_MASK__DISK_ADD;
    }

    /* Save action */
    vm.vdi_sys_size = 0;
    if (0 <= (int)(msg_vm_modify.usr_disk_size - vm.vdi_usr_size)) {
        vm.vdi_usr_size = msg_vm_modify.usr_disk_size - vm.vdi_usr_size;
    } else {
        vm.vdi_usr_size = 0;
    }
    if (0 <= (int)(msg_vm_modify.mem_size - vm.mem_size)) {
        vm.mem_size = msg_vm_modify.mem_size - vm.mem_size;
    } else {
        vm.mem_size = 0;
    }
    if (0 <= (int)(msg_vm_modify.vcpu_num - vm.vcpu_num)) {
        vm.vcpu_num = msg_vm_modify.vcpu_num - vm.vcpu_num;
    } else {
        vm.vcpu_num = 0;
    }
    if (0 <= (int)(msg_vm_modify.ha_disk_size - vdisk.vdisk_size)) {
        vm.vdi_vdisk_size = msg_vm_modify.ha_disk_size - vdisk.vdisk_size;
    } else {
        vm.vdi_vdisk_size = 0;
    }

    lc_rm_save_vm_action(&vm, TABLE_ACTION_TYPE_MODIFY, 0);

    /* Send Msg to vmdriver */
    lc_rm_send_vm_modify_msg(&msg_vm_modify, &vdisk, seq);

    pthread_mutex_unlock(&vm_alloc_mutex);

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

static int lc_rm_vm_snapshot_request(Talker__VmCreateSnapshotReq *msg, uint32_t seq)
{
    int   ret     = 0;
    char  where[LC_NAMEDES];
    rm_vm_t vm;
    msg_vm_snapshot_t msg_vm_snapshot;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "VM snapshot msg is NULL");
        ret = -1;
        goto error;
    }
    /* Message parsing */
    memset(&msg_vm_snapshot, 0, sizeof(msg_vm_snapshot_t));
    msg_vm_snapshot.vm_id = msg->vm_id;
    msg_vm_snapshot.snapshot_id = msg->snapshot_id;

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VM_ID, msg_vm_snapshot.vm_id);
    memset(&vm, 0, sizeof(rm_vm_t));
    if (lc_rmdb_vm_load(&vm, "*", where)) {
        RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", msg_vm_snapshot.vm_id);
        memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
        msg_ops_reply.id = vm.vm_id;
        msg_ops_reply.type = TALKER__VM_TYPE__VM;
        msg_ops_reply.ops = TALKER__VM_OPS__SNAPSHOT_CREATE;
        msg_ops_reply.result = TALKER__RESULT__FAIL;
        lc_rm_send_ops_reply(&msg_ops_reply, seq);
        ret = -1;
        goto error;
    }

    pthread_mutex_lock(&vm_alloc_mutex);
    /* Resource assign */
    if (alloc_vm_snapshot(&msg_vm_snapshot)) {
        RM_LOG(LOG_ERR, "Vm:%d snapshot resource allocation failed",
               msg_vm_snapshot.vm_id);
        pthread_mutex_unlock(&vm_alloc_mutex);
        memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
        msg_ops_reply.id = msg_vm_snapshot.vm_id;
        msg_ops_reply.type = TALKER__VM_TYPE__VM;
        msg_ops_reply.ops = TALKER__VM_OPS__SNAPSHOT_CREATE;
        msg_ops_reply.result = TALKER__RESULT__FAIL;
        lc_rm_send_ops_reply(&msg_ops_reply, seq);
        ret = -1;
        goto error;
    }

    /* Save action */
    vm.vcpu_num = 0;
    vm.mem_size = 0;
    lc_rm_save_vm_action(&vm, TABLE_ACTION_TYPE_SNAPSHOT, 0);

    /* Send Msg to vmdriver */
    lc_rm_send_vm_snapshot_msg(&msg_vm_snapshot, seq);

    pthread_mutex_unlock(&vm_alloc_mutex);

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

static int lc_rm_vm_ops_reply(Talker__VmOpsReply *msg, uint32_t seq)
{
    int  ret     = 0;
    char where[LC_NAMEDES];
    rm_vm_t vm;
    rm_vgw_t vgw;
    rm_snapshot_t snapshot;
    rm_action_t action;
    msg_ops_reply_t msg_ops_reply;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (!msg) {
        RM_LOG(LOG_ERR, "VM ops reply msg is NULL");
        ret = -1;
        goto error;
    }
    /* Message parsing */
    memset(&msg_ops_reply, 0, sizeof(msg_ops_reply_t));
    msg_ops_reply.id     = msg->id;
    msg_ops_reply.type   = msg->type;
    msg_ops_reply.ops    = msg->ops;
    msg_ops_reply.err    = msg->err;
    msg_ops_reply.result = msg->result;

    /* Release action */
    if (TALKER__VM_TYPE__VM == msg_ops_reply.type) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VM_ID, msg_ops_reply.id);
        memset(&vm, 0, sizeof(rm_vm_t));
        if (lc_rmdb_vm_load(&vm, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", msg_ops_reply.id);
            ret = -1;
        }

        if (vm.vm_flag & TABLE_VM_FLAG_THIRD_HW) {
            RM_LOG(LOG_DEBUG, "Vm:%d is third party device", msg_ops_reply.id);
            goto send_msg;
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
                 TABLE_ACTION_OBJECTID, vm.vm_id,
                 TABLE_ACTION_OBJECTTYPE, TABLE_ACTION_OBJECTTYPE_VM);
        memset(&action, 0, sizeof(rm_action_t));
        if (lc_rmdb_action_load(&action, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d action in DB", vm.vm_id);
            ret = -1;
        }

        pthread_mutex_lock(&vm_alloc_mutex);
        lc_rm_del_vm_action(&vm, &action);
        pthread_mutex_unlock(&vm_alloc_mutex);

    } else if (TALKER__VM_TYPE__VGW == msg_ops_reply.type) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VGW_ID, msg_ops_reply.id);
        memset(&vgw, 0, sizeof(rm_vgw_t));
        if (lc_rmdb_vgw_load(&vgw, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vgw:%d in DB", msg_ops_reply.id);
            ret = -1;
        }

        if (vgw.vgw_flag & TABLE_VGW_FLAG_THIRD_HW) {
            RM_LOG(LOG_DEBUG, "Vgw:%d is third party device", msg_ops_reply.id);
            goto send_msg;
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
                 TABLE_ACTION_OBJECTID, msg_ops_reply.id,
                 TABLE_ACTION_OBJECTTYPE, TABLE_ACTION_OBJECTTYPE_VGW);
        memset(&action, 0, sizeof(rm_action_t));
        if (lc_rmdb_action_load(&action, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vgw:%d action in DB", msg_ops_reply.id);
            ret = -1;
        }
        pthread_mutex_lock(&vgw_alloc_mutex);
        lc_rm_del_vgw_action(&vgw, &action);
        pthread_mutex_unlock(&vgw_alloc_mutex);

    } else if (TALKER__VM_TYPE__SNAPSHOT == msg_ops_reply.type) {

        msg_ops_reply.type = TALKER__VM_TYPE__VM;
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VM_ID, msg_ops_reply.id);
        memset(&vm, 0, sizeof(rm_vm_t));
        memset(&snapshot, 0, sizeof(rm_snapshot_t));
        if (lc_rmdb_snapshot_load(&snapshot, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm/snapshot:%d in DB", msg_ops_reply.id);
            ret = -1;
        }
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_VM_ID, snapshot.vm_id);
        if (lc_rmdb_vm_load(&vm, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d in DB", snapshot.vm_id);
            ret = -1;
        }

        if (vm.vm_flag & TABLE_VM_FLAG_THIRD_HW) {
            RM_LOG(LOG_DEBUG, "Vm:%d is third party device", msg_ops_reply.id);
            goto send_msg;
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
                 TABLE_ACTION_OBJECTID, vm.vm_id,
                 TABLE_ACTION_OBJECTTYPE, TABLE_ACTION_OBJECTTYPE_VM);
        memset(&action, 0, sizeof(rm_action_t));
        if (lc_rmdb_action_load(&action, "*", where)) {
            RM_LOG(LOG_ERR, "Cannot find vm:%d action in DB", vm.vm_id);
            ret = -1;
        }

        pthread_mutex_lock(&vm_alloc_mutex);
        lc_rm_del_vm_action(&vm, &action);
        pthread_mutex_unlock(&vm_alloc_mutex);
    } else {
        RM_LOG(LOG_ERR, "vm/vgw/snapshot:%d type is err", msg_ops_reply.id);
    }

send_msg:
    /* Send Msg to talker */
    lc_rm_send_ops_reply(&msg_ops_reply, seq);

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}
