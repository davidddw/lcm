#include <inttypes.h>

#include "cloudmessage.pb-c.h"
#include "lc_mon_vmware.h"
#include "lc_header.h"
#include "lc_db_vm.h"

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

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_host_usage_reply_msg(vmware_host_info_t *host, void *buf, int len)
{
    Cloudmessage__Message *msg = NULL;
    Cloudmessage__HostsInfoResp *resp = NULL;
    int i = 0, j =0, k = 0, r;
    int64_t disk_usage = 0;
    char sr_usage[AGENT_DISK_SIZE_LEN];

    msg = cloudmessage__message__unpack(NULL, len, (uint8_t *)buf);
    if (!msg) {
        return 1;
    }
    resp = msg->host_info_response;
    if (!resp) {
        return 1;
    }
    r = resp->result;
    if (!r) {
        strncpy(host->host_address, resp->host_address,
                MAX_HOST_ADDRESS_LEN - 1);
        host->host_state = resp->status;
        host->cpu_count = resp->cpu_num;
        host->role = resp->role;
        strncpy(host->cpu_usage, resp->cpu_usage, AGENT_CPU_USAGE_STR_LEN - 1);
        if (resp->n_storage_info > LC_MAX_SR_NUM) {
            host->sr_count = LC_MAX_SR_NUM;
        } else {
            host->sr_count = resp->n_storage_info;
        }
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

        for (i = 0; i < host->sr_count; ++i) {
            memset(sr_usage, 0, AGENT_DISK_SIZE_LEN);
            strncpy(host->sr_name + j, resp->storage_info[i]->ds_name, SR_NAME_LEN - 1);
            j += SR_NAME_LEN;
            host->sr_disk_total[i] = resp->storage_info[i]->disk_total;
            disk_usage = resp->storage_info[i]->disk_total
                         - resp->storage_info[i]->disk_free;
            snprintf(sr_usage, AGENT_DISK_SIZE_LEN, "%"PRId64, disk_usage*LC_GB);
            strncpy(host->sr_usage + k, sr_usage, AGENT_DISK_SIZE_LEN - 1);
            k += AGENT_DISK_SIZE_LEN;
        }
    }

    cloudmessage__message__free_unpacked(msg, NULL);

    return r;
}

int lc_vcd_vm_stats_msg(agent_vm_vif_id_t *vm, int count, void *buf)
{
    header_t hdr;
    int hdr_len = 0, msg_len = 0;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMStatsReq vm_stats = CLOUDMESSAGE__VMSTATS_REQ__INIT;
    int i;

    hdr_len = get_message_header_pb_len();
    vm_stats.n_vm_name = count;
    vm_stats.vm_name = (char **)malloc(count * sizeof(char *));
    if (!vm_stats.vm_name) {
        return -1;
    }
    for (i = 0; i < count; ++i) {
        vm_stats.vm_name[i] = vm[i].vm_label;
    }
    msg.vm_stats_request = &vm_stats;

    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    free(vm_stats.vm_name);
    return hdr_len + msg_len;
}

int lc_vcd_vm_stats_reply_msg(vcd_vm_stats_t *vm, int *count, void *buf, int len, int max)
{
    Cloudmessage__Message *msg = NULL;
    Cloudmessage__VMStatsResp *resp = cloudmessage__vmstats_resp__unpack(NULL, len, buf);
    int r, i;

    msg = cloudmessage__message__unpack(NULL, len, (uint8_t *)buf);
    if (!msg) {
        return 1;
    }
    resp = msg->vm_stats_response;
    if (!resp) {
        return 1;
    }
    r = resp->result;
    if (!r) {
        *count = resp->n_vm_stats;
        if (resp->n_vm_stats >= max) {
            *count = max;
            resp->n_vm_stats = max;
        }
        for (i = 0; i < resp->n_vm_stats; ++i) {
            strncpy(vm[i].vm_name, resp->vm_stats[i]->vm_name,
                    MAX_VM_NAME_LEN - 1);

            switch (resp->vm_stats[i]->vm_state) {
            case CLOUDMESSAGE__VMSTATES__VM_STATS_ON:
                vm[i].vm_state = LC_VM_STATE_RUNNING;
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_OFF:
                vm[i].vm_state = LC_VM_STATE_STOPPED;
                break;
            case CLOUDMESSAGE__VMSTATES__VM_STATS_SUSPEND:
                vm[i].vm_state = LC_VM_STATE_SUSPENDED;
                break;
            default:
                break;
            }

            strncpy(vm[i].cpu_usage, resp->vm_stats[i]->cpu_usage,
                    MAX_VM_CPU_USAGE_LEN - 1);
            strncpy(vm[i].host_address, resp->vm_stats[i]->host_address,
                    AGENT_IPV4_ADDR_LEN - 1);
        }
    }

    cloudmessage__message__free_unpacked(msg, NULL);

    return r;
}

