#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "mysql.h"
#include "lc_db.h"
#include "lc_netcomp.h"
#include "lc_vnet_worker.h"
#include "lc_kernel.h"
#include "lc_lcmg_api.h"

extern MYSQL *conn_handle;

static pthread_mutex_t vlantag_mutex = PTHREAD_MUTEX_INITIALIZER;

static int vnet_process(void *vnet_info, char *field, char *value)
{
    vnet_t *vnet = (vnet_t *)vnet_info;

    if (strcmp(field, "id") == 0) {
        vnet->vnet_id = atoi(value);
    } else if (strcmp(field, "opaque_id") == 0) {
        strcpy(vnet->opaque_id, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(vnet->vnet_name, value);
    } else if (strcmp(field, "vl2_num") == 0) {
        vnet->vl2num = atoi(value);
    } else if (strcmp(field, "gw_poolid") == 0) {
        vnet->poolid = atoi(value);
    } else if (strcmp(field, "gw_launch_server") == 0) {
        snprintf(vnet->launch_server, MAX_HOST_ADDRESS_LEN, "%s", value);

    } else if (strcmp(field, "private_if1id") == 0) {
        vnet->pvt_intf[0].if_id = atoi(value);
    } else if (strcmp(field, "private_if2id") == 0) {
        vnet->pvt_intf[1].if_id = atoi(value);
    } else if (strcmp(field, "private_if3id") == 0) {
        vnet->pvt_intf[2].if_id = atoi(value);
    } else if (strcmp(field, "private_if4id") == 0) {
        vnet->pvt_intf[3].if_id = atoi(value);
    } else if (strcmp(field, "private_if5id") == 0) {
        vnet->pvt_intf[4].if_id = atoi(value);
    } else if (strcmp(field, "public_if1id") == 0) {
        vnet->pub_intf[0].if_id = atoi(value);
    } else if (strcmp(field, "public_if2id") == 0) {
        vnet->pub_intf[1].if_id = atoi(value);
    } else if (strcmp(field, "public_if3id") == 0) {
        vnet->pub_intf[2].if_id = atoi(value);
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(vnet->lcuuid, value);
    } else if (strcmp(field, "role") == 0) {
        vnet->role = atoi(value);
    }

    return 0;
}

static int vm_process(void *vm_info, char *field, char *value)
{
    vm_t *vm = (vm_t *)vm_info;

    if (strcmp(field, "id") == 0) {
        vm->vm_id = atoi(value);
    } else if (strcmp(field, "launch_server") == 0) {
        snprintf(vm->launch_server, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "vnetid") == 0) {
        vm->vnetid = atoi(value);
    } else if (strcmp(field, "vl2id") == 0) {
        vm->vl2id = atoi(value);
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
    } else if (strcmp(field, "flag") == 0) {
        pintf->if_flag = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        pintf->if_errno = atoi(value);
    } else if (strcmp(field, "eventid") == 0) {
        pintf->if_eventid = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(pintf->if_ip, value);
    } else if (strcmp(field, "netmask") == 0) {
        mask.s_addr = htonl(0xFFFFFFFF << (32 - atoi(value)));
        strcpy(pintf->if_netmask, inet_ntoa(mask));
        pintf->if_mask = atoi(value);
    } else if (strcmp(field, "iftype") == 0) {
        pintf->if_type = atoi(value);
    } else if (strcmp(field, "subtype") == 0) {
        pintf->subtype = atoi(value);
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
    } else if (strcmp(field, "pg_id") == 0) {
        strcpy(pintf->if_pgid, value);
    } else if (strcmp(field, "netdevice_lcuuid") == 0) {
        strcpy(pintf->if_netdevice_lcuuid, value);
    } else if (strcmp(field, "domain") == 0) {
        strcpy(pintf->if_domain, value);
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(pintf->if_lcuuid, value);
    }

    return 0;
}

static int vif_ip_process(void *intf_ip_info, char *field, char *value)
{
    intf_ip_t* pintf = (intf_ip_t*) intf_ip_info;

    if (strcmp(field, "ip") == 0) {
        strcpy(pintf->ip, value);
    } else if (strcmp(field, "netmask") == 0) {
        strcpy(pintf->netmask, value);
    }

    return 0;
}

static int vl2_process(void *vl2_info, char *field, char *value)
{
    vl2_t *vl2 = (vl2_t *)vl2_info;

    if (strcmp(field, "id") == 0) {
        vl2->id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        vl2->state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        vl2->err = atoi(value);
    } else if (strcmp(field, "net_type") == 0) {
        vl2->net_type = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(vl2->name, value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(vl2->desc, value);
    } else if (strcmp(field, "port_num") == 0) {
        vl2->portnum = atoi(value);
    } else if (strcmp(field, "vlan_id") == 0) {
        vl2->vlan_request = atoi(value);
    } else if (strcmp(field, "vcloudid") == 0) {
        vl2->vcloudid = atoi(value);
    } else if (strcmp(field, "vnetid") == 0) {
        vl2->vnetid = atoi(value);
    } else if (strcmp(field, "pg_id") == 0) {
        strcpy(vl2->pg_id, value);
    }

    return 0;
}

static int vl2_vlan_process(void *vl2_vlan_info, char *field, char *value)
{
    vl2_vlan_t *vl2_vlan = (vl2_vlan_t *)vl2_vlan_info;

    if (strcmp(field, "id") == 0) {
        vl2_vlan->id = atoi(value);
    } else if (strcmp(field, "vl2id") == 0) {
        vl2_vlan->vl2id = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        vl2_vlan->rackid = atoi(value);
    } else if (strcmp(field, "vlantag") == 0) {
        vl2_vlan->vlantag = atoi(value);
    }

    return 0;
}

static int network_resource_process(void *nr_info, char *field, char *value)
{
    network_resource_t *nr = (network_resource_t *)nr_info;

    if (strcmp(field, "id") == 0) {
        nr->id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        nr->state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        nr->err = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        nr->type = atoi(value);
    } else if (strcmp(field, "portnum") == 0) {
        nr->portnum = atoi(value);
    } else if (strcmp(field, "poolid") == 0) {
        nr->poolid = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(nr->ip, value);
    } else if (strcmp(field, "netdeviceid") == 0) {
        nr->netdeviceid = atoi(value);
    } else if (strcmp(field, "tunnel_ip") == 0) {
        strcpy(nr->tunnel_ip, value);
    }

    return 0;
}

static int sys_config_process(void *p, char *field, char *value)
{
    char *str = (char *)p;

    if (strcmp(field, "value") == 0) {
        strcpy(str, value);
    }

    return 0;
}

static int host_port_config_process(void *host_port_info, char *field, char *value)
{
    host_port_config_t *host_info = (host_port_config_t *)host_port_info;

    if (strcmp(field, "id") == 0) {
        host_info->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(host_info->name, value);
    } else if (strcmp(field, "mac") == 0) {
        strcpy(host_info->mac, value);
    } else if (strcmp(field, "host_ip") == 0) {
        strcpy(host_info->host_ip, value);
    } else if (strcmp(field, "type") == 0) {
        host_info->type = atoi(value);
    } else if (strcmp(field, "torswitch_port") == 0) {
        strcpy(host_info->ofs_port, value);
    } else if (strcmp(field, "torswitch_id") == 0) {
        host_info->ofs_id = atoi(value);
    }
    return 0;
}

static int name_label_process(void *p, char *field, char *value)
{
    char *label = (char *)p;

    if (strcmp(field, "label") == 0) {
        strcpy(label, value);
    }

    return 0;
}

static int pool_ctype_process(void *p, char *field, char *value)
{
    int *type = (int *)p;

    if (strcmp(field, "ctype") == 0) {
        *type = atoi(value);
    }

    return 0;
}

static int host_process(void *p, char *field, char *value)
{
    host_t *host = (host_t *)p;

    if (strcmp(field, "id") == 0) {
        host->id = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(host->ip, value);
    } else if (strcmp(field, "inf0type") == 0) {
        host->if0_type = atoi(value);
    } else if (strcmp(field, "inf1type") == 0) {
        host->if1_type = atoi(value);
    } else if (strcmp(field, "inf2type") == 0) {
        host->if2_type = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        host->host_role = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        host->rackid = atoi(value);
    }

    return 0;
}

static int email_address_process(void *cfg_info, char *field, char *value)
{
    nsp_host_t *pcfg = (nsp_host_t *)cfg_info;

    if (strcmp(field, "email") == 0) {
        strcpy(pcfg->email_address, value);
    }else if (strcmp(field, "ccs") == 0) {
        strcpy(pcfg->email_address_ccs, value);
    }

    return 0;
}

static int ipmi_process(void *p, char *field, char *value)
{
    ipmi_t *ipmip = (ipmi_t *)p;

    if (strcmp(field, "id") == 0) {
        ipmip->id = atoi(value);
    } else if (strcmp(field, "device_id") == 0) {
        ipmip->device_id = atoi(value);
    } else if (strcmp(field, "device_type") == 0) {
        ipmip->device_type = atoi(value);
    } else if (strcmp(field, "ipmi_ip") == 0) {
        strcpy(ipmip->ip, value);
    } else if (strcmp(field, "ipmi_user_name") == 0) {
        strcpy(ipmip->username, value);
    } else if (strcmp(field, "ipmi_passwd") == 0) {
        strcpy(ipmip->password, value);
    }

    return 0;
}

static int compute_resource_process(void *p, char *field, char *value)
{
    host_t *host = (host_t *)p;

    if (strcmp(field, "id") == 0) {
        host->id = atoi(value);
    } else if (strcmp(field, "dbr_peer_server") == 0) {
        strcpy(host->peer_address, value);
    } else if (strcmp(field, "service_flag") == 0) {
        host->host_service_flag = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        snprintf(host->ip, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "uplink_ip") == 0) {
        snprintf(host->uplink_ip, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "uplink_netmask") == 0) {
        snprintf(host->uplink_netmask, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "uplink_gateway") == 0) {
        snprintf(host->uplink_gateway, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "rackid") == 0) {
        host->rackid = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        host->type = atoi(value);
    } else if (strcmp(field, "domain") == 0) {
        snprintf(host->domain, UUID_LEN, "%s", value);
    }

    return 0;
}

static int vlan_process(void *p, char *field, char *value)
{
    vlan_t *vlan = (vlan_t *)p;

    if (strcmp(field, "id") == 0) {
        vlan->id = atoi(value);
    } else if (strcmp(field, "rack") == 0) {
        vlan->rack = atoi(value);
    } else if (strcmp(field, "ranges") == 0) {
        strcpy(vlan->ranges, value);
    }

    return 0;
}

int lc_db_vnet_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vnet", column_list, value_list);
}

int lc_db_vnet_delete(char *condition)
{
    return lc_db_table_delete("vnet", condition);
}

int lc_db_vnet_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("vnet", columns_update, condition);
}

int lc_db_vnet_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vnet", column, value, condition);
}

int lc_db_vnet_dump(char *column_list)
{
    return lc_db_table_dump("vnet", column_list);
}

int lc_db_vm_load(vm_t* vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_info, "vm", column_list, condition, vm_process);
}

int lc_db_vm_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vm", column, value, condition);
}

int lc_db_vnet_load(vnet_t* vnet_info, char *column_list, char *condition)
{
    return lc_db_table_load(vnet_info, "vnet", column_list, condition, vnet_process);
}

int lc_db_vl2_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vl2", column_list, value_list);
}

int lc_db_vl2_delete(char *condition)
{
    return lc_db_table_delete("vl2", condition);
}

int lc_db_vl2_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vl2", column, value, condition);
}

int lc_db_vl2_dump(char *column_list)
{
    return lc_db_table_dump("vl2", column_list);
}

int lc_db_vl2_load(vl2_t* vl2_info, char *column_list, char *condition)
{
    return lc_db_table_load(vl2_info, "vl2", column_list, condition, vl2_process);
}

int lc_db_vl2_vlan_delete(char *condition)
{
    return lc_db_table_delete("vl2_vlan", condition);
}

int lc_db_vl2_vlan_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vl2_vlan", column_list, value_list);
}

#if 0
static int lc_db_vl2_vlan_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vl2_vlan", column, value, condition);
}
#endif

int lc_db_vl2_vlan_load(vl2_vlan_t* vl2_vlan_info, char *column_list, char *condition)
{
    return lc_db_table_load(vl2_vlan_info, "vl2_vlan", column_list, condition, vl2_vlan_process);
}

int lc_db_network_resource_load(network_resource_t* nr_info, char *column_list, char *condition)
{
    return lc_db_table_load(nr_info, "network_resource", column_list, condition, network_resource_process);
}

int lc_db_network_resource_load_all(network_resource_t* nr_info, char *column_list, char *condition, int offset, int *nr_num)
{
    return lc_db_table_load_all(nr_info, "network_resource", column_list, condition, offset, nr_num, network_resource_process);
}

int lc_db_network_resource_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("network_resource", columns_update, condition);
}

int lc_db_network_resource_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("network_resource", column, value, condition);
}

int lc_db_vif_load(intf_t* intf_info, char *column_list, char *condition)
{
    return lc_db_table_load(intf_info, "vinterface", column_list, condition, vif_process);
}

int lc_db_vif_load_all(intf_t* intf_info, char *column_list, char *condition, int *vif_num)
{
    return lc_db_table_load_all(intf_info, "vinterface", column_list, condition, sizeof(intf_t), vif_num, vif_process);
}

int lc_db_vif_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("vinterface", column, value, condition);
}

int lc_db_vif_ip_loadn(intf_ip_t* intf_ip, int len, char *condition)
{
    return lc_db_table_loadn(intf_ip, sizeof(intf_ip_t), len, "vinterface_ip", "*", condition, vif_ip_process);
}

int lc_db_host_load(host_t* host, char *column_list, char *condition)
{
    return lc_db_table_load(host, "host_device", column_list, condition, host_process);
}

int lc_db_ipmi_load(ipmi_t *ipmi, char *column_list, char *condition)
{
    return lc_db_table_load(ipmi, "ipmi_info", column_list, condition, ipmi_process);
}

int lc_db_compute_resource_load(host_t* host, char *column_list, char *condition)
{
    return lc_db_table_load(host, "compute_resource", column_list, condition, compute_resource_process);
}

int lc_db_vlan_load(vlan_t* vlan, char *column_list, char *condition)
{
    return lc_db_table_load(vlan, "vlan", column_list, condition, vlan_process);
}

int lc_db_get_sys_running_mode()
{
    char mode[LC_DB_BUF_LEN] = {0};
    char condition[LC_DB_BUF_LEN] = {0};

    snprintf(condition, LC_DB_BUF_LEN,
            "module_name='SYSTEM' AND param_name='running_mode'");

    if (lc_db_table_load(mode, "sys_configuration", "value",
                condition, sys_config_process)) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s.\n", __FUNCTION__, condition);
        return LCC_SYS_RUNNING_MODE_UNKNOWN;
    }

    if (strcmp(mode, "independent") == 0) {
        return LCC_SYS_RUNNING_MODE_INDEPENDENT;
    } else if (strcmp(mode, "hierarchical") == 0) {
        return LCC_SYS_RUNNING_MODE_HIERARCHICAL;
    }

    LC_DB_LOG(LOG_ERR, "%s: invalid running_mode=%s.\n", __FUNCTION__, mode);
    return LCC_SYS_RUNNING_MODE_UNKNOWN;
}

int lc_vmdb_vnet_insert (vnet_t *vnet)
{
    char db_buffer[MAX_VNET_DB_BUFFER];

    memset(db_buffer, 0, MAX_VNET_DB_BUFFER);
    sprintf(db_buffer, "'%d','%s','%d','13900','23900'",
        vnet->vnet_id, vnet->vnet_name, vnet->vl2num);

    LC_DB_LOG(LOG_DEBUG, "%s db buf=%s\n", __FUNCTION__, db_buffer);
    if (lc_db_vnet_insert(LC_DB_VNET_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vnet_delete (uint32_t vnet_id)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    sprintf(db_buffer, "id='%d'", vnet_id);

    if (lc_db_vnet_delete(db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vm_load(vm_t *vm_info, uint32_t id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "id=%u", id);

    if (lc_db_vm_load(vm_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_load_by_vl2id(vm_t *vm_info, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "vl2id=%u", vl2_id);

    if (lc_db_vm_load(vm_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_load(vnet_t *vnet_info, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "id='%d'", vnet_id);

    if (lc_db_vnet_load(vnet_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_name (char *name, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vnet_id);
    sprintf(buffer, "'%s'", name);

    if (lc_db_vnet_update("name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_vl2num (uint32_t vl2_num, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vnet_id);
    sprintf(buffer, "'%d'", vl2_num);

    if (lc_db_vnet_update("vl2_num", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_insert (vl2_t *vl2, uint32_t vnet_id)
{
    char db_buffer[MAX_VNET_DB_BUFFER];

    memset(db_buffer, 0, MAX_VNET_DB_BUFFER);
    sprintf(db_buffer, "'%d','%s','%d','%d'",
        vl2->id, vl2->name, vl2->portnum, vnet_id);

    if (lc_db_vl2_insert(LC_DB_VL2_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vl2_delete(uint32_t vl2_id)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    snprintf(db_buffer, LC_DB_BUF_LEN, "id='%u'", vl2_id);

    if (lc_db_vl2_delete(db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_load(vl2_t *vl2_info, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    snprintf(condition, LC_DB_BUF_LEN, "id='%u'", vl2_id);

    if (lc_db_vl2_load(vl2_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_update_name (char *vl2_name, uint32_t vnet_id, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d' and vnet='%d'", vl2_id, vnet_id);
    sprintf(buffer, "'%s'", vl2_name);

    if (lc_db_vl2_update("name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_update_state (uint32_t state, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vl2_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vl2_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_update_errno(uint32_t err, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vl2_id);
    sprintf(buffer, "'%d'", err);

    if (lc_db_vl2_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_update_portnum (int port_num, uint32_t vnet_id, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d, vnet=%d", vl2_id, vnet_id);
    sprintf(buffer, "'%d'", port_num);

    if (lc_db_vl2_update("port_num", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_vlan_delete_one(uint32_t vl2id, uint32_t rackid)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    snprintf(db_buffer, LC_DB_BUF_LEN, "vl2id=%u AND rackid=%u", vl2id, rackid);
    if (lc_db_vl2_vlan_delete(db_buffer)) {
        LC_DB_LOG(LOG_ERR, "%s: error.\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vl2_vlan_delete_all(uint32_t vl2id)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    snprintf(db_buffer, LC_DB_BUF_LEN, "vl2id=%u", vl2id);
    if (lc_db_vl2_vlan_delete(db_buffer)) {
        LC_DB_LOG(LOG_ERR, "%s: error.\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_vl2_vlan_insert(uint32_t vl2id, uint32_t rack, uint32_t vlantag)
{
    char values[LC_DB_BUF_LEN];
    char columns[LC_DB_BUF_LEN];

    memset(values, 0, LC_DB_BUF_LEN);
    memset(columns, 0, LC_DB_BUF_LEN);
    snprintf(values, LC_DB_BUF_LEN,
            "%u, %u, %u", vl2id, rack, vlantag);
    snprintf(columns, LC_DB_BUF_LEN, "vl2id, rackid, vlantag");

    if (lc_db_vl2_vlan_insert(columns, values) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

#if 0
static int lc_vmdb_vl2_vlan_update(uint32_t vl2id, uint32_t rack, uint32_t vlantag)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vl2id=%d and rackid=%d", vl2id, rack);
    sprintf(buffer, "%d", vlantag);

    if (lc_db_vl2_vlan_update("vlantag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}
#endif

int lc_vmdb_vl2_vlan_load(uint32_t *p_vlantag, uint32_t vl2id, uint32_t rackid)
{
    char condition[LC_DB_BUF_LEN] = {0};
    vl2_vlan_t vl2_vlan = {0};

    if (p_vlantag) {
        *p_vlantag = 0;
    } else {
        return -1;
    }

    if (rackid) {
        snprintf(condition, LC_DB_BUF_LEN,
                "vl2id=%u AND (rackid=%u OR rackid=0)", vl2id, rackid);
    } else {
        snprintf(condition, LC_DB_BUF_LEN,
                "vl2id=%u AND rackid=0", vl2id);
    }

    if (lc_db_vl2_vlan_load(&vl2_vlan, "vlantag", condition) != 0) {
        LC_DB_LOG(LOG_DEBUG, "%s: vl2-%u has no vlantag in rack-%u now.\n",
                __FUNCTION__, vl2id, rackid);
        return -1;
    }

    *p_vlantag = vl2_vlan.vlantag;
    return 0;
}

int lc_vmdb_vl2_vlan_check_occupy(
        uint32_t vl2id, uint32_t rackid, uint32_t vlantag)
{
    char condition[LC_DB_BUF_LEN] = {0};
    vl2_vlan_t vl2_vlan = {0};

    if (rackid) {
        snprintf(condition, LC_DB_BUF_LEN,
                "vl2id!=%u AND (rackid=%u OR rackid=0) AND vlantag=%u",
                vl2id, rackid, vlantag);
    } else {
        snprintf(condition, LC_DB_BUF_LEN,
                "vl2id!=%u AND vlantag=%u",
                vl2id, vlantag);
    }

    if (lc_db_vl2_vlan_load(&vl2_vlan, "*", condition) != 0) {
        return 0;
    }

    return 1;
}

int lc_vmdb_vl2_update_net_type (int net_type, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vl2_id);
    sprintf(buffer, "'%d'", net_type);

    if (lc_db_vl2_update("net_type", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vl2_update_pgid(char *pg_id, uint32_t vl2_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vl2_id);
    sprintf(buffer, "'%s'", pg_id);

    if (lc_db_vl2_update("pg_id", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_network_resource_load(network_resource_t *nr_info, uint32_t id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "id='%d'", id);

    if (lc_db_network_resource_load(nr_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_network_resource_load_by_ip(network_resource_t *nr_info, char *ip)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "ip='%s'", ip);

    if (lc_db_network_resource_load(nr_info, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_network_resource_load_all(network_resource_t *nr_info, int offset, int *nr_num)
{
    int result;

    result = lc_db_network_resource_load_all(nr_info, "*", "", offset, nr_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    }

    return 0;
}

int lc_vmdb_network_resource_update_state(uint32_t state, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_network_resource_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_network_resource_update_errno(uint32_t err, uint32_t id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);
    sprintf(buffer, "'%d'", err);

    if (lc_db_network_resource_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_load(intf_t *intf_info, uint32_t vif_id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", vif_id);

    result = lc_db_vif_load(intf_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int lc_vmdb_vif_load_all(intf_t *intf_info,
        char *column_list, char *condition, int *vif_num)
{
    int result;

    if (vif_num) {
        *vif_num = 0;
    }

    if (!condition || !vif_num) {
        return LC_DB_COMMON_ERROR;
    }

    result = lc_db_vif_load_all(intf_info, column_list, condition, vif_num);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_INFO, "%s: can not find object, condition=%s column_list=%s\n",
                __FUNCTION__, condition, column_list);
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int lc_vmdb_vif_load_by_vl2_and_device_id(intf_t *intf_info, uint32_t vl2, uint32_t deviceid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "subnetid=%d and deviceid=%d", vl2, deviceid);

    result = lc_db_vif_load(intf_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
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

int lc_vmdb_vif_update_vlantag(uint32_t vlantag, uint32_t ifid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN, "id='%u'", ifid);
    snprintf(buffer, LC_DB_BUF_LEN, "%u", vlantag);

    if (lc_db_vif_update("vlantag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_vif_update_vlantag_by_vl2id(uint32_t vlantag, uint32_t vl2id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN, "subnetid=%u", vl2id);
    snprintf(buffer, LC_DB_BUF_LEN, "%u", vlantag);

    if (lc_db_vif_update("vlantag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    snprintf(condition, LC_DB_BUF_LEN, "vl2id=%u", vl2id);
    if (lc_db_table_update("tunnel_policy", "vlantag", buffer, condition)) {
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

int lc_db_host_port_config_load(host_port_config_t* host_port_info,
                                               char *column_list, char *condition)
{
    return lc_db_table_load(host_port_info, "host_port_config",
                               column_list, condition, host_port_config_process);
}

static int lc_vmdb_get_name_label(int id, char *table, char *label)
{
    char cond[LC_DB_BUF_LEN];

    memset(cond, 0, LC_DB_BUF_LEN);
    sprintf(cond, "id=%d", id);

    return lc_db_table_load(label, table, "label", cond,
            name_label_process);
}

int lc_vmdb_get_vm_name_label(int id, char *label)
{
    return lc_vmdb_get_name_label(id, "vm", label);
}

int lc_vmdb_get_vedge_name_label(int id, char *label)
{
    return lc_vmdb_get_name_label(id, "vnet", label);
}

int lc_vmdb_get_pool_htype(uint32_t poolid)
{
    int type = 0;
    char cond[LC_DB_BUF_LEN];

    memset(cond, 0, LC_DB_BUF_LEN);
    sprintf(cond, "id=%d", poolid);

    lc_db_table_load(&type, "pool", "ctype", cond, pool_ctype_process);

    return type;
}

int lc_vmdb_host_port_config_load_by_id(host_port_config_t *host_port_config,
                                                             int id)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_host_port_config_load(host_port_config, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}


int lc_vmdb_host_load(host_t *host, char *ip)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_load(host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_load_rackid(uint32_t *p_rackid, char *ip)
{
    host_t host = {0};
    int result;
    char condition[LC_DB_BUF_LEN];

    if (p_rackid) {
        *p_rackid = 0;
    } else {
        return LC_DB_COMMON_ERROR;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_load(&host, "rackid", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        *p_rackid = host.rackid;
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_load_hostid(int *p_hostid, char *ip)
{
    host_t host = {0};
    int result;
    char condition[LC_DB_BUF_LEN];

    if (p_hostid) {
        *p_hostid = 0;
    } else {
        return LC_DB_COMMON_ERROR;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_load(&host, "id", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        *p_hostid = host.id;
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load(host_t *host, char *ip)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_compute_resource_load(host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_vlan_range_load_by_rack(vlan_t *vlan, int rack)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "rack=%d", rack);

    result = lc_db_vlan_load(vlan, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VLAN_NOT_FOUND;
    } else {
        return LC_DB_VLAN_FOUND;
    }
}

/*
 * rackid:
 *  =0: all racks
 *  >=1: dedicated rack
 */
static int lc_db_alloc_vl2_vlantag(uint32_t *p_vlantag,
        uint32_t vl2id, uint32_t rackid, uint32_t vlan_request, uint32_t force)
{
    return call_url_post_vl2_vlan(p_vlantag, vl2id, rackid,
                                  vlan_request, force);
#if 0
    vlan_t vlan;
    char *p, *q;
    int min, max;
    int exist = 0;

    if (p_vlantag) {
        *p_vlantag = 0;
    } else {
        return -1;
    }

    /* STEP 1. if vl2 already has a vlantag in rack */
    if (0 == lc_vmdb_vl2_vlan_load(p_vlantag, vl2id, rackid)) {
        if (force) {
            exist = 1;
        }
        else {
            if (vlan_request && *p_vlantag != vlan_request) {
                LC_DB_LOG(LOG_ERR, "%s: vl2-%u already has vlan-%u "
                        "in rack-%u, can not set to requested vlan-%u.\n",
                        __FUNCTION__, vl2id, *p_vlantag, rackid, vlan_request);
                *p_vlantag = 0;
                return -1;
            }
            return 0;
        }
    }

    /* STEP 2. find an available vlantag in rack */

    if (vlan_request) {
        if (lc_vmdb_vl2_vlan_check_occupy(vl2id, rackid, vlan_request)) {
            LC_DB_LOG(LOG_ERR, "%s: rack-%u already alloc vlan-%u "
                    "for other vl2, can not alloc to vl2-%u.\n",
                    __FUNCTION__, rackid, vlan_request, vl2id);
            return -1;
        }
        *p_vlantag = vlan_request;

    } else {
        if (rackid == 0) {
            /* delete all <vl2,*,*> */
            if (lc_vmdb_vl2_vlan_delete_all(vl2id)) {
                LC_DB_LOG(LOG_ERR, "%s: delete old vlantag for vl2-%u failed.\n",
                        __FUNCTION__, vl2id);
                return -1;
            }
        }

        /* find an available vlantag */

        memset(&vlan, 0, sizeof(vlan));
        if (lc_vmdb_vlan_range_load_by_rack(&vlan, rackid)) {
            /* use default from min to max */
            if (atomic_lc_db_get_available_vlantag(
                        p_vlantag, rackid, 0, 0)) {
                LC_DB_LOG(LOG_ERR, "%s: can not alloc vlan for "
                        "vl2-%u in rack-%u.\n", __FUNCTION__, vl2id, rackid);
                return -1;
            }
        }

        /* parse range, range are like: '10-204,405-405,594-' */
        p = vlan.ranges;
        q = p;
        while (*p) {
            if (!isdigit(*p) && *p != '-' && *p != ',') {
                ++p;
                continue;
            }
            *q++ = *p++;
        }
        *q = 0;

        p = vlan.ranges;
        while (*p) {
            min = 0;
            max = 0;
            sscanf(p, "%d-%d", &min, &max);
            if (0 == atomic_lc_db_get_available_vlantag(
                        p_vlantag, rackid, min, max)) {
                break;
            }
            while (*p++) {
                if (*(p - 1) == ',') {
                    break;
                }
            }
        }
        if (*p_vlantag == 0) {
            LC_DB_LOG(LOG_ERR, "%s: can not alloc vlan for "
                    "vl2-%u in rack-%u.\n", __FUNCTION__, vl2id, rackid);
            return -1;
        }
    }

    /* STEP 3: save <vl2id,rackid,vlantag> to vl2_vlan */

    if (exist) {
        if (lc_vmdb_vl2_vlan_update(vl2id, rackid, *p_vlantag)) {
            LC_DB_LOG(LOG_ERR, "%s: update vl2_vlan error, vl2id=%u "
                    "rackid=%u vlantag=%u.\n", __FUNCTION__,
                    vl2id, rackid, *p_vlantag);
            return -1;
        }
    } else {
        if (lc_vmdb_vl2_vlan_insert(vl2id, rackid, *p_vlantag)) {
            LC_DB_LOG(LOG_ERR, "%s: save vl2_vlan error, vl2id=%u "
                    "rackid=%u vlantag=%u.\n", __FUNCTION__,
                    vl2id, rackid, *p_vlantag);
            return -1;
        }
    }

    LC_DB_LOG(LOG_DEBUG, "%s: alloc vlan-%u for vl2-%u in rack-%u.",
            __FUNCTION__, *p_vlantag, vl2id, rackid);
    return 0;
#endif
}

static int lc_db_free_vl2_vlantag(uint32_t vl2id, uint32_t rackid)
{
    /*
     * TODO
     * If no vm and vnet of $vl2id is located in $rackid,
     *   free <vl2id,rackid,vlantag>.
     * Currently we only free all <vl2id,*,*> when delete $vl2id.
     *   see lc_db_free_all_vlantag_of_vl2()
     */

    return 0;
}

/*
 * vlan_request: try to use it if not equal to zero
 */
int lc_db_alloc_vlantag(uint32_t *p_vlantag,
        uint32_t vl2id, uint32_t rackid, uint32_t vlan_request, uint32_t force)
{
    int err = 0;

    pthread_mutex_lock(&vlantag_mutex);
    err = lc_db_alloc_vl2_vlantag(p_vlantag, vl2id, rackid, vlan_request, force);
    pthread_mutex_unlock(&vlantag_mutex);

    return err;
}

int lc_db_free_vlantag(uint32_t vl2id, uint32_t rackid)
{
    int err = 0;

    pthread_mutex_lock(&vlantag_mutex);
    err = lc_db_free_vl2_vlantag(vl2id, rackid);
    pthread_mutex_unlock(&vlantag_mutex);

    return err;
}

int lc_db_free_all_vlantag_of_vl2(uint32_t vl2id)
{
    int err = 0;

    pthread_mutex_lock(&vlantag_mutex);
    err = lc_vmdb_vl2_vlan_delete_all(vl2id);
    pthread_mutex_unlock(&vlantag_mutex);

    return err;
}

int lc_db_vnet_load_all_v2(vnet_t* vnet_info, char *column_list,
        char *condition, int offset, int *vnet_num)
{
    return lc_db_table_loadn_all(vnet_info, "vnet", column_list,
            condition, offset, vnet_num, MAX_VNET_NUM, vnet_process);
}

int lc_db_compute_resource_load_all_v2(host_t *host_info, char *column_list,
        char *condition, int offset, int *host_num)
{
    return lc_db_table_loadn_all(host_info, "compute_resource", column_list,
            condition, offset, host_num, MAX_HOST_NUM, compute_resource_process);
}

int lc_db_host_device_load_all_v2(host_t* host_info, char *column_list,
        char *condition, int offset, int *host_num)
{
    return lc_db_table_loadn_all(host_info, "host_device", column_list,
            condition, offset, host_num, MAX_HOST_NUM, host_process);
}

int lc_vmdb_vnet_load_all_v2_by_server_for_migrate(
        vnet_t *vnet, int offset, int *vnet_num, char *server_addr)
{
#define LC_VEDGE_STATE_TO_CREATE 1
#define LC_VEDGE_STATE_TO_MIGRATE 11
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int result;

    /* NSP maybe bootup (and callback talker) during VGateway migrate,
     * set 'state' to TO_MIGRATE to avoid Talker recreate VGateway
     */
    snprintf(condition, sizeof(condition),
            "gw_launch_server='%s' AND state!=%d",
            server_addr, LC_VEDGE_STATE_TO_CREATE);
    snprintf(buffer, sizeof(buffer), "%d", LC_VEDGE_STATE_TO_MIGRATE);
    if (lc_db_vnet_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    *vnet_num = 0;
    snprintf(condition, sizeof(condition),
            "gw_launch_server='%s' AND state=%d",
            server_addr, LC_VEDGE_STATE_TO_MIGRATE);
    result = lc_db_vnet_load_all_v2(vnet, "*", condition, offset, vnet_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VEDGE_NOT_FOUND;
    } else {
        return LC_DB_VEDGE_FOUND;
    }
}

int lc_vmdb_host_device_load_all_v2_by_state(host_t *host, int offset,
        int *host_num, int poolid, int state)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    *host_num = 0;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "poolid=%d and state=%d", poolid, state);

    result = lc_db_host_device_load_all_v2(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_all_v2_by_state(host_t *host, int offset,
        int *host_num, int poolid, int state)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    *host_num = 0;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "poolid=%d and state=%d", poolid, state);

    result = lc_db_compute_resource_load_all_v2(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_vnet_get_num_by_server(int *vnet_num, char *server_addr)
{
    char condition[LC_DB_BUF_LEN];

    *vnet_num = 0;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "gw_launch_server='%s'", server_addr);

    return lc_db_table_count("vnet", "*", condition, vnet_num);
}

int lc_vmdb_vnet_update_state(int state, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_vnet_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vnet_update_server(char *server_addr, uint32_t vnet_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", vnet_id);
    sprintf(buffer, "'%s'", server_addr);

    if (lc_db_vnet_update("gw_launch_server", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error buffer=%s condition=%s \n",
                __FUNCTION__, buffer, condition);
        return -1;
    }

    return 0;
}

int lc_db_compute_resource_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("compute_resource", column, value, condition);
}

int lc_vmdb_compute_resource_state_update(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", state);

    if (lc_db_compute_resource_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_ipmi_load_by_type(ipmi_t *ipmi, int id, int type)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "device_id='%d' and device_type='%d'", id, type);

    result = lc_db_ipmi_load(ipmi, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_nsp_host_load(nsp_host_t *nsp_host, char *ip)
{
    host_t host = {0};
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_compute_resource_load(&host, "id", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        nsp_host->cr_id = host.id;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=1");

    result = lc_db_table_load(nsp_host, "fdb_user", "*", condition, email_address_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}
