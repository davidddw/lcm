#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <xen/api/xen_all.h>
#include <assert.h>

#include "vm/vm_host.h"
#include "vm/vm_template.h"
#include "lc_vm.h"
#include "lc_db.h"
#include "lc_db_vm.h"
#include "lc_db_pool.h"
#include "lc_db_host.h"
#include "lc_db_storage.h"
#include "lc_db_log.h"
#include "lc_db_errno.h"
#include "lc_xen.h"
#include "message/msg.h"
#include "message/msg_lcm2dri.h"
#include "message/msg_ker2vmdrv.h"
#include "lc_agent_msg.h"
#include "lc_agexec_msg.h"
#include "lc_utils.h"

#define  LC_VM_QOS_BURST  10
extern xen_session *
xen_create_session(char *server_url, char *username, char *password);

int lc_xen_test_init()
{
    xmlInitParser();
    xmlKeepBlanksDefault(0);
    xen_init();
    curl_global_init(CURL_GLOBAL_ALL);

    return 0;
}

bool lc_host_health_check(char *host_address, xen_session *session)
{
    bool result = 0;
    xen_host_ha_xapi_healthcheck(session, &result);
    if (!result) {
        lc_print_xapi_error("Add Host", "Host", host_address, session, lc_vmdriver_db_log);
    }
    return result;
}

int lc_start_vnctunnel (vm_info_t *vm_info)
{
    char cmd[LC_EXEC_CMD_LEN];
    int result = 0;

    VMDRIVER_LOG(LOG_INFO, "Ready to start VNC daemon \n");
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "/usr/local/livecloud/script/vnctunnel.sh -h %s -u %s -p %s"
            " -n %s -t %d -l %d",
            vm_info->hostp->host_address, vm_info->hostp->username,
            vm_info->hostp->password, vm_info->vm_uuid, vm_info->tport, vm_info->lport);
    result = lc_call_system(cmd, "Start VNC", "vm", vm_info->vm_label, lc_vmdriver_db_log);
    return result;
}

int lc_stop_vnctunnel (vm_info_t *vm_info)
{
    char cmd[LC_EXEC_CMD_LEN];

    VMDRIVER_LOG(LOG_INFO, "Ready to kill VNC daemon");
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "/usr/local/livecloud/script/vnckill.sh -t %d", vm_info->tport);
    lc_call_system(cmd, "Stop VNC", "vm", vm_info->vm_label, lc_vmdriver_db_log);
    return 0;
}

int lc_start_vmware_vnctunnel (vm_info_t *vm_info)
{
    char cmd[LC_EXEC_CMD_LEN];
    int result = 0;

    VMDRIVER_LOG(LOG_INFO, "Ready to start VNC daemon \n");
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "/usr/local/livecloud/script/vmware_vnctunnel.sh "
                 "-h '%s' -t '%d' -l '%d' -o launch",
            vm_info->hostp->host_address, vm_info->tport, vm_info->lport);
    result = lc_call_system(cmd, "Start VNC", "vm",
            vm_info->vm_label, lc_vmdriver_db_log);
    return result;
}

int lc_stop_vmware_vnctunnel (vm_info_t *vm_info)
{
    char cmd[LC_EXEC_CMD_LEN];

    VMDRIVER_LOG(LOG_INFO, "Ready to kill VNC daemon");
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "/usr/local/livecloud/script/vmware_vnctunnel.sh "
                 "-t '%d' -o kill", vm_info->tport);
    lc_call_system(cmd, "Stop VNC", "vm",
            vm_info->vm_label, lc_vmdriver_db_log);
    return 0;
}

int lc_learn_mac_for_vm(host_t *phost, char *vm_label, char *if_mac, int device)
{
    char  cmd[LC_EXEC_CMD_LEN];
    FILE *stream;
    int result = 0;

    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vif-list vm-name-label=%s params=MAC device=%d --minimal",
            phost->host_address, phost->username, phost->password, vm_label, device);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "stream is NULL.\n");
        return -1;
    }
    fscanf(stream, "%s", if_mac);
    pclose(stream);
    result = lc_call_file_stream(if_mac, cmd, "Add VM", "VM", vm_label, lc_vmdriver_db_log);
    return result;
}

int lc_get_vif_num(host_t *phost, char *vm_label, int *vifnum)
{
    char  cmd[LC_EXEC_CMD_LEN];
    char  vifstr[LC_EXEC_CMD_LEN];
    FILE *stream;
    int i      = 0;
    int result = 0;

    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vif-list vm-name-label=%s params=device --minimal",
            phost->host_address, phost->username, phost->password, vm_label);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "stream is NULL.\n");
        return -1;
    }
    fscanf(stream, "%s", vifstr);
    pclose(stream);

    while ('\0' != vifstr[i]) {
        if (',' == vifstr[i]) {
            (*vifnum)++;
        }
        i++;
    }
    (*vifnum)++;
    result = lc_call_file_stream(vifstr, cmd, "Add VM", "VM", vm_label, lc_vmdriver_db_log);
    return result;
}

int lc_start_vm_by_import(vm_info_t *vm_info, host_t *phost)
{
    int i;

    VMDRIVER_LOG(LOG_DEBUG, "using existing vm_info to start VM by import %s\n",
                 vm_info->vm_label);

    lc_vmdb_vif_update_name(vm_info->pub_vif[0].if_name,
            vm_info->pub_vif[0].if_id);
    lc_vmdb_vif_update_ofport(vm_info->pub_vif[0].if_port,
            vm_info->pub_vif[0].if_id);

    if (vm_info->vm_type == LC_VM_TYPE_GW) {
        for (i = 1; i < LC_PUB_VIF_MAX; ++i) {
            if (vm_info->pub_vif[i].if_state != LC_VIF_STATE_ATTACHED) {
                continue;
            }
            lc_vmdb_vif_update_name(vm_info->pub_vif[i].if_name,
                    vm_info->pub_vif[i].if_id);
            lc_vmdb_vif_update_ofport(vm_info->pub_vif[i].if_port,
                    vm_info->pub_vif[i].if_id);
        }
        for (i = 0; i < LC_L2_MAX; i++) {
            if (vm_info->pvt_vif[i].if_state != LC_VIF_STATE_ATTACHED) {
                continue;
            }
            lc_vmdb_vif_update_name(vm_info->pvt_vif[i].if_name,
                    vm_info->pvt_vif[i].if_id);
            lc_vmdb_vif_update_ofport(vm_info->pvt_vif[i].if_port,
                    vm_info->pvt_vif[i].if_id);
        }
    }
    /* add monitor */

    lc_start_vnctunnel(vm_info);

    return 0;
}

int lc_start_vm_by_vminfo(vm_info_t *vm_info, int msg_type, uint32_t seqid)
{
    if (vm_info->vm_type == LC_VM_TYPE_GW) {
        lc_vm_insert_configure_iso(vm_info);
    }
    return lc_agexec_vm_ops(vm_info, msg_type, seqid);
}

int lc_migrate_vm_sys_disk(vm_info_t *vm_info)
{
    return lc_agexec_vm_ops(vm_info, LC_MSG_VM_SYS_DISK_MIGRATE, 0);
}

int lc_vm_add_configure_iso(vm_info_t *vm_info)
{
    char cmd[512];
    host_t *master = vm_info->hostp->master;

    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vm-cd-add cd-name=xs-tools.iso vm=%s device=3",
            master->host_address, master->username, master->password,
            vm_info->vm_label);
    lc_call_system(cmd, "VM cd-add", "vm", vm_info->vm_label, lc_vmdriver_db_log);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vm-cd-eject vm=%s",
            master->host_address, master->username, master->password,
            vm_info->vm_label);
    lc_call_system(cmd, "VM cd-eject", "vm", vm_info->vm_label, lc_vmdriver_db_log);

    return 0;
}

int lc_vm_insert_configure_iso(vm_info_t *vm_info)
{
    char cmd[MAX_CMD_BUF_SIZE];
    char err_msg[MAX_CMD_BUF_SIZE];
    host_t *host = vm_info->hostp;
    int ret;

    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    sprintf(cmd, LC_ISO_UPLOAD_STR, host->host_address, "*****",
            "*****", vm_info->vm_label);
    VMDRIVER_LOG(LOG_DEBUG, "system [%s]", cmd);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    sprintf(cmd, LC_ISO_UPLOAD_STR, host->host_address, host->username,
            host->password, vm_info->vm_label);
    ret = lc_call_system(cmd, "VM ISO", "vm", vm_info->vm_label, lc_vmdriver_db_log);
    if (ret) {
        memset(err_msg, 0, MAX_CMD_BUF_SIZE);
        strerror_r(ret, err_msg, MAX_CMD_BUF_SIZE);
        VMDRIVER_LOG(LOG_ERR, "system() error %s\n",err_msg);
    }
    return 0;
}

int lc_vm_eject_configure_iso(vm_info_t *vm_info)
{
    char cmd[MAX_CMD_BUF_SIZE];
    char err_msg[MAX_CMD_BUF_SIZE];
    host_t *host = vm_info->hostp;
    int ret;

    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    sprintf(cmd, LC_ISO_DESTROY_STR, host->host_address, "******",
            "******", vm_info->vm_label);
    VMDRIVER_LOG(LOG_DEBUG, "system [%s]\n",cmd);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    sprintf(cmd, LC_ISO_DESTROY_STR, host->host_address, host->username,
            host->password, vm_info->vm_label);
    VMDRIVER_LOG(LOG_DEBUG, "Eject configure ISO in %s", vm_info->vm_label);
    ret = lc_call_system(cmd, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
    if (ret) {
        memset(err_msg, 0, MAX_CMD_BUF_SIZE);
        strerror_r(ret, err_msg, MAX_CMD_BUF_SIZE);
        VMDRIVER_LOG(LOG_ERR, "system() error %s\n",err_msg);
    }

    return 0;
}

int lc_get_host_mac_info(host_t *host)
{
    uint8_t agexec_msg_buf[sizeof(agexec_msg_t)+sizeof(agexec_pif)*6] = {0};
    agexec_msg_t *p_req = (agexec_msg_t *)agexec_msg_buf;
    agexec_pif *p_pif = NULL;
    agexec_msg_t *p_res = NULL;
    char dumpbuf[LC_AGEXEC_LEN_OVS_RESULT_LINE] = {0};
    int res,i;

    p_req->hdr.action_type = AGEXEC_ACTION_GET;
    p_req->hdr.object_type = AGEXEC_OBJ_PIF;
    p_req->hdr.length = sizeof(agexec_pif)*LC_HOST_ETH_MAX_COUT;
    VMDRIVER_LOG(LOG_DEBUG, "Get eth port info of %s\n", host->host_address);
    res = lc_agexec_exec(p_req, &p_res, host->host_address);
    VMDRIVER_LOG(LOG_DEBUG, "%s:res %d\n", __FUNCTION__, res);

    if (res == AGEXEC_TRUE) {
        res = p_res->hdr.err_code;
        snprintf(dumpbuf,LC_AGEXEC_LEN_OVS_RESULT_LINE, "get host eth port info success.");
        lc_vmdriver_db_log("ADD Host eth port info", dumpbuf, LOG_ERR);
    } else {
        res = -1;
        snprintf(dumpbuf,LC_AGEXEC_LEN_OVS_RESULT_LINE, "can not connect to agexec.");
        lc_vmdriver_db_log("ADD Host eth port info", dumpbuf, LOG_ERR);
        goto out;
    }

    lc_vmdriver_db_log("ADD Host eth port info", dumpbuf, LOG_ERR);
    for (i = 0; i < LC_HOST_ETH_MAX_COUT; i++) {
        p_pif = (agexec_pif *)((p_res)->data + i * sizeof(agexec_pif));
        VMDRIVER_LOG(LOG_DEBUG,
            "eth%d:%s mac:%s sizeof(p_pif->name):%zd sizeof(p_pif->mac):%zd\n", i,
            p_pif->name, p_pif->mac, strlen(p_pif->name), strlen(p_pif->mac));

        if (0 == strlen(p_pif->name) || 0 == strlen(p_pif->mac)) {
            ;
        } else {
            res = lc_vmdb_host_port_config_insert(
                    host->host_address, p_pif->name, p_pif->mac);
            if (res != 0){
                VMDRIVER_LOG(LOG_DEBUG,
                    "Write host:%s eth%d:%s mac:%s fail! return value is:%d\n",
                    host->host_address, i, p_pif->name, p_pif->mac, res);
            }
        }
    }

out:
    if (p_res) {
        free(p_res);
        p_res = NULL;
    }

    return res;
}
int lc_get_host_if_type(host_t *host)
{
    uint8_t agexec_msg_buf[sizeof(agexec_msg_t) +
        sizeof(agexec_br_t) * LC_N_BR_ID] = {0};
    agexec_msg_t *p_req = (agexec_msg_t *)agexec_msg_buf;
    agexec_br_t  *p_br  = NULL;
    agexec_msg_t *p_res = NULL;
    char     dumpbuf[LC_AGEXEC_LEN_OVS_RESULT_LINE] = {0};
    uint32_t br_id;
    int      i, res;

    p_req->hdr.action_type = AGEXEC_ACTION_GET;
    p_req->hdr.object_type = AGEXEC_OBJ_BR;
    p_req->hdr.length = sizeof(agexec_br_t) * LC_N_BR_ID;

    VMDRIVER_LOG(LOG_DEBUG, "Get bridge type of %s\n",host->host_address);
    p_br = (agexec_br_t *)(p_req->data);
    for (br_id = 0; br_id < LC_N_BR_ID; ++br_id, ++p_br) {
        p_br->req_field_mask |= AGEXEC_FIELD_BR_COOKIE;
        AGEXEC_UNSET_COOKIE(p_br->cookie);
        AGEXEC_SET_OBJ_ID(BR, p_br->cookie, br_id);
        p_br->res_field_mask |= AGEXEC_FIELD_BR_TYPE;

        agexec_br_dump(p_br, p_br->req_field_mask,
                dumpbuf, LC_AGEXEC_LEN_OVS_RESULT_LINE);
        VMDRIVER_LOG(LOG_DEBUG, "req-br %s\n",dumpbuf);
    }
    res = lc_agexec_exec(p_req, &p_res, host->host_address);
    if (res != AGEXEC_TRUE || !p_res ||
            p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
        if (p_res && p_res->hdr.err_code != AGEXEC_ERR_SUCCESS) {
            res = p_res->hdr.err_code;
            snprintf(dumpbuf, LC_AGEXEC_LEN_OVS_RESULT_LINE,
                    "get bridge type failed.");
            lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_AGENT_ERROR,
                    host->host_address);
        } else {
            res = -1;
            snprintf(dumpbuf, LC_AGEXEC_LEN_OVS_RESULT_LINE,
                    "can not connect to agent.");
            lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_CON_ERROR,
                    host->host_address);
        }
        lc_vmdriver_db_log("ADD Host", dumpbuf, LOG_ERR);

        VMDRIVER_LOG(LOG_ERR, "Get bridge type of %s error, "
                "err_code=%d\n", host->host_address, res);

        if (p_res) {
            free(p_res);
        }
        p_res = NULL;
        return -1;
    }

    /*
     * LC_BR_NONE = AGEXEC_BR_NONE = 0
     * LC_BR_PIF  = AGEXEC_BR_PIF  = 1
     * LC_BR_BOND = AGEXEC_BR_BOND = 2
     */
    host->if0_type = host->if1_type = host->if2_type = LC_BR_NONE;
    if (p_res->hdr.object_type == AGEXEC_OBJ_BR) {
        p_br = (agexec_br_t *)(p_res->data);
        for (i = sizeof(agexec_br_t); i <= p_res->hdr.length;
                i += sizeof(agexec_br_t), ++p_br) {
            if (!(p_br->res_field_mask & AGEXEC_FIELD_BR_COOKIE) ||
                !(p_br->res_field_mask & AGEXEC_FIELD_BR_TYPE)) {
                continue;
            }
            switch (AGEXEC_GET_OBJ_ID(BR, p_br->cookie)) {
            case LC_CTRL_BR_ID:
                host->if0_type = p_br->type;
                break;
            case LC_DATA_BR_ID:
                host->if1_type = p_br->type;
                break;
            case LC_ULNK_BR_ID:
                host->if2_type = p_br->type;
                break;
            }
        }
    }

    if (p_res) {
        free(p_res);
    }
    p_res = NULL;

    if (host->if0_type == LC_BR_NONE || host->if1_type == LC_BR_NONE ||
        (host->host_role == LC_HOST_ROLE_GATEWAY && host->if2_type == LC_BR_NONE)) {
        res = -1;
        snprintf(dumpbuf, LC_AGEXEC_LEN_OVS_RESULT_LINE,
                "host %s has no control/data%s bridge.", host->host_address,
                (host->host_role == LC_HOST_ROLE_GATEWAY ?  "/uplink" : ""));
        lc_vmdriver_db_log("ADD Host", dumpbuf, LOG_ERR);
        VMDRIVER_LOG(LOG_ERR, "%s\n", dumpbuf);
        lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_BR_ERROR,
                host->host_address);
    }

    return res;
}

int lc_get_host_device_info(host_t *host)
{
    int ret, i;
    xen_session *session = 0;

    /* used only to check password */
    LC_HOST_CONNECT_NO_CHECK(host, session);
    if (!session->ok &&
        (session->error_description_count == 0 ||
         strcmp(session->error_description[0], "HOST_IS_SLAVE") != 0)) {

        lc_print_xapi_error("Add Host", "Host", host->host_address,
                            session, lc_vmdriver_db_log);
        ret = -1;
        goto free_mem;
    }

    ret = lc_agexec_get_host_info(host);
    VMDRIVER_LOG(LOG_DEBUG, "Host %s: CPU:%s, Memory:%dM, Disk:%dG, Master: %s\n",
                       host->host_address, host->cpu_info,
                       host->mem_total, host->disk_total,
                       host->master_address);
    VMDRIVER_LOG(LOG_DEBUG, "Storage:\n");
    for (i = 0; i < host->n_storage; ++i) {
        VMDRIVER_LOG(LOG_DEBUG, "%s [%s], %s, %d/%d\n",
                host->storage[i].name_label,
                host->storage[i].uuid,
                host->storage[i].is_shared ? "shared" : "not shared",
                host->storage[i].disk_used,
                host->storage[i].disk_total);
    }
    VMDRIVER_LOG(LOG_DEBUG, "HA Storage:\n");
    for (i = 0; i < host->n_hastorage; ++i) {
        VMDRIVER_LOG(LOG_DEBUG, "%s [%s], %s, %d/%d\n",
                host->ha_storage[i].name_label,
                host->ha_storage[i].uuid,
                host->ha_storage[i].is_shared ? "shared" : "not shared",
                host->ha_storage[i].disk_used,
                host->ha_storage[i].disk_total);
    }

free_mem:

    LC_HOST_FREE(host, session);
    return ret;
}

int lc_del_network_storage (host_t *phost, char *sr_uuid)
{
    xen_session *session = NULL;
    xen_sr sr;
    xen_pbd_set *pbd_set = NULL;
    xen_host host;
    char *address = NULL;
    int i;

    LC_HOST_CONNECT(phost->master, session);
    if (!session) {
        VMDRIVER_LOG(LOG_ERR, "connect to host %s failure\n",phost->host_name);
        LC_HOST_FREE(phost->master, session);
        return -1;
    }

    if (sr_uuid) {

        if (!xen_sr_get_by_uuid(session, &sr, sr_uuid)) {
            VMDRIVER_LOG(LOG_ERR, "get sr by uuid error.\n");
            goto error;
        }

        if (!xen_sr_get_pbds(session, &pbd_set, sr)) {
            VMDRIVER_LOG(LOG_ERR, "get pbds error.\n");
            goto error;
        }

        for (i=0; i<pbd_set->size; i++) {
            if (!xen_pbd_get_host(session, &host, pbd_set->contents[i])) {
                VMDRIVER_LOG(LOG_ERR, "get pbd's host error.\n");
                goto error;
            }

            if (!xen_host_get_address(session, &address, host)) {
                VMDRIVER_LOG(LOG_ERR, "get host's address error.\n");
                goto error;
            }

            if (phost->role != LC_HOST_SLAVE) {
                if (!xen_pbd_unplug(session, pbd_set->contents[i])) {
                    VMDRIVER_LOG(LOG_ERR, "No pbd unplug.\n");
                    goto error;
                }

                if (!xen_sr_forget(session, sr)) {
                    VMDRIVER_LOG(LOG_ERR, "Cannot forget sr.\n");
                    goto error;
                }
            }
            else if (phost->role == LC_HOST_SLAVE &&
                    strcmp(address, phost->host_address) == 0) {
                if (!xen_pbd_unplug(session, pbd_set->contents[i])) {
                    VMDRIVER_LOG(LOG_ERR, "No pbd unplug.\n");
                    goto error;
                }

                if (!xen_pbd_destroy(session, pbd_set->contents[i])) {
                    VMDRIVER_LOG(LOG_ERR, "No pbd destroy.\n");
                    goto error;
                }
            }
        }
    }

    if (address) {
        free(address);
    }
    if (pbd_set) {
        xen_pbd_set_free(pbd_set);
    }
    LC_HOST_FREE(phost->master, session);
    return 0;

error:
    if (address) {
        free(address);
    }
    if (pbd_set) {
        xen_pbd_set_free(pbd_set);
    }
    LC_HOST_FREE(phost->master, session);
    return -1;
}

int lc_del_local_storage (host_t *phost)
{
    storage_resource_t sr;

    while (!lc_vmdb_storage_resource_load(&sr, phost->host_address)) {
        if (lc_vmdb_storage_resource_delete(sr.sr_uuid)) {
            VMDRIVER_LOG(LOG_ERR, "delete storage_resource from database error.\n");
            goto error;
        }
    }

    return 0;

error:
    return -1;
}

int lc_xen_reconfigure_vm_cpu(host_t * host, vm_info_t *vm_info, int cpu)
{
    char value[LC_EXEC_CMD_LEN];
    char cmd[LC_EXEC_CMD_LEN];
    char buffer[LC_EXEC_CMD_LEN];
    char logbuffer[LC_EXEC_CMD_LEN];
    int ret;
    int i;

    memset(value, 0, sizeof(value));
    sprintf(value, "%d", cpu);
    for (i = 0; i < 2; i++) {
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_VCPU_MAX, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n",logbuffer);
        }
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_VCPU, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n",logbuffer);
        }
    }

    return ret;
}

int lc_xen_reconfigure_vm_memory(host_t * host, vm_info_t *vm_info, int memory)
{
    char value[LC_EXEC_CMD_LEN];
    char cmd[LC_EXEC_CMD_LEN];
    char buffer[LC_EXEC_CMD_LEN];
    char logbuffer[LC_EXEC_CMD_LEN];
    int ret;
    int i;

    memset(value, 0, sizeof(value));
    sprintf(value, "%dMiB", memory);
    for (i = 0; i < 4; i++) {
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_MEM_SMIN, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n", logbuffer);
        }
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_MEM_DMIN, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n",logbuffer);
        }
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_MEM_DMAX, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n", logbuffer);
        }
        memset(cmd, 0, sizeof(cmd));
        memset(buffer, 0, sizeof(buffer));
        memset(logbuffer, 0, sizeof(logbuffer));
        sprintf(cmd, XEN_VM_PARAM_SET_STR, vm_info->vm_uuid,
                XEN_VM_PARAM_MEM_SMAX, value);
        sprintf(buffer, SSHPASS_STR, host->password, host->username,
                host->host_address, cmd);
        sprintf(logbuffer, SSHPASS_STR, "xxxxxxxx", host->username,
                host->host_address, cmd);
        ret = lc_call_system(buffer, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);
        if (ret) {
            VMDRIVER_LOG(LOG_ERR, "error executing %s\n",logbuffer);
        }
    }

    return ret;
}

int lc_xen_reconfigure_vm_by_template(host_t *host, vm_info_t *vm_info)
{
    lc_xen_reconfigure_vm_cpu(host, vm_info, vm_info->vcpu_num);
    lc_xen_reconfigure_vm_memory(host, vm_info, vm_info->mem_size);

    return 0;
}

int lc_check_host_device(host_t *phost)
{
    int ret = 1;
    xen_session *session = NULL;

    LC_HOST_CONNECT(phost, session);
    if (session) {
        ret = 0;
    }
    LC_HOST_FREE(phost, session);

    return ret;
}


int lc_vm_snapshot_uninstall(host_t *phost, vm_snapshot_info_t *snapshot_info)
{
    int  ret = 0;
    return ret;
}

int lc_vm_snapshot_take(vm_info_t *vm_info, vm_snapshot_info_t *snapshot_info)
{
    int  ret = 0;
    return ret;
}

int lc_vm_snapshot_revert(host_t *phost, vm_snapshot_info_t *snapshot_info)
{
    int  ret = 0;
    return ret;
}

int lc_vm_backup_create_script(char *vm_label, host_t *phost, host_t *peerhost)
{
    char exec_cmd[LC_VM_CMD_SIZE];

    memset(exec_cmd, 0, LC_VM_CMD_SIZE);
    sprintf(exec_cmd, LC_BACKUP_CREATE_STR,
            vm_label, phost->host_address, phost->username, phost->password,
            phost->sr_uuid, peerhost->host_address, peerhost->username,
            peerhost->password, peerhost->sr_uuid);
    return system(exec_cmd);
}

int lc_vm_withsp_backup_create_script(char *vm_label, char *sp_label, host_t *phost, host_t *peerhost)
{
    char exec_cmd[LC_VM_CMD_SIZE];

    memset(exec_cmd, 0, LC_VM_CMD_SIZE);
    sprintf(exec_cmd, LC_BACKUP_CREATE_STR1,
            vm_label, sp_label,
            phost->host_address, phost->username, phost->password,
            phost->sr_uuid, peerhost->host_address, peerhost->username,
            peerhost->password, peerhost->sr_uuid);
    return system(exec_cmd);
}

int lc_vm_backup_delete_script(vm_backup_info_t *backup_info)
{
    char exec_cmd[LC_VM_CMD_SIZE];

    memset(exec_cmd, 0, LC_VM_CMD_SIZE);
    sprintf(exec_cmd, LC_BACKUP_DELETE_STR,
            backup_info->vm_label, backup_info->peer_server,
            backup_info->peer_username, backup_info->peer_passwd,
            backup_info->peer_sr_uuid);
    return system(exec_cmd);
}

int lc_vm_backup_restore_script(vm_info_t *vm_info)
{
    char    exec_cmd[LC_VM_CMD_SIZE];
    host_t *phost = NULL;
    agent_vm_id_t vm_id;

    if ((NULL == vm_info) || (NULL == vm_info->hostp)) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        return -1;
    }
    phost = vm_info->hostp;

    memset(exec_cmd, 0, LC_VM_CMD_SIZE);
    sprintf(exec_cmd, LC_BACKUP_RESTORE_STR,
            vm_info->vm_label, phost->host_address, phost->username,
            phost->password, phost->sr_uuid);
    if (0 != system(exec_cmd)) {
        VMDRIVER_LOG(LOG_ERR, "system failed\n");
        return -1;
    }

    /* add monitor */
    memset(&vm_id, 0, sizeof(vm_id));
//    strcpy(vm_id.vm_uuid, vm_info->vm_uuid);
//    strcpy(vm_id.vdi_sys_uuid, vm_info->vdi_sys_uuid);
//    strcpy(vm_id.vdi_user_uuid, vm_info->vdi_user_uuid);
//    lc_agent_add_vm_monitor(&vm_id, 1, phost->host_address);

    return 0;
}

int lc_xen_migrate_vm_with_snapshot(char *vm_uuid, host_t *phost, host_t *peerhost)
{
    char cmd[LC_VM_CMD_SIZE];

    /*
     * python vm_migrate.py <local-ip> <local-password> <remote-ip> <remote-password>
     * <vm-uuid>
     */
    memset(cmd, 0, LC_VM_CMD_SIZE);
    snprintf(cmd, LC_VM_CMD_SIZE, LC_XEN_VM_MIGRATE_S_STR,
            phost->host_address, phost->password,
            peerhost->host_address, peerhost->password,
            vm_uuid);
    VMDRIVER_LOG(LOG_INFO, "VM %s Migrating: Remote: %s (UUID: %s) Remote-SR: %s",
                     vm_uuid, peerhost->host_address, peerhost->host_uuid,
                     peerhost->sr_uuid);
    return lc_call_system(cmd, "Migrating", "vm", vm_uuid, lc_vmdriver_db_log);
}


int lc_xen_migrate_vm(char *vm_label, host_t *phost, host_t *peerhost)
{
    char cmd[LC_VM_CMD_SIZE];

    /*
     * xe vm-migrate remote-master=<ip> remote-username=<username> remote-password=<>
     * host=<host-uuid> vm=<vm-name-label> destination-sr-uuid=<sr-uuid>
     */
    memset(cmd, 0, LC_VM_CMD_SIZE);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vm-migrate "
            "remote-master=%s remote-username=%s remote-password=%s "
            "host=%s vm=%s destination-sr-uuid=%s",
            phost->host_address, phost->username, phost->password,
            peerhost->host_address, peerhost->username, peerhost->password,
            peerhost->host_uuid, vm_label, peerhost->sr_uuid);
    VMDRIVER_LOG(LOG_INFO, "VM %s Migrating: Remote: %s (UUID: %s) Remote-SR: %s",
                     vm_label, peerhost->host_address, peerhost->host_uuid,
                     peerhost->sr_uuid);
    return lc_call_system(cmd, "Migrating", "vm", vm_label, lc_vmdriver_db_log);
}

int lc_xen_check_vm_migrate(char *vm_label, host_t *phost, host_t *peerhost)
{
    int ret = 0;
    char cmd[LC_VM_CMD_SIZE];
    char vmuuid[MAX_VM_UUID_LEN];

    memset(cmd , 0, LC_VM_CMD_SIZE);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vm-list name-label=%s "
            "params=uuid --minimal",
            phost->host_address, phost->username, phost->password,
            vm_label);
    memset(vmuuid, 0, sizeof(vmuuid));
    if (get_param(vmuuid, sizeof(vmuuid), cmd) == 0) {
        VMDRIVER_LOG(LOG_ERR, "vm %s exists in host %s, checking info\n",
                    vm_label, phost->host_address);
        ret = -1;
        goto error;
    }

    memset(cmd , 0, LC_VM_CMD_SIZE);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vm-list name-label=%s "
            "params=uuid --minimal",
            peerhost->host_address, peerhost->username, peerhost->password,
            vm_label);
    memset(vmuuid, 0, sizeof(vmuuid));
    if (get_param(vmuuid, sizeof(vmuuid), cmd) == 0) {
        VMDRIVER_LOG(LOG_DEBUG, "vm %s exists in host %s\n",
                     vm_label, peerhost->host_address);
    } else {
        VMDRIVER_LOG(LOG_ERR, "vm %s not exists in host %s, checking info\n",
                     vm_label, peerhost->host_address);
        ret = -1;
        goto error;
    }

error:
    return ret;
}

int lc_xen_get_vdi_uuid(vm_info_t *vm_info)
{
    int ret = 0;
    char cmd[LC_VM_CMD_SIZE];
    host_t      *phost   = NULL;
    FILE        *stream  = NULL;

    if ((NULL == vm_info) || (NULL == vm_info->hostp)) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }
    phost = vm_info->hostp;

    /* Get vdi_sys_uuid
     * xe vbd-list vm=<vm uuid> params=vdi-uuid userdevice=0
     */
    memset(cmd, 0, LC_VM_CMD_SIZE);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vbd-list "
            "vm-uuid=%s params=vdi-uuid userdevice=0 --minimal",
            phost->host_address, phost->username, phost->password,
            vm_info->vm_uuid);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "stream is NULL.\n");
        ret = -1;
        goto error;
    }
    fscanf(stream, "%s", vm_info->vdi_sys_uuid);
    pclose(stream);

    /* Get vdi_user_uuid
     * xe vbd-list vm=<vm uuid> params=vdi-uuid userdevice=1
     */
    if (vm_info->vdi_user_size > 0) {
        memset(cmd, 0, LC_VM_CMD_SIZE);
        sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s vbd-list "
                "vm-uuid=%s params=vdi-uuid userdevice=1 --minimal",
                phost->host_address, phost->username, phost->password,
                vm_info->vm_uuid);
        stream = popen(cmd, "r");
        if (NULL == stream) {
            VMDRIVER_LOG(LOG_ERR, "stream is NULL.\n");
            ret = -1;
            goto error;
        }
        fscanf(stream, "%s", vm_info->vdi_user_uuid);
        pclose(stream);
    }
error:
    return ret;
}

int lc_xen_get_host_uuid(host_t *host)
{
    int   ret = 0;
    char  cmd[LC_VM_CMD_SIZE];
    FILE *stream  = NULL;

    if ((NULL == host) || (NULL == host->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    /* Get host_uuid
     * xe host-list params=uuid
     */
    memset(cmd, 0, LC_VM_CMD_SIZE);
    sprintf(cmd, "xe -s %s -p 443 -u %s -pw %s host-list params=uuid --minimal",
            host->host_address, host->username, host->password);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "stream is NULL.\n");
        ret = -1;
        goto error;
    }
    fscanf(stream, "%s", host->host_uuid);
    pclose(stream);

error:
    return ret;
}

int lc_nsp_get_host_name(host_t *host)
{
    int   ret = 0;
    char  cmd[LC_EXEC_CMD_LEN];
    FILE *stream  = NULL;

    if ((NULL == host) || (NULL == host->host_address)) {
        VMDRIVER_LOG(LOG_ERR, "Input Para is NULL\n");
        ret = -1;
        goto error;
    }

    /* Get nsp name
     */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "sshpass -p %s ssh %s@%s hostname",
            host->password, host->username, host->host_address);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "host_name stream is NULL.\n");
        ret = -1;
        goto error;
    }
    fscanf(stream, "%s", host->host_name);
    if (0 == strlen(host->host_name)) {
        VMDRIVER_LOG(LOG_ERR, "host_name is empty.\n");
        ret = -1;
        goto error;
    }
    pclose(stream);

    /* Get cpu_info
     */
    memset(cmd, 0, LC_EXEC_CMD_LEN);
    sprintf(cmd, "sshpass -p %s ssh %s@%s lscpu -e=cpu | grep -v CPU | wc -l",
            host->password, host->username, host->host_address);
    stream = popen(cmd, "r");
    if (NULL == stream) {
        VMDRIVER_LOG(LOG_ERR, "cpu_info stream is NULL.\n");
        ret = -1;
        goto error;
    }
    fscanf(stream, "%s", host->cpu_info);
    strcat(host->cpu_info,
        ",Contact XiangYang if you see this on the website :)");
    if (0 == strlen(host->cpu_info)) {
        VMDRIVER_LOG(LOG_ERR, "cpu_info is empty.\n");
        ret = -1;
        goto error;
    }
    pclose(stream);

error:
    return ret;
}
