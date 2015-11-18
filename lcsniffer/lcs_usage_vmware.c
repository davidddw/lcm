/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Shi Lei
 * Finish Date: 2013-07-11
 *
 */

#include "lcs_usage_db.h"
#include "cloudmessage.pb-c.h"
#include "lcs_usage_vmware.h"
#include "lc_header.h"
#include "lc_mongo_db.h"
#include "lcs_utils.h"

static void vl2_id_to_pg_name(int id, char *pg_name, int size)
{
    memset(pg_name, 0, size);
    snprintf(pg_name, size, LC_VCD_PG_PREFIX "%06d", id);
}

int lc_vcd_hosts_info_msg(char *host_address, void *buf)
{
    header_t hdr;
    int hdr_len = 0, msg_len = 0;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__HostsInfoReq host_info = CLOUDMESSAGE__HOSTS_INFO_REQ__INIT;

    hdr_len = get_message_header_pb_len();
    host_info.host_address = host_address;
    msg.host_info_request = &host_info;
    cloudmessage__message__pack(&msg, buf + hdr_len);

    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCSNFD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_host_usage_reply_msg(vmware_host_info_t *host, void *buf, int len)
{
    Cloudmessage__HostsInfoResp *resp = (Cloudmessage__HostsInfoResp *)buf;
    int i = 0, j =0, k = 0, r;
    int64_t disk_usage = 0;
    char sr_usage[AGENT_DISK_SIZE_LEN];

    if (!resp) {
        return 1;
    }
    r = resp->result;
    if (!r) {
        strncpy(host->host_address, resp->host_address,
                MAX_HOST_ADDRESS_LEN - 1);
        host->host_state = resp->status;
        host->cpu_count = resp->cpu_num;
        strncpy(host->cpu_usage, resp->cpu_usage, AGENT_CPU_USAGE_STR_LEN - 1);
        host->sr_count = resp->n_storage_info;
        host->sr_name = (char *)malloc(SR_NAME_LEN * host->sr_count);
        if (NULL == host->sr_name) {
            return 1;
        }
        memset(host->sr_name, 0, SR_NAME_LEN * host->sr_count);

        host->sr_disk_total = (int *)malloc(sizeof(int) * host->sr_count);
        if (NULL == host->sr_disk_total) {
            free(host->sr_name);
            return 1;
        }
        memset(host->sr_disk_total, 0, sizeof(int) * host->sr_count);

        host->sr_usage = (char *)malloc(AGENT_DISK_SIZE_LEN * host->sr_count);
        if (NULL == host->sr_usage) {
            free(host->sr_name);
            free(host->sr_disk_total);
            return 1;
        }
        memset(host->sr_usage, 0, AGENT_DISK_SIZE_LEN * host->sr_count);

        for (i = 0; i < resp->n_storage_info; ++i) {
            memset(sr_usage, 0, AGENT_DISK_SIZE_LEN);
            strncpy(host->sr_name + j, resp->storage_info[i]->ds_name, SR_NAME_LEN - 1);
            j += SR_NAME_LEN;
            host->sr_disk_total[i] = resp->storage_info[i]->disk_total;
            disk_usage = resp->storage_info[i]->disk_total
                         - resp->storage_info[i]->disk_free;
            snprintf(sr_usage, AGENT_DISK_SIZE_LEN, "%"PRId64, disk_usage);
            strncpy(host->sr_usage + k, sr_usage, AGENT_DISK_SIZE_LEN - 1);
            k += AGENT_DISK_SIZE_LEN;
        }
    }
    return r;
}

int lc_vcd_vm_vif_usage_msg(agent_vm_vif_id_t *vm_intf, int count, void *buf)
{
    header_t hdr;
    vcdc_t vcdc;
    int hdr_len = 0, msg_len = 0;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMVifUsageReq vmvif_stats = CLOUDMESSAGE__VMVIF_USAGE_REQ__INIT;
    Cloudmessage__VMIntfInfo *vm_vif_info;
    Cloudmessage__IntfInfo *vif_info;
    Cloudmessage__IntfInfo *free_vif_info;
    char *pg_name;
    int i;

    hdr_len = get_message_header_pb_len();
    vmvif_stats.n_vm_intf_info = count;
    vmvif_stats.vm_intf_info = (Cloudmessage__VMIntfInfo **)malloc(count * sizeof(Cloudmessage__VMIntfInfo *));
    vm_vif_info = (Cloudmessage__VMIntfInfo *)malloc(count * sizeof(Cloudmessage__VMIntfInfo));
    vif_info = (Cloudmessage__IntfInfo *)malloc(count * sizeof(Cloudmessage__IntfInfo));
    free_vif_info = vif_info;
    pg_name = (char *)malloc(count * LC_NAMESIZE * sizeof(char));
    if (!vmvif_stats.vm_intf_info || ! vm_vif_info || !vif_info || !pg_name) {
        if (vmvif_stats.vm_intf_info) {
            free(vmvif_stats.vm_intf_info);
        }
        if (vm_vif_info) {
            free(vm_vif_info);
        }
        if (vif_info) {
            free(vif_info);
        }
        if (pg_name) {
            free(pg_name);
        }
        return -1;
    }
    memset(pg_name, 0, count * LC_NAMESIZE * sizeof(char));

    for (i = 0; i < count; ++i) {
        cloudmessage__vmintf_info__init(&vm_vif_info[i]);
        vm_vif_info[i].vm_name = vm_intf[i].vm_label;
        vm_vif_info[i].vm_type = vm_intf[i].vm_type;
        vm_vif_info[i].vm_id = vm_intf[i].vm_id;
        vm_vif_info[i].vdc_id = vm_intf[i].vdc_id;
        vm_vif_info[i].host_id = vm_intf[i].host_id;
        cloudmessage__intf_info__init(vif_info);
        vif_info->mac = vm_intf[i].vifs[0].mac;
        if(0 != lcs_db_vcdc_load_by_host_id(&vcdc, vm_intf[i].host_id)){
            SNF_SYSLOG(LOG_ERR,
                    "%s: switch name is not found according to hostid %u.\n",
                    __FUNCTION__, vm_intf[i].host_id);
            free(vmvif_stats.vm_intf_info);
            free(vm_vif_info);
            free(free_vif_info);
            free(pg_name);
            return -1;
        }
        vl2_id_to_pg_name(
                vm_intf[i].vifs[0].if_subnetid,
                pg_name + i * LC_NAMESIZE,
                LC_NAMESIZE);
        vif_info->portgroup_name = pg_name + i * LC_NAMESIZE;
        vif_info->if_index = vm_intf[i].vifs[0].if_index;
        vif_info->vif_id = vm_intf[i].vifs[0].vif_id;
        vm_vif_info[i].vm_intf = vif_info;
        vmvif_stats.vm_intf_info[i] = &vm_vif_info[i];
        vif_info++;
    }

    msg.vm_vif_usage_request = &vmvif_stats;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCSNFD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    free(vmvif_stats.vm_intf_info);
    free(vm_vif_info);
    free(free_vif_info);
    free(pg_name);

    return hdr_len + msg_len;
}

int lc_vcd_vm_vif_reply_msg(msg_vm_info_response_t *vms, void *buf, int len)
{
    int r, i, j, count;
    uint32_t ifindex = 0;
    vm_result_t *vm_result_p;
    Cloudmessage__VMVifUsageResp *resp = (Cloudmessage__VMVifUsageResp *)buf;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    count = resp->n_vm_vif_usage;
    if(count > MAX_VM_PER_HOST){
        count = MAX_VM_PER_HOST;
    }
    vms->n = count;
    if (!r) {
        for (i = 0; i < count; ++i) {
            vm_result_p = &vms->data[i];
            vm_result_p->curr_time = time(NULL);
            strncpy(vm_result_p->vm_label, resp->vm_vif_usage[i]->vm_name,
                    MAX_VM_NAME_LEN - 1);

            switch (resp->vm_vif_usage[i]->vm_state) {
            case CLOUDMESSAGE__VMSTATES__VM_STATS_ON:
                sprintf(vm_result_p->state, "running");
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_OFF:
                sprintf(vm_result_p->state, "halted");
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_SUSPEND:
                sprintf(vm_result_p->state, "suspended");
                break;
            default:
                break;
            }

            strncpy(vm_result_p->cpu_usage, resp->vm_vif_usage[i]->cpu_usage,
                    MAX_VM_CPU_USAGE_LEN - 1);
            vm_result_p->vm_type = resp->vm_vif_usage[i]->vm_type;
            vm_result_p->vm_id = resp->vm_vif_usage[i]->vm_id;
            vm_result_p->vdc_id = resp->vm_vif_usage[i]->vdc_id;
            vm_result_p->host_id = resp->vm_vif_usage[i]->host_id;
            snprintf(vm_result_p->memory_free, AGENT_MEMORY_SIZE_LEN, "%d", resp->vm_vif_usage[i]->mem_free);
            for(j = 0;j < resp->vm_vif_usage[i]->n_vif_usage;j++){
                ifindex = resp->vm_vif_usage[i]->vif_usage[j]->if_index;
                if(ifindex >= AGENT_VM_MAX_VIF){
                    continue;
                }
                vm_result_p->vifs[j].rx_bytes =
                    resp->vm_vif_usage[i]->vif_usage[j]->rx_bytes;
                vm_result_p->vifs[j].rx_dropped =
                    resp->vm_vif_usage[i]->vif_usage[j]->rx_dropped;
                vm_result_p->vifs[j].rx_errors =
                    resp->vm_vif_usage[i]->vif_usage[j]->rx_errors;
                vm_result_p->vifs[j].rx_packets =
                    resp->vm_vif_usage[i]->vif_usage[j]->rx_packets;
                vm_result_p->vifs[j].rx_bps =
                    8 * resp->vm_vif_usage[i]->vif_usage[j]->rx_bps;
                vm_result_p->vifs[j].rx_pps =
                    resp->vm_vif_usage[i]->vif_usage[j]->rx_pps;
                vm_result_p->vifs[j].tx_bytes =
                    resp->vm_vif_usage[i]->vif_usage[j]->tx_bytes;
                vm_result_p->vifs[j].tx_errors =
                    resp->vm_vif_usage[i]->vif_usage[j]->tx_errors;
                vm_result_p->vifs[j].tx_dropped =
                    resp->vm_vif_usage[i]->vif_usage[j]->tx_dropped;
                vm_result_p->vifs[j].tx_packets =
                    resp->vm_vif_usage[i]->vif_usage[j]->tx_packets;
                vm_result_p->vifs[j].tx_bps =
                    8 * resp->vm_vif_usage[i]->vif_usage[j]->tx_bps;
                vm_result_p->vifs[j].tx_pps =
                    resp->vm_vif_usage[i]->vif_usage[j]->tx_pps;
                vm_result_p->vifs[j].vif_id =
                    resp->vm_vif_usage[i]->vif_usage[j]->vif_id;
            }
        }
    }

    return r;
}

