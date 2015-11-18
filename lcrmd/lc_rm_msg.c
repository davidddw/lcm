/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-04
 */

#include "message/msg.h"
#include "message/msg_vm.h"
#include "lc_rm.h"
#include "lc_rm_msg.h"

void lc_rm_send_vm_add_msg(rm_vm_t *vm, char *passwd, uint32_t seq)
{
    lc_mbuf_t m;
    struct msg_vm_opt *vm_opt = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(&m, 0, sizeof(lc_mbuf_t));
    m.hdr.type      = LC_MSG_VM_ADD;
    m.hdr.magic     = MSG_MG_UI2DRV;
    m.hdr.seq       = seq;
    m.hdr.length    = sizeof(struct msg_vm_opt);

    vm_opt = (struct msg_vm_opt*)m.data;
    vm_opt->vm_id   = vm->vm_id;
    vm_opt->vl2_id  = vm->vl2_id;
    vm_opt->vnet_id = vm->vdc_id;
    if (NULL != passwd) {
        memcpy(vm_opt->vm_passwd, passwd, MAX_VM_PASSWD_LEN);
    }

    RM_LOG(LOG_DEBUG, "MSG HEADER: magic:%d  type:%d  length:%d  seq:%d\n"
           "ADD VM: vnetid:%d  vl2id:%d  vmid:%d\n",
           m.hdr.magic, m.hdr.type, m.hdr.length, m.hdr.seq,
           vm_opt->vnet_id, vm_opt->vl2_id, vm_opt->vm_id);

    lc_publish_msg(&m);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void lc_rm_send_vgw_add_msg(rm_vgw_t *vgw, uint32_t seq)
{
    lc_mbuf_t m;
    struct msg_vedge_opt *vgw_opt = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(&m, 0, sizeof(lc_mbuf_t));
    m.hdr.type      = LC_MSG_VEDGE_ADD;
    m.hdr.magic     = MSG_MG_UI2MON;
    m.hdr.seq       = seq;
    m.hdr.length    = sizeof(struct msg_vedge_opt);

    vgw_opt = (struct msg_vedge_opt*)m.data;
    vgw_opt->vnet_id = vgw->vgw_id;

    RM_LOG(LOG_DEBUG, "MSG HEADER: magic:%d  type:%d  length:%d  seq:%d\n"
           "ADD Vgw: vnetid:%d\n",
           m.hdr.magic, m.hdr.type, m.hdr.length, m.hdr.seq, vgw_opt->vnet_id);

    lc_publish_msg(&m);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void lc_rm_send_vgateway_add_msg(rm_vgw_t *vgw, uint32_t seq)
{
    lc_mbuf_t m;
    struct msg_vedge_opt *vgw_opt = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(&m, 0, sizeof(lc_mbuf_t));
    m.hdr.type      = LC_MSG_VGATEWAY_ADD;
    m.hdr.magic     = MSG_MG_UI2KER;
    m.hdr.seq       = seq;
    m.hdr.length    = sizeof(struct msg_vedge_opt);

    vgw_opt = (struct msg_vedge_opt*)m.data;
    vgw_opt->vnet_id = vgw->vgw_id;

    RM_LOG(LOG_DEBUG, "MSG HEADER: magic:%d  type:%d  length:%d  seq:%d\n"
           "ADD Vgw: vnetid:%d\n",
           m.hdr.magic, m.hdr.type, m.hdr.length, m.hdr.seq, vgw_opt->vnet_id);

    lc_publish_msg(&m);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}


void lc_rm_send_vm_modify_msg(msg_vm_modify_t *modify, rm_vdisk_t *vdisk, uint32_t seq)
{
    lc_mbuf_t m;
    struct msg_vm_opt *vm_opt = NULL;
    struct msg_vm_opt_modify *vm_modify = NULL;
    struct msg_vm_ha_disk *vm_vdisk = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(&m, 0, sizeof(lc_mbuf_t));
    m.hdr.type      = LC_MSG_VM_CHG;
    m.hdr.magic     = MSG_MG_UI2DRV;
    m.hdr.seq       = seq;
    m.hdr.length    = sizeof(struct msg_vm_opt)
                    + sizeof(struct msg_vm_opt_modify)
                    + sizeof(struct msg_vm_ha_disk) * LC_HA_DISK_NUM;

    vm_opt = (struct msg_vm_opt*)m.data;
    vm_opt->vm_id   = modify->vm_id;
    vm_opt->vl2_id  = modify->vl2_id;
    vm_opt->vnet_id = modify->vdc_id;
    vm_opt->vm_chgmask = modify->mask;
    RM_LOG(LOG_DEBUG, "MSG HEADER: magic:%d  type:%d  length:%d  seq:%d\n"
           "Modify VM: vnetid:%d  vl2id:%d  vmid:%d\n",
           m.hdr.magic, m.hdr.type, m.hdr.length, m.hdr.seq,
           vm_opt->vnet_id, vm_opt->vl2_id, vm_opt->vm_id);

    vm_modify = (struct msg_vm_opt_modify*)vm_opt->data;
    vm_modify->cpu  = modify->vcpu_num;
    vm_modify->mem  = modify->mem_size;
    vm_modify->disk_size = modify->usr_disk_size;
    vm_modify->vnetid = modify->vdc_id;
    vm_modify->vl2id  = modify->vl2_id;
    vm_modify->ha_disk_num = LC_HA_DISK_NUM;
    memcpy(vm_modify->ip, modify->ip, MAX_HOST_ADDRESS_LEN - 1);
    RM_LOG(LOG_DEBUG, "Modify VM: cpu:%d  mem:%d  disk_size:%d\n",
           vm_modify->cpu, vm_modify->mem, vm_modify->disk_size);

    vm_vdisk = (struct msg_vm_ha_disk*)vm_modify->ha_disks;
    vm_vdisk->userdevice = vdisk->userdevice;
    vm_vdisk->ha_disk_size = modify->ha_disk_size;
    RM_LOG(LOG_DEBUG, "Modify VM: vdisk_size:%d  userdevice:%d\n",
           vm_vdisk->ha_disk_size, vm_vdisk->userdevice);

    lc_publish_msg(&m);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void lc_rm_send_vm_snapshot_msg(msg_vm_snapshot_t *snapshot, uint32_t seq)
{
    lc_mbuf_t m;
    struct msg_vm_snapshot_opt *vm_opt = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(&m, 0, sizeof(lc_mbuf_t));
    m.hdr.type      = LC_MSG_VM_SNAPSHOT_CREATE;
    m.hdr.magic     = MSG_MG_UI2DRV;
    m.hdr.seq       = seq;
    m.hdr.length    = sizeof(struct msg_vm_snapshot_opt);

    vm_opt = (struct msg_vm_snapshot_opt *)m.data;
    vm_opt->vm_id   = snapshot->vm_id;
    vm_opt->snapshot_id  = snapshot->snapshot_id;

    RM_LOG(LOG_DEBUG, "MSG HEADER: magic:%d  type:%d  length:%d  seq:%d\n"
           "VM Snapshot: vmid:%d  snapshot_id:%d\n",
           m.hdr.magic, m.hdr.type, m.hdr.length, m.hdr.seq,
           vm_opt->vm_id, vm_opt->snapshot_id);

    lc_publish_msg(&m);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void lc_rm_send_vgateway_ops_reply(msg_ops_reply_t *ops, uint32_t seq)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VgatewayOpsReply ops_reply = TALKER__VGATEWAY_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    ops_reply.id         = ops->id;
    ops_reply.has_id     = 1;
    ops_reply.type       = ops->type;
    ops_reply.has_type   = 1;
    ops_reply.ops        = ops->ops;
    ops_reply.has_ops    = 1;
    ops_reply.err        = ops->err;
    ops_reply.has_err    = 1;
    ops_reply.result     = ops->result;
    ops_reply.has_result = 1;
    msg.vgateway_ops_reply = &ops_reply;

    memset(buf, 0, MAX_MSG_BUFFER);
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    memset(&hdr, 0, sizeof(header_t));
    hdr.sender = HEADER__MODULE__LCRMD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    lc_bus_rm_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}
void lc_rm_send_ops_reply(msg_ops_reply_t *ops, uint32_t seq)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VmOpsReply ops_reply = TALKER__VM_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    ops_reply.id         = ops->id;
    ops_reply.has_id     = 1;
    ops_reply.type       = ops->type;
    ops_reply.has_type   = 1;
    ops_reply.ops        = ops->ops;
    ops_reply.has_ops    = 1;
    ops_reply.err        = ops->err;
    ops_reply.has_err    = 1;
    ops_reply.result     = ops->result;
    ops_reply.has_result = 1;
    msg.vm_ops_reply = &ops_reply;

    memset(buf, 0, MAX_MSG_BUFFER);
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    memset(&hdr, 0, sizeof(header_t));
    hdr.sender = HEADER__MODULE__LCRMD;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    lc_bus_rm_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}
