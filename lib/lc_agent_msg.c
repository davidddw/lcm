/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_agent_msg.c
 * Author Name: Jin Jie
 * Date: 2012-1-16
 *
 * Description: Agent message sender
 *
 */

#include <arpa/inet.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "agent_message.h"
#include "agent_message_interface.h"
#include "agent_message_vm.h"
#include "agent_message_hwdev.h"
#include "agent_sizes.h"
#include "lc_agent_msg.h"
#include "agent_message_common.h"
#include "message/msg_enum.h"
#include "lc_bus.h"


#define AGENT_PORT  20002
#define MAX_DATA_BUF_LEN (1 << 15)

#define LC_TYPE_XEN    1
#define LC_TYPE_VMWARE 2
#define LC_TYPE_KVM    3
static int lc_send_agent_message(lc_mbuf_t *msg, int len, char *ip)
{
    struct sockaddr_in addr;
    int fp = -1, nsend;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton(ip, &addr.sin_addr);
    addr.sin_port = htons(AGENT_PORT);

    fp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fp < 0) {
        return -1;
    }

    nsend = sendto(fp, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr));

    close(fp);

    return nsend > 0 ? 0 : -1;
}

/* vm */
static int lc_agent_vm_monitor(agent_vm_id_t *vm, int count, char *ip, int op)
{
    lc_mbuf_t msg;
    msg_vm_task_ops_t *payload;
    vm_task_t *task;
    int len, i;

    if (!vm || count <= 0 || !ip) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));

    len = sizeof(msg_vm_task_ops_t) + count * sizeof(vm_task_t);

    msg.hdr.magic = htonl(MSG_MG_DRV2AGN);
    msg.hdr.type = htonl(op);
    msg.hdr.length = htonl(len);

    payload = (msg_vm_task_ops_t *)msg.data;
    payload->n = htonl(count);
    for (i = 0; i < count; ++i) {
        task = payload->data + i;
//        strcpy(task->vm_uuid, vm[i].vm_uuid);
//        strcpy(task->vdi_sys_uuid, vm[i].vdi_sys_uuid);
//        strcpy(task->vdi_user_uuid, vm[i].vdi_user_uuid);
    }

    return lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
}

static int lc_agent_vm_req_monitor(agent_vm_vif_id_t *vm, int count, char *ip, int op)
{
    lc_mbuf_t msg;
    msg_vm_info_request_t *payload;
    vm_query_t *task;
    int len, i, remain_data_cnt;
    int ret = 0, delta_cnt = 0, offset = 0, j = 0;

    if (!vm || count <= 0 || !ip) {
        return -1;
    }

    remain_data_cnt = count;
    delta_cnt = MAX_DATA_BUF_LEN/sizeof(vm_query_t);
    while (1) {
        memset(&msg, 0, sizeof(msg));

        msg.hdr.magic = htonl(MSG_MG_MON2AGN);
        msg.hdr.type = htonl(op);
        if (remain_data_cnt <= delta_cnt) {
            len = sizeof(msg_vm_info_request_t)
                  + remain_data_cnt * sizeof(vm_query_t);

            msg.hdr.length = htonl(len);

            payload = (msg_vm_info_request_t *)msg.data;
            payload->n = htonl(remain_data_cnt);
            for (i = 0; i < remain_data_cnt; ++i) {
                 task = payload->data + i;
                 strcpy(task->vm_label, vm[i + offset].vm_label);
                 task->vm_type = htonl(vm[i + offset].vm_type);
                 task->vm_id = htonl(vm[i + offset].vm_id);
                 task->vdc_id = htonl(vm[i + offset].vdc_id);
                 task->host_id = htonl(vm[i + offset].host_id);
                 for (j = 0; j < vm->vif_num; j ++) {
                     task->vifs[j].vlan_tag = htons(vm[i + offset].vifs[j].vlan_tag);
                     strcpy(task->vifs[j].mac, vm[i + offset].vifs[j].mac);
                 }
            }
            return lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
        } else {
            len = sizeof(msg_vm_info_request_t)
                  + delta_cnt * sizeof(vm_query_t);
            msg.hdr.length = htonl(len);

            payload = (msg_vm_info_request_t *)msg.data;
            payload->n = htonl(delta_cnt);
            for (i = 0; i < delta_cnt; ++i) {
                 task = payload->data + i;
                 strcpy(task->vm_label, vm[i + offset].vm_label);
                 task->vm_type = htonl(vm[i + offset].vm_type);
                 task->vm_id = htonl(vm[i + offset].vm_id);
                 task->vdc_id = htonl(vm[i + offset].vdc_id);
                 task->host_id = htonl(vm[i + offset].host_id);
                 for (j = 0; j < vm->vif_num; j ++) {
                     task->vifs[j].vlan_tag = htons(vm[i + offset].vifs[j].vlan_tag);
                     strcpy(task->vifs[j].mac, vm[i + offset].vifs[j].mac);
                 }
            }
            ret = lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
            if (ret) {
                return ret;
            }
            remain_data_cnt -=delta_cnt;
            offset += i;
        }
    }

    return ret;
}

static int lc_agent_vm_vif_req_usage(agent_vm_vif_id_t *vm, int count, char *ip, int op, uint32_t timestamp)
{
    lc_mbuf_t msg;
    msg_vm_info_request_t *payload;
    vm_query_t *task;
    int len, i, j, remain_data_cnt;
    int ret = 0, delta_cnt = 0, offset = 0;

    if (!vm || count <= 0 || !ip) {
        return -1;
    }
    remain_data_cnt = count;
    delta_cnt = MAX_DATA_BUF_LEN/sizeof(vm_query_t);
    while (1) {
        memset(&msg, 0, sizeof(msg));

        msg.hdr.magic = htonl(MSG_MG_SNF2AGN);
        msg.hdr.type = htonl(op);
        msg.hdr.time = htonl(timestamp);
        if (remain_data_cnt <= delta_cnt) {
            len = sizeof(msg_vm_info_request_t)
                  + remain_data_cnt * sizeof(vm_query_t);

            msg.hdr.length = htonl(len);

            payload = (msg_vm_info_request_t *)msg.data;
            payload->n = htonl(remain_data_cnt);
            for (i = 0; i < remain_data_cnt; ++i) {
                 task = payload->data + i;
                 strcpy(task->vm_label, vm[i + offset].vm_label);
                 task->vm_type = htonl(vm[i + offset].vm_type);
                 task->vm_id = htonl(vm[i + offset].vm_id);
                 task->vdc_id = htonl(vm[i + offset].vdc_id);
                 task->host_id = htonl(vm[i + offset].host_id);
                 for (j = 0; j < AGENT_VM_MAX_VIF; ++j) {
                     task->vifs[j].vif_id = htonl(vm[i + offset].vifs[j].vif_id);
                     strncpy(task->vifs[j].mac, vm[i + offset].vifs[j].mac, AGENT_MAC_LEN);
                     task->vifs[j].vlan_tag = htonl(vm[i + offset].vifs[j].vlan_tag);
                 }
            }
            return lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
        } else {
            len = sizeof(msg_vm_info_request_t)
                  + delta_cnt * sizeof(vm_query_t);
            msg.hdr.length = htonl(len);

            payload = (msg_vm_info_request_t *)msg.data;
            payload->n = htonl(delta_cnt);
            for (i = 0; i < delta_cnt; ++i) {
                 task = payload->data + i;
                 strcpy(task->vm_label, vm[i + offset].vm_label);
                 task->vm_type = htonl(vm[i + offset].vm_type);
                 task->vm_id = htonl(vm[i + offset].vm_id);
                 task->vdc_id = htonl(vm[i + offset].vdc_id);
                 task->host_id = htonl(vm[i + offset].host_id);
                 for (j = 0; j < AGENT_VM_MAX_VIF; ++j) {
                     task->vifs[j].vif_id = htonl(vm[i + offset].vifs[j].vif_id);
                     strncpy(task->vifs[j].mac, vm[i + offset].vifs[j].mac, AGENT_MAC_LEN);
                     task->vifs[j].vlan_tag = htonl(vm[i + offset].vifs[j].vlan_tag);
                 }
            }
            ret = lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
            if (ret) {
                return ret;
            }
            remain_data_cnt -=delta_cnt;
            offset += i;
        }
    }

    return ret;
}

/* TODO: Implement the following function to construct and send the request message to LCG.-kzs*/
static int lc_agent_hwdev_req_usage(agent_hwdev_t *hwdev, char *ip, int op)
{
    lc_mbuf_t msg;
    msg_hwdev_info_request_t *payload;
    hwdev_query_t *task;
    int len;
    int ret = 0;

    if (!hwdev || !ip) {
        return -1;
    }
    memset(&msg, 0, sizeof(msg));

    msg.hdr.magic = htonl(MSG_MG_SNF2AGN);
    msg.hdr.type = htonl(op);
    len = sizeof(msg_hwdev_info_request_t)
          + sizeof(hwdev_query_t);
    msg.hdr.length = htonl(len);

    payload = (msg_hwdev_info_request_t *)msg.data;
    payload->n = htonl(1);
    task = payload->data;
    strcpy(task->hwdev_ip, hwdev->hwdev_ip);
    strcpy(task->community, hwdev->community);
    strcpy(task->username, hwdev->username);
    strcpy(task->password, hwdev->password);
    ret = lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
    return ret;
}

int lc_agent_add_vm_monitor(agent_vm_id_t *vm, int count, char *ip)
{
    return lc_agent_vm_monitor(vm, count, ip, MSG_VM_TASK_ADD);
}

int lc_agent_del_vm_monitor(agent_vm_id_t *vm, int count, char *ip)
{
    return lc_agent_vm_monitor(vm, count, ip, MSG_VM_TASK_DEL);
}

int lc_agent_request_vm_monitor(agent_vm_vif_id_t *vm, int count, char *ip)
{
    return lc_agent_vm_req_monitor(vm, count, ip, MSG_VM_INFO_REQUEST);
}

int lc_agent_request_vm_vif_usage(agent_vm_vif_id_t *vm, int count, char *ip, uint32_t timestamp)
{
    return lc_agent_vm_vif_req_usage(vm, count, ip, MSG_VM_INFO_REQUEST, timestamp);
}

/* TODO: Call the following function to construct and send the request message to LCG.--kzs*/

int lc_agent_request_hwdev_usage(agent_hwdev_t *hwdev, char *ip)
{
    return lc_agent_hwdev_req_usage(hwdev, ip, MSG_HWDEV_INFO_REQUEST);
}

static int lc_agent_request_host(char *ip, int op)
{
    lc_mbuf_t msg;
    int len;

    if (!ip) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));

    len = 0;

    msg.hdr.magic = htonl(MSG_MG_MON2AGN);
    msg.hdr.type = htonl(op);
    msg.hdr.length = htonl(len);

    return lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
}

static int lc_request_vmware_host(char *msg, int len, uint32_t queue_id)
{
    return 0;
}

int lc_agent_request_host_monitor(char *ip, int htype, int fd, char *buf, int len)
{
    if (htype == LC_TYPE_XEN || htype == LC_TYPE_KVM) {
        return lc_agent_request_host(ip, MSG_HOST_INFO_REQUEST);
    } else if (htype == LC_TYPE_VMWARE) {
        return lc_request_vmware_host(buf, len, LC_BUS_QUEUE_VMADAPTER);
    }
    return 0;
}

int lc_agent_request_host_usage(char *ip)
{
    lc_mbuf_t msg;
    int len;

    if (!ip) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));

    len = 0;

    msg.hdr.magic = htonl(MSG_MG_SNF2AGN);
    msg.hdr.type = htonl(MSG_HOST_INFO_REQUEST);
    msg.hdr.time = htonl((uint32_t)(time(NULL)));
    msg.hdr.length = htonl(len);

    return lc_send_agent_message(&msg, len + sizeof(lc_mbuf_hdr_t), ip);
}

int lc_agent_request_vmware_host_usage(int fd, char *buf, int data_len)
{
    lc_mbuf_t msg;
    int len, mlen;

    memset(&msg, 0, sizeof(msg));

    mlen = data_len;
    len = sizeof(struct lc_msg_head) + mlen;

    msg.hdr.magic = htonl(MSG_MG_SNF2AGN);
    msg.hdr.type = htonl(LC_MSG_VMD_REQUEST_VCD_HOST_STATS);
    msg.hdr.length = htonl(len);
    memcpy(&msg.hdr + 1, buf, mlen);
    if (write(fd, (void *)&msg, len) != len) {
        return -1;
    }

    return 0;
}

int lc_vcd_request_vm_monitor(int fd, char *buf, int len)
{
    return lc_request_vmware_host(buf, len, LC_BUS_QUEUE_VMADAPTER);
}

int lc_vcd_request_vm_vif_usage(int fd, char *buf, int data_len)
{
    lc_mbuf_t msg;
    int len, mlen;

    memset(&msg, 0, sizeof(msg));

    mlen = data_len;
    len = sizeof(struct lc_msg_head) + mlen;

    msg.hdr.magic = htonl(MSG_MG_SNF2AGN);
    msg.hdr.type = htonl(LC_MSG_VMD_REQUEST_VCD_VM_VIF_USAGE);
    msg.hdr.length = htonl(len);
    memcpy(&msg.hdr + 1, buf, mlen);
    if (write(fd, (void *)&msg, len) != len) {
        return -1;
    }

    return 0;
}

