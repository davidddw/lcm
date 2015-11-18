#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "vm/vm_host.h"
#include "mysql.h"
#include "lc_db.h"

extern MYSQL *conn_handle;

static int get_email_address(void *email, char *field, char *value)
{
    char* email_address= (char *) email;
    if (strcmp(field, "email") == 0) {
        strcpy(email_address, value);
    }
    return 0;
}

static int vedge_process(void *vm_info, char *field, char *value)
{
    vm_info_t *pvm = (vm_info_t *)vm_info;

    if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pvm->vm_name, value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "state") == 0) {
        pvm->vm_state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        pvm->vm_error = atoi(value);
    } else if (strcmp(field, "vl2_num") == 0) {
        pvm->intf_num = atoi(value);
    } else if (strcmp(field, "gw_poolid") == 0) {
        pvm->pool_id = atoi(value);
    } else if (strcmp(field, "gw_launch_server") == 0) {
        strcpy(pvm->host_address, value);
    } else if (strcmp(field, "gwtemplate") == 0) {
        strcpy(pvm->vm_template, value);
    } else if (strcmp(field, "gwimport") == 0) {
        strcpy(pvm->vm_import, value);
    } else if (strcmp(field, "flag") == 0) {
        pvm->vm_flags = atoi(value);
    } else if (strcmp(field, "opaque_id") == 0) {
        strcpy(pvm->vm_opaque_id, value);
    } else if (strcmp(field, "vdi_sys_sr_name") == 0) {
        strcpy(pvm->vdi_sys_sr_name, value);
    } else if (strcmp(field, "tport") == 0) {
        pvm->tport = atoi(value);
    } else if (strcmp(field, "lport") == 0) {
        pvm->lport = atoi(value);
    } else if (strcmp(field, "public_if1id") == 0) {
        pvm->pub_vif[0].if_id = atoi(value);
    } else if (strcmp(field, "public_if2id") == 0) {
        pvm->pub_vif[1].if_id = atoi(value);
    } else if (strcmp(field, "public_if3id") == 0) {
        pvm->pub_vif[2].if_id = atoi(value);
    } else if (strcmp(field, "private_if1id") == 0) {
        pvm->pvt_vif[0].if_id = atoi(value);
    } else if (strcmp(field, "private_if2id") == 0) {
        pvm->pvt_vif[1].if_id = atoi(value);
    } else if (strcmp(field, "private_if3id") == 0) {
        pvm->pvt_vif[2].if_id = atoi(value);
    } else if (strcmp(field, "ctrl_ifid") == 0) {
        pvm->ctrl_vif.if_id   = atoi(value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(pvm->vm_uuid, value);
    } else if (strcmp(field, "action") == 0) {
        pvm->action = atoi(value);
    } else if (strcmp(field, "cpu_start_time") == 0) {
        pvm->cpu_start_time = atol(value);
    } else if (strcmp(field, "disk_start_time") == 0) {
        pvm->disk_start_time = atol(value);
    } else if (strcmp(field, "check_start_time") == 0) {
        pvm->check_start_time = atol(value);
    } else if (strcmp(field, "userid") == 0) {
        pvm->user_id = atoi(value);
    } else if (strcmp(field, "vdcid") == 0) {
        pvm->vnet_id = atoi(value);
    } else if (strcmp(field, "havrid") == 0) {
        pvm->vrid = atoi(value);
    } else if (strcmp(field, "haflag") == 0) {
        pvm->ha_flag = atoi(value);
    } else if (strcmp(field, "hastate") == 0) {
        pvm->ha_state = atoi(value);
    } else if (strcmp(field, "haswitchmode") == 0) {
        pvm->ha_switchmode = atoi(value);
    }

    return 0;
}

static int vgw_ha_process(void *vgw_info, char *field, char *value)
{
    vgw_ha_query_info_t *pvgw = (vgw_ha_query_info_t *)vgw_info;

    if (strcmp(field, "ip") == 0) {
        strcpy(pvgw->ctl_ip, value);
    } else if (strcmp(field, "id") == 0) {
        pvgw->vnet_id = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvgw->label, value);
    } else if (strcmp(field, "hastate") == 0) {
        pvgw->ha_state = atoi( value);
    }

    return 0;
}

static int bk_vm_health_process(void *lb_info, char *field, char *value)
{
    lb_query_info_t *plb = (lb_query_info_t *)lb_info;

    if (strcmp(field, "ip") == 0) {
        strcpy(plb->ctl_ip, value);
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(plb->lcuuid, value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(plb->label, value);
    } else if (strcmp(field, "launch_server") == 0) {
        strcpy(plb->server, value);
    }

    return 0;
}

static int vedge_passwd_process(void *vm_passwd_info, char *field, char *value)
{
    char *pvm = (char *)vm_passwd_info;
    strcpy(pvm, value);
    return 0;
}

static int vm_process(void *vm_info, char *field, char *value)
{
    vm_info_t* pvm = (vm_info_t*) vm_info;

    if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "poolid") == 0) {
        pvm->pool_id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pvm->vm_name, value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(pvm->vm_uuid, value);
    } else if (strcmp(field, "state") == 0) {
        pvm->vm_state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        pvm->vm_error = atoi(value);
    } else if (strcmp(field, "os") == 0) {
        strcpy(pvm->vm_template, value);
    } else if (strcmp(field, "flag") == 0) {
        pvm->vm_flags = atoi(value);
    } else if (strcmp(field, "action") == 0) {
        pvm->action = atoi(value);
    //} else if (strcmp(field, "template") == 0) {
    //    strcpy(pvm->vm_template, value);
    } else if (strcmp(field, "import") == 0) {
        strcpy(pvm->vm_import, value);
    } else if (strcmp(field, "launch_server") == 0) {
        strcpy(pvm->host_address, value);
    } else if (strcmp(field, "vl2id") == 0) {
        pvm->vl2_id = atoi(value);
    } else if (strcmp(field, "vnetid") == 0) {
        pvm->vnet_id = atoi(value);
    } else if (strcmp(field, "opaque_id") == 0) {
        strcpy(pvm->vm_opaque_id, value);
    } else if (strcmp(field, "vdi_sys_uuid") == 0) {
        strcpy(pvm->vdi_sys_uuid, value);
    } else if (strcmp(field, "vdi_sys_sr_uuid") == 0) {
        strcpy(pvm->vdi_sys_sr_uuid, value);
    } else if (strcmp(field, "vdi_sys_sr_name") == 0) {
        strcpy(pvm->vdi_sys_sr_name, value);
    } else if (strcmp(field, "vdi_sys_size") == 0) {
        pvm->vdi_sys_size = atoi(value);
    } else if (strcmp(field, "vdi_user_uuid") == 0) {
        strcpy(pvm->vdi_user_uuid, value);
    } else if (strcmp(field, "vdi_user_sr_uuid") == 0) {
        strcpy(pvm->vdi_user_sr_uuid, value);
    } else if (strcmp(field, "vdi_user_sr_name") == 0) {
        strcpy(pvm->vdi_user_sr_name, value);
    } else if (strcmp(field, "vdi_user_size") == 0) {
        pvm->vdi_user_size = atoi(value);
    } else if (strcmp(field, "tport") == 0) {
        pvm->tport = atoi(value);
    } else if (strcmp(field, "lport") == 0) {
        pvm->lport = atoi(value);
    } else if (strcmp(field, "mem_size") == 0) {
        pvm->mem_size = atoi(value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        pvm->vcpu_num = atoi(value);
    } else if (strcmp(field, "cpu_start_time") == 0) {
        pvm->cpu_start_time = atol(value);
    } else if (strcmp(field, "disk_start_time") == 0) {
        pvm->disk_start_time = atol(value);
    } else if (strcmp(field, "traffic_start_time") == 0) {
        pvm->traffic_start_time = atol(value);
    } else if (strcmp(field, "check_start_time") == 0) {
        pvm->check_start_time = atol(value);
    } else if (strcmp(field, "userid") == 0) {
        pvm->user_id = atoi(value);
    }

    return 0;
}

static int vm_query_process(void *vm_info, char *field, char *value)
{
    vm_query_info_t* pvm = (vm_query_info_t*) vm_info;

    if (strcmp(field, "uuid") == 0) {
        strcpy(pvm->vm_uuid, value);
    } else if (strcmp(field, "vdi_sys_uuid") == 0) {
        strcpy(pvm->vdi_sys_uuid, value);
    } else if (strcmp(field, "vdi_user_uuid") == 0) {
        strcpy(pvm->vdi_user_uuid, value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        pvm->cpu_number = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pvm->pw_state = atoi(value);
    } else if (strcmp(field, "vdi_sys_size") == 0) {
        pvm->sys_disk = atoi(value);
    } else if (strcmp(field, "vdi_user_size") == 0) {
        pvm->user_disk = atoi(value);
    } else if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "mem_size") == 0) {
        pvm->mem_total = atoi(value);
    } else if (strcmp(field, "os") == 0) {
        strcpy(pvm->vm_template, value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    }

    return 0;
}

static int vnet_query_process(void *vm_info, char *field, char *value)
{
    vnet_query_info_t* pvm = (vnet_query_info_t*) vm_info;

    /* TODO: Add public and private interfaces for vnet */
    if (strcmp(field, "uuid") == 0) {
        strcpy(pvm->vm_uuid, value);
    } else if (strcmp(field, "private_if1id") == 0) {
        pvm->pvt_ifid[0] = atoi(value);
    } else if (strcmp(field, "private_if2id") == 0) {
        pvm->pvt_ifid[1] = atoi(value);
    } else if (strcmp(field, "private_if3id") == 0) {
        pvm->pvt_ifid[2] = atoi(value);
    } else if (strcmp(field, "public_if1id") == 0) {
        pvm->pub_ifid[0] = atoi(value);
    } else if (strcmp(field, "public_if2id") == 0) {
        pvm->pub_ifid[1] = atoi(value);
    } else if (strcmp(field, "public_if3id") == 0) {
        pvm->pub_ifid[2] = atoi(value);
    } else if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "gwtemplate") == 0) {
        strcpy(pvm->vm_template, value);
    } else if (strcmp(field, "ctrl_ifid") == 0) {
        pvm->ctrl_ifid   = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pvm->pw_state = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    }

    return 0;
}

static int vif_process(void *intf_info, char *field, char *value)
{
    intf_t* pintf = (intf_t*) intf_info;
    struct in_addr mask;

    if (strcmp(field, "id") == 0) {
        pintf->if_id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pintf->if_name, value);
    } else if (strcmp(field, "ifindex") == 0) {
        pintf->if_index = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pintf->if_state = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(pintf->if_ip, value);
    } else if (strcmp(field, "netmask") == 0) {
        mask.s_addr = htonl(0xFFFFFFFF << (32 - atoi(value)));
        inet_ntop(AF_INET, &mask, pintf->if_netmask, MAX_VM_ADDRESS_LEN);
        pintf->if_mask = atoi(value);
    } else if (strcmp(field, "gateway") == 0) {
        strcpy(pintf->if_gateway, value);
    } else if (strcmp(field, "mac") == 0) {
        strcpy(pintf->if_mac, value);
    } else if (strcmp(field, "bandw") == 0) {
        pintf->if_bandw = atoi(value);
    } else if (strcmp(field, "ofport") == 0) {
        pintf->if_port = atoi(value);
    } else if (strcmp(field, "subnetid") == 0) {
        pintf->if_subnetid = atoi(value);
    } else if (strcmp(field, "vlantag") == 0) {
        pintf->if_vlan = atoi(value);
    } else if (strcmp(field, "devicetype") == 0) {
        pintf->if_devicetype = atoi(value);
    } else if (strcmp(field, "deviceid") == 0) {
        pintf->if_deviceid = atoi(value);
    } else if (strcmp(field, "port_id") == 0) {
        strcpy(pintf->if_portid, value);
    }

    return 0;
}

static int vm_snapshot_process(void *vm_snapshot_info, char *field, char *value)
{
    vm_snapshot_info_t *pvm = (vm_snapshot_info_t *)vm_snapshot_info;

    if (strcmp(field, "id") == 0) {
        pvm->snapshot_id = atoi(value);
    } else if (strcmp(field, "vm_id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pvm->snapshot_name, value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->snapshot_label, value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(pvm->snapshot_uuid, value);
    } else if (strcmp(field, "state") == 0) {
        pvm->state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        pvm->error = atoi(value);
    }

    return 0;
}

static int vm_backup_process(void *vm_backup_info, char *field, char *value)
{
    vm_backup_info_t *pvm = (vm_backup_info_t *)vm_backup_info;

    if (strcmp(field, "id") == 0) {
        pvm->dbr_id = atoi(value);
    } else if (strcmp(field, "vm_id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "vm_label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "state") == 0) {
        pvm->state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        pvm->error = atoi(value);
    } else if (strcmp(field, "resident_server") == 0) {
        strcpy(pvm->resident_server, value);
    } else if (strcmp(field, "resident_username") == 0) {
        strcpy(pvm->resident_username, value);
    } else if (strcmp(field, "resident_passwd") == 0) {
        strcpy(pvm->resident_passwd, value);
    } else if (strcmp(field, "peer_server") == 0) {
        strcpy(pvm->peer_server, value);
    } else if (strcmp(field, "peer_username") == 0) {
        strcpy(pvm->peer_username, value);
    } else if (strcmp(field, "peer_passwd") == 0) {
        strcpy(pvm->peer_passwd, value);
    } else if (strcmp(field, "local_sr_uuid") == 0) {
        strcpy(pvm->local_sr_uuid, value);
    } else if (strcmp(field, "peer_sr_uuid") == 0) {
        strcpy(pvm->peer_sr_uuid, value);
    } else if (strcmp(field, "sys_disk_usage") == 0) {
        pvm->sys_disk = atoi(value);
    } else if (strcmp(field, "usr_disk_usage") == 0) {
        pvm->usr_disk = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        pvm->backup_flags = atoi(value);
    } else if (strcmp(field, "local_vdi_sys_uuid") == 0) {
        strcpy(pvm->local_vdi_sys_uuid, value);
    } else if (strcmp(field, "local_vdi_user_uuid") == 0) {
        strcpy(pvm->local_vdi_user_uuid, value);
    } else if (strcmp(field, "peer_vdi_sys_uuid") == 0) {
        strcpy(pvm->peer_vdi_sys_uuid, value);
    } else if (strcmp(field, "peer_vdi_user_uuid") == 0) {
        strcpy(pvm->peer_vdi_user_uuid, value);
    }

    return 0;
}

static int disk_process(void *disk_info, char *field, char *value)
{
    disk_t *pdisk = (disk_t *)disk_info;

    if (strcmp(field, "id") == 0) {
        pdisk->id = atoi(value);
    } else if (strcmp(field, "vdi_uuid") == 0) {
        strcpy(pdisk->vdi_uuid, value);
    } else if (strcmp(field, "vdi_name") == 0) {
        strcpy(pdisk->vdi_name, value);
    } else if (strcmp(field, "vdi_sr_name") == 0) {
        strcpy(pdisk->vdi_sr_name, value);
    } else if (strcmp(field, "vdi_sr_uuid") == 0) {
        strcpy(pdisk->vdi_sr_uuid, value);
    } else if (strcmp(field, "size") == 0) {
        pdisk->size = atoi(value);
    } else if (strcmp(field, "vmid") == 0) {
        pdisk->vmid = atoi(value);
    } else if (strcmp(field, "userdevice") == 0) {
        pdisk->userdevice = atoi(value);
    }

    return 0;
}

static int third_device_process(void *device_info, char *field, char *value)
{
    third_device_t *pdevice = (third_device_t *)device_info;

    if (strcmp(field, "id") == 0) {
        pdevice->id = atoi(value);
    } else if (strcmp(field, "vm_id") == 0) {
        pdevice->vm_id = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        pdevice->type = atoi(value);
    } else if (strcmp(field, "data1_mac") == 0) {
        strcpy(pdevice->data_mac[0], value);
    } else if (strcmp(field, "data2_mac") == 0) {
        strcpy(pdevice->data_mac[1], value);
    } else if (strcmp(field, "data3_mac") == 0) {
        strcpy(pdevice->data_mac[2], value);
    }

    return 0;
}

static int vmwaf_query_process(void *info, char *field, char *value)
{
    vmwaf_query_info_vm_t *p = (vmwaf_query_info_vm_t *)info;

    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        p->state = atoi(value);
    } else if (strcmp(field, "pre_state") == 0) {
        p->pre_state = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        p->flag = atoi(value);
    } else if (strcmp(field, "eventid") == 0) {
        p->eventid = atoi(value);
    } else if (strcmp(field, "errorno") == 0) {
        p->errorno = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strncpy(p->label, value, MAX_VM_UUID_LEN);
    } else if (strcmp(field, "description") == 0) {
        strncpy(p->description, value, LC_NAMEDES);
    } else if (strcmp(field, "userid") == 0) {
        p->userid = atoi(value);
    } else if (strcmp(field, "ctrl_ifid") == 0) {
        p->ctrl_ifid = atoi(value);
    } else if (strcmp(field, "wan_if1id") == 0) {
        p->wan_if1id = atoi(value);
    } else if (strcmp(field, "lan_if1id") == 0) {
        p->lan_if1id = atoi(value);
    } else if (strcmp(field, "launch_server") == 0) {
        strncpy(p->launch_server, value, MAX_HOST_ADDRESS_LEN);
    } else if (strcmp(field, "mem_size") == 0) {
        p->mem_size = atoi(value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        p->vcpu_num = atoi(value);
    } else if (strcmp(field, "domain") == 0) {
        strncpy(p->domain, value, LC_DOMAIN_LEN);
    }

    return 0;
}

int lb_listener_process(void *listener_info, char *field, char *value)
{
    lb_listener_t* plistener = (lb_listener_t*) listener_info;

    if (strcmp(field, "id") == 0) {
        plistener->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(plistener->name, value);
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(plistener->lcuuid, value);
    } else if (strcmp(field, "lb_lcuuid") == 0) {
        strcpy(plistener->lb_lcuuid, value);
    }

    return 0;
}

int lb_bk_vm_process(void *bk_vm_info, char *field, char *value)
{
    lb_bk_vm_t* pbk_vm = (lb_bk_vm_t*)bk_vm_info;

    if (strcmp(field, "id") == 0) {
        pbk_vm->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pbk_vm->name, value);
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(pbk_vm->lcuuid, value);
    } else if (strcmp(field, "lb_listener_lcuuid") == 0) {
        strcpy(pbk_vm->lb_listener_lcuuid, value);
    } else if (strcmp(field, "lb_lcuuid") == 0) {
        strcpy(pbk_vm->lb_lcuuid, value);
    } else if (strcmp(field, "health_state") == 0) {
        pbk_vm->health_state = atoi(value);
    }

    return 0;
}

int lc_db_disk_load(disk_t* disk_info, char *column_list, char *condition)
{
    return lc_db_table_load(disk_info, "vdisk", column_list, condition, disk_process);
}

int lc_db_disk_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vdisk", column, value, condition);
}

int lc_db_disk_delete(char *condition)
{
    return lc_db_table_delete("vdisk", condition);
}

int lc_db_vedge_load(vm_info_t* vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_info, "vnet", column_list, condition, vedge_process);
}

int lc_db_vedge_passwd_load(char* vm_passwd_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_passwd_info, "vdc", column_list, condition, vedge_passwd_process);
}

int lc_db_vm_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vm", column_list, value_list);
}

int lc_db_vm_delete(char *condition)
{
    return lc_db_table_delete("vm", condition);
}

int lc_db_vm_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vm", column, value, condition);
}

int lc_db_master_vm_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("vm", column, value, condition);
}

int lc_db_vm_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vm", columns_update, condition);
}

int lc_db_master_vm_multi_update(char *columns_update, char *condition)
{
    return lc_db_master_table_multi_update("vm", columns_update, condition);
}

int lc_db_vedge_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vnet", columns_update, condition);
}

int lc_db_master_vedge_multi_update(char *columns_update, char *condition)
{
    return lc_db_master_table_multi_update("vnet", columns_update, condition);
}

int lc_db_vm_dump(char *column_list)
{
    return lc_db_table_dump("vm", column_list);
}

int lc_db_vm_load(vm_info_t* vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_info, "vm", column_list, condition, vm_process);
}

int lc_db_vedge_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vnet", column, value, condition);
}

int lc_db_master_vedge_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("vnet", column, value, condition);
}

int lc_db_vedge_delete(char *condition)
{
    return lc_db_table_delete("vnet", condition);
}

int lc_db_vif_load(intf_t* intf_info, char *column_list, char *condition)
{
    return lc_db_table_load(intf_info, "vinterface", column_list, condition, vif_process);
}

int lc_db_vif_loadn(intf_t *intf_info, int max_count,
                    char *column_list, char *condition)
{
    return lc_db_table_loadn_order_by_id(intf_info, sizeof(intf_t), max_count,
                                         "vinterface", column_list, condition,
                                         vif_process);
}

int lc_db_vif_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vinterface", column, value, condition);
}

int lc_db_vm_snapshot_load(vm_snapshot_info_t *vm_snapshot_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_snapshot_info, "vm_snapshot", column_list, condition, vm_snapshot_process);
}

int lc_db_vm_snapshot_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vm_snapshot", column, value, condition);
}

int lc_db_vm_snapshot_delete(char *condition)
{
    return lc_db_table_delete("vm_snapshot", condition);
}

int lc_db_vm_backup_load(vm_backup_info_t *vm_backup_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_backup_info, "vm_backup", column_list, condition, vm_backup_process);
}

int lc_db_vm_backup_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vm_backup", column, value, condition);
}

int lc_db_vm_backup_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vm_backup", columns_update, condition);
}

int lc_db_vm_backup_delete(char *condition)
{
    return lc_db_table_delete("vm_backup", condition);
}

int lc_db_sysconf_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("sys_configuration", column, value, condition);
}

int lc_db_third_device_load(third_device_t *device, char *column_list, char *condition)
{
    return lc_db_table_load(device, "third_party_device", column_list, condition, third_device_process);
}
int lc_db_master_vmwaf_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("vmwaf", column, value, condition);
}

int lc_db_lb_listener_load(lb_listener_t* listener_info, char *column_list, char *condition)
{
    return lc_db_table_load(listener_info, "lb_listener", column_list, condition,
                            lb_listener_process);
}

int lc_db_lb_bk_vm_load(lb_bk_vm_t* bk_vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(bk_vm_info, "lb_bk_vm", column_list, condition,
                            lb_bk_vm_process);
}

int lc_vmdb_vedge_load(vm_info_t *vm_info, uint32_t vnet_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);

    result = lc_db_vedge_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VEDGE_NOT_FOUND;
    } else {
        vm_info->vm_type = LC_VM_TYPE_GW;
        return LC_DB_VEDGE_FOUND;
    }

    return 0;
}

int lc_vmdb_vedge_passwd_load(char *vm_passwd_info, uint32_t vnet_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];
    char column_list[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);

    memset(column_list, 0, LC_DB_BUF_LEN);
    sprintf(column_list, "AES_DECRYPT(UNHEX(passwd),%d)", vnet_id);

    result = lc_db_vedge_passwd_load(vm_passwd_info, column_list, condition);
    if (result == LC_DB_OK) {
        return LC_DB_VEDGE_FOUND;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VEDGE_NOT_FOUND;
    } else {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    }

    return 0;
}

int lc_vmdb_vedge_load_by_import_namelable(vm_info_t *vm_info,char *importvm_lable)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", importvm_lable);

    result = lc_db_vedge_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VEDGE_NOT_FOUND;
    } else {
        vm_info->vm_type = LC_VM_TYPE_GW;
        return LC_DB_VEDGE_FOUND;
    }
}

int lc_vmdb_vm_update_ip (char *vm_ip, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", vm_ip);

    if (lc_db_vm_update("ip", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_label (char *vm_label, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", vm_label);

    if (lc_db_vm_update("label", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_uuid (char *uuid, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_vm_update("uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_vdi_size (uint64_t size, char *field, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%"PRIu64"'", size);

    if (lc_db_vm_update(field, buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_vdi_info (char *vdi_info, char *vdi_info_field,
                                uint32_t vm)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm);
    sprintf(buffer, "'%s'", vdi_info);

    if (lc_db_vm_update(vdi_info_field, buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vm_update_disk_info(vm_info_t *vm_info, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "vdi_sys_sr_name='%s',vdi_sys_size=%"PRId64",vdi_user_sr_name='%s',vdi_user_size=%"PRId64, vm_info->vdi_sys_sr_name, vm_info->vdi_sys_size, vm_info->vdi_user_sr_name, vm_info->vdi_user_size);

    if (lc_db_vm_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_cpu(int cpu, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", cpu);

    if (lc_db_vm_update("vcpu_num", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_memory(int memory, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", memory);

    if (lc_db_vm_update("mem_size", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_flag (vm_info_t *vm_info, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char bufex[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(bufex, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "flag='%d'", vm_info->vm_flags);

    if (lc_db_vm_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_flag_by_label(vm_info_t *vm_info, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char bufex[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(bufex, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "flag='%d'", vm_info->vm_flags);

    if (lc_db_vm_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vm_update_time (vm_info_t *vm_info)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char bufex[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(bufex, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_info->vm_id);
    sprintf(buffer, "cpu_start_time='%ld', disk_start_time='%ld', check_start_time='%ld'",
                     vm_info->cpu_start_time, vm_info->disk_start_time, vm_info->check_start_time);

    if (lc_db_master_vm_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_time (vm_info_t *vm_info)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char bufex[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(bufex, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_info->vm_id);
    sprintf(buffer, "cpu_start_time='%ld', disk_start_time='%ld', check_start_time='%ld'",
                     vm_info->cpu_start_time, vm_info->disk_start_time, vm_info->check_start_time);

    if (lc_db_vedge_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vedge_update_time (vm_info_t *vm_info)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char bufex[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    memset(bufex, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_info->vm_id);
    sprintf(buffer, "cpu_start_time='%ld', disk_start_time='%ld', check_start_time='%ld'",
                     vm_info->cpu_start_time, vm_info->disk_start_time, vm_info->check_start_time);

    if (lc_db_master_vedge_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n", __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_state (int state, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vm_update_state (int state, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_master_vm_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_state_by_label(int state, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_action (int action, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", action);

    if (lc_db_vm_update("action", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vm_update_action (int action, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", action);

    if (lc_db_master_vm_update("action", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_action (int action, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", action);

    if (lc_db_vedge_update("action", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vnet_update_action (int action, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", action);

    if (lc_db_master_vedge_update("action", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_errno(int err_no, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", err_no);

    if (lc_db_vm_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vm_update_errno(int err_no, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", err_no);

    if (lc_db_master_vm_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_errno_by_label(int err_no, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "'%d'", err_no);

    if (lc_db_vm_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_tport (int tport, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", tport);

    if (lc_db_vm_update("tport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_lport (int lport, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", lport);

    if (lc_db_vm_update("lport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_opaque_id(char *opaque_id, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", opaque_id);

    if (lc_db_vm_update("opaque_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_opaque_id_by_label(char *opaque_id, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "'%s'", opaque_id);

    if (lc_db_vm_update("opaque_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_thumbprint_by_label(char *thumbprint, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "'%s'", thumbprint);

    if (lc_db_vm_update("thumbprint", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_vm_path_by_label(char *vm_path, char *vm_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_label);
    sprintf(buffer, "'%s'", vm_path);

    if (lc_db_vm_update("vm_path", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_launchserver(char *launch_server, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", launch_server);

    if (lc_db_vm_update("launch_server", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_sr_name(char *sr_name, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", sr_name);

    if (lc_db_vm_update("vdi_sys_sr_name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_sys_sr_uuid(char *sr_name, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", sr_name);

    if (lc_db_vm_update("vdi_sys_sr_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_user_sr_uuid(char *sr_name, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%s'", sr_name);

    if (lc_db_vm_update("vdi_user_sr_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_load (vm_info_t *vm_info, uint32_t vm_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);

    result = lc_db_vm_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        vm_info->vm_type = LC_VM_TYPE_VM;
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vm_load_by_import_namelable(vm_info_t *vm_info,char *importvm_lable)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", importvm_lable);

    result = lc_db_vm_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        vm_info->vm_type = LC_VM_TYPE_VM;
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vm_load_by_namelable(vm_info_t *vm_info,char *vm_lable)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_lable);

    result = lc_db_vm_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vm_load_by_uuid(vm_info_t * vm_info, char *vm_uuid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "uuid='%s'", vm_uuid);

    result = lc_db_vm_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vedge_load_by_namelable(vm_info_t * vm_info,char *vm_lable)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", vm_lable);

    result = lc_db_vedge_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vedge_load_by_ha(vm_info_t * vm_info, int vdc_id, int vnet_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vdcid=%d and id<>%d", vdc_id, vnet_id);

    result = lc_db_vedge_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_check_vm_exist (int vnet_id, int vl2_id, int vm_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);

    result = lc_db_vm_load(NULL, "*", condition);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vnet_update_vedge_server (char *server_addr, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%s'", server_addr);

    if (lc_db_vedge_update("gw_launch_server", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_uuid (char *uuid, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_vedge_update("uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_vedge_status (uint32_t status, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "%d", status);

    if (lc_db_vedge_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_opaque_id(char *opaque_id, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%s'", opaque_id);

    if (lc_db_vedge_update("opaque_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_load(intf_t *intf_info, uint32_t vif_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vif_id);

    result = lc_db_vif_load(intf_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: cond: %s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: VIF not found: %s \n", __FUNCTION__, condition);
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int lc_vmdb_vif_load_by_id(intf_t *intf_info, uint32_t vif_index, int vm_type, int vmid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ifindex=%d AND devicetype=%d AND deviceid=%d",
                       vif_index, vm_type, vmid);

    result = lc_db_vif_load(intf_info, "mac,vlantag", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: cond: %s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: VIF not found: %s \n", __FUNCTION__, condition);
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int lc_vmdb_vif_loadn_by_vm_id(intf_t *intf_info, int max_count, int vmid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "devicetype=1 AND deviceid=%d", vmid);

    result = lc_db_vif_loadn(intf_info, max_count, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: cond: %s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: VIF not found: %s \n", __FUNCTION__, condition);
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int lc_vmdb_vif_update_name(char *name, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%s'", name);

    if (lc_db_vif_update("name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_ofport(int ofport, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%d'", ofport);

    if (lc_db_vif_update("ofport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_state(int state, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vif_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_mac(char *mac, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%s'", mac);

    if (lc_db_vif_update("mac", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_netmask(uint32_t netmask, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "%d", netmask);

    if (lc_db_vif_update("netmask", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_port_id(char *port_id, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%s'", port_id);

    if (lc_db_vif_update("port_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_pg_id(char *pg_id, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%s'", pg_id);

    if (lc_db_vif_update("pg_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_ip(char *ip, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%s'", ip);

    if (lc_db_vif_update("ip", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_delete (uint32_t vnet_id)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    sprintf(db_buffer, "id='%d'", vnet_id);

    if (lc_db_vedge_delete(db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vedge_update_state (int state, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vedge_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vedge_update_state (int state, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_master_vedge_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_errno(int err_no, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", err_no);

    if (lc_db_vedge_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_vedge_update_errno(int err_no, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vm_id);
    sprintf(buffer, "'%d'", err_no);

    if (lc_db_master_vedge_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_tport (int tport, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", tport);

    if (lc_db_vedge_update("tport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_lport (int lport, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", lport);

    if (lc_db_vedge_update("lport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_label(char *vm_name, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%s'", vm_name);

    if (lc_db_vedge_update("label", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_flag(uint32_t vm_flags, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", vm_flags);

    if (lc_db_vedge_update("flag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vedge_update_vrid (int vrid, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", vrid);

    if (lc_db_vedge_update("havrid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_vlantag(int vlan, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "%d", vlan);

    if (lc_db_vif_update("vlantag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_load(vm_snapshot_info_t *snapshot_info,
                             uint32_t            snapshot_id)
{
    int  result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", snapshot_id);

    result = lc_db_vm_snapshot_load(snapshot_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_SNAPSHOT_NOT_FOUND;
    } else {
        return LC_DB_SNAPSHOT_FOUND;
    }
}

int lc_vmdb_vm_snapshot_update_state(int state, uint32_t snapshot_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", snapshot_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_snapshot_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_state_by_label(int state, char *snapshot_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", snapshot_label);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_snapshot_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_state_of_to_create_by_label(int state, char *snapshot_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s' and state=%d",
            snapshot_label, LC_VM_SNAPSHOT_STATE_TO_CREATE);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_snapshot_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_error(int error, uint32_t snapshot_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", snapshot_id);
    sprintf(buffer, "'%d'", error);

    if (lc_db_vm_snapshot_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_error_by_label(int error, char *snapshot_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", snapshot_label);
    sprintf(buffer, "'%d'", error);

    if (lc_db_vm_snapshot_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_error_of_to_create_by_label(int error, char *snapshot_label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s' and state=%d",
            snapshot_label, LC_VM_SNAPSHOT_STATE_TO_CREATE);
    sprintf(buffer, "'%d'", error);

    if (lc_db_vm_snapshot_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_label(char *label, uint32_t snapshot_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", snapshot_id);
    sprintf(buffer, "'%s'", label);

    if (lc_db_vm_snapshot_update("label", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_update_uuid(char *uuid, uint32_t snapshot_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", snapshot_id);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_vm_snapshot_update("uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_snapshot_load_by_vmid(vm_snapshot_info_t *snapshot_info,
                                     uint32_t            vm_id)
{
    int  result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vm_id=%d", vm_id);

    result = lc_db_vm_snapshot_load(snapshot_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_SNAPSHOT_NOT_FOUND;
    } else {
        return LC_DB_SNAPSHOT_FOUND;
    }
}

int lc_vmdb_vm_backup_load(vm_backup_info_t *backup_info, uint32_t backup_id)
{
    int  result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);

    result = lc_db_vm_backup_load(backup_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_BACKUP_NOT_FOUND;
    } else {
        return LC_DB_BACKUP_FOUND;
    }
}

int lc_vmdb_vm_backup_load_by_vmid(vm_backup_info_t *backup_info, uint32_t vm_id)
{
    int  result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vm_id=%d", vm_id);

    result = lc_db_vm_backup_load(backup_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_BACKUP_NOT_FOUND;
    } else {
        return LC_DB_BACKUP_FOUND;
    }
}

int lc_vmdb_vm_backup_update_state(int state, uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vm_backup_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_backup_update_error(int error, uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "'%d'", error);

    if (lc_db_vm_backup_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_backup_update_flag(uint32_t flag, uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "'%d'", flag);

    if (lc_db_vm_backup_update("flag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_backup_update_local_info(char *local_info, char *local_info_field,
                                        uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "'%s'", local_info);

    if (lc_db_vm_backup_update(local_info_field, buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vm_backup_update_peer_vdi_uuid(char *vdi_sys_uuid,
                                           char *vdi_user_uuid,
                                           uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "peer_vdi_sys_uuid = \'%s\', peer_vdi_user_uuid=\'%s\'",
            vdi_sys_uuid, vdi_user_uuid);

    if (lc_db_vm_backup_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_backup_update_local_vdi_uuid(char *vdi_sys_uuid,
                                            char *vdi_user_uuid,
                                            uint32_t backup_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", backup_id);
    sprintf(buffer, "local_vdi_sys_uuid = \'%s\', local_vdi_user_uuid=\'%s\'",
            vdi_sys_uuid, vdi_user_uuid);

    if (lc_db_vm_backup_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_backup_delete(uint32_t backup_id)
{
    char buffer[LC_DB_BUF_LEN];

    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(buffer, "id='%d'", backup_id);

    if (lc_db_vm_backup_delete(buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_get_vm_by_server (vm_query_info_t *vm_query, int offset, int *vm_num, char *server, int skip_tmp_state, int max)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    if (skip_tmp_state) {
        sprintf(condition, "launch_server='%s'&&state!=%d && (flag&%d = 0)",
                server, LC_VM_STATE_TMP, LC_VM_THIRD_HW_FLAG);
    } else {
        sprintf(condition, "launch_server='%s' && (flag&%d = 0)",
                server, LC_VM_THIRD_HW_FLAG);
    }

    result = lc_db_table_load_by_server(vm_query, "vm", "*", condition,
                                     offset, vm_num, max, vm_query_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_update_master_ha_vnet_hastate(int hastate, char *label)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s'", label);
    sprintf(buffer, "'%d'", hastate);

    if (lc_db_master_vedge_update("hastate", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_get_ha_vnet_ctl_ip_by_domain(vgw_ha_query_info_t *vgw_query, int offset, int *ha_vnet_num, int max, char *domain)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN, "haflag=2 and table1.domain='%s'", domain);
    result = lc_db_table_load_inner(vgw_query, "table1.id,label,hastate,ip", "vnet", "vinterface",
                                    "table1.state<>0 AND table1.ctrl_ifid = table2.id", condition,
                                    offset, ha_vnet_num, max, vgw_ha_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_get_lb_ctl_ip_by_domain(lb_query_info_t *lb_query, int offset, int *lb_num, int max, char *domain)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN,
             "role=2 and table1.state=%d and table1.domain='%s'",
             LC_VM_STATE_RUNNING, domain);
    result = lc_db_table_load_inner(lb_query,
                                    "table1.label,table1.lcuuid,launch_server,ip",
                                    "vm", "vinterface",
                                    "table1.state<>0 AND table2.devicetype=1 "
                                    "AND table2.ifindex=6 AND table1.id = table2.deviceid",
                                    condition, offset, lb_num, max, bk_vm_health_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_get_vnet_by_server (vnet_query_info_t *vm_query, int offset, int *vnet_num, char *server, int skip_tmp_state, int max)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    if (skip_tmp_state) {
        sprintf(condition, "gw_launch_server='%s'&&state!=%d && (flag&%d = 0)",
                server, LC_VEDGE_STATE_TMP, LC_VEDGE_THIRD_HW_FLAG);
    } else {
        sprintf(condition, "gw_launch_server='%s' && (flag&%d = 0)",
                server, LC_VEDGE_THIRD_HW_FLAG);
    }
    result = lc_db_table_load_by_server(vm_query, "vnet", "*", condition,
                              offset, vnet_num, max, vnet_query_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_vm_backup_disks_update(int sys_disk, int usr_disk, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "sys_disk_usage = \'%d\', usr_disk_usage=\'%d\'", sys_disk, usr_disk);

    if (lc_db_vm_backup_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;

}

int lc_vmdb_vmware_session_ticket_update(char *ticket)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "param_name='vmware_session_ticket'");
    sprintf(buffer, "'%s'", ticket);

    if (lc_db_sysconf_update("value", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;

}

int lc_get_email_address_from_user_db(char *email, int user_id)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", user_id);

    if (lc_db_table_load((void*)email, "user", "email", condition,
        get_email_address) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_disk_load(disk_t *disk_info, uint32_t vm_id, uint32_t userdevice)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vmid=%d and userdevice=%d", vm_id, userdevice);

    result = lc_db_disk_load(disk_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_disk_load_by_id(disk_t *disk_info, uint32_t id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_disk_load(disk_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_disk_load_all(disk_t *disk_info, int offset, int *cnt, uint32_t vm_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vmid=%d", vm_id);

    result = lc_db_table_load_all(disk_info, "vdisk", "*", condition, offset, cnt, disk_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_disk_update_sr_uuid(char *sr_uuid, uint32_t disk_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", disk_id);
    sprintf(buffer, "'%s'", sr_uuid);

    if (lc_db_disk_update("vdi_sr_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_update_uuid(char *uuid, uint32_t disk_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", disk_id);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_disk_update("vdi_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_update_size(uint64_t size, uint32_t disk_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", disk_id);
    sprintf(buffer, "%"PRId64, size);

    if (lc_db_disk_update("size", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_update_uuid_by_userdevice(char *uuid, uint32_t vm_id, uint32_t userdevice)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vmid=%d and userdevice=%d", vm_id, userdevice);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_disk_update("vdi_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_update_size_by_userdevice(uint64_t size, uint32_t vm_id, uint32_t userdevice)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vmid=%d and userdevice=%d", vm_id, userdevice);
    sprintf(buffer, "%"PRId64, size);

    if (lc_db_disk_update("size", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_update_name_by_userdevice(char *name, uint32_t vm_id, uint32_t userdevice)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vmid=%d and userdevice=%d", vm_id, userdevice);
    sprintf(buffer, "'%s'", name);

    if (lc_db_disk_update("vdi_name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_disk_delete(uint32_t disk_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "id=%d", disk_id);

    if (lc_db_disk_delete(condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_disk_delete_by_userdevice(uint32_t vm_id, uint32_t userdevice)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "vmid=%d and userdevice=%d", vm_id, userdevice);

    if (lc_db_disk_delete(condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_disk_delete_by_vm(uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "vmid=%d", vm_id);

    if (lc_db_disk_delete(condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_third_device_load(third_device_t *device, uint32_t vm_id, uint32_t type)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vm_id=%d AND type=%d", vm_id, type);

    result = lc_db_third_device_load(device, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: cond: %s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: third_party_device not found: %s \n",
                  __FUNCTION__, condition);
        return LC_DB_THIRD_DEVICE_NOT_FOUND;
    } else {
        return LC_DB_THIRD_DEVICE_FOUND;
    }
}

int lc_db_vmwaf_load(vmwaf_query_info_vm_t* vmwaf_info, char *column_list, char *condition)
{
    return lc_db_table_load(vmwaf_info, "vmwaf", column_list, condition, vmwaf_query_process);
}

int lc_db_get_vmwaf_by_launch_server(vmwaf_query_info_vm_t *vmwaf_query, char* launch_server)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "launch_server='%s'", launch_server);

    result = lc_db_vmwaf_load(vmwaf_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "conditon=[%s]\n", condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VMWAF_NOT_FOUND;
    } else {
        return LC_DB_VMWAF_FOUND;
    }
}

int lc_db_master_vmwaf_update_state_by_launch_server (int state, char* launch_server)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "launch_server='%s'", launch_server);
    sprintf(buffer, "'%d'", state);

    if (lc_db_master_vmwaf_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_lb_load_by_namelable(vm_info_t *vm_info,char *lb_lable)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "label='%s' and role=2", lb_lable);

    result = lc_db_vm_load(vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_lb_listener_load_by_name(lb_listener_t *listener_info,
                                     char *listener_name,
                                     char *lb_lcuuid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "name='%s' and lb_lcuuid='%s'",
            listener_name, lb_lcuuid);

    result = lc_db_lb_listener_load(listener_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_vmdb_lb_bk_vm_load_by_name(lb_bk_vm_t *bk_vm_info,
                                  char *lb_bk_vm_name,
                                  char *lb_listener_lcuuid,
                                  char *lb_lcuuid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "name='%s' and lb_listener_lcuuid='%s' and lb_lcuuid='%s'",
            lb_bk_vm_name, lb_listener_lcuuid, lb_lcuuid);

    result = lc_db_lb_bk_vm_load(bk_vm_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
}

int lc_db_master_lb_bk_vm_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("lb_bk_vm", column, value, condition);
}

int lc_update_master_lb_bk_vm_health_state(int health_state, char *lcuuid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "lcuuid='%s'", lcuuid);
    sprintf(buffer, "'%d'", health_state);

    if (lc_db_master_lb_bk_vm_update("health_state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}
