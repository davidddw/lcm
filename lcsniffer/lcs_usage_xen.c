/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Shi Lei
 * Finish Date: 2013-07-11
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "agent_msg_interface_info.h"
#include "lc_snf.h"

#include "lcs_utils.h"
#include "lc_mongo_db.h"
#include "lcs_usage_db.h"
#include "lc_db.h"
#include "lc_livegate_api.h"
#include "lc_lcmg_api.h"

#define INSERT_COLS "vif_id, \
rx_bytes, rx_errors, rx_dropped, rx_packets, rx_bps, rx_pps, \
tx_bytes, tx_errors, tx_dropped, tx_packets, tx_bps, tx_pps"
#define AGENT_LCSNF_PORT 20012
#define MAX_DATA_BUF_LEN (1 << 15)
#define SOCK_LCSNF_API_TIMEOUT 3

unsigned long long ntohll(unsigned long long val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN) {
       return (((unsigned long long )ntohl((int)((val << 32) >> 32))) << 32
               ) | (unsigned int)ntohl((int)(val >> 32));
    } else if (__BYTE_ORDER == __BIG_ENDIAN) {
       return val;
    }
}

unsigned long long htonll(unsigned long long val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN) {
        return (((unsigned long long )htonl((int)((val << 32) >> 32))) << 32
                ) | (unsigned int)htonl((int)(val >> 32));
    } else if (__BYTE_ORDER == __BIG_ENDIAN) {
        return val;
    }
}

int lcs_vif_traffic_response_parse(interface_result_t *pdata)
{
    int rv = 0;
    interface_result_t *pintf = pdata;
    traffic_usage_t entry;

    memset(&entry, 0, sizeof(traffic_usage_t));

    entry.timestamp = time(NULL);
    entry.vif_name = pintf->mac;
    entry.vif_id = pintf->vif_id;
    entry.rx_bytes = pintf->rx_bytes;
    entry.rx_errors = pintf->rx_errors;
    entry.rx_dropped = pintf->rx_dropped;
    entry.rx_packets = pintf->rx_packets;
    entry.rx_bps = pintf->rx_bps;
    entry.rx_pps = pintf->rx_pps;
    entry.tx_bytes = pintf->tx_bytes;
    entry.tx_errors = pintf->tx_errors;
    entry.tx_dropped = pintf->tx_dropped;
    entry.tx_packets = pintf->tx_packets;
    entry.tx_bps = pintf->tx_bps;
    entry.tx_pps = pintf->tx_pps;

    rv = mongo_db_entry_insert(MONGO_APP_TRAFFIC_USAGE, &entry);
    if (rv < 0) {
        return -1;
    }

    return 0;
}

static int log_vm_result_t(vm_result_t *pvm, int htype, const char *fun_name)
{
#ifdef DEBUG_LCSNFD
    int i, vifnum = 0;
    SNF_SYSLOG(LOG_DEBUG, \
               "%s: vm name label %s, "
               "type %u, "
               "vm_id %u, "
               "vdc_id %u, "
               "curr_time %u, "
               "host_id %u, "
               "state %s, "
               "cpu_number %s, "
               "cpu_usage %s, "
               "memory_total %s, "
               "memory_free %s, "
               "used_sys_disk %s, "
               "sys_disk_write_rate %s, "
               "sys_disk_read_rate %s, "
               "used_user_disk %s, "
               "user_disk_write_rate %s, "
               "user_disk_read_rate %s\n",
               fun_name,
               pvm->vm_label,
               pvm->vm_type,
               pvm->vm_id,
               pvm->vdc_id,
               pvm->curr_time,
               pvm->host_id,
               pvm->state,
               pvm->cpu_number,
               pvm->cpu_usage,
               pvm->memory_total,
               pvm->memory_free,
               pvm->used_sys_disk,
               pvm->sys_disk_write_rate,
               pvm->sys_disk_read_rate,
               pvm->used_user_disk,
               pvm->user_disk_write_rate,
               pvm->user_disk_read_rate
                );
    if(htype == LC_POOL_TYPE_XEN){
        vifnum = 7;
    }else{
        vifnum = 1;
    }
    for(i = 0;i < vifnum;i++){
        SNF_SYSLOG(LOG_DEBUG, "%s: "
                   "vm name label %s, "
                   "vif %d, "
                   "vif_id %u, "
                   "mac %s, "
                   "rx_bytes %"PRIu64", "
                   "rx_dropped %"PRIu64", "
                   "rx_errors %"PRIu64", "
                   "rx_packets %"PRIu64", "
                   "rx_bps %"PRIu64", "
                   "rx_pps %"PRIu64", "
                   "tx_bytes %"PRIu64", "
                   "tx_dropped %"PRIu64", "
                   "tx_errors %"PRIu64", "
                   "tx_packets %"PRIu64", "
                   "tx_bps %"PRIu64", "
                   "tx_pps %"PRIu64"\n",
                   fun_name,
                   pvm->vm_label,
                   i,
                   pvm->vifs[i].vif_id,
                   pvm->vifs[i].mac,
                   pvm->vifs[i].rx_bytes,
                   pvm->vifs[i].rx_dropped,
                   pvm->vifs[i].rx_errors,
                   pvm->vifs[i].rx_packets,
                   pvm->vifs[i].rx_bps,
                   pvm->vifs[i].rx_pps,
                   pvm->vifs[i].tx_bytes,
                   pvm->vifs[i].tx_dropped,
                   pvm->vifs[i].tx_errors,
                   pvm->vifs[i].tx_packets,
                   pvm->vifs[i].tx_bps,
                   pvm->vifs[i].tx_pps
                   );
    }
    for (i = 0; i < pvm->disk_num; ++i) {
        SNF_SYSLOG(LOG_DEBUG, "%s: "
                   "vm name label %s, "
                   "disk %s, "
                   "read_rate [%s], "
                   "write_rate [%s]\n",
                   fun_name,
                   pvm->vm_label,
                   pvm->disks[i].device,
                   pvm->disks[i].read_rate,
                   pvm->disks[i].write_rate
                   );
    }
#endif
    return 0;
}

int lcs_vm_vif_response_parse(vm_result_t *pvm, int htype)
{
    monitor_config_t mon_cfg;
    monitor_host_t mon_host;
    int real_state = 0;
    vm_info_t vm_info;
    memset(&vm_info, 0, sizeof(vm_info_t));

    memset(&mon_cfg, 0, sizeof(monitor_config_t));
    memset(&mon_host, 0, sizeof(monitor_host_t));

    log_vm_result_t(pvm,htype, __FUNCTION__);
    if(LC_VM_TYPE_GW == pvm->vm_type){
        lc_get_vedge_from_db_by_namelable(&vm_info, pvm->vm_label);
    }else{
        lc_get_vm_from_db_by_namelable(&vm_info, pvm->vm_label);
    }

    mon_host.htype = htype;
    lc_mon_get_config_info(&mon_cfg, &mon_host, 0);

    vm_info.vm_type = pvm->vm_type;

    real_state = lc_get_vm_current_state(pvm->state, vm_info.vm_type);

    if (real_state == LC_VM_STATE_RUNNING
        || real_state == LC_VEDGE_STATE_RUNNING) {
        lc_monitor_get_vm_res_usg_fields(pvm, &mon_host, &vm_info, htype);
    } else {
        SNF_SYSLOG(LOG_INFO,
            "Skipping to update the monitor result of this VM %s (%s)"\
            " on host %s, since the state is not running (%d, %s)",
            vm_info.vm_name, vm_info.vm_label,
            vm_info.host_name, real_state, pvm->state);
    }

    return 0;

}

static int message_handler_agent(lc_mbuf_hdr_t *hdr)
{
    msg_vm_info_response_t *vm_vif_info = NULL;

    switch (hdr->type) {
        case MSG_VM_INFO_RESPONSE:
            vm_vif_info = (msg_vm_info_response_t *)(hdr + 1);
            msg_vm_info_response_ntoh(vm_vif_info);
            break;
    }

    return 0;
}

static int lcs_send_msg_rt(lc_mbuf_t *msg, int len, char *ip, char *buf)
{
    struct sockaddr_in addr;
    lc_mbuf_hdr_t *hdr;
    int fd, nsend, nrecv, offset = 0;
    struct timeval timeout = { tv_sec: SOCK_LCSNF_API_TIMEOUT, tv_usec: 0 };

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton(ip, &addr.sin_addr);
    addr.sin_port = htons(AGENT_LCSNF_PORT);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
            (char *)&timeout, sizeof(struct timeval));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
            (char *)&timeout, sizeof(struct timeval));

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    nsend = send(fd, msg, len, 0);
    if (nsend < 0) {
        close(fd);
        return -1;
    }

    memset(buf, 0, MAX_DATA_BUF_LEN);
    if ((nrecv = recv(fd, buf, MAX_DATA_BUF_LEN, 0)) > 0) {
        if (nrecv >= sizeof(lc_mbuf_hdr_t)) {
            hdr = (lc_mbuf_hdr_t *)buf;
            lc_mbuf_hdr_ntoh(hdr);
            offset += nrecv;
            while (offset < hdr->length + sizeof(lc_mbuf_hdr_t)) {
                if ((nrecv = recv(fd, buf + offset, MAX_DATA_BUF_LEN - offset, 0)) > 0) {
                    offset += nrecv;
                } else {
                    break;
                }
            }
            if (offset != hdr->length + sizeof(lc_mbuf_hdr_t)) {
                SNF_SYSLOG(LOG_DEBUG, "Warning %s: offset=%d, hdr->length=%d",\
                        __FUNCTION__, offset, hdr->length);
                close(fd);
                return -1;
            }
            message_handler_agent(hdr);
        } else {
            close(fd);
            return -1;
        }
    } else {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int lcs_vm_vif_req_usage_rt(agent_vm_vif_id_t *vm, char *ip, int op, vm_result_t *vm_result)
{
    lc_mbuf_t msg;
    msg_vm_info_request_t *payload = (msg_vm_info_request_t *)msg.data;
    msg_vm_info_response_t *response;
    vm_query_t *task;
    int len, i;
    int ret = 0;
    char buf[MAX_DATA_BUF_LEN];
    lc_mbuf_hdr_t *hdr;

    if (!vm || !ip) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.hdr.magic = MSG_MG_SNF2AGN;
    msg.hdr.type = op;
    msg.hdr.length = sizeof(msg_vm_info_request_t) + sizeof(vm_query_t);
    len = sizeof(lc_mbuf_hdr_t) + msg.hdr.length;
    lc_mbuf_hdr_hton(&msg.hdr);
    payload->n = 1;
    task = payload->data;
    snprintf(task->vm_label, AGENT_LABEL_LEN, vm->vm_label);
    task->vm_type = vm->vm_type;
    task->vm_id = vm->vm_id;
    task->vdc_id = vm->vdc_id;
    task->host_id = vm->host_id;
    for (i = 0; i < AGENT_VM_MAX_VIF; ++i) {
        task->vifs[i].vif_id = vm->vifs[i].vif_id;
        snprintf(task->vifs[i].mac, AGENT_MAC_LEN, vm->vifs[i].mac);
        task->vifs[i].vlan_tag = vm->vifs[i].vlan_tag;
    }
    msg_vm_info_request_hton(payload);
    if((ret = lcs_send_msg_rt(&msg, len, ip, buf)) < 0) {
        return -1;
    }

    hdr = (lc_mbuf_hdr_t *)buf;
    response = (msg_vm_info_response_t *)(hdr + 1);
    memcpy(vm_result, response->data, sizeof(vm_result_t));

    return 0;
}

static int lcs_nsp_stat_process(char *result, agent_vm_vif_id_t *vgw, vm_result_t *vgw_stat)
{
    const nx_json *data = NULL, *datum = NULL, *item = NULL, *json = NULL;
    const nx_json *item_j = NULL, *json_j = NULL;
    intf_t vintf_info;
    int cpu_id, vgw_id, vif_id;
    double cpu_usage_i;
    char cpu_usage_a[LC_HOST_CPU_USAGE];
    int i, j, k;

    data = nx_json_get(*((const nx_json **)result), LCSNF_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json recvived, no data\n",
                __FUNCTION__);
        return -1;
    }

    memset(vgw_stat, 0, sizeof(vm_result_t));

    vgw_stat->vm_type = vgw->vm_type;
    vgw_stat->vm_id = vgw->vm_id;
    vgw_stat->vdc_id = vgw->vdc_id;
    vgw_stat->host_id = vgw->host_id;

    strcpy(vgw_stat->vm_label, vgw->vm_label);

    vgw_stat->curr_time = time(NULL);
    strcpy(vgw_stat->state, "running");

    /* CPU */
    datum = nx_json_get(data, "HOST_CPUS");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json recvived, no datum\n",
                __FUNCTION__);
        return -1;
    }
    i = 0;
    do {
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        json = nx_json_get(item, "CPU_ID");
        cpu_id = json->int_value;
        json = nx_json_get(item, "LOAD");
        cpu_usage_i = json->int_value;
        memset(cpu_usage_a, 0, sizeof(cpu_usage_a));
        sprintf(cpu_usage_a, "%d,%.3f#", cpu_id, (cpu_usage_i*1.0/100));
        strcat(vgw_stat->cpu_usage, cpu_usage_a);
        if (strlen(vgw_stat->cpu_usage) > sizeof(vgw_stat->cpu_usage)) {
            SNF_SYSLOG(LOG_ERR, "%s: excessive cpu info\n",
                    __FUNCTION__);
            return -1;
        }
        i++;
    } while (1);
    sprintf(vgw_stat->cpu_number, "%d", i);

    /* MEM */
    datum = nx_json_get(data, "HOST_MEMORY");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json recvived, no datum\n",
                __FUNCTION__);
        return -1;
    }
    json = nx_json_get(datum, "TOTAL");
    sprintf(vgw_stat->memory_total, "%ld", json->int_value);
    json = nx_json_get(datum, "FREE");
    sprintf(vgw_stat->memory_free, "%ld", json->int_value);

    /* TRAFFIC */
    datum = nx_json_get(data, "ROUTER_TRAFFICS");
    if (datum->type == NX_JSON_NULL) {
        SNF_SYSLOG(LOG_ERR, "%s: invalid json recvived, no datum\n",
                __FUNCTION__);
        return -1;
    }
    i = 0;
    do {
        item = nx_json_item(datum, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }

        json = nx_json_get(item, "ROUTER_ID");
        vgw_id = json->int_value;
        if (vgw_id != vgw->vm_id) {
            i++;
            continue;
        }

        SNF_SYSLOG(LOG_ERR, "%s: nsp_vgw_stat %d\n", __FUNCTION__, vgw_id);
        json = nx_json_get(item, "WANS");
        j = 0;
        do {
            item_j = nx_json_item(json, j);
            if (item_j->type == NX_JSON_NULL || j >= AGENT_VM_MAX_VIF) {
                break;
            }

            json_j = nx_json_get(item_j, "IF_INDEX");
            vif_id = json_j->int_value;
            memset(&vintf_info, 0 , sizeof(intf_t));
            if (lc_vmdb_vinterface_load_by_type_and_id(&vintf_info,
                    LC_VIF_DEVICE_TYPE_VGATEWAY, vgw_id, vif_id)
                    != LC_DB_VIF_FOUND) {
                j++;
                continue;
            }
            SNF_SYSLOG(LOG_ERR, "%s: nsp_vgw_stat vif %d %d\n", __FUNCTION__, vintf_info.if_id, vif_id);
            vgw_stat->vifs[j].vif_id = vintf_info.if_id;
            strcpy(vgw_stat->vifs[j].mac, vintf_info.if_mac);
            json_j = nx_json_get(item_j, "RX_BPS");
            vgw_stat->vifs[j].rx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BPS");
            vgw_stat->vifs[j].tx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PACKETS");
            vgw_stat->vifs[j].rx_packets = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PACKETS");
            vgw_stat->vifs[j].tx_packets = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_BYTES");
            vgw_stat->vifs[j].rx_bytes = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BYTES");
            vgw_stat->vifs[j].tx_bytes = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PPS");
            vgw_stat->vifs[j].rx_pps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PPS");
            vgw_stat->vifs[j].tx_pps = json_j->int_value;

            j++;
        } while (1);

        json = nx_json_get(item, "LANS");
        k = 0;
        do {
            item_j = nx_json_item(json, k);
            if (item_j->type == NX_JSON_NULL || j >= AGENT_VM_MAX_VIF) {
                break;
            }

            json_j = nx_json_get(item_j, "IF_INDEX");
            vif_id = json_j->int_value;
            memset(&vintf_info, 0 , sizeof(intf_t));
            if (lc_vmdb_vinterface_load_by_type_and_id(&vintf_info,
                    LC_VIF_DEVICE_TYPE_VGATEWAY, vgw_id, vif_id)
                    != LC_DB_VIF_FOUND) {
                k++;
                j++;
                continue;
            }
            SNF_SYSLOG(LOG_ERR, "%s: nsp_vgw_stat vif %d %d\n", __FUNCTION__, vintf_info.if_id, vif_id);
            vgw_stat->vifs[j].vif_id = vintf_info.if_id;
            strcpy(vgw_stat->vifs[j].mac, vintf_info.if_mac);
            json_j = nx_json_get(item_j, "RX_BPS");
            vgw_stat->vifs[j].rx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BPS");
            vgw_stat->vifs[j].tx_bps = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PACKETS");
            vgw_stat->vifs[j].rx_packets = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PACKETS");
            vgw_stat->vifs[j].tx_packets = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_BYTES");
            vgw_stat->vifs[j].rx_bytes = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_BYTES");
            vgw_stat->vifs[j].tx_bytes = json_j->int_value;
            json_j = nx_json_get(item_j, "RX_PPS");
            vgw_stat->vifs[j].rx_pps = json_j->int_value;
            json_j = nx_json_get(item_j, "TX_PPS");
            vgw_stat->vifs[j].tx_pps = json_j->int_value;

            k++;
            j++;
        } while (1);

        break;
    } while (1);

    return 0;
}

int lcs_vgw_vif_req_usage_rt(agent_vm_vif_id_t *vgw, char *ip, vm_result_t *vgw_result)
{
    char result[LCMG_API_SUPER_BIG_RESULT_SIZE] = {0};
    int ret;

    if (lc_db_thread_init() < 0) {
        lc_db_thread_term();
    }
#if 0
    host_device_t host_info;

    if (lc_vmdb_host_device_usage_load(&host_info, ip, "*")
            != LC_DB_HOST_FOUND) {
        return -1;
    }

    if (host_info.type == LC_SERVER_TYPE_NSP
            && host_info.htype == LC_POOL_TYPE_KVM) {
#endif

        SNF_SYSLOG(LOG_INFO, "%s: Will request %d nsp vgws' usage from %s.\n",
                __FUNCTION__, 1, ip);
        if (call_curl_get_nsp_stat_rt(ip, result) < 0) {
            return -1;
        }
#if 0
    }
#endif

    ret = lcs_nsp_stat_process(result, vgw, vgw_result);
    lc_db_thread_term();

    return ret;
}
