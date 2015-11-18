/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-12
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lc_snf.h"
#include "lc_db.h"
#include "lcs_utils.h"
#include "lcs_usage_db.h"

static int lcs_traffic_vif_process(void *intf_info,
        char *field, char *value)
{
    agent_interface_id_t* pintf = (agent_interface_id_t*)intf_info;

    if (strcmp(field, "id") == 0) {
        pintf->vif_id = atoi(value);
    } else if (strcmp(field, "mac") == 0) {
        snprintf(pintf->mac, AGENT_MAC_LEN, "%s", value);
    } else if (strcmp(field, "vlantag") == 0) {
        pintf->vlan_tag = atoi(value);
    } else if (strcmp(field, "ofport") == 0) {
        pintf->ofport = atoi(value);
    } else if (strcmp(field, "ifindex") == 0) {
        pintf->if_index = atoi(value);
    } else if (strcmp(field, "subnetid") == 0) {
        pintf->if_subnetid = atoi(value);
    } else if (strcmp(field, "devicetype") == 0) {
        pintf->device_type = atoi(value);
    } else if (strcmp(field, "deviceid") == 0) {
        pintf->device_id = atoi(value);
    }

    return 0;
}

static int lcs_usage_host_process(void *host_info, char *field, char *value)
{
    host_device_t *phost = (host_device_t *)host_info;

    if (strcmp(field, "id") == 0) {
        phost->id = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        snprintf(phost->ip, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "htype") == 0) {
        phost->htype = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        phost->type = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        snprintf(phost->name, LC_NAMESIZE, "%s", value);
    }

    return 0;
}

static int lcs_usage_switch_process(void *switch_info, char *field, char *value)
{
    network_device_t *pswitch = (network_device_t *)switch_info;

    if (strcmp(field, "id") == 0) {
        pswitch->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        snprintf(pswitch->name, LC_NAMESIZE, "%s", value);
    } else if (strcmp(field, "mip") == 0) {
        snprintf(pswitch->mip, MAX_HOST_ADDRESS_LEN, "%s", value);
    }

    return 0;
}

static int lcs_usage_lcg_process(void *lcg_info, char *field, char *value)
{
    lcg_device_t *plcg = (lcg_device_t *)lcg_info;

    if (strcmp(field, "id") == 0) {
        plcg->id = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        snprintf(plcg->ip, MAX_HOST_ADDRESS_LEN, "%s", value);
    } else if (strcmp(field, "htype") == 0) {
        plcg->htype = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        plcg->type = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        snprintf(plcg->name, LC_NAMESIZE, "%s", value);
    }else if(strcmp(field,"rackid") == 0){
		plcg->rackid = atoi(value);
	}

    return 0;
}
static int lcs_traffic_str_process(void *str, char *field, char *value)
{
    char *ps = (char *)str;

    if (!str || !field || !value) {
        return -1;
    }

    strcpy(ps, value);
    return 0;
}

static int vif_process(void *intf_info, char *field, char *value)
{
    intf_t* pintf = (intf_t*) intf_info;

    if (strcmp(field, "id") == 0) {
        pintf->if_id = atoi(value);
    } else if (strcmp(field, "ifindex") == 0) {
        pintf->if_index = atoi(value);
    } else if (strcmp(field, "mac") == 0) {
        strcpy(pintf->if_mac, value);
    }

    return 0;
}

int lcs_traffic_db_vif_load_by_hostip(agent_interface_id_t *vintf_info,
        int buf_size, char *hostip)
{
    char condition[LC_DB_BUF_LEN] = {0};
    char ids[MAX_VM_PER_HOST][LC_DB_INTEGER_LEN];
    int i, nvm = 0, ngw = 0, len = 0, ret = 0;

    if (!vintf_info || !hostip) {
        return -1;
    }

    memset(ids, 0, sizeof(ids));

    /* get vm & vgw */

    snprintf(condition, LC_DB_BUF_LEN, "launch_server='%s' AND state=%d",
            hostip, TABLE_VM_STATE_RUNNING);
    ret = lc_db_table_loadn(ids, LC_DB_INTEGER_LEN, MAX_VM_PER_HOST,
            "vm", "id", condition, lcs_traffic_str_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        return -1;
    }
    for (nvm = 0; nvm < MAX_VM_PER_HOST && ids[nvm][0]; ++nvm) {
    }

    snprintf(condition, LC_DB_BUF_LEN, "gw_launch_server='%s' AND state=%d",
            hostip, TABLE_VNET_STATE_RUNNING);
    ret = lc_db_table_loadn(ids + nvm, LC_DB_INTEGER_LEN, MAX_VM_PER_HOST - nvm,
            "vnet", "id", condition, lcs_traffic_str_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        return -1;
    }
    for (ngw = nvm; ngw < MAX_VM_PER_HOST && ids[ngw][0]; ++ngw) {
    }

    SNF_SYSLOG(LOG_DEBUG, "nvm=%d, ngw=%d, %s.", nvm, ngw - nvm, hostip);

    /* get vif */

    len = snprintf(condition, LC_DB_BUF_LEN,
            "state=%d AND ((devicetype=%d AND deviceid IN(-1",
            LC_VIF_STATE_ATTACHED, LC_VIF_DEVICE_TYPE_VM);
    for (i = 0; i < nvm; ++i) {
        len += snprintf(condition + len, LC_DB_BUF_LEN - len, ",%s", ids[i]);
    }
    len += snprintf(condition + len, LC_DB_BUF_LEN - len,
            ")) OR (devicetype=%d AND deviceid IN(-1",
            LC_VIF_DEVICE_TYPE_GW);
    for (i = nvm; i < ngw; ++i) {
        len += snprintf(condition + len, LC_DB_BUF_LEN - len, ",%s", ids[i]);
    }
    len += snprintf(condition + len, LC_DB_BUF_LEN - len, ")))");

    ret = lc_db_table_loadn(vintf_info, sizeof(agent_interface_id_t), buf_size,
                "vinterface", "*", condition, lcs_traffic_vif_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        return -1;
    }

    /* fill vm-uuid */

    for (i = 0; i < buf_size && vintf_info[i].vif_id; ++i) {
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", vintf_info[i].device_id);
        if (vintf_info[i].device_type == LC_VIF_DEVICE_TYPE_VM) {
            ret = lc_db_table_load(vintf_info[i].vm_uuid, "vm", "uuid",
                    condition, lcs_traffic_str_process);
            ret += lc_db_table_load(vintf_info[i].vm_name, "vm", "label",
                    condition, lcs_traffic_str_process);
        } else if (vintf_info[i].device_type == LC_VIF_DEVICE_TYPE_GW) {
            ret = lc_db_table_load(vintf_info[i].vm_uuid, "vnet", "uuid",
                    condition, lcs_traffic_str_process);
            ret += lc_db_table_load(vintf_info[i].vm_name, "vnet", "label",
                    condition, lcs_traffic_str_process);
        }
        if (ret) {
            return -1;
        }
    }
    return i;
}

int lcs_traffic_db_vif_load_by_vm(agent_interface_id_t *vintf_info,
        int type, int vmid)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int i, len = 0, ret = 0;

    if (!vintf_info) {
        return -1;
    }

    /* get vif */
    len = snprintf(condition, LC_DB_BUF_LEN,
            "devicetype=%d AND deviceid=%d", type, vmid);
    ret = lc_db_table_loadn(vintf_info, sizeof(agent_interface_id_t), LC_VM_MAX_VIF,
                "vinterface", "*", condition, lcs_traffic_vif_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        return -1;
    }

    /* fill vm-uuid */
    for (i = 0; i < LC_VM_MAX_VIF && vintf_info[i].vif_id; ++i) {
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", vintf_info[i].device_id);
        if (vintf_info[i].device_type == LC_VIF_DEVICE_TYPE_VM) {
            ret = lc_db_table_load(vintf_info[i].vm_uuid, "vm", "uuid",
                    condition, lcs_traffic_str_process);
            ret += lc_db_table_load(vintf_info[i].vm_name, "vm", "label",
                    condition, lcs_traffic_str_process);
        } else if (vintf_info[i].device_type == LC_VIF_DEVICE_TYPE_GW) {
            ret = lc_db_table_load(vintf_info[i].vm_uuid, "vnet", "uuid",
                    condition, lcs_traffic_str_process);
            ret += lc_db_table_load(vintf_info[i].vm_name, "vnet", "label",
                    condition, lcs_traffic_str_process);
        }
        if (ret) {
            return -1;
        }
    }
    return i;
}

static int rackid_process(void *rack_info, char *field, char *value)
{
    int *prackid = (int *)rack_info;

    if (strcmp(field, "id") == 0) {
        *prackid = atoi(value);
    }

    return 0;
}

int lcs_usage_db_domain_rackid_loadn(int *rackid_infos,
        uint32_t rackid_size, int rackid_len, char *domain)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int n = 0, ret = 0;

    if (!rackid_infos) {
        return -1;
    }

    snprintf(condition, LC_DB_BUF_LEN,
            "domain='%s'", domain);
    ret = lc_db_table_loadn(rackid_infos, rackid_size, rackid_len,
                "rack", "id",
                condition, rackid_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: Failed to load rackid, ret=%d.\n",
                __FUNCTION__, ret);
        return -1;
    }

    for (n = 0; n < rackid_len && rackid_infos[n]; ++n) {
    }
    return n;
}

int lcs_usage_db_host_loadn(host_device_t *host_infos,
        uint32_t hostid_offset, int buf_size)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int n = 0, ret = 0;

    if (!host_infos) {
        return -1;
    }
    snprintf(condition, LC_DB_BUF_LEN,
            "id>=%d and domain='%s' ORDER BY id LIMIT %d",
            hostid_offset, lcsnf_domain.lcuuid, buf_size);
    ret = lc_db_table_loadn(host_infos, sizeof(host_device_t), buf_size,
                "host_device", "id,ip,htype,type,name",
                condition, lcs_usage_host_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: Failed to load host_device, ret=%d.\n",
                __FUNCTION__, ret);
        return -1;
    }

    for (n = 0; n < buf_size && host_infos[n].id; ++n) {
    }
    return n;
}

int lcs_usage_db_host_loadn_by_type(host_device_t *host_infos,
        uint32_t hostid_offset, int buf_size, int type)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int n = 0, ret = 0;

    if (!host_infos) {
        return -1;
    }

    snprintf(condition, LC_DB_BUF_LEN,
            "id>=%d and type=%d and domain='%s' ORDER BY id LIMIT %d",
            hostid_offset, type, lcsnf_domain.lcuuid, buf_size);
    ret = lc_db_table_loadn(host_infos, sizeof(host_device_t), buf_size,
                "host_device", "id,ip,htype,type,name",
                condition, lcs_usage_host_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: Failed to load host_device, ret=%d.\n",
                __FUNCTION__, ret);
        return -1;
    }

    for (n = 0; n < buf_size && host_infos[n].id; ++n) {
    }
    return n;
}

int lcs_usage_db_switch_loadn(network_device_t *switch_infos,
        uint32_t switchid_offset, int buf_size)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int n = 0, ret = 0;

    if (!switch_infos) {
        return -1;
    }
    snprintf(condition, LC_DB_BUF_LEN,
            "id>=%d and domain='%s' ORDER BY id LIMIT %d",
            switchid_offset, lcsnf_domain.lcuuid, buf_size);
    ret = lc_db_table_loadn(switch_infos, sizeof(network_device_t), buf_size,
                "network_device", "id,name,mip",
                condition, lcs_usage_switch_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: Failed to load network_device, ret=%d.\n",
                __FUNCTION__, ret);
        return -1;
    }

    for (n = 0; n < buf_size && switch_infos[n].id; ++n) {
    }
    return n;
}

/*kzs added for 3-party hw begin*/
int lcs_usage_db_lcg_loadn(lcg_device_t *lcg_infos,uint32_t lcg_offset, int buf_size)
{
    char condition[LC_DB_BUF_LEN] = {0};
    int n = 0, ret = 0;

    if (!lcg_infos) {
        return -1;
    }

    snprintf(condition, LC_DB_BUF_LEN,
            "id>=%d AND type=2 AND domain='%s' ORDER BY id LIMIT %d",
            lcg_offset, lcsnf_domain.lcuuid, buf_size);
    ret = lc_db_table_loadn(lcg_infos, sizeof(lcg_device_t), buf_size,
                "host_device", "id,ip,htype,type,name,rackid",
                condition, lcs_usage_lcg_process);
    if (ret && ret != LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: Failed to load lcg_device, ret=%d.\n",
                __FUNCTION__, ret);
        return -1;
    }
	/*统计lcg的信息*/
    for (n = 0; n < buf_size && lcg_infos[n].id; ++n)
	{
	}
    return n;
}
static int hwdev_query_process(void *hwdev_info, char *field, char *value)
{
    hwdev_query_info_t* phwdev = (hwdev_query_info_t*) hwdev_info;

    if (strcmp(field, "ctrl_ip") == 0) {
        strcpy(phwdev->hwdev_ip, value);
    } else if (strcmp(field, "community") == 0) {
        strcpy(phwdev->community, value);
    } else if (strcmp(field, "user_name") == 0) {
        strcpy(phwdev->username, value);
    } else if (strcmp(field, "user_passwd") == 0) {
       strcpy(phwdev->password, value);
    }
    return 0;
}
/*kzs added for 3-party hw end*/
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
    } else if (strcmp(field, "vnetid") == 0) {
        pvm->vdc_id = atoi(value);
    } else if (strcmp(field, "mem_size") == 0) {
        pvm->mem_total = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "userid") == 0) {
        pvm->user_id = atoi(value);
    }

    return 0;
}

int lc_db_vif_loadn(intf_t *intf_info, int max_count,
                    char *column_list, char *condition)
{
    return lc_db_table_loadn_order_by_id(intf_info, sizeof(intf_t), max_count,
                                         "vinterface", column_list, condition,
                                         vif_process);
}

int lc_get_vm_by_server (vm_query_info_t *vm_query, int offset, int *vm_num, char *server, int skip_tmp_state, int max)
{
    int i, j;
    int result;
    char condition[LC_DB_BUF_LEN];
    vm_query_info_t *p = 0;
    intf_t iface[LC_VM_IFACES_MAX];

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
    }

    for (i = 0; i < *vm_num; ++i) {
        p = vm_query + i;
        memset(condition, 0, LC_DB_BUF_LEN);
        sprintf(condition, "devicetype=1 AND deviceid=%d", p->vm_id);
        result = lc_db_vif_loadn(iface, LC_VM_IFACES_MAX, "*", condition);
        if (result == LC_DB_COMMON_ERROR) {
            LC_DB_LOG(LOG_ERR, "%s: cond: %s \n", __FUNCTION__, condition);
            return result;
        } else if (result == LC_DB_EMPTY_SET) {
            LC_DB_LOG(LOG_ERR, "%s: VIF not found: %s \n",
                      __FUNCTION__, condition);
            return LC_DB_VIF_NOT_FOUND;
        }
        for (j = 0; j < LC_VM_IFACES_MAX; ++j) {
            if (iface[j].if_index < 0 ||
                    iface[j].if_index >= LC_VM_IFACES_MAX) {
                LC_DB_LOG(LOG_ERR, "%s: VIF %d index %d out of range: %s \n",
                          __FUNCTION__, iface[j].if_id,
                          iface[j].if_index, condition);
                return LC_DB_VIF_NOT_FOUND;
            }
            p->all_ifid[iface[j].if_index] = iface[j].if_id;
        }
    }

    return LC_DB_VM_FOUND;
}

int lc_get_hwdev_by_server (hwdev_query_info_t *hwdev_query, int offset, int *hwdev_num, char *server, int max)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "launch_server='%s'&&state=3", server);

    result = lc_db_table_load_by_server(hwdev_query, "third_party_device", "*", condition,
                                             offset, hwdev_num, max, hwdev_query_process);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VM_NOT_FOUND;
    } else {
        return LC_DB_VM_FOUND;
    }
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
#if 0
    } else if (strcmp(field, "private_if4id") == 0) {
        pvm->pvt_ifid[3] = atoi(value);
    } else if (strcmp(field, "private_if5id") == 0) {
        pvm->pvt_ifid[4] = atoi(value);
#endif
    } else if (strcmp(field, "public_if1id") == 0) {
        pvm->pub_ifid[0] = atoi(value);
    } else if (strcmp(field, "public_if2id") == 0) {
        pvm->pub_ifid[1] = atoi(value);
    } else if (strcmp(field, "public_if3id") == 0) {
        pvm->pub_ifid[2] = atoi(value);
    } else if (strcmp(field, "id") == 0) {
        pvm->vm_id = atoi(value);
    } else if (strcmp(field, "label") == 0) {
        strcpy(pvm->vm_label, value);
    } else if (strcmp(field, "vdcid") == 0) {
        pvm->vdc_id = atoi(value);
    } else if (strcmp(field, "userid") == 0) {
        pvm->user_id = atoi(value);
    }

    return 0;
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

int lc_mon_get_config_info(monitor_config_t *pcfg,
                              monitor_host_t *pmon_host, int is_host)
{
    lc_get_config_info_from_system_config(pcfg);

    if (is_host) {
        pmon_host->host_cpu_threshold = pcfg->host_cpu_threshold;
        pmon_host->host_cpu_warning_usage = pcfg->host_cpu_warning_usage;
        pmon_host->host_mem_threshold = pcfg->host_mem_threshold;
        pmon_host->host_mem_warning_usage = pcfg->host_mem_warning_usage;
        pmon_host->host_disk_threshold = pcfg->host_disk_threshold;
        pmon_host->host_disk_warning_usage = pcfg->host_disk_warning_usage;
        pmon_host->host_check_interval = pcfg->host_warning_interval;
    } else {
        pmon_host->vm_cpu_threshold = pcfg->vm_cpu_threshold;
        pmon_host->vm_cpu_warning_usage = pcfg->vm_cpu_warning_usage;
        pmon_host->vm_mem_threshold = pcfg->vm_mem_threshold;
        pmon_host->vm_mem_warning_usage = pcfg->vm_mem_warning_usage;
        pmon_host->vm_disk_threshold = pcfg->vm_disk_threshold;
        pmon_host->vm_disk_warning_usage = pcfg->vm_disk_warning_usage;
        pmon_host->vm_check_interval = pcfg->vm_warning_interval;
    }
    if (pcfg->email_address[0]) {
        strcpy(pmon_host->email_address, pcfg->email_address);
        if (!strlen(pcfg->from)) {
            strcpy(pmon_host->from, "stats@yunshan.net.cn");
        } else {
            strcpy(pmon_host->from, pcfg->from);
        }
        pmon_host->email_enable = 1;
    } else {
        pmon_host->email_enable = 0;
    }
    if (pcfg->email_address_ccs[0]) {
        strcpy(pmon_host->email_address_ccs, pcfg->email_address_ccs);
    }

    return 0;

}

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
    } else if (strcmp(field, "ccs") == 0) {
        strcpy(pcfg->email_address_ccs, value);
    }
    return 0;
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
             SNF_SYSLOG(LOG_ERR, "%s: error=%d\n", __FUNCTION__, result);
             return result;
         } else if (result == LC_DB_EMPTY_SET) {
             SNF_SYSLOG(LOG_ERR, "%s: system config table is NULL\n", __FUNCTION__);
             return -1;
         }
    }
    id = 1;
    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    memset(cfg_info->email_address, 0, MAIL_ADDR_LEN);
    result = lc_db_table_load((void *)cfg_info, "fdb_user", "email", condition, email_address_process);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: get email address error", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: system config table is NULL", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_db_compute_resource_multi_update(char * columns, char * condition)
{
    return lc_db_master_table_multi_update("compute_resource", columns, condition);
}

int lc_vmdb_compute_resource_usage_update(char *cpu_usage, int cpu_used, int mem_usage,
                                          int disk_usage, int ha_disk_usage, char *ip)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "cpu_usage=\'%s\', cpu_used=\'%d\', mem_usage=\'%d\', disk_usage=\'%d\', ha_disk_usage=\'%d\'",
            cpu_usage, cpu_used, mem_usage, disk_usage, ha_disk_usage);

    if (lc_db_compute_resource_multi_update(buffer, condition) != 0) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_usage_update_v2(char *cpu_usage, int cpu_used,
        int mem_total, int mem_usage, int disk_usage, int ha_disk_usage, char *ip)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "cpu_usage=\'%s\', cpu_used=\'%d\', mem_total=\'%d\', mem_usage=\'%d\', disk_usage=\'%d\', ha_disk_usage=\'%d\'",
            cpu_usage, cpu_used, mem_total, mem_usage, disk_usage, ha_disk_usage);

    if (lc_db_compute_resource_multi_update(buffer, condition) != 0) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_shared_storage_resource_load(shared_storage_resource_t *ssr, char *sr_name, int rack_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "sr_name='%s' and rackid=%d", sr_name, rack_id);

    if (lc_db_shared_storage_resource_load(ssr, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

static int shared_storage_resource_process(void *ssr_info, char *field, char *value)
{
    shared_storage_resource_t *pssr = (shared_storage_resource_t *)ssr_info;

    if (strcmp(field, "id") == 0) {
        pssr->storage_id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pssr->storage_state = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        pssr->storage_flag = atoi(value);
    } else if (strcmp(field, "htype") == 0) {
        pssr->storage_htype = atoi(value);
    } else if (strcmp(field, "sr_name") == 0) {
        strncpy(pssr->sr_name, value, MAX_SR_NAME_LEN - 1);
    } else if (strcmp(field, "disk_total") == 0) {
        pssr->disk_total = atoi(value);
    } else if (strcmp(field, "disk_used") == 0) {
        pssr->disk_used = atoi(value);
    }

    return 0;
}

int lc_db_shared_storage_resource_load(shared_storage_resource_t *sr_info, char *column_list, char *condition)
{
    return lc_db_table_load(sr_info, "shared_storage_resource", column_list, condition, shared_storage_resource_process);
}

static int compute_resource_process(void *host_info, char *field, char *value)
{
    host_t* phost = (host_t*) host_info;

    /* TODO: Add or remove columns */
    if (strcmp(field, "id") == 0) {
        phost->cr_id = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        phost->host_role = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(phost->host_address, value);
    } else if (strcmp(field, "dbr_peer_server") == 0) {
        strcpy(phost->peer_address, value);
    } else if (strcmp(field, "cpu_info") == 0) {
        strcpy(phost->cpu_info, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(phost->host_name, value);
    } else if (strcmp(field, "flag") == 0) {
        phost->host_flag = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        phost->host_state = atoi(value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(phost->host_description, value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(phost->host_uuid, value);
    } else if (strcmp(field, "user_name") == 0) {
        strcpy(phost->username, value);
    } else if (strcmp(field, "user_passwd") == 0) {
        strcpy(phost->password, value);
    } else if (strcmp(field, "max_vm") == 0) {
        phost->max_vm = atoi(value);
    } else if (strcmp(field, "poolid") == 0) {
        phost->pool_index = atoi(value);
    } else if (strcmp(field, "role") == 0) {
        phost->role = atoi(value);
    } else if (strcmp(field, "health_state") == 0) {
        phost->health_check = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        phost->error_number = atoi(value);
    } else if (strcmp(field, "cpu_start_time") == 0) {
        phost->cpu_start_time = atol(value);
    } else if (strcmp(field, "disk_start_time") == 0) {
        phost->disk_start_time = atol(value);
    } else if (strcmp(field, "traffic_start_time") == 0) {
        phost->traffic_start_time = atol(value);
    } else if (strcmp(field, "check_start_time") == 0) {
        phost->check_start_time = atol(value);
    } else if (strcmp(field, "stats_time") == 0) {
        phost->stats_time = atol(value);
    } else if (strcmp(field, "mem_total") == 0) {
        phost->mem_total = atoi(value);
    } else if (strcmp(field, "mem_alloc") == 0) {
        phost->mem_alloc = atoi(value);
    } else if (strcmp(field, "mem_usage") == 0) {
        phost->mem_usage = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        phost->disk_total = atoi(value);
    } else if (strcmp(field, "disk_alloc") == 0) {
        phost->disk_alloc = atoi(value);
    } else if (strcmp(field, "service_flag") == 0) {
        phost->host_service_flag = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        phost->rack_id = atoi(value);
    }
    return 0;
}

static int storage_connection_process(void *storage_connection, char *field, char *value)
{
    storage_connection_t* sct = (storage_connection_t*) storage_connection;

    /* TODO: Add or remove columns */
    if (strcmp(field, "id") == 0) {
        sct->id = atoi(value);
    } else if (strcmp(field, "host_address") == 0) {
        strcpy(sct->host_address, value);
    } else if (strcmp(field, "storage_id") == 0) {
        sct->storage_id = atoi(value);
    } else if (strcmp(field, "active") == 0) {
        sct->active = atoi(value);
    }
    return 0;
}

static int storage_process(void *storage, char *field, char *value)
{
    db_storage_t* st = (db_storage_t*) storage;

    /* TODO: Add or remove columns */
    if (strcmp(field, "id") == 0) {
        st->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(st->name, value);
    } else if (strcmp(field, "rack_id") == 0) {
        st->rack_id = atoi(value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(st->uuid, value);
    } else if (strcmp(field, "is_shared") == 0) {
        st->is_shared = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        st->disk_total = atoi(value);
    } else if (strcmp(field, "disk_used") == 0) {
        st->disk_used = atoi(value);
    } else if (strcmp(field, "disk_alloc") == 0) {
        st->disk_alloc = atoi(value);
    }
    return 0;
}

static int lc_db_storage_connection_load_all(storage_connection_t* storage_connection, char *column_list, char *condition, int offset, int *host_num)
{
    return lc_db_table_load_all(storage_connection, "storage_connection", column_list, condition, offset, host_num, storage_connection_process);
}

int lc_vmdb_storage_connection_get_num_by_ip(int *conn_num, char *ip)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, " host_address='%s' ", ip);

    return lc_db_table_count("storage_connection", "*", condition, conn_num);
}

int lc_vmdb_storage_connection_load_all(storage_connection_t *storage_connection, char *ip, int *host_num)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "host_address='%s' and active=1", ip);

    result = lc_db_storage_connection_load_all(storage_connection, "*", condition, sizeof(storage_connection_t), host_num);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

static int lc_db_storage_load(db_storage_t* storage, char *column_list, char *condition)
{
    return lc_db_table_load(storage, "storage", column_list, condition, storage_process);
}

int lc_vmdb_storage_load(db_storage_t* storage, int id)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", id);

    result = lc_db_storage_load(storage, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_db_compute_resource_load(host_t* host_info, char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "compute_resource", column_list, condition, compute_resource_process);
}

int lc_vmdb_compute_resource_load (host_t *host, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_compute_resource_load(host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_get_vm_from_db_by_namelable(vm_info_t *vm_info, char *name_lable)
{
    int code;

    code = lc_vmdb_vm_load_by_namelable(vm_info, name_lable);

    if (code == LC_DB_VM_NOT_FOUND) {
        SNF_SYSLOG(LOG_ERR, "%s vm%s not found\n", __FUNCTION__, name_lable);
    }

    return code;
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

int lc_get_vm_from_db_by_id(vm_info_t *vm_info, int id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

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

int lc_get_vedge_from_db_by_namelable(vm_info_t *vm_info, char *name_lable)
{
    int code;

    code = lc_vmdb_vedge_load_by_namelable(vm_info, name_lable);

    if (code == LC_DB_VM_NOT_FOUND) {
        SNF_SYSLOG(LOG_ERR, "%s vm%s not found\n", __FUNCTION__, name_lable);
    }

    return code;
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

int lc_get_vedge_from_db_by_str(vm_info_t *vm_info, char *str)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, " %s ", str);

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
int lc_get_vedge_from_db_by_id(vm_info_t *vm_info, int id)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

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

int lc_get_email_address_from_user_db(char *email, int user_id)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", user_id);

    if (lc_db_table_load((void*)email, "user", "email", condition,
        get_email_address) != 0) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_db_vm_load(vm_info_t* vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_info, "vm", column_list, condition, vm_process);
}

int lc_db_vedge_load(vm_info_t* vm_info, char *column_list, char *condition)
{
    return lc_db_table_load(vm_info, "vnet", column_list, condition, vedge_process);
}

int lc_db_vinterface_load(intf_t* intf_info, char *column_list, char *condition)
{
    return lc_db_table_load(intf_info, "vinterface", column_list, condition, vif_process);
}

int lc_db_lb_listener_load(lb_listener_t* listener_info, char *column_list, char *condition)
{
    return lc_db_table_load(listener_info, "lb_listener", column_list, condition,
                            lb_listener_process);
}

int get_email_address(void *email, char *field, char *value)
{
    char* email_address= (char *) email;
    if (strcmp(field, "email") == 0) {
        strcpy(email_address, value);
    }
    return 0;
}

int vm_process(void *vm_info, char *field, char *value)
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
        strcpy(pvm->host_name, value);
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
    } else if (strcmp(field, "lcuuid") == 0) {
        strcpy(pvm->vm_lcuuid, value);
    }

    return 0;
}

int vedge_process(void *vm_info, char *field, char *value)
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
        strcpy(pvm->host_name, value);
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
    } else if (strcmp(field, "eventid") == 0) {
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
    }

    return 0;
}

static int host_device_process(void *host_info, char *field, char *value)
{
    host_t* phost = (host_t*) host_info;

    /* TODO: Add or remove columns */
    if (strcmp(field, "ip") == 0) {
        strcpy(phost->host_address, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(phost->host_name, value);
    } else if (strcmp(field, "state") == 0) {
        phost->host_state = atoi(value);
    } else if (strcmp(field, "htype") == 0) {
        phost->host_htype = atoi(value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(phost->host_description, value);
    } else if (strcmp(field, "user_name") == 0) {
        strcpy(phost->username, value);
    } else if (strcmp(field, "user_passwd") == 0) {
        strcpy(phost->password, value);
    } else if (strcmp(field, "pool") == 0) {
        phost->pool_index = atoi(value);
    } else if (strcmp(field, "inf0type") == 0) {
        phost->if0_type = atoi(value);
    } else if (strcmp(field, "inf1type") == 0) {
        phost->if1_type = atoi(value);
    } else if (strcmp(field, "inf2type") == 0) {
        phost->if2_type = atoi(value);
    } else if (strcmp(field, "cpu_info") == 0) {
        strcpy(phost->cpu_info, value);
    } else if (strcmp(field, "mem_total") == 0) {
        phost->mem_total = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        phost->disk_total = atoi(value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        phost->vcpu_num = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        phost->host_role = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        phost->rack_id = atoi(value);
    } else if (strcmp(field, "vmware_cport") == 0) {
        phost->vmware_cport = atoi(value);
    } else if (strcmp(field, "master_ip") == 0) {
        strcpy(phost->master_address, value);
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

static int lc_db_host_device_load(host_t* host_info, char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "host_device", column_list, condition, host_device_process);
}

int lc_vmdb_host_device_load (host_t *host, char *ip, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_device_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

static int lc_db_host_device_usage_load(host_device_t* host_info,
        char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "host_device", column_list, condition,
            lcs_usage_host_process);
}

int lc_vmdb_host_device_usage_load(host_device_t *host, char *ip, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_device_usage_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_load_by_id(host_t *host, int id, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_host_device_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int get_poolid_process(void *poolid, char *field, char *value)
{
    int *poolid_p = (int *)poolid;
    if (strcmp(field, "poolid") == 0) {
        *poolid_p = atoi(value);
    }
    return 0;
}

int lc_get_poolid_from_compute_resource(int *poolid, char *ip)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    if (lc_db_table_load((void*)poolid, "compute_resource", "poolid", condition,
        get_poolid_process) != 0) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

static int lcs_host_process(void *p, char *field, char *value)
{
    host_t *host = (host_t *)p;

    if (strcmp(field, "id") == 0) {
        host->id = atoi(value);
    } else if (strcmp(field, "inf0type") == 0) {
        host->if0_type = atoi(value);
    } else if (strcmp(field, "inf1type") == 0) {
        host->if1_type = atoi(value);
    } else if (strcmp(field, "inf2type") == 0) {
        host->if2_type = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        host->host_role = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        host->rack_id = atoi(value);
    }
    return 0;
}

int lcs_db_host_load(host_t* host, char *column_list, char *condition)
{
    return lc_db_table_load(host, "host_device", column_list, condition, lcs_host_process);
}

static int lcs_rack_process(void *rack_info, char *field, char *value)
{
    rack_t *p = (rack_t *) rack_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "clusterid") == 0) {
        p->cluster_id = atoi(value);
    }

    return 0;
}

int lcs_db_rack_load(rack_t* rack_info, char *column_list, char *condition)
{
    return lc_db_table_load(rack_info, "rack", column_list, condition,
            lcs_rack_process);
}

static int lcs_cluster_process(void *cluster_info, char *field, char *value)
{
    cluster_t *p = (cluster_t *) cluster_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "vc_datacenter_id") == 0) {
        p->vcdc_id = atoi(value);
    }

    return 0;
}

int lcs_db_cluster_load(cluster_t* cluster_info, char *column_list, char *condition)
{
    return lc_db_table_load(cluster_info, "cluster", column_list, condition,
            lcs_cluster_process);
}

static int lcs_vcdc_process(void *vcdc_info, char *field, char *value)
{
    vcdc_t *p = (vcdc_t *) vcdc_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "vswitch_name") == 0) {
        strncpy(p->vswitch_name, value, LC_NAMESIZE - 1);
    }

    return 0;
}

int lcs_db_vcdc_load(vcdc_t* vcdc_info, char *column_list, char *condition)
{
    return lc_db_table_load(vcdc_info, "vc_datacenter", column_list, condition,
            lcs_vcdc_process);
}

int lcs_db_vcdc_load_by_host_id(vcdc_t *vcdc, int host_id)
{
    char condition[LC_DB_BUF_LEN];
    int result;
    host_t host;
    rack_t rack;
    cluster_t cluster;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", host_id);
    memset(&host, 0, sizeof host);
    result = lcs_db_host_load(&host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: host (%d) not found\n", __FUNCTION__, host_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%u", host.rack_id);
    memset(&rack, 0, sizeof rack);
    result = lcs_db_rack_load(&rack, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: rack (%d) not found\n", __FUNCTION__, host.rack_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", rack.cluster_id);
    memset(&cluster, 0, sizeof cluster);
    result = lcs_db_cluster_load(&cluster, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: cluster (%d) not found\n", __FUNCTION__,
                rack.cluster_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", cluster.vcdc_id);
    result = lcs_db_vcdc_load(vcdc, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        SNF_SYSLOG(LOG_ERR, "%s: vcdc (%d) not found\n", __FUNCTION__,
                cluster.vcdc_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    return LC_DB_VCDC_FOUND;
}

static int vmwaf_query_process(void *info, char *field, char *value)
{
    vmwaf_query_info_lcs_t *p = (vmwaf_query_info_lcs_t *)info;

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

int lcs_db_vmwaf_load(vmwaf_query_info_lcs_t* vmwaf_info, char *column_list, char *condition)
{
    return lc_db_table_load(vmwaf_info, "vmwaf", column_list, condition, vmwaf_query_process);
}

int lcs_db_get_vmwaf_by_launch_server(vmwaf_query_info_lcs_t *vmwaf_query, char* launch_server)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "launch_server='%s'", launch_server);

    result = lcs_db_vmwaf_load(vmwaf_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "conditon=[%s]\n", condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VMWAF_NOT_FOUND;
    } else {
        return LC_DB_VMWAF_FOUND;
    }
}

int lc_vmdb_vinterface_load_by_type_and_id(intf_t *intf_info,
        int devicetype, int deviceid, int ifindex)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "devicetype=%d and deviceid=%d and ifindex=%d",
            devicetype, deviceid, ifindex);

    result = lc_db_vinterface_load(intf_info, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        SNF_SYSLOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_VIF_NOT_FOUND;
    } else {
        return LC_DB_VIF_FOUND;
    }
}

int get_vifid_process(void *vifid, char *field, char *value)
{
    int *vifid_p = (int *)vifid;
    if (strcmp(field, "id") == 0) {
        *vifid_p = atoi(value);
    }
    return 0;
}

int lc_get_vifid_by_mac(int *vifid, char *mac)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "mac='%s'", mac);

    if (lc_db_table_load((void *)vifid, "vinterface", "id", condition,
        get_vifid_process) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: invalid mac:%s\n", __FUNCTION__, mac);
        return -1;
    }
    return 0;
}
