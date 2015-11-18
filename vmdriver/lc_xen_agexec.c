#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <xen/api/xen_all.h>
#include <assert.h>

#include "vm/vm_host.h"
#include "lc_db_vm.h"
#include "lc_xen.h"
#include "message/msg.h"
#include "lc_agent_msg.h"
#include "lc_agexec_msg.h"
#include "lc_utils.h"
#include "lc_db_errno.h"
#include "lc_db_log.h"
#include "lc_db.h"

static int lc_agexec_vm_snapshot_action(agexec_msg_t *p_req, agexec_msg_t **p_res, char *host_address, int action)
{
    int ret = 0;
    agexec_vm_t  *p_req_vm = NULL;

    p_req_vm = (agexec_vm_t *)(p_req->data);
    p_req_vm->vm_action = action;

    if (AGEXEC_FALSE == lc_agexec_exec(p_req, p_res, host_address)) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed\n");
        ret = -1;
        goto error;
    }
error:
    return ret;
}

int lc_agexec_vm_create(vm_info_t *vm_info, uint32_t seqid)
{
    int ret = 0;
    int i;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;

    if (NULL == vm_info) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    /* xe vm-create */
    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));

    p_req_vm = (agexec_vm_t *)p_req->data;
    strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
            AGEXEC_LEN_LABEL);
    strncpy(p_req_vm->template_name_label, vm_info->vm_template,
            AGEXEC_LEN_LABEL);
    strncpy(p_req_vm->vdi_sys_sr_uuid, vm_info->vdi_sys_sr_uuid,
            AGEXEC_LEN_LABEL);
    p_req_vm->vdi_sys_size = vm_info->vdi_sys_size;
    strncpy(p_req_vm->vdi_user_sr_uuid, vm_info->vdi_user_sr_uuid,
            AGEXEC_LEN_LABEL);
    p_req_vm->vdi_user_size = vm_info->vdi_user_size;
    p_req_vm->vcpu_num = vm_info->vcpu_num;
    p_req_vm->mem_size = vm_info->mem_size;
    /* result field represents a local or NFS template in request */
    if (vm_info->vm_template_type == LC_TEMPLATE_OS_LOCAL) {
        p_req_vm->result = VM_TEMPLATE_TYPE_LOCAL;
    } else {
        p_req_vm->result = VM_TEMPLATE_TYPE_NFS;
    }

    if (vm_info->vm_type == LC_VM_TYPE_GW) {
        p_req_vm->vm_type = VM_TYPE_GW;
        for (i = 0; i < LC_PUB_VIF_MAX; ++i) {
            p_req_vm->vifs[i].network_type = LC_ULNK_BR_ID;
            p_req_vm->vifs[i].in_use       = VIF_IN_USE_Y;
        }
        for (i = LC_PUB_VIF_MAX; i < (LC_PUB_VIF_MAX + LC_L2_MAX); ++i) {
            p_req_vm->vifs[i].network_type = LC_DATA_BR_ID;
            p_req_vm->vifs[i].in_use       = VIF_IN_USE_Y;
        }
        for (i = (LC_PUB_VIF_MAX + LC_L2_MAX); i < VM_MAX_VIFS; ++i) {
            p_req_vm->vifs[i].network_type = LC_CTRL_BR_ID;
            p_req_vm->vifs[i].in_use       = VIF_IN_USE_Y;
        }
    } else if (vm_info->vm_type == LC_VM_TYPE_VM) {
        p_req_vm->vm_type = VM_TYPE_VM;
        for (i = 0; i < LC_VM_IFACE_CTRL; ++i) {
            p_req_vm->vifs[i].network_type = LC_DATA_BR_ID;
            p_req_vm->vifs[i].in_use = VIF_IN_USE_Y;
        }
        p_req_vm->vifs[LC_VM_IFACE_CTRL].network_type = LC_CTRL_BR_ID;
        p_req_vm->vifs[LC_VM_IFACE_CTRL].in_use = VIF_IN_USE_Y;
    }

    strncpy(p_req_vm->ctrl_dev, "eth6", AGEXEC_LEN_LABEL - 1);
    snprintf(p_req_vm->ctrl_ip, AGEXEC_LEN_NW_ADDR, "%s/%d",
             vm_info->ifaces[LC_VM_IFACE_CTRL].if_ip,
             vm_info->ifaces[LC_VM_IFACE_CTRL].if_mask);
    snprintf(p_req_vm->ctrl_netmask, AGEXEC_LEN_NW_ADDR, "%s",
             vm_info->ifaces[LC_VM_IFACE_CTRL].if_netmask);
    strncpy(p_req_vm->init_passwd, vm_info->vm_passwd,
            AGEXEC_LEN_PASSWORD - 1);
    strncpy(p_req_vm->vm_hostname, vm_info->vm_name,
            AGEXEC_LEN_LABEL - 1);

    p_req->hdr.seq = seqid;
    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.action_type = AGEXEC_ACTION_ADD;
    p_req->hdr.flag |= AGEXEC_FLG_ASYNC_MSG;

    if (lc_agexec_exec(p_req, &p_res, vm_info->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed\n");
        ret = -1;
        goto error;
    }

    free(p_req);
    p_req = NULL;

    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    return ret;
}

int lc_agexec_vm_ops(vm_info_t *vm_info, int msg_type, uint32_t seqid)
{
    int ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;

    if (NULL == vm_info) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.seq = seqid;
    p_req->hdr.flag |= AGEXEC_FLG_ASYNC_MSG;

    p_req_vm = (agexec_vm_t *)p_req->data;
    switch (msg_type) {
    case LC_MSG_VM_START:
    case LC_MSG_VEDGE_START:
        strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
                AGEXEC_LEN_LABEL);
        strncpy(p_req_vm->vm_uuid, vm_info->vm_uuid,
                AGEXEC_LEN_UUID);
        /*FIXME: Just don't know update the agexec version. To
         * load host_name with template_name_label */
        strncpy(p_req_vm->template_name_label, vm_info->host_name_label,
                AGEXEC_LEN_LABEL);
        p_req->hdr.action_type = AGEXEC_ACTION_START;
        break;
    case LC_MSG_VM_SYS_DISK_MIGRATE:
        strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
                AGEXEC_LEN_LABEL);
        strncpy(p_req_vm->vdi_sys_uuid, vm_info->vdi_sys_uuid,
                AGEXEC_LEN_UUID);
        strncpy(p_req_vm->sr_uuid, vm_info->vdi_user_sr_uuid,
                AGEXEC_LEN_UUID);
        p_req_vm->vdi_sys_size = 1;
        p_req->hdr.action_type = AGEXEC_ACTION_START;
        break;
    case LC_MSG_VM_STOP:
        strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
                AGEXEC_LEN_LABEL);
        p_req->hdr.action_type = AGEXEC_ACTION_STOP;
        break;
    case LC_MSG_VM_DEL:
        strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
                AGEXEC_LEN_LABEL);
        p_req->hdr.action_type = AGEXEC_ACTION_DEL;
        break;
    }

    if (vm_info->vm_type == LC_VM_TYPE_GW) {
        p_req_vm->vm_type = VM_TYPE_GW;
    } else if (vm_info->vm_type == LC_VM_TYPE_VM) {
        p_req_vm->vm_type = VM_TYPE_VM;
    }

    if (lc_agexec_exec(p_req, &p_res, vm_info->hostp->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed\n");
        ret = -1;
        goto error;
    }

    free(p_req);
    p_req = NULL;

    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    return ret;
}

int lc_agexec_vm_snapshot_ops(vm_info_t *vm_info, vm_snapshot_info_t *snapshot_info, int action)
{
    int ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_snapshot = NULL;
    agexec_vm_t *p_res_snapshot = NULL;
    vm_snapshot_info_t vm_backup;

    memset(&vm_backup, 0, sizeof(vm_snapshot_info_t));

    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.action_type = AGEXEC_ACTION_BACPUP;

    p_req_snapshot = (agexec_vm_t *)p_req->data;
    strncpy(p_req_snapshot->vm_name_label, vm_info->vm_label,
                                           AGEXEC_LEN_LABEL);
    p_req_snapshot->vdi_user_size = vm_info->vdi_user_size;
    strcpy(p_req_snapshot->vm_uuid, vm_info->vm_uuid);
    strncpy(p_req_snapshot->vm_snapshot_label,
            snapshot_info->snapshot_label, AGEXEC_LEN_LABEL);
    if (strlen(snapshot_info->snapshot_uuid)) {
        strcpy(p_req_snapshot->vm_snapshot_uuid, snapshot_info->snapshot_uuid);
    }
    switch (action) {
    case XEN_VM_SNAPSHOT_CREATE:
        if (lc_agexec_vm_snapshot_action(p_req, &p_res, vm_info->hostp->host_address,
                    AGEXEC_VM_ACTION_CREATE_SNAPSHOT)) {
            VMDRIVER_LOG(LOG_ERR, "Create snapshot failed");
            ret = -1;
            goto error;
        }
        p_res_snapshot = (agexec_vm_t *)p_res->data;
        ret = p_res_snapshot->result;
        if (ret == 0 ) {
            strcpy(snapshot_info->snapshot_uuid, p_res_snapshot->vm_snapshot_uuid);
        }
        break;
    case XEN_VM_SNAPSHOT_DESTROY:
        if (lc_agexec_vm_snapshot_action(p_req, &p_res, vm_info->hostp->host_address,
                    AGEXEC_VM_ACTION_DELETE_SNAPSHOT)) {
            VMDRIVER_LOG(LOG_ERR, "Delete snapshot failed");
            ret = -1;
            goto error;
        }
        p_res_snapshot = (agexec_vm_t *)p_res->data;
        ret = p_res_snapshot->result;
        break;
    case XEN_VM_SNAPSHOT_REVERT:
        if (lc_agexec_vm_snapshot_action(p_req, &p_res, vm_info->hostp->host_address,
                                                  AGEXEC_VM_ACTION_REVERT_SNAPSHOT)) {
            VMDRIVER_LOG(LOG_ERR, "Revert snapshot failed");
            ret = -1;
            goto error;
        }
        p_res_snapshot = (agexec_vm_t *)p_res->data;
        ret = p_res_snapshot->result;
        if ((ret == 0) ||
            (ret & (AGEXEC_CPU_GET_ERROR | AGEXEC_MEM_GET_ERROR |
                    AGEXEC_SYS_DISK_GET_ERROR | AGEXEC_USER_DISK_GET_ERROR))) {
            if (strlen(p_res_snapshot->vdi_sys_uuid)) {
                strcpy(vm_info->vdi_sys_uuid, p_res_snapshot->vdi_sys_uuid);
            }
            if (strlen(p_res_snapshot->vdi_user_uuid)) {
                strcpy(vm_info->vdi_user_uuid, p_res_snapshot->vdi_user_uuid);
            }
            if (vm_info->vdi_user_size != p_res_snapshot->vdi_user_size &&
                0 == (ret & AGEXEC_USER_DISK_GET_ERROR)) {
                vm_info->vdi_user_size = p_res_snapshot->vdi_user_size;
            }
            if (0 == (ret & AGEXEC_CPU_GET_ERROR)) {
                vm_info->vcpu_num = p_res_snapshot->vcpu_num;
            }
            if (0 == (ret & AGEXEC_MEM_GET_ERROR)) {
                vm_info->mem_size = p_res_snapshot->mem_size;
            }
        }
        break;
    default:
        break;
    }

error:
    if (p_req) {
        free(p_req);
        p_req = NULL;
    }
    if (p_res) {
        free(p_res);
        p_res = NULL;
    }
    return ret;
}

int lc_agexec_vm_backup_remote(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                               vm_backup_info_t *vm_backup_info)
{
    return 0;
}

int lc_agexec_vm_recovery(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                          vm_backup_info_t *vm_backup_info)
{
    return 0;
}

int lc_agexec_vm_backup_delete(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                               vm_backup_info_t *vm_backup_info)
{
    return 0;
}

int lc_agexec_vm_backup_chkdisk(vm_info_t *vm_info, host_t *phost, host_t *peerhost)
{
    return 0;
}

int lc_agexec_vm_migrate_remote(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                                vm_backup_info_t *vm_backup_info)
{
    return 0;
}

int lc_agexec_vm_migrate_local(vm_info_t *vm_info, host_t *phost, host_t *peerhost)
{
    return 0;
}

int lc_agexec_get_host_info(host_t *phost)
{
    int ret = 0, i, j;
    agexec_msg_t p_req;
    agexec_msg_t *p_res;
    agexec_host_t *p_res_host;
    char dumpbuf[MAX_CMD_BUF_SIZE];

    memset(&p_req, 0, sizeof(p_req));
    p_req.hdr.object_type = AGEXEC_OBJ_HO;
    p_req.hdr.action_type = AGEXEC_ACTION_GET;
    p_req.hdr.length = 0;

    ret = lc_agexec_exec(&p_req, &p_res, phost->host_address);
    if (ret != AGEXEC_TRUE || !p_res ||
            p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
        if (p_res && p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
            ret = p_res->hdr.err_code;
            snprintf(dumpbuf, MAX_CMD_BUF_SIZE,
                    "get host info failed.");
            phost->error_number |= LC_HOST_ERRNO_AGENT_ERROR;
        } else {
            ret = -1;
            snprintf(dumpbuf, MAX_CMD_BUF_SIZE,
                    "cannot connect to agent.");
            phost->error_number |= LC_HOST_ERRNO_CON_ERROR;
        }
        lc_vmdriver_db_log("ADD Host", dumpbuf, LOG_ERR);

        VMDRIVER_LOG(LOG_ERR, "Get host info of %s error, err_code=%d\n", phost->host_address, ret);

        goto error;
    }

    if (p_res) {
        p_res_host = (agexec_host_t *)p_res->data;
        strncpy(phost->host_name, p_res_host->name_label, MAX_HOST_NAME_LEN - 1);
        strncpy(phost->cpu_info, p_res_host->cpu_info, AGEXEC_LEN_CPU_INFO - 1);
        phost->mem_total = p_res_host->mem_total;
        phost->disk_total = p_res_host->disk_total - p_res_host->disk_template;
        phost->disk_total -= RESERVED_UNUSED_SIZE;
        strncpy(phost->master_address, p_res_host->master_ip,
                MAX_HOST_ADDRESS_LEN - 1);
        memset(phost->storage, 0, LC_MAX_SR_NUM * sizeof(storage_t));
        if (p_res_host->n_storage > LC_MAX_SR_NUM) {
            VMDRIVER_LOG(LOG_ERR, "too many SRs\n");
            phost->n_storage = LC_MAX_SR_NUM;
        } else {
            phost->n_storage = p_res_host->n_storage;
        }
        for (i = 0; i < phost->n_storage; ++i) {
            strncpy(phost->storage[i].name_label, p_res_host->storage[i].name_label,
                    MAX_SR_NAME_LEN - 1);
            strncpy(phost->storage[i].uuid, p_res_host->storage[i].uuid,
                    MAX_UUID_LEN - 1);
            phost->storage[i].is_shared = p_res_host->storage[i].is_shared;
            phost->storage[i].disk_total = p_res_host->storage[i].disk_total;
            phost->storage[i].disk_used = p_res_host->storage[i].disk_used;
        }
        memset(phost->ha_storage, 0, MAX_VM_HA_DISK * sizeof(storage_t));
        if (p_res_host->n_storage > MAX_VM_HA_DISK) {
            VMDRIVER_LOG(LOG_ERR, "too many HA SRs\n");
            phost->n_hastorage = MAX_VM_HA_DISK;
        } else {
            phost->n_hastorage = p_res_host->n_hastorage;
        }
        for (i = phost->n_storage, j = 0; i < phost->n_storage + phost->n_hastorage; ++i, ++j) {
            strncpy(phost->ha_storage[j].name_label, p_res_host->storage[i].name_label,
                    MAX_SR_NAME_LEN - 1);
            strncpy(phost->ha_storage[j].uuid, p_res_host->storage[i].uuid,
                    MAX_UUID_LEN - 1);
            phost->ha_storage[j].is_shared = p_res_host->storage[i].is_shared;
            phost->ha_storage[j].disk_total = p_res_host->storage[i].disk_total;
            phost->ha_storage[j].disk_used = p_res_host->storage[i].disk_used;
        }
    }

error:
    if (p_res) {
        free(p_res);
        p_res = 0;
    }
    return ret;
}

int lc_agexec_vm_uninstall(vm_info_t *vm_info)
{
    int ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;

    if (NULL == vm_info) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));

    p_req_vm = (agexec_vm_t *)p_req->data;
    p_req_vm->vm_action = AGEXEC_VM_ACTION_UNINSTALL_VM;
    strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
            AGEXEC_LEN_LABEL);

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.action_type = AGEXEC_ACTION_DEL;

    if (lc_agexec_exec(p_req, &p_res, vm_info->host_address) ||
        !p_res || p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed\n");
        ret = -1;
        goto error;
    }

    free(p_req);
    free(p_res);
    p_res = NULL;
    p_req = NULL;

    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    if (NULL != p_res) {
        free(p_res);
        p_res = NULL;
    }
    return ret;
}

int lc_agexec_vm_stop(vm_info_t *vm_info)
{
    int ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;
    char err_desc[AGEXEC_ERRDESC_LEN];

    if (NULL == vm_info) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    memset(err_desc, 0, sizeof(err_desc));
    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));

    p_req_vm = (agexec_vm_t *)p_req->data;
    strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
            AGEXEC_LEN_LABEL);

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.action_type = AGEXEC_ACTION_STOP;

    if (vm_info->vm_type == LC_VM_TYPE_GW) {
        p_req_vm->vm_type = VM_TYPE_GW;
    } else if (vm_info->vm_type == LC_VM_TYPE_VM) {
        p_req_vm->vm_type = VM_TYPE_VM;
    }

    if (lc_agexec_exec(p_req, &p_res, vm_info->host_address) ||
        !p_res || p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed\n");
        ret = -1;
        goto error;
    }

    free(p_req);
    free(p_res);
    p_res = NULL;
    p_req = NULL;

    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    if (p_res &&
        (p_res->hdr.err_code == AGEXEC_ERR_VM_OP_ERROR)) {
        snprintf(err_desc, AGEXEC_ERRDESC_LEN, "%s", (char *)p_res->data);
        if (vm_info->vm_type == LC_VM_TYPE_VM) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info->vm_id, vm_info->vm_label, err_desc,
                              LC_SYSLOG_LEVEL_ERR);
        } else {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VGW, LC_SYSLOG_OBJECTTYPE_VGW,
                              vm_info->vm_id, vm_info->vm_label, err_desc,
                              LC_SYSLOG_LEVEL_ERR);
        }
    } else {
        snprintf(err_desc, AGEXEC_ERRDESC_LEN, "failed to stop");
        if (vm_info->vm_type == LC_VM_TYPE_VM) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info->vm_id, vm_info->vm_label, err_desc,
                              LC_SYSLOG_LEVEL_ERR);
        } else {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VGW, LC_SYSLOG_OBJECTTYPE_VGW,
                              vm_info->vm_id, vm_info->vm_label, err_desc,
                              LC_SYSLOG_LEVEL_ERR);
        }
    }
    if (NULL != p_res) {
        free(p_res);
        p_res = NULL;
    }
    return ret;
}

int lc_agexec_vm_modify(vm_info_t *vm_info, int disk_num, disk_t disks[])
{
    int i, ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;
    agexec_vm_t *p_res_vm = NULL;

    if (NULL == vm_info) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    p_req = (agexec_msg_t *)
             malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t));

    p_req_vm = (agexec_vm_t *)p_req->data;
    strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
            AGEXEC_LEN_LABEL);
    if (vm_info->vdi_user_size) {
        p_req_vm->vdi_user_size = vm_info->vdi_user_size;
        if (strlen(vm_info->vdi_user_uuid)) {
            strcpy(p_req_vm->vdi_user_uuid, vm_info->vdi_user_uuid);
        }
        /*just backward compatible, one 2.0 user maybe need to add user disk
         * in 2.2
         * */
        if (strlen(vm_info->vdi_sys_sr_uuid)) {
            /*Assume the same uuid between sys and user srs*/
            strcpy(p_req_vm->vdi_user_sr_uuid, vm_info->vdi_sys_sr_uuid);
        }
    }
    if (vm_info->vcpu_num) {
        p_req_vm->vcpu_num = vm_info->vcpu_num;
    }
    if (vm_info->mem_size) {
        p_req_vm->mem_size = vm_info->mem_size;
    }
    p_req_vm->ha_disk_num = disk_num;
    for (i=0; i<disk_num; i++) {
        p_req_vm->ha_disks[i].vdi_size = disks[i].size;
        p_req_vm->ha_disks[i].userdevice = disks[i].userdevice;
        if (disks[i].vdi_uuid[0]) {
            strcpy(p_req_vm->ha_disks[i].vdi_uuid, disks[i].vdi_uuid);
        }
        strcpy(p_req_vm->ha_disks[i].vdi_sr_uuid, disks[i].vdi_sr_uuid);
    }

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t);
    p_req->hdr.action_type = AGEXEC_ACTION_MODIFY;

    if (lc_agexec_exec(p_req, &p_res, vm_info->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed");
        ret = -1;
        goto error;
    }

    free(p_req);
    p_req = NULL;
    p_res_vm = (agexec_vm_t *)p_res->data;

    ret = p_res_vm->result;
    if (0 == ret) {
        if (strlen(p_res_vm->vdi_user_uuid)) {
            if (strlen(vm_info->vdi_user_uuid)) {
                VMDRIVER_LOG(LOG_ERR, "user_disk_uuid is conflicted(old: %s, new: %s), overwritten.",
                                              vm_info->vdi_user_uuid, p_res_vm->vdi_user_uuid);
            }
            strcpy(vm_info->vdi_user_uuid, p_res_vm->vdi_user_uuid);
        }
    }

    for (i=0; i<p_res_vm->ha_disk_num; i++) {
        if (0 == p_res_vm->ha_disks[i].ret) {
            lc_vmdb_disk_update_size(p_res_vm->ha_disks[i].vdi_size, disks[i].id);
            lc_vmdb_disk_update_uuid(p_res_vm->ha_disks[i].vdi_uuid, disks[i].id);
        } else if (!strlen(disks[i].vdi_uuid)) {
            lc_vmdb_disk_delete(disks[i].id);
        }
    }

    free(p_res);
    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    if (NULL != p_res) {
        free(p_res);
        p_res = NULL;
    }
    return ret;
}

int lc_agexec_vm_disk_set(vm_info_t *vm_info, int disk_num, disk_t disks[], int type)
{
    int i, ret = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;
    agexec_vm_t *p_res_vm = NULL;

    if (NULL == vm_info || NULL == disks) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    p_req = (agexec_msg_t *)
        malloc(sizeof(agexec_msg_t) + sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t));
    if (NULL == p_req) {
        VMDRIVER_LOG(LOG_ERR, "Malloc is failed\n");
        ret = -1;
        goto error;
    }

    memset(p_req, 0, sizeof(agexec_msg_t) + sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t));

    p_req_vm = (agexec_vm_t *)p_req->data;
    strncpy(p_req_vm->vm_name_label, vm_info->vm_label,
            AGEXEC_LEN_LABEL);
    p_req_vm->ha_disk_num = disk_num;
    for (i=0; i<disk_num; i++) {
        p_req_vm->ha_disks[i].vdi_size = disks[i].size;
        p_req_vm->ha_disks[i].userdevice = disks[i].userdevice;
        strcpy(p_req_vm->ha_disks[i].vdi_uuid, disks[i].vdi_uuid);
        strcpy(p_req_vm->ha_disks[i].vdi_sr_uuid, disks[i].vdi_sr_uuid);
    }

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t) + disk_num * sizeof(agexec_vm_disk_t);
    p_req->hdr.action_type = AGEXEC_ACTION_SET;
    if (type == LC_MSG_VM_PLUG_DISK) {
        p_req_vm->vm_action = AGEXEC_VM_ACTION_ATTACH_VDI;
    } else {
        p_req_vm->vm_action = AGEXEC_VM_ACTION_DETACH_VDI;
    }

    if (lc_agexec_exec(p_req, &p_res, vm_info->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "lc_agexec_exec() failed");
        ret = -1;
        goto error;
    }

    free(p_req);
    p_req = NULL;
    p_res_vm = (agexec_vm_t *)p_res->data;

    ret = p_res_vm->result;

    free(p_res);
    return ret;

error:

    if (NULL != p_req) {
        free(p_req);
        p_req = NULL;
    }
    if (NULL != p_res) {
        free(p_res);
        p_res = NULL;
    }
    return ret;
}
