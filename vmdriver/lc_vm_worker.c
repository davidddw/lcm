#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xen/api/xen_all.h>
#include "message/msg_ker2vmdrv.h"
#include "message/msg_lcm2dri.h"
#include "lc_vm.h"
#include "lc_db.h"
#include "lc_db_pool.h"
#include "lc_db_host.h"
#include "lc_db_vm.h"
#include "lc_db_storage.h"
#include "lc_db_log.h"
#include "lc_db_sys.h"
#include "vm/vm_host.h"
#include "lc_queue.h"
#include "lc_vmware.h"
#include "lc_xen.h"
#include "lc_utils.h"
#include "lc_db_errno.h"
#include "lc_agent_msg.h"
#include "lc_agexec_msg.h"
#include "cloudmessage.pb-c.h"
#include "talker.pb-c.h"
#include "lc_vmware.h"

static int lc_vm_create(struct lc_msg_head *m_head);
static int lc_vm_create_resume(struct lc_msg_head *m_head);
static int lc_vm_del(struct lc_msg_head *m_head);
static int lc_vm_del_resume(struct lc_msg_head *m_head);
static int lc_vm_start(struct lc_msg_head *m_head);
static int lc_vm_start_resume(struct lc_msg_head *m_head);
static int lc_vm_stop(struct lc_msg_head *m_head);
static int lc_vm_stop_resume(struct lc_msg_head *m_head);
static int lc_vm_vnc_connect(struct lc_msg_head *m_head);
static int lc_vm_replace(struct lc_msg_head *m_head);
static int lc_vm_modify(struct lc_msg_head *m_head);
static int lc_storage_resource_opt(struct lc_msg_head *m_head);
static int lc_storage_resource_del(struct lc_msg_head *m_head);
static int lc_host_add(struct lc_msg_head *m_head);
static int lc_host_change(struct lc_msg_head *m_head);
static int lc_host_del(struct lc_msg_head *m_head);
static int lc_host_join(struct lc_msg_head *m_head);
static int lc_host_eject(struct lc_msg_head *m_head);
static int vmware_msg_handler(struct lc_msg_head *m_head);
static int lc_vcd_vm_add_reply(Cloudmessage__VMAddResp *resp, uint32_t seqid);
static int lc_vcd_vm_update_reply(Cloudmessage__VMUpdateResp *resp, uint32_t seqid);
static int lc_vcd_vm_ops_reply(Cloudmessage__VMOpsResp *resp, uint32_t seqid);
static int lc_vcd_vm_del_reply(Cloudmessage__VMDeleteResp *resp, uint32_t seqid);
static int lc_vcd_vm_snapshot_add_reply(Cloudmessage__VMSnapshotAddResp *resp, uint32_t seqid);
static int lc_vcd_vm_snapshot_del_reply(Cloudmessage__VMSnapshotDelResp *resp, uint32_t seqid);
static int lc_vcd_vm_snapshot_revert_reply(Cloudmessage__VMSnapshotRevertResp *resp, uint32_t seqid);
static int lc_vcd_hosts_info_reply(Cloudmessage__HostsInfoResp *resp);
static int lc_vcd_vm_set_disk_reply(Cloudmessage__VMSetDiskResp *resp, uint32_t seqid);
static int lc_vm_snapshot_create(struct lc_msg_head *m_head);
static int lc_vm_snapshot_delete(struct lc_msg_head *m_head);
static int lc_vm_snapshot_recovery(struct lc_msg_head *m_head);
static int lc_vm_backup_create(struct lc_msg_head *m_head);
static int lc_vm_backup_delete(struct lc_msg_head *m_head);
static int lc_vm_backup_recovery(struct lc_msg_head *m_head);
static int lc_vm_migrate(struct lc_msg_head *m_head);
//static int lc_vm_sys_migrate_resume(struct lc_msg_head *m_head);
static int lc_disk_set(struct lc_msg_head *m_head);

#define UPDATE_NAME_LABEL_STR "xe -s %s -p 443 -u %s -pw %s vm-param-set name-label=%s uuid=%s --minimal"
#define MAX_LOG_MSG_LEN              150

static int lc_convert_protobuf_msg(struct lc_msg_head *m_head)
{
    Talker__Message *msg = NULL;
    int ret = 0;
    struct msg_vm_opt vmops;
    struct msg_vm_opt *mvm = NULL;

    msg = talker__message__unpack(NULL, m_head->length,
                              (uint8_t *)(m_head + 1));

    if (!msg) {
        return -1;
    }
    memset(&vmops, 0, sizeof(struct msg_vm_opt));

    if (msg->vm_start_req) {
        m_head->type = LC_MSG_VM_START;
        vmops.vm_id = msg->vm_start_req->id;
        vmops.vnet_id = msg->vm_start_req->vnet_id;
        vmops.vl2_id = msg->vm_start_req->vl2_id;
    } else if (msg->vm_revert_snapshot_req) {
        /*  reuse the space of vm_opt to place snapshot info
         *  struct msg_vm_opt {            struct msg_vm_snapshot_opt {
         *  u32         vnet_id;   <==     u32         snapshot_id;
         *  u32         vl2_id;    <==     u32         vm_id;
         *  ...                            }
         *  }
         * */
        m_head->type = LC_MSG_VM_SNAPSHOT_RECOVERY;
        vmops.vnet_id = msg->vm_revert_snapshot_req->snapshot_id;
        vmops.vl2_id = msg->vm_revert_snapshot_req->vm_id;
    } else if (msg->vm_delete_snapshot_req) {
        m_head->type = LC_MSG_VM_SNAPSHOT_DELETE;
        vmops.vnet_id = msg->vm_delete_snapshot_req->snapshot_id;
        vmops.vl2_id = msg->vm_delete_snapshot_req->vm_id;
    } else if (msg->ha_disk_ops_req) {
        if (msg->ha_disk_ops_req->ops == TALKER__HADISK_OPS__HA_DISK_PLUG) {
            m_head->type = LC_MSG_VM_PLUG_DISK;
        } else if (msg->ha_disk_ops_req->ops == TALKER__HADISK_OPS__HA_DISK_UNPLUG){
            m_head->type = LC_MSG_VM_UNPLUG_DISK;
        }
        vmops.vnet_id = 1;
        vmops.vl2_id = msg->ha_disk_ops_req->vdisk_id;
    } else {
        return -1;
    }
    mvm = (struct msg_vm_opt *) (m_head + 1);
    /*make sure it's safe*/
    talker__message__free_unpacked(msg, NULL);
    memset((uint8_t *)(m_head + 1), 0, m_head->length);
    mvm->vm_id = vmops.vm_id;
    mvm->vnet_id = vmops.vnet_id;
    mvm->vl2_id = vmops.vl2_id;
    m_head->length = sizeof(struct msg_vm_opt);

    return ret;
}

int vm_msg_handler(struct lc_msg_head *m_head)
{
    int ret;

    if (!is_lc_msg(m_head)) {
        return -1;
    }

    if (!is_msg_to_drv(m_head)) {
        return -1;
    }
    if (m_head->type == LC_MSG_NOTHING) {
        lc_convert_protobuf_msg(m_head);
    }
    VMDRIVER_LOG(LOG_DEBUG, "type=%d, len=%d\n",
                 m_head->type, m_head->length);
    switch (m_head->type) {
    case LC_MSG_VM_ADD:
        ret = lc_vm_create(m_head);
        break;
    case LC_MSG_VM_ADD_RESUME:
        ret = lc_vm_create_resume(m_head);
        break;
    case LC_MSG_VM_DEL:
        ret = lc_vm_del(m_head);
        break;
    case LC_MSG_VM_DEL_RESUME:
        ret = lc_vm_del_resume(m_head);
        break;
    case LC_MSG_VM_START:
        ret = lc_vm_start(m_head);
        break;
    case LC_MSG_VM_START_RESUME:
        ret = lc_vm_start_resume(m_head);
        break;
    case LC_MSG_VM_STOP:
        ret = lc_vm_stop(m_head);
        break;
    case LC_MSG_VM_STOP_RESUME:
        ret = lc_vm_stop_resume(m_head);
        break;
    case LC_MSG_VM_CHG:
        ret = lc_vm_modify(m_head);
        break;
    case LC_MSG_VM_VNC_CONNECT:
        ret = lc_vm_vnc_connect(m_head);
        break;
    case LC_MSG_VM_REPLACE:
        ret = lc_vm_replace(m_head);
        break;
    case LC_MSG_SR_JOIN:
        ret = lc_storage_resource_opt(m_head);
        break;
    case LC_MSG_SR_EJECT:
        ret = lc_storage_resource_del(m_head);
        break;
    case LC_MSG_HOST_ADD:
        ret = lc_host_add(m_head);
        break;
    case LC_MSG_HOST_CHG:
        ret = lc_host_change(m_head);
        break;
    case LC_MSG_HOST_DEL:
        ret = lc_host_del(m_head);
        break;
    case LC_MSG_HOST_JOIN:
        ret = lc_host_join(m_head);
        break;
    case LC_MSG_HOST_EJECT:
        ret = lc_host_eject(m_head);
        break;

/* VM disk operations */
    /* TODO: Add disk ops msg */
    //case LC_MSG_KER_REQUEST_DRV_ADD_DISK:
    //    lc_vm_add_disk(m_head);
    //    break;

    //case LC_MSG_KER_REQUEST_DRV_REMOVE_DISK:
    //    lc_vm_remove_disk(m_head);
    //    break;

    //case LC_MSG_KER_REQUEST_DRV_RESIZE_DISK:
    //    lc_vm_resize_disk(m_head);
    //    break;

    /* Snapshot operations */
    case LC_MSG_VM_SNAPSHOT_CREATE:
        ret = lc_vm_snapshot_create(m_head);
        break;
    case LC_MSG_VM_SNAPSHOT_DELETE:
        ret = lc_vm_snapshot_delete(m_head);
        break;
    case LC_MSG_VM_SNAPSHOT_RECOVERY:
        ret = lc_vm_snapshot_recovery(m_head);
        break;

    /* Backup operations */
    case LC_MSG_VM_BACKUP_CREATE:
        ret = lc_vm_backup_create(m_head);
        break;
    case LC_MSG_VM_BACKUP_DELETE:
        ret = lc_vm_backup_delete(m_head);
        break;
    case LC_MSG_VM_RECOVERY:
        ret = lc_vm_backup_recovery(m_head);
        break;

    /* Migrate operations */
    case LC_MSG_VM_MIGRATE:
        ret = lc_vm_migrate(m_head);
        break;

    case LC_MSG_VM_PLUG_DISK:
    case LC_MSG_VM_UNPLUG_DISK:
        ret = lc_disk_set(m_head);
        break;
#if 0
    case LC_MSG_VM_SYS_DISK_MIGRATE:
        ret = lc_vm_sys_migrate_resume(m_head);
        break;
#endif
    case LC_MSG_DUMMY:
    default:
        return vmware_msg_handler(m_head);
    }

    return 0;
}

static int lc_msg_forward(lc_mbuf_t *msg)
{
    int magic = lc_msg_get_path(msg->hdr.type, msg->hdr.magic);

    if (magic == -1) {
        VMDRIVER_LOG(LOG_ERR, "Find next module from %x error\n",msg->hdr.magic);
        return -1;
    } else if (magic == MSG_MG_END) {
        VMDRIVER_LOG(LOG_DEBUG, "Message %d complete\n",msg->hdr.type);
        return 0;
    } else {
        VMDRIVER_LOG(LOG_DEBUG, "Message %d forwarding\n", msg->hdr.type);
        msg->hdr.magic = magic;
        lc_publish_msg(msg);
        return 0;
    }
}

static int lc_msg_rollback(vm_info_t *vm_info, vm_snapshot_info_t *snapshot_info,
                       uint32_t result, uint32_t seq, int ops, uint32_t receiver)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VmOpsReply vm_ops = TALKER__VM_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];

    memset(buf, 0, MAX_MSG_BUFFER);

    if (vm_info) {
        vm_ops.id = vm_info->vm_id;
        vm_ops.err = vm_info->vm_error;
        if (LC_VM_TYPE_VM == vm_info->vm_type) {
            vm_ops.type = TALKER__VM_TYPE__VM;
        }
    } else {
        vm_ops.id = snapshot_info->snapshot_id;
        vm_ops.err = snapshot_info->error;
        if (ops == TALKER__VM_OPS__SNAPSHOT_CREATE) {
            vm_ops.type = TALKER__VM_TYPE__SNAPSHOT;
        } else {
            vm_ops.type = TALKER__VM_TYPE__VM;
        }
    }
    vm_ops.result = result;
    vm_ops.ops = ops;
    vm_ops.has_id = vm_ops.has_err = 1;
    vm_ops.has_type = vm_ops.has_result = 1;
    vm_ops.has_ops = 1;

    msg.vm_ops_reply = &vm_ops;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    lc_bus_vmdriver_publish_unicast(buf, (hdr_len + msg_len), receiver);
    return 0;
}

static struct lc_msg_head *
lc_get_vm_forward_create_msg(struct lc_msg_head *m_head, vm_info_t *vm_info)
{
    struct lc_msg_head *m_send;
    struct msg_vm_opt *m_opt;
    struct msg_vm_opt_start *m_start;

    m_send = malloc(sizeof(*m_head) + m_head->length + sizeof(*m_start));

    if (!m_send) {
        return NULL;
    }

    m_opt = (struct msg_vm_opt *)(m_send + 1);
    m_start = (struct msg_vm_opt_start *)m_opt->data;

    memset(m_send, 0, sizeof(*m_head) + m_head->length + sizeof(*m_start));
    memcpy(m_send, m_head, sizeof(*m_head) + m_head->length);
    m_send->length += sizeof(*m_start);

    m_opt->vm_id = vm_info->vm_id;
    memcpy(m_start->vm_uuid, vm_info->vm_uuid, MAX_VM_UUID_LEN);
    memcpy(m_start->vm_server, vm_info->hostp->host_address, MAX_HOST_ADDRESS_LEN);
    copy_intf_to_intf_core(&m_start->vm_intf, vm_info->ifaces, 1);

    return m_send;
}

struct lc_msg_head *
lc_get_vm_forward_start_msg(struct lc_msg_head *m_head, vm_info_t *vm_info)
{
    struct lc_msg_head *m_send;
    struct msg_vm_opt *m_opt;
    struct msg_vm_opt_start *m_start;

    m_send = malloc(sizeof(*m_head) + m_head->length + sizeof(*m_start));

    if (!m_send) {
        return NULL;
    }

    m_opt = (struct msg_vm_opt *)(m_send + 1);
    m_start = (struct msg_vm_opt_start *)m_opt->data;

    memset(m_send, 0, sizeof(*m_head) + m_head->length + sizeof(*m_start));
    memcpy(m_send, m_head, sizeof(*m_head) + m_head->length);
    m_send->length += sizeof(*m_start);

    m_opt->vm_id = vm_info->vm_id;
    memcpy(m_start->vm_uuid, vm_info->vm_uuid, MAX_VM_UUID_LEN);
    copy_intf_to_intf_core(m_start->vm_ifaces, vm_info->ifaces,
                           LC_VM_IFACES_MAX);
    memcpy(m_start->vm_server, vm_info->hostp->host_address,
           MAX_HOST_ADDRESS_LEN);

    return m_send;
}

static int lc_vm_create_configure_iso(vm_info_t *vm_info)
{
    char cmd[MAX_CMD_BUF_SIZE];
    char ips[MAX_CMD_BUF_SIZE];

    memset(ips, 0, MAX_CMD_BUF_SIZE);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    if (vm_info->vm_type == LC_VM_TYPE_VM) {
        if (!vm_info->hostp) {
            VMDRIVER_LOG(LOG_ERR, "Host not set.\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info->vm_id, vm_info->vm_label, "create iso error",
                                  LC_SYSLOG_LEVEL_WARNING);
            return -1;
        }
        sprintf(cmd, LC_MAKE_ISO_STR,
                vm_info->vm_label, vm_info->vm_name,
                "true", vm_info->vm_passwd, "true",
                vm_info->ifaces[LC_VM_IFACE_CTRL].if_gateway,
                vm_info->ifaces[LC_VM_IFACE_CTRL].if_index,
                vm_info->ifaces[LC_VM_IFACE_CTRL].if_ip,
                vm_info->ifaces[LC_VM_IFACE_CTRL].if_netmask,
                vm_info->ifaces[LC_VM_IFACE_CTRL].if_mac,
                ips);
    } else if (vm_info->vm_type == LC_VM_TYPE_GW) {
        VMDRIVER_LOG(LOG_ERR, "Not support create vgw iso\n");
    }

    VMDRIVER_LOG(LOG_DEBUG, "%s", cmd);
    lc_call_system(cmd, "VM", "vm", vm_info->vm_label, lc_vmdriver_db_log);

    return 0;
}

static int lc_vm_remove_configure_iso(vm_info_t *vm_info)
{
    char cmd[MAX_CMD_BUF_SIZE];

    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    sprintf(cmd, "rm -f %s/%s.iso", LC_ISO_DIR, vm_info->vm_label);
    lc_call_system(cmd, "VM ISO", "vm", vm_info->vm_label, lc_vmdriver_db_log);

    return 0;
}

static int lc_vm_create(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    rpool_t pool;
    host_t phost;
    host_t master;
    vm_info_t vm_info;
    vm_info_t vm_info_temp;
    struct lc_msg_head *m_send  = NULL;
    int mlen;
    char buf[MAX_MSG_BUFFER];
    char message[MAX_LOG_MSG_LEN];
    int ret = 0;
    vcdc_t vcdc;
    third_device_t device;
    template_os_info_t template_info;
    uint32_t seqid = m_head->seq;
    int i;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&master, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vm_info_temp, 0, sizeof(vm_info_t));
    memset(buf, 0, MAX_MSG_BUFFER);
    memset(message, 0, sizeof(message));
    memset(&vcdc, 0, sizeof vcdc);
    memset(&device, 0, sizeof(third_device_t));
    memset(&template_info, 0, sizeof(template_os_info_t));

    mvm = (struct msg_vm_opt*)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {

        VMDRIVER_LOG(LOG_ERR, "vm %d to be created not found\n", mvm->vm_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "It is NOT existed in DataBase",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_vif_loadn_by_vm_id(&vm_info.ifaces[0],
                                   LC_VM_IFACES_MAX,
                                   mvm->vm_id) != LC_DB_VIF_FOUND) {
        VMDRIVER_LOG(LOG_WARNING, "VM %d interfaces not found", mvm->vm_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label,
                              "Virtual Interfaces NOT exist in Database",
                              LC_SYSLOG_LEVEL_WARNING);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
        if (vm_info.ifaces[i].if_id && vm_info.ifaces[i].if_index == i) {
            /* has data */
            continue;
        }
        VMDRIVER_LOG(LOG_WARNING, "VM %d interfaces not enough", mvm->vm_id);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        snprintf(message, MAX_LOG_MSG_LEN,
                 "The pool %d is NOT existed in DataBase", vm_info.pool_id);
        VMDRIVER_LOG(LOG_ERR, "The pool %d is NOT existed in DataBase",
                     vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, message,
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_compute_resource_load(&phost, vm_info.host_address) != LC_DB_HOST_FOUND) {
        snprintf (message, MAX_LOG_MSG_LEN, "The Host %s is NOT existed in DataBase", vm_info.host_address);
        VMDRIVER_LOG(LOG_ERR, "The host %s not found", vm_info.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, message,
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (pool.ctype == LC_POOL_TYPE_XEN &&
        !(vm_info.vm_flags & LC_VM_INSTALL_TEMPLATE)) {
        if (strcmp(phost.host_address, phost.master_address)) {
            VMDRIVER_LOG(LOG_ERR, "The host %s not master, find master %s\n",phost.host_address, phost.master_address);
            memset(vm_info.host_address, 0, MAX_HOST_ADDRESS_LEN);

            if (lc_vmdb_compute_resource_load(&phost, phost.master_address) != LC_DB_HOST_FOUND) {
                snprintf (message, MAX_LOG_MSG_LEN, "The Master %s is NOT existed in DataBase", phost.master_address);
                VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",phost.master_address);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, message,
                                      LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
        }
    }

    if (lc_vmdb_network_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found",phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "The LC create vm vmname:%s, server:%s\n",
                 vm_info.vm_label, phost.host_address);

    phost.master = &phost;
    vm_info.hostp = &phost;

    if (pool.ctype == LC_POOL_TYPE_XEN) {
        vm_info.hostp->host_htype = LC_HOST_HTYPE_XEN;
    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {
        vm_info.hostp->host_htype = LC_HOST_HTYPE_VMWARE;
    }

    if (vm_info.vm_flags & LC_VM_THIRD_HW_FLAG) {

        /* Update vif mac by third_party device */
        if (lc_vmdb_third_device_load(
            &device, vm_info.vm_id, LC_THIRD_DEVICE_TYPE_VM)) {
            VMDRIVER_LOG(LOG_ERR, "The third device of vm %d not found",vm_info.vm_id);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      vm_info.vm_id, vm_info.vm_label,
                                      "Third party device is NOT existed in DataBase",
                                      LC_SYSLOG_LEVEL_WARNING);
        }
        lc_vmdb_vif_update_mac(device.data_mac[0], vm_info.ifaces[0].if_id);
        lc_vmdb_vif_update_state(LC_VIF_STATE_ATTACHED, vm_info.ifaces[0].if_id);

        /* Send msg to lcpd */
        m_send = lc_get_vm_forward_create_msg(m_head, &vm_info);
        if (m_send) {
            lc_msg_forward((lc_mbuf_t *)m_send);
            free(m_send);
        }
        lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm_info.vm_id);
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                              TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
        goto finish;
    }


    if (vm_info.vm_flags & LC_VM_INSTALL_TEMPLATE) {
        strncpy(vm_info.vm_passwd, mvm->vm_passwd, MAX_VM_PASSWD_LEN - 1);
/*        lc_vm_create_configure_iso(&vm_info);
        if (pool.ctype == LC_POOL_TYPE_XEN) {
            lc_vm_insert_configure_iso(&vm_info);
        }
*/
    }

    /*
     * VM tport allocate is moved to talker
    lc_vmdb_vm_update_tport(MAX_VM_TPORT+1, vm_info.vm_id);
    if (-1 == lc_get_available_vm_tport(&vm_info)) {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "get tport error",
                              LC_SYSLOG_LEVEL_ERR);
    }
    */

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        /* get sr uuid */
        if (lc_storagedb_host_device_get_storage_uuid(
                    vm_info.vdi_sys_sr_uuid, MAX_UUID_LEN,
                    vm_info.host_address, vm_info.vdi_sys_sr_name)) {
            VMDRIVER_LOG(LOG_ERR, "The storage %s not found\n",vm_info.vdi_sys_sr_name);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label, "storage not found",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
        lc_vmdb_vm_update_sys_sr_uuid(vm_info.vdi_sys_sr_uuid, mvm->vm_id);

        if (vm_info.vdi_user_size > 0) {
            if (lc_storagedb_host_device_get_storage_uuid(
                        vm_info.vdi_user_sr_uuid, MAX_UUID_LEN,
                        vm_info.host_address, vm_info.vdi_user_sr_name)) {
                VMDRIVER_LOG(LOG_ERR, "The storage %s not found\n",vm_info.vdi_user_sr_name);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, "storage not found",
                                      LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
            lc_vmdb_vm_update_user_sr_uuid(vm_info.vdi_user_sr_uuid, mvm->vm_id);
        }

        if (vm_info.vm_flags & LC_VM_INSTALL_TEMPLATE) {
            if (lc_get_template_info(&template_info, vm_info.vm_template) != LC_DB_VM_FOUND) {
                VMDRIVER_LOG(LOG_ERR, "vm %s's template %s not be found",
                             vm_info.vm_label, vm_info.vm_template);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, "template not found",
                                      LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
            vm_info.vm_template_type = template_info.type;
            if ((ret = lc_agexec_vm_create(&vm_info, seqid))) {
                VMDRIVER_LOG(LOG_ERR, "Install vm %s error, code %d\n",vm_info.vm_label, ret);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, "create command error",
                                      LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_INSTALL_OP_FAILED;
                goto error;
            }
            goto finish;
        }

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        if (vm_info.vm_flags & LC_VM_INSTALL_TEMPLATE) {
            if (lc_vmdb_vcdc_load_by_rack_id(&vcdc, phost.rack_id)) {
                VMDRIVER_LOG(LOG_ERR, "Query vCenter DC error");
                goto error;
            }
            if ((mlen = lc_vcd_vm_add_msg(&vm_info, &phost, &vcdc, buf, seqid)) <= 0) {
                VMDRIVER_LOG(LOG_ERR, "Construct vcd msg error\n");
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, "Construct vcd msg error",
                                      LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_INSTALL_OP_FAILED;
                goto error;
            }

            if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      mvm->vm_id, vm_info.vm_label, "send to vcd error",
                                      LC_SYSLOG_LEVEL_ERR);
                goto error;
            }
        } else {
            goto error;
        }

        goto finish;

    } else {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "pool type error",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_POOL_ERROR;
        goto error;
    }

    m_send = lc_get_vm_forward_create_msg(m_head, &vm_info);

    if (m_send) {
        lc_msg_forward((lc_mbuf_t *)m_send);
    } else {
        free(m_send);
        goto error;
    }
    free(m_send);

finish:

    return 0;

error:

    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    if (m_head->type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                           TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                         TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }
    return -1;
}

static int lc_vm_create_resume(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    struct lc_msg_head *m_send;
    lc_mbuf_t m;
    agexec_vm_t *av;
    host_t  phost;
    vm_info_t vm_info;
    vm_info_t havm_info;
    agent_vm_id_t vm_id;
    int  i = 0, vm_state = 0, type = LC_MSG_NOTHING;
    char cmd[MAX_CMD_BUF_SIZE];

    av = (agexec_vm_t *)(m_head + 1);

    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&havm_info, 0, sizeof(vm_info_t));
    memset(&m, 0, sizeof(m));

    m.hdr.seq = m_head->seq;
    m.hdr.magic = MSG_MG_UI2DRV;
    if (lc_vmdb_vm_load_by_namelable(
                &vm_info, av->vm_name_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;
        m.hdr.type = LC_MSG_VM_ADD;
        m.hdr.length = sizeof(struct msg_vm_opt);

        mvm = (struct msg_vm_opt *)m.data;
        mvm->vm_id = vm_info.vm_id;
        lc_vmdb_vedge_passwd_load(vm_info.vm_passwd, vm_info.vnet_id);
    } else {
        VMDRIVER_LOG(LOG_ERR, "The VM %s not found\n",av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, av->vm_name_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        if (vm_info.vm_flags & LC_VM_REPLACE_FLAG) {
            type = LC_MSG_VM_REPLACE;
        }
    }

    if (lc_vmdb_compute_resource_load(&phost, vm_info.host_address) !=
        LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found",vm_info.host_address);
        if (vm_info.vm_type == LC_VM_TYPE_VM) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "host not found",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        }
        av->result = 0;
        goto error;
    }
    vm_info.hostp = &phost;

    if (av->result) {
        if (vm_info.vm_type == LC_VM_TYPE_VM) {
            VMDRIVER_LOG(LOG_ERR, "The VM %s create error\n",av->vm_name_label);
        }

        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "completing %s", av->vm_name_label);

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        if (lc_vmdb_vif_loadn_by_vm_id(&vm_info.ifaces[0],
                                       LC_VM_IFACES_MAX,
                                       vm_info.vm_id) != LC_DB_VIF_FOUND) {
            VMDRIVER_LOG(LOG_WARNING, "VM %d interfaces not found",
                         vm_info.vm_id);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
    }

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
            if (av->vifs[i].network_type == LC_DATA_BR_ID ||
                    av->vifs[i].network_type == LC_CTRL_BR_ID) {
                strncpy(vm_info.ifaces[i].if_mac, av->vifs[i].dl_addr,
                        MAX_VIF_MAC_LEN);
                lc_vmdb_vif_update_mac(av->vifs[i].dl_addr,
                                       vm_info.ifaces[i].if_id);
            }

            if (vm_info.ifaces[i].if_state == LC_VIF_STATE_ATTACHING) {
                lc_vmdb_vif_update_state(LC_VIF_STATE_ATTACHED,
                                         vm_info.ifaces[i].if_id);
            }
        }

        lc_vmdb_vm_update_uuid(av->vm_uuid, vm_info.vm_id);
        lc_vmdb_vm_update_vdi_info(av->vdi_sys_uuid, "vdi_sys_uuid",
                vm_info.vm_id);
        lc_vmdb_vm_update_vdi_info(av->vdi_user_uuid, "vdi_user_uuid",
                vm_info.vm_id);
    }

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_EXPORT_STR, vm_info.vm_label);
    lc_call_system(cmd, "VM create", "vm", vm_info.vm_label, lc_vmdriver_db_log);

    memset(&vm_id, 0, sizeof(vm_id));

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        lc_vm_eject_configure_iso(&vm_info);
    }

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        m_send = lc_get_vm_forward_create_msg(&(m.hdr), &vm_info);
        if (m_send) {
            lc_msg_forward((lc_mbuf_t *)m_send);
            free(m_send);
        }
        lc_vmdb_vm_update_action(LC_VM_ACTION_DONE, vm_info.vm_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm_info.vm_id);
        vm_info.vm_flags &= (~LC_VM_REPLACE_FLAG);
        lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    }
    if (type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                              TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                            TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }

    return 0;

error:

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        vm_state = LC_VM_STATE_ERROR;
        switch (av->result) {
        case AGEXEC_VM_CREATE_TEMPLATE_NOT_EXIST:
            vm_info.vm_error |= LC_VM_ERRNO_NO_TEMPLATE;
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, av->err_desc,
                                  LC_SYSLOG_LEVEL_ERR);
            break;
        case AGEXEC_VM_CREATE_INSTALL_FAILED:
            vm_info.vm_error |= LC_VM_ERRNO_INSTALL_OP_FAILED;
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, av->err_desc,
                                  LC_SYSLOG_LEVEL_ERR);
            break;
        case AGEXEC_VM_CREATE_SYS_SR_NOT_FOUND:
        case AGEXEC_VM_CREATE_SYS_DISK_NOT_FOUND:
        case AGEXEC_VM_CREATE_USER_SR_NOT_FOUND:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, av->err_desc,
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DISK_ADD;
            break;
        case AGEXEC_VM_CREATE_SYS_SR_INSUFFICIENT_SPACE:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "No enough system storage space",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_NO_DISK;
            break;
        case AGEXEC_VM_CREATE_SYS_DISK_RESIZE_FAILED:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "Failed to resize system disk",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DISK_SYS_RESIZE;
            break;
        case AGEXEC_VM_CREATE_USER_SR_INSUFFICIENT_SPACE:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "No enough user storge space",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_NO_DISK;
            break;
        case AGEXEC_VM_CREATE_USER_DISK_CREATE_FAILED:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "Failed to resize user disk",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DISK_USER_RESIZE;
            break;
        case AGEXEC_VM_CREATE_CPU_RECONFIGURE_FAILED:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "Failed to configure cpu",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_NO_CPU;
            break;
        case AGEXEC_VM_CREATE_MEMORY_RECONFIGURE_FAILED:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "Failed to configure memory",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_NO_MEMORY;
            break;
        case AGEXEC_VM_SET_HA_FAILED:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label,
                                  "HA pool can't restart all VMs after a Host failure once starting it",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
            vm_state = LC_VM_STATE_STOPPED;
            break;
        case AGEXEC_VM_OPS_ERROR:
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, av->err_desc,
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_CREATE_ERROR;
            break;
        }
        lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*        lc_vmdb_vm_update_state(vm_state, vm_info.vm_id);*/
        lc_vm_eject_configure_iso(&vm_info);
    }

    if (type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                           TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                         TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }
    return -1;
}

static int lc_vm_del(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    vm_info_t vm_info;
    host_t phost;
    host_t peer_phost;
    rpool_t pool;
    int mlen, ret = 0;
    char buf[MAX_MSG_BUFFER];
    char cmd[MAX_CMD_BUF_SIZE];
    vm_backup_info_t vm_backup_info;
    vm_snapshot_info_t vm_snapshot_info;
    uint32_t seq = m_head->seq;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&peer_phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vm_backup_info, 0, sizeof(vm_backup_info_t));
    memset(&vm_snapshot_info, 0, sizeof(vm_snapshot_info_t));
    memset(buf, 0, MAX_MSG_BUFFER);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);

    mvm = (struct msg_vm_opt *)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {

        VMDRIVER_LOG(LOG_ERR, "The vm %d to be destroyed not found\n", mvm->vm_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "LC delete vm (%s)\n",vm_info.vm_label);

    if (lc_vmdb_compute_resource_load(&phost, vm_info.host_address)
            != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",vm_info.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (strcmp(phost.host_address, phost.master_address)) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not master, find master %s\n",phost.host_address, phost.master_address);

        if (lc_vmdb_compute_resource_load(&phost, phost.master_address)
                != LC_DB_HOST_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",phost.host_address);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "host not found",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
    }

    phost.master = &phost;
    vm_info.hostp = &phost;

    lc_vmdb_vif_update_state(LC_VIF_STATE_DETACHED, vm_info.pub_vif[0].if_id);

    if (vm_info.vm_flags & LC_VM_THIRD_HW_FLAG) {
        lc_vmdb_vm_update_state(LC_VM_STATE_DESTROYED, vm_info.vm_id);
        lc_msg_forward((lc_mbuf_t *)m_head);
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                             TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
        goto finish;
    }

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        if (lc_agexec_vm_ops(&vm_info, LC_MSG_VM_DEL, seq)) {
            VMDRIVER_LOG(LOG_ERR, "VM %s del failure\n",vm_info.vm_label);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "uninstall error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DELETE_ERROR;
            goto error;
        }
        goto finish;

        lc_vmdb_vm_update_state(LC_VM_STATE_DESTROYED, vm_info.vm_id);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        if ((mlen = lc_vcd_vm_del_msg(&vm_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Construct vcd msg error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DELETE_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DELETE_ERROR;
            goto error;
        }
        ret = LC_POOL_TYPE_VMWARE;
        goto finish;

    } else {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_POOL_ERROR;
        goto error;
    }

    lc_vm_remove_configure_iso(&vm_info);

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_DELETE_STR, vm_info.vm_label);
    lc_call_system(cmd, "VM del", "vm", vm_info.vm_label, lc_vmdriver_db_log);

    if (m_head->type != LC_MSG_VM_REPLACE) {
        lc_msg_forward((lc_mbuf_t *)m_head);
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                             TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    }

finish:

    return ret;

error:

    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/

    if (m_head->type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                          TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                         TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }
    return -1;
}

static int lc_vm_start(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    rpool_t pool;
    host_t phost;
    host_t master;
    vm_info_t vm_info;
    int mlen;
    char buf[MAX_MSG_BUFFER];
    uint32_t seq = m_head->seq;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&master, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(buf, 0, MAX_MSG_BUFFER);

    mvm = (struct msg_vm_opt *)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {

        VMDRIVER_LOG(LOG_ERR, "The vm to run not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, vm_info.host_address);
    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) !=
            LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found", phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    /*Load host_device table to get host name*/
    if (lc_vmdb_host_device_load(&phost, phost.host_address, "name")
        != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found", phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    vm_info.hostp = &phost;
    strcpy(vm_info.host_name_label, phost.host_name);

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        VMDRIVER_LOG(LOG_DEBUG, "Start VM %s on host %s",
                     vm_info.vm_label, vm_info.hostp->host_address);

        if (lc_start_vm_by_vminfo(&vm_info, LC_MSG_VM_START, seq)) {
            VMDRIVER_LOG(LOG_ERR, "Start VM:(%s) failure\n",
                         vm_info.vm_label);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label,
                                  "start command error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
            goto error;
        }
        goto finish;

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        VMDRIVER_LOG(LOG_DEBUG, "Start VM %s on host %s",
                     vm_info.vm_label, vm_info.hostp->host_address);

        if ((mlen = lc_vcd_vm_start_msg(&vm_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Construct vcd msg error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label,
                                  "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen,
                                            LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label,
                                  "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
            goto error;
        }

        goto finish;

    } else {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label,
                              "pool type error",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_POOL_ERROR;
        goto error;
    }
finish:

    return 0;

error:
    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                  TALKER__VM_OPS__VM_START, LC_BUS_QUEUE_TALKER);
    return -1;
}

static int lc_vm_stop(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    rpool_t pool;
    host_t phost;
    host_t master;
    vm_info_t vm_info;
    int mlen;
    char buf[MAX_MSG_BUFFER];
    uint32_t seq = m_head->seq;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&master, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(buf, 0, MAX_MSG_BUFFER);

    mvm = (struct msg_vm_opt *)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {

        VMDRIVER_LOG(LOG_ERR, "The vm to be stopped not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "LC stop vm vmname: %s\n",vm_info.vm_label);

    if (lc_vmdb_compute_resource_load(&phost, vm_info.host_address)
            != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n", phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    phost.master = &phost;
    vm_info.hostp = &phost;

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        if (lc_agexec_vm_ops(&vm_info, LC_MSG_VM_STOP, seq)) {
            VMDRIVER_LOG(LOG_ERR, "Stop vm:(%s) failure\n",vm_info.vm_label);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label,
                                  "stop command error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_STOP_ERROR;
            goto error;
        }
        goto finish;

        lc_vmdb_vm_update_action(LC_VM_ACTION_DONE, vm_info.vm_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm_info.vm_id);
        if (vm_info.vm_type == LC_VM_TYPE_GW) {
            lc_vm_eject_configure_iso(&vm_info);
        }

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        if ((mlen = lc_vcd_vm_shutdown_msg(&vm_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Construct vcd msg error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label, "onstruct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_STOP_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  mvm->vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }

        goto finish;

    } else {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "pool type error",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_POOL_ERROR;
        goto error;
    }

    lc_msg_forward((lc_mbuf_t *)m_head);
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                            TALKER__VM_OPS__VM_STOP, LC_BUS_QUEUE_TALKER);

finish:

    return 0;

error:

    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/

    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                  TALKER__VM_OPS__VM_STOP, LC_BUS_QUEUE_TALKER);
    return -1;
}

static int lc_vm_modify(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    vm_modify_t  *pvm_modify = NULL;
    rpool_t pool;
    host_t phost;
    vm_info_t vm_info;
    disk_t disks[MAX_VM_HA_DISK];
    int  disk_num = 0;
    char cmd[MAX_CMD_BUF_SIZE];
    int mlen;
    char buf[MAX_MSG_BUFFER];
    int op;
    int i,ret = 0;
    vcdc_t vcdc;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(disks, 0, sizeof(disks));
    memset(&vcdc, 0, sizeof vcdc);

    mvm = (struct msg_vm_opt*)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The vm to be modified not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto finish;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto finish;
    }

    strcpy(phost.host_address, vm_info.host_address);
    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n", phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto finish;
    }

    vm_info.hostp = &phost;

    if (mvm->vm_chgmask == VM_CHANGE_MASK_VL2_ID) {
        goto finish;
    }
    pvm_modify = (vm_modify_t *)mvm->data;
    vm_info.vcpu_num = 0;
    vm_info.mem_size = 0;
    vm_info.vdi_user_size = 0;
    if (mvm->vm_chgmask & VM_CHANGE_MASK_HA_DISK) {
        for (i=0; i<pvm_modify->ha_disk_num && pvm_modify->ha_disks[i].ha_disk_size; i++) {
            disk_num++;
            if (lc_vmdb_disk_load(disks + i, mvm->vm_id, pvm_modify->ha_disks[i].userdevice) != LC_DB_VM_FOUND) {
                VMDRIVER_LOG(LOG_ERR, "The ha disk to be modified not found\n");
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto finish;
            }
            if (!disks[i].vdi_sr_uuid[0]) {
                if (lc_hastoragedb_host_device_get_storage_uuid(
                            disks[i].vdi_sr_uuid, MAX_UUID_LEN,
                            vm_info.host_address, disks[i].vdi_sr_name)) {
                    VMDRIVER_LOG(LOG_ERR, "The storage %s not found\n",
                           disks[i].vdi_sr_name);
                    vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                    goto finish;
                }
                lc_vmdb_disk_update_sr_uuid(disks[i].vdi_sr_uuid, disks[i].id);
            }
            disks[i].size = pvm_modify->ha_disks[i].ha_disk_size;
        }
    }
    if (pool.ctype == LC_POOL_TYPE_XEN) {

        VMDRIVER_LOG(LOG_DEBUG, "LC modify vm configuration:%s, server:%s mask=%d\n",
               vm_info.vm_label, phost.host_address, mvm->vm_chgmask);

        if (mvm->vm_chgmask & VM_CHANGE_MASK_DISK_USER_RESIZE) {
            memset(vm_info.vdi_sys_sr_uuid, 0, AGEXEC_LEN_UUID);
            vm_info.vdi_user_size = pvm_modify->disk_size;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_DISK_ADD) {
            memset(vm_info.vdi_user_uuid, 0, AGEXEC_LEN_UUID);
            vm_info.vdi_user_size = pvm_modify->disk_size;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_CPU_RESIZE) {
            vm_info.vcpu_num = pvm_modify->cpu;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_MEM_RESIZE) {
            vm_info.mem_size = pvm_modify->mem;
        }
        ret = lc_agexec_vm_modify(&vm_info, disk_num, disks);
        if (ret & AGEXEC_CPU_RECONFIG_ERROR) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "reconfig cpu error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_CPU_RESIZE;
        } else {
            if (vm_info.vcpu_num) {
                lc_vmdb_vm_update_cpu(vm_info.vcpu_num, vm_info.vm_id);
            }
            vm_info.vm_error &= ~LC_VM_ERRNO_CPU_RESIZE;
        }
        if (ret & AGEXEC_MEM_RECONFIG_ERROR) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "reconfig memory error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_MEM_RESIZE;
        } else {
            if (vm_info.mem_size) {
                lc_vmdb_vm_update_memory(vm_info.mem_size, vm_info.vm_id);
            }
            vm_info.vm_error &= ~LC_VM_ERRNO_MEM_RESIZE;
        }
        if (ret & AGEXEC_USER_DISK_RESIZE_ERROR) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "resize usr disk error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DISK_USER_RESIZE;
        } else if (ret & AGEXEC_USER_DISK_SET_ERROR) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "add usr disk error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_VM_ERRNO_DISK_ADD;
        } else {
            if (vm_info.vdi_user_size > 0) {
                lc_vmdb_vm_update_vdi_size(vm_info.vdi_user_size,
                                 "vdi_user_size", vm_info.vm_id);
            }
            vm_info.vm_error &= ~LC_VM_ERRNO_DISK_USER_RESIZE;
            vm_info.vm_error &= ~LC_VM_ERRNO_DISK_ADD;
        }
        if ((0 == ret) && (mvm->vm_chgmask & VM_CHANGE_MASK_DISK_ADD)) {
            lc_vmdb_vm_update_vdi_info(vm_info.vdi_user_uuid, "vdi_user_uuid",
                                                               vm_info.vm_id);
            lc_vmdb_vm_update_vdi_info(vm_info.vdi_sys_sr_uuid, "vdi_user_sr_uuid",
                                                               vm_info.vm_id);
            lc_vmdb_vm_update_vdi_info(vm_info.vdi_sys_sr_name, "vdi_user_sr_name",
                                                               vm_info.vm_id);
        }
        if (ret & AGEXEC_HA_DISK_ERROR) {
            vm_info.vm_error |= LC_VM_ERRNO_DISK_HA;
        } else {
            vm_info.vm_error &= ~LC_VM_ERRNO_DISK_HA;
            if (mvm->vm_chgmask & VM_CHANGE_MASK_HA_DISK) {
                vm_info.vm_flags |= LC_VM_HA_DISK_FLAG;
            }
        }
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_EXPORT_STR, vm_info.vm_label);
        lc_call_system(cmd, "VM modify", "vm", vm_info.vm_label, lc_vmdriver_db_log);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        op = 0;
        vm_info.vcpu_num = pvm_modify->cpu;
        vm_info.mem_size = pvm_modify->mem;
        vm_info.vdi_user_size = pvm_modify->disk_size;
        if (mvm->vm_chgmask & VM_CHANGE_MASK_DISK_SYS_RESIZE) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_SYS_DISK;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_DISK_ADD ||
            mvm->vm_chgmask & VM_CHANGE_MASK_DISK_DEL ||
            mvm->vm_chgmask & VM_CHANGE_MASK_DISK_USER_RESIZE) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_USER_DISK;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_CPU_RESIZE) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_CPU_NUM;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_MEM_RESIZE) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_MEMORY_SIZE;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_BW_MODIFY) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_INTF;
        }
        if (mvm->vm_chgmask & VM_CHANGE_MASK_HA_DISK && disk_num) {
            op |= CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_HA_DISK;
        }
        if (lc_vmdb_vcdc_load_by_rack_id(&vcdc, phost.rack_id)) {
            VMDRIVER_LOG(LOG_ERR, "Query vCenter DC error");
            goto finish;
        }
        if ((mlen = lc_vcd_vm_update_msg(&vm_info, op,
                        disk_num, disks, &vcdc, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Construct vcd msg error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto finish;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto finish;
        }

    }

finish:
    vm_info.vm_flags &= (~LC_VM_CHG_FLAG);
    lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
    if (mvm->vm_chgmask & VM_CHANGE_MASK_VL2_ID) {
        lc_msg_forward((lc_mbuf_t *)m_head);
    }
    if (vm_info.vm_error) {
/*        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                           TALKER__VM_OPS__VM_MODIFY, LC_BUS_QUEUE_LCRMD);
        return -1;
    }
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                       TALKER__VM_OPS__VM_MODIFY, LC_BUS_QUEUE_LCRMD);
    return 0;
}

static int lc_storage_resource_opt(struct lc_msg_head *m_head)
{
    struct msg_storage_resource_opt *mvm;
    storage_resource_t sr_info, peer_sr;
    host_t phost, peer_host;
    int type;

    memset(&sr_info, 0, sizeof(storage_resource_t));
    memset(&peer_sr, 0, sizeof(storage_resource_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&peer_host, 0, sizeof(host_t));

    mvm = (struct msg_storage_resource_opt *)(m_head + 1);

    if (lc_vmdb_storage_resource_load_by_id(&sr_info, mvm->storage_id) != LC_DB_STORAGE_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
        sr_info.storage_error |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    if (sr_info.storage_state == LC_SR_STATE_COMPLETE) {
        VMDRIVER_LOG(LOG_DEBUG, "Peer storage resource skipped\n");
        lc_msg_forward((lc_mbuf_t *)m_head);
        return 0;
    }

    if (sr_info.storage_state != LC_SR_STATE_TO_JOIN) {
        VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
        sr_info.storage_error |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, sr_info.host_address);

    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",
                     phost.host_address);
        sr_info.storage_error |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    phost.master = &phost;

    if ((phost.host_service_flag == LC_HOST_PEER) ||
        (phost.host_service_flag == LC_HOST_HA)) {
        if (lc_vmdb_storage_resource_load_by_ip(&peer_sr,
                    phost.peer_address) != LC_DB_STORAGE_FOUND) {

            VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
            peer_sr.storage_error |= LC_SR_ERRNO_DB_ERROR;
            goto error;
        }

        if (peer_sr.storage_state != LC_SR_STATE_TO_JOIN) {
            VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
            peer_sr.storage_error |= LC_SR_ERRNO_DB_ERROR;
            goto error;
        }
    }

    type = lc_vmdb_get_pool_htype(phost.pool_index);
    if (type == LC_POOL_TYPE_XEN) {

    } else if (type == LC_POOL_TYPE_VMWARE) {
        /* TODO: Fix hack */
        char dsname[LC_NAMESIZE];
        sprintf(dsname, "datastore%s", sr_info.host_address);
        lc_vmdb_storage_resource_sr_name_update(dsname, sr_info.storage_id);
    } else if (type == LC_POOL_TYPE_KVM) {
    } else {
        sr_info.storage_error |= LC_SR_ERRNO_POOL_ERROR;
        goto error;
    }

    if ((phost.host_service_flag == LC_HOST_PEER) ||
        (phost.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_storage_resource_state_update(LC_SR_STATE_COMPLETE,
                peer_sr.storage_id);
    }
    lc_vmdb_storage_resource_state_update(LC_SR_STATE_COMPLETE,
            sr_info.storage_id);

/*    if (m_send) {
        lc_msg_forward((lc_mbuf_t *)m_send);
    } else {
        free(m_send);
        goto error;
    }

    free(m_send);
*/
    return 0;

error:

    if ((phost.host_service_flag == LC_HOST_PEER)||
        (phost.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_storage_resource_errno_update(peer_sr.storage_error,
                peer_sr.storage_id);
        lc_vmdb_storage_resource_state_update(LC_SR_STATE_ERROR,
                peer_sr.storage_id);
    }
    lc_vmdb_storage_resource_errno_update(sr_info.storage_error,
            sr_info.storage_id);
    lc_vmdb_storage_resource_state_update(LC_SR_STATE_ERROR,
            sr_info.storage_id);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_storage_resource_del(struct lc_msg_head *m_head)
{
    struct msg_storage_resource_opt *mvm;
    storage_resource_t sr_info;
    host_t phost;
    int type;

    memset(&sr_info, 0, sizeof(storage_resource_t));
    memset(&phost, 0, sizeof(host_t));

    mvm = (struct msg_storage_resource_opt *)(m_head + 1);

    if (lc_vmdb_storage_resource_load_by_id(&sr_info, mvm->storage_id) != LC_DB_STORAGE_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
        sr_info.storage_error |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, sr_info.host_address);

    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",
                      phost.host_address);
        sr_info.storage_error |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    phost.master = &phost;

    type = lc_vmdb_get_pool_htype(phost.pool_index);
    if (type == LC_POOL_TYPE_XEN) {
        if (sr_info.storage_state == LC_SR_STATE_TO_EJECT) {
            if (lc_del_network_storage(&phost, sr_info.sr_uuid)) {
                VMDRIVER_LOG(LOG_ERR, "The shared storage delete error\n");
                sr_info.storage_error |= LC_SR_ERRNO_GET_INFO_ERROR;
                goto error;
            }
            if (lc_vmdb_storage_resource_state_update(LC_SR_STATE_EJECTED, sr_info.storage_id)) {
                VMDRIVER_LOG(LOG_ERR, "Update storage resource sr state error.\n");
            }
        }

    } else if (type == LC_POOL_TYPE_VMWARE) {
    } else {
        sr_info.storage_error |= LC_SR_ERRNO_POOL_ERROR;
        goto error;
    }

/*    if (m_send) {
        lc_msg_forward((lc_mbuf_t *)m_send);
    } else {
        free(m_send);
        goto error;
    }

    free(m_send);
*/
    return 0;

error:

    lc_vmdb_storage_resource_errno_update(sr_info.storage_error,
            sr_info.storage_id);
    lc_vmdb_storage_resource_state_update(LC_SR_STATE_ERROR,
            sr_info.storage_id);

/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_host_add(struct lc_msg_head *m_head)
{
    struct msg_host_device_opt *host_add;
    host_t host;
    int mlen, i;
    char buf[MAX_MSG_BUFFER];

    host_add = (struct msg_host_device_opt *)(m_head + 1);

    memset(&host, 0, sizeof(host_t));

    VMDRIVER_LOG(LOG_DEBUG, "Add host (%d) and host_is_ofs:%d\n",
                 host_add->host_id,host_add->host_is_ofs);

    if (lc_vmdb_host_device_load_by_id(&host, host_add->host_id, "*") !=  LC_DB_HOST_FOUND
            || host.host_state != LC_HOST_STATE_CREATING) {
        lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_DB_ERROR,
                host.host_address);
        VMDRIVER_LOG(LOG_ERR, "LC add host failure: host not found\n");
        goto error;
    }

    if (host.host_htype == LC_HOST_HTYPE_XEN) {
        if(host_add->host_is_ofs){
            if(lc_get_host_mac_info(&host) != 0){
                goto error;
            }
        }
        if (lc_get_host_if_type(&host) != 0) {
            goto error;
        }
        lc_vmdb_host_device_iftype_update(&host, host.host_address);

        if (lc_get_host_device_info(&host) != 0) {
            lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_CON_ERROR,
                    host.host_address);
            VMDRIVER_LOG(LOG_ERR, "LC get host device error\n");
            goto error;
        }
        lc_vmdb_host_device_info_update(&host, host.host_address);
        lc_storagedb_host_device_del_storage(&host);
        for (i = 0; i < host.n_storage; ++i) {
            lc_storagedb_host_device_add_storage(&host, &host.storage[i]);
        }
        lc_storagedb_host_device_del_ha_storage(&host);
        for (i = 0; i < host.n_hastorage; ++i) {
            lc_storagedb_host_device_add_ha_storage(&host, &host.ha_storage[i]);
        }

    } else if (host.host_htype == LC_POOL_TYPE_VMWARE) {

        memset(buf, 0, sizeof(buf));
        if ((mlen = lc_vcd_hosts_info_msg(&host, buf)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Host add error\n");
            host.error_number |= LC_HOST_ERRNO_CON_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            goto error;
        }

        goto finish;
    } else if (host.host_htype == LC_POOL_TYPE_KVM) {
        if (lc_nsp_get_host_name(&host) != 0) {
            VMDRIVER_LOG(LOG_ERR, "NSP add error\n");
            goto error;
        }
        lc_vmdb_host_device_info_update(&host, host.host_address);
    }

    lc_vmdb_host_device_state_update(LC_HOST_STATE_COMPLETE,
            host.host_address);

    VMDRIVER_LOG(LOG_DEBUG, "Add host (%d) finish\n",
                 host_add->host_id);

    lc_msg_forward((lc_mbuf_t *)m_head);

finish:

    return 0;

error:

    lc_vmdb_host_device_state_update(LC_HOST_STATE_ERROR,
            host.host_address);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_host_del(struct lc_msg_head *m_head)
{
    struct msg_host_device_opt *host_del;
    host_t host;

    host_del = (struct msg_host_device_opt *)(m_head + 1);

    memset(&host, 0, sizeof(host_t));

    VMDRIVER_LOG(LOG_DEBUG, "Delete host (%d)\n",
                 host_del->host_id);
    if (lc_vmdb_host_device_load_by_id(&host, host_del->host_id, "*") !=  LC_DB_HOST_FOUND) {
        lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_DB_ERROR,
                host.host_address);
        VMDRIVER_LOG(LOG_ERR, "LC delete host failure: host not found\n");
        goto error;
    }

    if (host.host_htype != LC_POOL_TYPE_KVM) {
        lc_storagedb_host_device_del_storage(&host);
        lc_storagedb_host_device_del_ha_storage(&host);
    }

    lc_vmdb_host_device_state_update(LC_HOST_STATE_COMPLETE,
            host.host_address);

    VMDRIVER_LOG(LOG_DEBUG, "Delete host (%d) finish\n",host_del->host_id);

    lc_msg_forward((lc_mbuf_t *)m_head);

    return 0;

error:

    lc_vmdb_host_device_state_update(LC_HOST_STATE_ERROR, host.host_address);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_host_change(struct lc_msg_head *m_head)
{
    struct msg_host_device_opt *host_chg;
    host_t host;
    int mlen, i;
    char buf[MAX_MSG_BUFFER];

    host_chg = (struct msg_host_device_opt *)(m_head + 1);

    memset(&host, 0, sizeof(host_t));

    VMDRIVER_LOG(LOG_DEBUG, "Change host (%d)\n",host_chg->host_id);

    if (lc_vmdb_host_device_load_by_id(&host, host_chg->host_id, "*") !=  LC_DB_HOST_FOUND
            || host.host_state != LC_HOST_STATE_CHANGING) {
        lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_DB_ERROR,
                host.host_address);
        VMDRIVER_LOG(LOG_ERR, "LC change host failure: host not found\n");
        goto error;
    }

    if (host.host_htype == LC_HOST_HTYPE_XEN) {

        if (lc_get_host_if_type(&host) != 0) {
            goto error;
        }
        lc_vmdb_host_device_iftype_update(&host, host.host_address);

        if (lc_get_host_device_info(&host)) {
            lc_vmdb_host_device_errno_update(LC_HOST_ERRNO_CON_ERROR,
                    host.host_address);
            VMDRIVER_LOG(LOG_ERR, "Change host (%d) error\n",host_chg->host_id);
            goto error;
        }
        lc_vmdb_host_device_info_update(&host, host.host_address);
        lc_storagedb_host_device_del_storage(&host);
        for (i = 0; i < host.n_storage; ++i) {
            lc_storagedb_host_device_add_storage(&host, &host.storage[i]);
        }
        lc_storagedb_host_device_del_ha_storage(&host);
        for (i = 0; i < host.n_hastorage; ++i) {
            lc_storagedb_host_device_add_ha_storage(&host, &host.ha_storage[i]);
        }
    } else if (host.host_htype == LC_POOL_TYPE_VMWARE) {

        memset(buf, 0, sizeof(buf));
        if ((mlen = lc_vcd_hosts_info_msg(&host, buf)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Host change error\n");
            host.error_number |= LC_HOST_ERRNO_CON_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            goto error;
        }

        goto finish;
    } else if (host.host_htype == LC_HOST_HTYPE_KVM) {
        if (lc_nsp_get_host_name(&host) != 0) {
            VMDRIVER_LOG(LOG_ERR, "NSP add error\n");
            goto error;
        }
    }

    lc_vmdb_host_device_state_update(LC_HOST_STATE_COMPLETE,
            host.host_address);

    VMDRIVER_LOG(LOG_DEBUG, "Change host (%d) finish\n",
                 host_chg->host_id);

    lc_msg_forward((lc_mbuf_t *)m_head);

finish:

    return 0;

error:

    lc_vmdb_host_device_state_update(LC_HOST_STATE_ERROR, host.host_address);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

#define MEM_OVERCOMMIT_ID 18
#define MEM_OVERCOMMIT_STRING "value"
static int get_memory_overcommit_process(void *cfg_info, char *field, char *value)
{
    int *mem_overcommit = (int *)cfg_info;
    if (strcmp(field, MEM_OVERCOMMIT_STRING) == 0) {
        *mem_overcommit = atoi(value);
    }
    return 0;
}

static int lc_get_memory_overcommit(void)
{
    char condition[LC_DB_BUF_LEN];
    int result = 0;
    int id = MEM_OVERCOMMIT_ID;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    lc_db_table_load((void *)&result, "sys_configuration", MEM_OVERCOMMIT_STRING,
                                       condition, get_memory_overcommit_process);
    return result;
}

static int lc_host_join(struct lc_msg_head *m_head)
{
    struct msg_compute_resource_opt *host_join;
    rpool_t pool;
    host_t host, peer;
    host_t local_host, peer_host;
    int local_mem_total = 0, peer_mem_total = 0;

    host_join = (struct msg_compute_resource_opt *)(m_head + 1);

    memset(&pool, 0, sizeof(rpool_t));
    memset(&host, 0, sizeof(host_t));
    memset(&peer, 0, sizeof(host_t));

    if (lc_vmdb_compute_resource_load_by_id(&host, host_join->compute_id) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "LC host join failure: host not found"
        "(id=%d)\n", host_join->compute_id);
        host.error_number |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }
    if (host.host_state == LC_CR_STATE_COMPLETE) {
        VMDRIVER_LOG(LOG_DEBUG, "LC host join skipped: "
                "host poolified\n");
        lc_msg_forward((lc_mbuf_t *)m_head);
        return 0;
    }

    if (host.host_state != LC_CR_STATE_TO_JOIN) {
        VMDRIVER_LOG(LOG_ERR, "LC host join failure: "
                "db state incorrect\n");
        host.error_number |= LC_NR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, host.pool_index) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "LC host join pool failure, "
                  "pool %d not exist\n",host.pool_index);
        host.error_number |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {

        if (lc_vmdb_compute_resource_load(&peer,
                    host.peer_address) != LC_DB_HOST_FOUND) {

            VMDRIVER_LOG(LOG_ERR, "LC host join failure: peer host not found"
            "(id=%d)\n",host_join->compute_id);
            peer.error_number |= LC_SR_ERRNO_DB_ERROR;
            goto error;
        }

        if (peer.host_state != LC_CR_STATE_TO_JOIN) {
            VMDRIVER_LOG(LOG_ERR, "LC host join failure: db state incorrect\n");
            peer.error_number |= LC_NR_ERRNO_DB_ERROR;
            goto error;
        }

    }

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        if (lc_xen_get_host_uuid(&host)) {
            VMDRIVER_LOG(LOG_ERR, "get host %s uuid error\n", host.host_address);
            host.error_number |= LC_CR_ERRNO_GET_INFO_ERROR;
            goto error;
        }
        lc_vmdb_compute_resource_uuid_update(host.host_uuid, host.host_address);

        memset(&local_host, 0, sizeof(host_t));
        if (lc_vmdb_host_device_load(&local_host, host.host_address, "*")
            != LC_DB_HOST_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "host %s not found\n", host.host_address);
            host.error_number |= LC_CR_ERRNO_DB_ERROR;
            goto error;
        }
        lc_vmdb_compute_resource_master_ip_update(local_host.master_address,
                host.host_address);

        if ((host.host_service_flag == LC_HOST_PEER) ||
            (host.host_service_flag == LC_HOST_HA)) {

            if (lc_xen_get_host_uuid(&peer)) {
                VMDRIVER_LOG(LOG_ERR, "get host %s info error\n", peer.host_address);
                peer.error_number |= LC_CR_ERRNO_GET_INFO_ERROR;
                goto error;
            }
            lc_vmdb_compute_resource_uuid_update(peer.host_uuid, peer.host_address);

            memset(&peer_host, 0, sizeof(host_t));
            if (lc_vmdb_host_device_load(&peer_host, peer.host_address, "*")
                != LC_DB_HOST_FOUND) {
                VMDRIVER_LOG(LOG_ERR, "host %s not found\n", peer.host_address);
                peer.error_number |= LC_CR_ERRNO_DB_ERROR;
                goto error;
            }
            lc_vmdb_compute_resource_master_ip_update(peer_host.master_address,
                    peer.host_address);
        }

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {
        memset(&local_host, 0, sizeof(host_t));
        if (lc_vmdb_host_device_load(&local_host, host.host_address, "*")
            != LC_DB_HOST_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "host %s not found\n", host.host_address);
            host.error_number |= LC_CR_ERRNO_DB_ERROR;
            goto error;
        }
        host.host_flag |= LC_HOST_STATE_INUSE;
        lc_vmdb_compute_resource_flag_update(host.host_flag, host.host_address);
        lc_vmdb_compute_resource_state_update(LC_CR_STATE_COMPLETE,
                host.host_address);
        lc_vmdb_compute_resource_master_ip_update(local_host.master_address,
                host.host_address);

        goto finish;
    } else if (pool.ctype == LC_POOL_TYPE_KVM) {
        /* Nothing to do */
    }

    if (host.host_service_flag == LC_HOST_PEER) {
        if (lc_get_memory_overcommit()) {
            local_mem_total = local_host.mem_total;
            peer_mem_total = peer_host.mem_total;
        } else {
            local_mem_total = local_host.mem_total / 2;
            peer_mem_total = peer_host.mem_total / 2;
        }
        lc_vmdb_compute_resource_memory_update(local_mem_total,
                                      local_host.host_address);
        lc_vmdb_compute_resource_memory_update(peer_mem_total,
                                      peer_host.host_address);
    }

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_compute_resource_state_update(LC_CR_STATE_COMPLETE,
                peer_host.host_address);
        peer.host_flag |= LC_HOST_STATE_INUSE;
        lc_vmdb_compute_resource_flag_update(peer.host_flag,
                peer_host.host_address);
    }
    lc_vmdb_compute_resource_state_update(LC_CR_STATE_COMPLETE,
            host.host_address);
    host.host_flag |= LC_HOST_STATE_INUSE;
    lc_vmdb_compute_resource_flag_update(host.host_flag, host.host_address);

    lc_msg_forward((lc_mbuf_t *)m_head);

finish:

    return 0;

error:

    if ((host.host_service_flag == LC_HOST_PEER) ||
        (host.host_service_flag == LC_HOST_HA)) {
        lc_vmdb_compute_resource_errno_update(peer.error_number,
                peer_host.host_address);
        lc_vmdb_compute_resource_state_update(LC_CR_STATE_ERROR,
                peer_host.host_address);
    }
    lc_vmdb_compute_resource_errno_update(host.error_number, host.host_address);
    lc_vmdb_compute_resource_state_update(LC_CR_STATE_ERROR, host.host_address);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int lc_host_eject(struct lc_msg_head *m_head)
{
    struct msg_compute_resource_opt *host_eject;
    rpool_t pool;
    host_t host;
    storage_resource_t sr_info;
    host_t local_host;

    host_eject = (struct msg_compute_resource_opt *)(m_head + 1);

    memset(&pool, 0, sizeof(rpool_t));
    memset(&host, 0, sizeof(host_t));
    memset(&sr_info, 0, sizeof(storage_resource_t));

    if (lc_vmdb_compute_resource_load_by_id(&host, host_eject->compute_id) != LC_DB_HOST_FOUND
        || host.host_state != LC_CR_STATE_TO_EJECT) {
        VMDRIVER_LOG(LOG_ERR, "LC add host failure: host not found\n");
        host.error_number |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, host.pool_index) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "LC host join pool failure, "
                  "pool %d not exist\n",host.pool_index);
        host.error_number |= LC_SR_ERRNO_DB_ERROR;
        goto error;
    }

    if (pool.ctype == LC_POOL_TYPE_XEN) {

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {
        memset(&local_host, 0, sizeof(host_t));
        if (lc_vmdb_host_device_load(&local_host, host.host_address, "*")
            != LC_DB_HOST_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "host %s not found\n", host.host_address);
            host.error_number |= LC_CR_ERRNO_DB_ERROR;
            goto error;
        }
    } else if (pool.ctype == LC_POOL_TYPE_KVM) {
        /* Nothing to do */
    }

    if (pool.ctype != LC_POOL_TYPE_KVM) {
        if (lc_vmdb_storage_resource_load_by_ip(&sr_info, host.host_address) != LC_DB_STORAGE_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "The storage resource not found\n");
            goto error;
        }

        if (lc_vmdb_storage_resource_state_update(LC_SR_STATE_EJECTED, sr_info.storage_id)) {
            VMDRIVER_LOG(LOG_ERR, "Update storage resource sr state error.\n");
            goto error;
        }
    }
    lc_vmdb_compute_resource_state_update(LC_CR_STATE_EJECTED, host.host_address);

    lc_msg_forward((lc_mbuf_t *)m_head);

    return 0;

error:

    lc_vmdb_compute_resource_errno_update(host.error_number, host.host_address);
    lc_vmdb_compute_resource_state_update(LC_CR_STATE_ERROR, host.host_address);
/*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);
    return -1;
}

static int vmware_msg_handler(struct lc_msg_head *m_head)
{
    Cloudmessage__Message *msg = NULL;
    int ret = 0;
    uint32_t seqid = m_head->seq;

    if (!m_head) {
        return -1;
    }

    msg = cloudmessage__message__unpack(NULL, m_head->length,
            (uint8_t *)(m_head + 1));

    if (!msg) {
        return -1;
    }

    if (msg->vm_add_response) {
        ret = lc_vcd_vm_add_reply(msg->vm_add_response, seqid);
    } else if (msg->vm_update_response) {
        ret = lc_vcd_vm_update_reply(msg->vm_update_response, seqid);
    } else if (msg->vm_ops_response) {
        ret = lc_vcd_vm_ops_reply(msg->vm_ops_response, seqid);
    } else if (msg->vm_del_response) {
        ret = lc_vcd_vm_del_reply(msg->vm_del_response, seqid);
    } else if (msg->vm_snapshot_add_response) {
        ret = lc_vcd_vm_snapshot_add_reply(msg->vm_snapshot_add_response, seqid);
    } else if (msg->vm_snapshot_del_response) {
        ret = lc_vcd_vm_snapshot_del_reply(msg->vm_snapshot_del_response, seqid);
    } else if (msg->vm_snapshot_revert_response) {
        ret = lc_vcd_vm_snapshot_revert_reply(msg->vm_snapshot_revert_response, seqid);
    } else if (msg->host_info_response) {
        ret = lc_vcd_hosts_info_reply(msg->host_info_response);
    } else if (msg->vm_disk_response) {
        ret = lc_vcd_vm_set_disk_reply(msg->vm_disk_response, seqid);
    }

    cloudmessage__message__free_unpacked(msg, NULL);

    return ret;
}

static int lc_vcd_vm_add_reply(Cloudmessage__VMAddResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    lc_mbuf_t m;
    host_t  phost;
    rpool_t pool;
    struct lc_msg_head *m_send = NULL;
    int r;
    int i = 0, type = LC_MSG_NOTHING;

    memset(&vm, 0, sizeof(vm));
    memset(&m, 0, sizeof(m));
    memset(&phost, 0, sizeof(phost));
    memset(&pool, 0, sizeof(pool));

    m.hdr.seq = seqid;
    r = lc_vcd_vm_add_reply_msg(resp, &vm);
    if (lc_vmdb_vm_load_by_namelable(
                &vm, vm.vm_label) == LC_DB_VM_FOUND) {
        vm.vm_type = LC_VM_TYPE_VM;

        m.hdr.type = LC_MSG_VM_ADD;
        m.hdr.magic = MSG_MG_UI2DRV;
        m.hdr.length = sizeof(struct msg_vm_opt);
        lc_vmdb_vedge_passwd_load(vm.vm_passwd, vm.vnet_id);
    } else {
        VMDRIVER_LOG(LOG_ERR, "The vm %s not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "replay vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;

    }

    if (vm.vm_type == LC_VM_TYPE_VM) {
        if (vm.vm_flags & LC_VM_REPLACE_FLAG) {
            type = LC_MSG_VM_REPLACE;
        }
    }

    if (r) {
        VMDRIVER_LOG(LOG_ERR, "%s %s create error (%d)\n",
                vm.vm_type == LC_VM_TYPE_VM ? "vm" : "vdc",
                vm.vm_label, r);
        goto error;
    }

    lc_vmdb_vif_update_mac(vm.pub_vif[0].if_mac, vm.pub_vif[0].if_id);
    if (lc_vmdb_vif_load(
                &vm.pub_vif[0],
                vm.pub_vif[0].if_id) != LC_DB_VIF_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The vinterface %d not found\n",
                     vm.pub_vif[0].if_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "vif not found",
                              LC_SYSLOG_LEVEL_ERR);
    }

    lc_vmdb_vif_update_state(LC_VIF_STATE_ATTACHED,
            vm.pub_vif[0].if_id);

    if (vm.vm_type == LC_VM_TYPE_GW) {
        for (i = 0; i < LC_L2_MAX; ++i) {
            lc_vmdb_vif_update_mac(vm.pvt_vif[i].if_mac, vm.pvt_vif[i].if_id);
            if (lc_vmdb_vif_load(
                        &vm.pvt_vif[i],
                        vm.pvt_vif[i].if_id) != LC_DB_VIF_FOUND) {
                VMDRIVER_LOG(LOG_ERR, "The vinterface %d not found\n",
                             vm.pvt_vif[i].if_id);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      vm.vm_id, vm.vm_label, "vif not found",
                                      LC_SYSLOG_LEVEL_ERR);
            }

            if (vm.pvt_vif[i].if_state == LC_VIF_STATE_ATTACHING) {
                lc_vmdb_vif_update_state(LC_VIF_STATE_ATTACHED,
                        vm.pvt_vif[i].if_id);
            }
        }
    }

    if (lc_vmdb_pool_load(&pool, vm.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",
                     vm.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (pool.stype == LC_POOL_TYPE_LOCAL) {
         strcpy(phost.host_address, vm.host_address);
    } else {
         strcpy(phost.host_address, pool.master_address);
    }
    vm.hostp = &phost;
    vm.hostp->host_htype = LC_HOST_HTYPE_VMWARE;

    lc_vm_create_configure_iso(&vm);

    if (vm.vm_type == LC_VM_TYPE_VM) {
        m_send = lc_get_vm_forward_create_msg(&(m.hdr), &vm);
        if (m_send) {
            lc_msg_forward((lc_mbuf_t *)m_send);
            free(m_send);
        }
        vm.vm_flags &= (~LC_VM_REPLACE_FLAG);
        lc_vmdb_vm_update_flag(&vm, vm.vm_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm.vm_id);
    }
    if (type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__SUCCESS, seqid,
                   TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
    } else {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__SUCCESS, seqid,
                 TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }

    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_TEMPLATE_NOT_FOUND:
        vm.vm_error |= LC_VM_ERRNO_NO_TEMPLATE;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: no template\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "no template",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_DISK_SPACE_INSUFFICIENT:
        vm.vm_error |= LC_VM_ERRNO_NO_DISK;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: disk space insufficient\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "disk space insufficient",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_CPU_NOT_ENOUGH:
        vm.vm_error |= LC_VM_ERRNO_NO_CPU;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: cpu not enough\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "cpu not enough",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_INTERFACE_CREATE_ERROR:
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_INTERFACE_BIND_ERROR:
        vm.vm_error |= LC_VM_ERRNO_NETWORK;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: interface error\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "create/bind vif error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_EXISTS_ERROR:
        vm.vm_error |= LC_VM_ERRNO_VM_EXISTS;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: vm exists\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "vm exists",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_CREATE_ERROR:
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_CREATE_MESSAGE_ERROR:
        vm.vm_error |= LC_VM_ERRNO_INSTALL_OP_FAILED;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: create error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "create command error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_SYS_DISK_ERROR:
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_USER_DISK_ERROR:
        vm.vm_error |= LC_VM_ERRNO_NO_DISK;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: disk error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "no disk",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_MEMORY_NOT_ENOUGH:
        vm.vm_error |= LC_VM_ERRNO_NO_MEMORY;
        VMDRIVER_LOG(LOG_ERR, "VM %s create error: memory enough\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "no memory",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        vm.vm_error |= LC_VM_ERRNO_CREATE_ERROR;
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "connect to vcenter error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        vm.vm_error |= LC_VM_ERRNO_CREATE_ERROR;
        VMDRIVER_LOG(LOG_ERR, "VM %s create unknown error\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }

    if (vm.vm_type == LC_VM_TYPE_VM) {
        lc_vmdb_vm_update_errno(vm.vm_error, vm.vm_id);
/*        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm.vm_id);*/
    }
    if (type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__FAIL, seqid,
                TALKER__VM_OPS__VM_CREATE, LC_BUS_QUEUE_LCRMD);
    } else {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__FAIL, seqid,
              TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }

    return r;
}

static int lc_vcd_vm_update_reply(Cloudmessage__VMUpdateResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    vm_info_t vm_info;
    disk_t disks[MAX_VM_HA_DISK];
    int i, disk_num, r;

    memset(&vm, 0, sizeof(vm));
    memset(disks, 0, sizeof(disks));
    r = lc_vcd_vm_update_reply_msg(resp, &vm, &disk_num, disks);
    VMDRIVER_LOG(LOG_ERR, "The vm %s update %s\n",vm.vm_label,
            r ? "error" : "successful");

    memset(&vm_info, 0, sizeof(vm_info));
    if (lc_vmdb_vm_load_by_namelable(
                &vm_info, vm.vm_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;
    } else {
        VMDRIVER_LOG(LOG_ERR, "The vm %s not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    if (r) {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_MODIFY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "modify command error",
                              LC_SYSLOG_LEVEL_ERR);
        if (vm.action & VM_CHANGE_MASK_HA_DISK) {
            for (i=0; i<disk_num; i++) {
                if (!disks[i].vdi_uuid[0]) {
                    lc_vmdb_disk_delete_by_userdevice(vm_info.vm_id, disks[i].userdevice);
                }
            }
        }
        vm_info.vm_flags &= (~LC_VM_CHG_FLAG);
        lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
/*        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, seqid,
                           TALKER__VM_OPS__VM_MODIFY, LC_BUS_QUEUE_LCRMD);

        return 0;
    }

    if (vm.action & VM_CHANGE_MASK_DISK_SYS_RESIZE) {
        lc_vmdb_vm_update_vdi_size(vm.vdi_sys_size, "vdi_sys_size",
                vm_info.vm_id);
    }
    if (vm.action & VM_CHANGE_MASK_DISK_USER_RESIZE) {
        lc_vmdb_vm_update_vdi_size(vm.vdi_user_size, "vdi_user_size",
                vm_info.vm_id);
    }
    if (vm.action & VM_CHANGE_MASK_CPU_RESIZE) {
        lc_vmdb_vm_update_cpu(vm.vcpu_num, vm_info.vm_id);
    }
    if (vm.action & VM_CHANGE_MASK_MEM_RESIZE) {
        lc_vmdb_vm_update_memory(vm.mem_size, vm_info.vm_id);
    }
    if (vm.action & VM_CHANGE_MASK_HA_DISK) {
        for (i=0; i<disk_num; i++) {
            lc_vmdb_disk_update_uuid_by_userdevice(disks[i].vdi_uuid, vm_info.vm_id, disks[i].userdevice);
            lc_vmdb_disk_update_size_by_userdevice(disks[i].size, vm_info.vm_id, disks[i].userdevice);
            lc_vmdb_disk_update_name_by_userdevice(disks[i].vdi_name, vm_info.vm_id, disks[i].userdevice);
        }
        vm_info.vm_flags |= LC_VM_HA_DISK_FLAG;
    }

    vm_info.vm_flags &= (~LC_VM_CHG_FLAG);
    lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, seqid,
                     TALKER__VM_OPS__VM_MODIFY, LC_BUS_QUEUE_LCRMD);

    return r;
}

static int lc_vcd_vm_ops_reply(Cloudmessage__VMOpsResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    int r;
    int state = 0, ops = 0;

    memset(&vm, 0, sizeof(vm));
    r = lc_vcd_vm_ops_reply_msg(resp, &vm);
    if (resp->ops == CLOUDMESSAGE__VMOPS__LC_VM_START) {
         ops = TALKER__VM_OPS__VM_START;
    } else if (resp->ops == CLOUDMESSAGE__VMOPS__LC_VM_SHUTDOWN) {
         ops = TALKER__VM_OPS__VM_STOP;
    }
    state = vm.vm_state;
    if (lc_vmdb_vm_load_by_namelable(
                &vm, vm.vm_label) == LC_DB_VM_FOUND) {
        vm.vm_type = LC_VM_TYPE_VM;
    } else {
        VMDRIVER_LOG(LOG_ERR, "The vm %s not found\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_OP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    if (r) {
        VMDRIVER_LOG(LOG_ERR, "%s %s ops error (%d)\n",
                vm.vm_type == LC_VM_TYPE_VM ? "vm" : "vdc",
                vm.vm_label, r);
        goto error;
    }

    if (vm.vm_type == LC_VM_TYPE_VM) {
        VMDRIVER_LOG(LOG_DEBUG, "The vm %d operation finished\n",vm.vm_id);
        lc_vmdb_vm_update_state(state, vm.vm_id);
    }
    lc_msg_rollback(&vm, NULL, TALKER__RESULT__SUCCESS, seqid,
                                      ops, LC_BUS_QUEUE_TALKER);

    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_NOT_FOUND:
        VMDRIVER_LOG(LOG_ERR, "VM %s ops error: vm not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_OP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_OP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "connect to vcenter erro",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "VM %s ops unknown error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_OP_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }

    lc_msg_rollback(&vm, NULL, TALKER__RESULT__FAIL, seqid,
                                   ops, LC_BUS_QUEUE_TALKER);

    return r;
}

static int lc_vcd_vm_del_reply(Cloudmessage__VMDeleteResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    struct lc_msg_head *m_head = NULL;
    struct msg_vm_opt *m_opt = NULL;
    int r = 0, type = LC_MSG_NOTHING;

    memset(&vm, 0, sizeof(vm));
    r = lc_vcd_vm_del_reply_msg(resp, &vm);
    if (lc_vmdb_vm_load_by_namelable(
                &vm, vm.vm_label) == LC_DB_VM_FOUND) {
        vm.vm_type = LC_VM_TYPE_VM;
    } else {
        VMDRIVER_LOG(LOG_ERR, "VM %s not found\n", vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }
    if (vm.vm_type == LC_VM_TYPE_VM) {
        if (vm.vm_flags & LC_VM_REPLACE_FLAG) {
            type = LC_MSG_VM_REPLACE;
        }
    }

    if (r) {
        VMDRIVER_LOG(LOG_ERR, "%s %s delete error (%d)\n",
                vm.vm_type == LC_VM_TYPE_VM ? "vm" : "vdc",
                vm.vm_label, r);
        goto error;
    }

    lc_vm_remove_configure_iso(&vm);

    /*Only support replace VM*/
    if (vm.vm_type == LC_VM_TYPE_VM) {
        if (vm.vm_flags & LC_VM_HA_DISK_FLAG) {
            lc_vmdb_disk_delete_by_vm(vm.vm_id);
        }
        lc_vmdb_vm_update_state(LC_VM_STATE_DESTROYED, vm.vm_id);
        if (vm.vm_flags & LC_VM_REPLACE_FLAG) {
            m_head = (struct lc_msg_head *)malloc(sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt));
            if (!m_head) {
                goto error;
            }
            memset(m_head, 0, sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt));
            m_opt = (struct msg_vm_opt *)(m_head + 1);
            m_head->type = LC_MSG_VM_REPLACE;
            m_head->magic = MSG_MG_KER2DRV;
            m_head->seq = seqid;
            m_head->length = sizeof(struct msg_vm_opt);
            m_opt->vm_id = vm.vm_id;
            lc_vm_create(m_head);
            free(m_head);
        }
    }
    if (type != LC_MSG_VM_REPLACE) {
        VMDRIVER_LOG(LOG_DEBUG, "%s %d deleted", vm.vm_type == LC_VM_TYPE_VM ? "vm" : "vedge", vm.vm_id);
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__SUCCESS, seqid,
                  TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    }
    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_NOT_FOUND:
        VMDRIVER_LOG(LOG_ERR, "VM %s delete error: vm not found, set to destroyed\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        if (vm.vm_type == LC_VM_TYPE_VM) {
            lc_vmdb_vm_update_state(LC_VM_STATE_DESTROYED, vm.vm_id);
        }
        return r;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "connect to vcenter error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "VM %s delete unknown error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }
    if (type != LC_MSG_VM_REPLACE) {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__FAIL, seqid,
               TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    } else {
        lc_msg_rollback(&vm, NULL, TALKER__RESULT__FAIL, seqid,
               TALKER__VM_OPS__VM_REPLACE, LC_BUS_QUEUE_TALKER);
    }

    return r;
}

static int lc_vcd_vm_snapshot_add_reply(Cloudmessage__VMSnapshotAddResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    vm_snapshot_info_t ss;
    int r;

    memset(&vm, 0, sizeof(vm));
    memset(&ss, 0, sizeof(ss));
    r = lc_vcd_vm_snapshot_add_reply_msg(resp, &vm, &ss);

    if (lc_vmdb_vm_load_by_namelable(&vm, vm.vm_label) != LC_DB_VM_FOUND) {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    if (r) {
        VMDRIVER_LOG(LOG_DEBUG, "Create snapshot (%s) error\n",
               ss.snapshot_label);
        goto error;
    }

    /* update snapshot state=LC_VM_SNAPSHOT_STATE_COMPLETE */
    if (lc_vmdb_vm_snapshot_update_state_of_to_create_by_label(
                LC_VM_SNAPSHOT_STATE_COMPLETE,
                ss.snapshot_label)) {
        VMDRIVER_LOG(LOG_ERR, "VM snapshot db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "db update error",
                              LC_SYSLOG_LEVEL_ERR);
        ss.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "Create snapshot (%s) finish\n",
                 ss.snapshot_label);

    lc_msg_rollback(NULL, &ss, TALKER__RESULT__SUCCESS, seqid,
         TALKER__VM_OPS__SNAPSHOT_CREATE, LC_BUS_QUEUE_TALKER);
    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_NOT_FOUND:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot add error: vm not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "replay vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "connect to vcenter error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot add unknown error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }

    lc_vmdb_vm_snapshot_update_error_of_to_create_by_label(
            ss.error, ss.snapshot_label);
/*    lc_vmdb_vm_snapshot_update_state_of_to_create_by_label(
            LC_VM_SNAPSHOT_STATE_ERROR, ss.snapshot_label);*/
    lc_msg_rollback(NULL, &ss, TALKER__RESULT__FAIL, seqid,
      TALKER__VM_OPS__SNAPSHOT_CREATE, LC_BUS_QUEUE_TALKER);
    return r;
}

static int lc_vcd_vm_snapshot_del_reply(Cloudmessage__VMSnapshotDelResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    vm_snapshot_info_t ss;
    int r;

    memset(&vm, 0, sizeof(vm));
    memset(&ss, 0, sizeof(ss));
    r = lc_vcd_vm_snapshot_del_reply_msg(resp, &vm, &ss);

    if (lc_vmdb_vm_load_by_namelable(&vm, vm.vm_label) != LC_DB_VM_FOUND) {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    if (r) {
        VMDRIVER_LOG(LOG_DEBUG, "Delete snapshot (%s) error\n",
              ss.snapshot_label);
        goto error;
    }

    /* update snapshot state=LC_VM_SNAPSHOT_STATE_COMPLETE */
    if (lc_vmdb_vm_snapshot_update_state_by_label(
                LC_VM_SNAPSHOT_STATE_DELETED,
                ss.snapshot_label)) {
        VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "db update error",
                              LC_SYSLOG_LEVEL_ERR);
        ss.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "Delete snapshot (%s) finish\n",
          ss.snapshot_label);

    lc_msg_rollback(NULL, &ss, TALKER__RESULT__SUCCESS, seqid,
         TALKER__VM_OPS__SNAPSHOT_DELETE, LC_BUS_QUEUE_TALKER);
    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_NOT_FOUND:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot delete error: vm not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "connect to vcenter error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot delete unknown error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }
    if (ss.error) {
        lc_vmdb_vm_snapshot_update_error_by_label(
                     ss.error, ss.snapshot_label);
        lc_vmdb_vm_snapshot_update_state_by_label(
            LC_VM_SNAPSHOT_STATE_COMPLETE, ss.snapshot_label);
    }
    lc_msg_rollback(NULL, &ss, TALKER__RESULT__FAIL, seqid,
      TALKER__VM_OPS__SNAPSHOT_DELETE, LC_BUS_QUEUE_TALKER);
    return r;
}

static int lc_vcd_vm_snapshot_revert_reply(Cloudmessage__VMSnapshotRevertResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    vm_snapshot_info_t ss;
    int r;

    memset(&vm, 0, sizeof(vm));
    memset(&ss, 0, sizeof(ss));
    r = lc_vcd_vm_snapshot_revert_reply_msg(resp, &vm, &ss);

    if (lc_vmdb_vm_load_by_namelable(&vm, vm.vm_label) != LC_DB_VM_FOUND) {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, vm.vm_label, "reply vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    if (r) {
        VMDRIVER_LOG(LOG_DEBUG, "Revert snapshot (%s) error\n",
                     ss.snapshot_label);
        goto error;
    }

    /* update snapshot state=LC_VM_SNAPSHOT_STATE_COMPLETE */
    if (lc_vmdb_vm_snapshot_update_state_by_label(
                LC_VM_SNAPSHOT_STATE_COMPLETE,
                ss.snapshot_label)) {
        VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "update db error",
                              LC_SYSLOG_LEVEL_ERR);
        ss.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* update vm state=LC_VM_STATE_STOPPED */
    if (lc_vmdb_vm_update_state_by_label(
                LC_VM_STATE_STOPPED, vm.vm_label)) {
        VMDRIVER_LOG(LOG_ERR, "vm db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "update db error",
                              LC_SYSLOG_LEVEL_ERR);
        ss.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "revert snapshot (%s) finish\n",ss.snapshot_label);

    lc_msg_rollback(NULL, &ss, TALKER__RESULT__SUCCESS, seqid,
         TALKER__VM_OPS__SNAPSHOT_REVERT, LC_BUS_QUEUE_TALKER);
    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VM_NOT_FOUND:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot revert error: vm not found\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "connect to vcenter error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "VM %s snapshot revert unknown error\n",vm.vm_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm.vm_id, vm.vm_label, "unkown error",
                              LC_SYSLOG_LEVEL_ERR);
        break;
    }
    if (ss.error) {
        lc_vmdb_vm_snapshot_update_error_by_label(ss.error,
                                        ss.snapshot_label);
        lc_vmdb_vm_snapshot_update_state_by_label(
                    LC_VM_SNAPSHOT_STATE_COMPLETE, ss.snapshot_label);
        lc_vmdb_vm_update_state_by_label(LC_VM_STATE_STOPPED, vm.vm_label);
    }
    lc_msg_rollback(NULL, &ss, TALKER__RESULT__FAIL, seqid,
      TALKER__VM_OPS__SNAPSHOT_REVERT, LC_BUS_QUEUE_TALKER);
    return r;
}

static int lc_vcd_hosts_info_reply(Cloudmessage__HostsInfoResp *resp)
{
    host_t host, host2;
    int r;
    shared_storage_resource_t ssr[LC_VCD_SSR_MAX_NUM];
    int ssr_num = 0, i;
    char mode[LC_NAMESIZE];
    char domain[LC_NAMESIZE];
    char lcm_ip[LC_NAMESIZE];
    char cmd[MAX_CMD_BUF_SIZE];

    memset(&host, 0, sizeof(host));
    memset(&host2, 0, sizeof(host2));
    memset(ssr, 0, LC_VCD_SSR_MAX_NUM * sizeof(shared_storage_resource_t));
    r = lc_vcd_hosts_info_reply_msg(resp, &host);
    if (r) {
        VMDRIVER_LOG(LOG_ERR, "The host %s add error (%d)\n",
                host.host_address, r);
        goto error;
    }

    if (lc_vmdb_host_device_load(&host2, host.host_address, "*")) {
        VMDRIVER_LOG(LOG_ERR, "Cannot find %s in database\n",
                host.host_address);
        goto error;
    }
    memset(mode, 0, sizeof(mode));
    lc_sysdb_get_running_mode(mode, LC_NAMESIZE);
    memset(domain, 0, sizeof(domain));
    lc_sysdb_get_domain(domain, LC_NAMESIZE);
    memset(lcm_ip, 0, sizeof(lcm_ip));
    lc_sysdb_get_lcm_ip(lcm_ip, LC_NAMESIZE);
    lc_storagedb_host_device_del_storage(&host2);
    for (i = 0; i < host.n_storage; ++i) {
        lc_storagedb_host_device_add_storage(&host2, &host.storage[i]);
    }
    lc_storagedb_host_device_del_ha_storage(&host2);
    for (i = 0; i < host.n_hastorage; ++i) {
        lc_storagedb_host_device_add_ha_storage(&host2, &host.ha_storage[i]);
    }
    for (i = 0; i < ssr_num; ++i) {
        ssr[i].rack_id = host2.rack_id;
        ssr[i].storage_state = LC_SSR_STATE_NORMAL;
        ssr[i].storage_flag = LC_SSR_FLAG_INUSE;
        ssr[i].storage_htype = LC_SSR_HTYPE_VMWARE;
        if (!lc_storagedb_shared_storage_resource_check(ssr[i].sr_name, ssr[i].rack_id)) {
            VMDRIVER_LOG(LOG_DEBUG, "Shared storage %s already exists\n",
                         ssr[i].sr_name);
        } else {
            VMDRIVER_LOG(LOG_DEBUG, "Insert shared storage %s\n",
                   ssr[i].sr_name);
            if (lc_storagedb_shared_storage_resource_insert(&ssr[i])) {
                VMDRIVER_LOG(LOG_ERR, "The storage %s insert error\n",
                             ssr[i].sr_name);
            }
            if (strcmp(mode, "hierarchical") == 0) {
                VMDRIVER_LOG(LOG_DEBUG, "Updating shared storage %s to lcm on %s\n",
                             ssr[i].sr_name, lcm_ip);
                if (lc_storagedb_shared_storage_resource_load(&ssr[i], ssr[i].sr_name, ssr[i].rack_id)) {
                    VMDRIVER_LOG(LOG_ERR, "The storage %s load error\n",
                                 ssr[i].sr_name);
                }
                memset(cmd, 0, sizeof(cmd));
                snprintf(cmd, MAX_CMD_BUF_SIZE,
                        "curl -X POST " \
                        "-k https://%s/sharedstorageresource/add -d " \
                        "\"data={" \
                        "\\\"id\\\":%d, " \
                        "\\\"state\\\":%d, " \
                        "\\\"flag\\\":%d, " \
                        "\\\"htype\\\":%d, " \
                        "\\\"iqn\\\":\\\"%s\\\", " \
                        "\\\"lun\\\":%d, " \
                        "\\\"sr_name\\\":\\\"%s\\\", " \
                        "\\\"disk_total\\\":%d, " \
                        "\\\"disk_used\\\":%d, " \
                        "\\\"disk_allocated\\\":%d, " \
                        "\\\"domain\\\":\\\"%s\\\"" \
                        "\\\"rackid\\\":%d, " \
                        "}\" 2>/dev/null",
                        lcm_ip,
                        ssr[i].storage_id,
                        ssr[i].storage_state,
                        ssr[i].storage_flag,
                        ssr[i].storage_htype,
                        "",
                        0,
                        ssr[i].sr_name,
                        ssr[i].disk_total,
                        ssr[i].disk_used,
                        0,
                        domain,
                        ssr[i].rack_id
                );
                VMDRIVER_LOG(LOG_DEBUG, "[%s]",cmd);
                lc_call_system(cmd, "VMWare", "", "", lc_vmdriver_db_log);
            }
        }
    }

    strncpy(host.master_address, host.host_address, MAX_HOST_ADDRESS_LEN - 1);
    lc_vmdb_host_device_info_update(&host, host.host_address);
    lc_vmdb_host_device_state_update(LC_HOST_STATE_COMPLETE, host.host_address);

    VMDRIVER_LOG(LOG_DEBUG, "Add host (%s) finish\n",
                  host.host_address);

    return 0;

error:

    switch (r) {
    case CLOUDMESSAGE__VMRESULT_TYPE__VM_RESULT_VCENTER_CONNECTED_ERROR:
        VMDRIVER_LOG(LOG_ERR, "Lost connection to vcenter\n");
        break;
    default:
        VMDRIVER_LOG(LOG_ERR, "The host %s get unknown error\n", host.host_address);
        break;
    }
    lc_vmdb_host_device_state_update(LC_HOST_STATE_ERROR, host.host_address);

    return r;
}

static int lc_vcd_vm_set_disk_reply(Cloudmessage__VMSetDiskResp *resp, uint32_t seqid)
{
    vm_info_t vm;
    vm_info_t vm_info;
    int r;

    memset(&vm, 0, sizeof(vm));
    r = lc_vcd_vm_set_disk_reply_msg(resp, &vm);
    VMDRIVER_LOG(LOG_ERR, "vm %s set disk %s\n", vm.vm_label,
            r ? "error" : "successful");

    memset(&vm_info, 0, sizeof(vm_info));
    if (lc_vmdb_vm_load_by_namelable(
                &vm_info, vm.vm_label) != LC_DB_VM_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "vm %s not found\n",vm.vm_label);
        return -1;
    }
    if (!r) {
        if (vm.vm_flags & LC_VM_HA_DISK_FLAG) {
            vm_info.vm_flags |= LC_VM_HA_DISK_FLAG;
        } else {
            vm_info.vm_flags &= (~LC_VM_HA_DISK_FLAG);
        }
    }
    vm_info.vm_flags &= (~LC_VM_SET_HADISK_FLAG);
    lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    if (vm.vm_flags & LC_VM_HA_DISK_FLAG) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, seqid,
                  TALKER__VM_OPS__VM_HADISK_PLUG, LC_BUS_QUEUE_TALKER);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, seqid,
                TALKER__VM_OPS__VM_HADISK_UNPLUG, LC_BUS_QUEUE_TALKER);
    }

    return r;
}

static int lc_vm_snapshot_create(struct lc_msg_head *m_head)
{
    struct msg_vm_snapshot_opt *vm_snapshot = NULL;
    char vm_snapshot_name[MAX_VM_NAME_LEN];
    rpool_t   pool;
    host_t    phost;
    vm_info_t vm_info;
    vm_snapshot_info_t vm_snapshot_info;
    char cmd[MAX_CMD_BUF_SIZE];
    int mlen;
    char buf[MAX_MSG_BUFFER];

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vm_snapshot_info, 0, sizeof(vm_snapshot_info_t));
    memset(vm_snapshot_name, 0, MAX_VM_NAME_LEN);

    /* Get msg info*/
    vm_snapshot = (struct msg_vm_snapshot_opt *)(m_head + 1);
    if (NULL == vm_snapshot) {
        VMDRIVER_LOG(LOG_ERR, "vm_snapshot is NULL\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              0, "", "recieve error msg", LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    /* Load vm_info from db */
    if (LC_DB_VM_FOUND != lc_vmdb_vm_load(&vm_info, vm_snapshot->vm_id)) {
        VMDRIVER_LOG(LOG_ERR, "vm to be snapshot not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_snapshot->vm_id, "", "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load pool_info from db */
    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",
                     vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }
    strcpy(phost.host_address, vm_info.host_address);
    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",
                     phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }
    vm_info.hostp = &phost;

    /* Load snapshot_info from db */
    if (LC_DB_SNAPSHOT_FOUND != lc_vmdb_vm_snapshot_load(&vm_snapshot_info,
                                                          vm_snapshot->snapshot_id)
        || LC_VM_SNAPSHOT_STATE_TO_CREATE != vm_snapshot_info.state) {
        VMDRIVER_LOG(LOG_ERR, "snapshot to be created not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "snapshot not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Create and update snapshot label */
    sprintf(vm_snapshot_info.snapshot_label, "%s-%d", vm_info.vm_label,
                                             vm_snapshot->snapshot_id);
    if (lc_vmdb_vm_snapshot_update_label(vm_snapshot_info.snapshot_label,
                                         vm_snapshot->snapshot_id)) {
        VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "update snapshot name erorr",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    if (pool.ctype == LC_POOL_TYPE_XEN) {
        /* Create vm snapshot */
        if (lc_agexec_vm_snapshot_ops(&vm_info, &vm_snapshot_info, XEN_VM_SNAPSHOT_CREATE)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot take is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "snapshot command erorr",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_CREATE_ERROR;
            goto error;
        }

        /* update snapshot uuid */
        if (lc_vmdb_vm_snapshot_update_uuid(
                    vm_snapshot_info.snapshot_uuid,
                    vm_snapshot->snapshot_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update snapshot uuid erorr",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        /*export vm metadata*/
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_EXPORT_STR,
                vm_info.vm_label);
        lc_call_system(cmd, "VM create snapshot", "vm",
                vm_info.vm_label, lc_vmdriver_db_log);

        /* update snapshot state=LC_VM_SNAPSHOT_STATE_COMPLETE */
        if (lc_vmdb_vm_snapshot_update_state(
                    LC_VM_SNAPSHOT_STATE_COMPLETE,
                    vm_snapshot->snapshot_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update snapshot db erorr",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        VMDRIVER_LOG(LOG_DEBUG, "Create snapshot (%s) finish\n",
                     vm_snapshot_info.snapshot_label);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        memset(buf, 0, MAX_MSG_BUFFER);
        if ((mlen = lc_vcd_vm_snapshot_add_msg(
                        &vm_info, &vm_snapshot_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Create vm snapshot error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_SNAPSHOT_ERRNO_CREATE_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }
        return 0;
    } else {
        VMDRIVER_LOG(LOG_ERR, "The pool ctype error %d\n",pool.ctype);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CREATE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool type error",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__SUCCESS, m_head->seq,
                    TALKER__VM_OPS__SNAPSHOT_CREATE, LC_BUS_QUEUE_LCRMD);
    return 0;

error:

    lc_vmdb_vm_snapshot_update_error(vm_snapshot_info.error,
                                     vm_snapshot_info.snapshot_id);
/*    lc_vmdb_vm_snapshot_update_state(LC_VM_SNAPSHOT_STATE_ERROR,
                                     vm_snapshot_info.snapshot_id);*/
    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__FAIL, m_head->seq,
                    TALKER__VM_OPS__SNAPSHOT_CREATE, LC_BUS_QUEUE_LCRMD);

    return -1;
}

static int lc_vm_snapshot_delete(struct lc_msg_head *m_head)
{
    struct msg_vm_snapshot_opt *vm_snapshot = NULL;
    rpool_t   pool;
    host_t    phost;
    vm_info_t vm_info;
    vm_snapshot_info_t vm_snapshot_info;
    char cmd[MAX_CMD_BUF_SIZE];
    int mlen;
    char buf[MAX_MSG_BUFFER];

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vm_snapshot_info, 0, sizeof(vm_snapshot_info_t));

    /* Get msg info*/
    vm_snapshot = (struct msg_vm_snapshot_opt *)(m_head + 1);
    if (NULL == vm_snapshot) {
        VMDRIVER_LOG(LOG_ERR, "vm_snapshot is NULL\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              0, "", "recieve error msg", LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    /* Load vm_info from db */
    if (LC_DB_VM_FOUND != lc_vmdb_vm_load(&vm_info, vm_snapshot->vm_id)) {
        VMDRIVER_LOG(LOG_ERR, "vm to delete snapshot not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_snapshot->vm_id, "", "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load snapshot_info from db */
    if (LC_DB_SNAPSHOT_FOUND != lc_vmdb_vm_snapshot_load(&vm_snapshot_info,
                                                          vm_snapshot->snapshot_id)
        || LC_VM_SNAPSHOT_STATE_TO_DELETE != vm_snapshot_info.state) {
        VMDRIVER_LOG(LOG_ERR, "snapshot to be deleted not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "snapshot not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load pool_info from db */
    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, vm_info.host_address);
    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n", phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }
    vm_info.hostp = &phost;

    if (pool.ctype == LC_POOL_TYPE_XEN) {
        /* uninstall vm snapshot*/
        if (lc_agexec_vm_snapshot_ops(&vm_info, &vm_snapshot_info, XEN_VM_SNAPSHOT_DESTROY)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot uninstall is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "delete command error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DELETE_ERROR;
            goto error;
        }

        /*export vm metadata*/
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_BUF_SIZE,
                LC_VM_META_EXPORT_STR, vm_info.vm_label);
        lc_call_system(cmd, "VM delete backup", "vm",
                vm_info.vm_label, lc_vmdriver_db_log);

        /* update snapshot state=LC_VM_SNAPSHOT_STATE_DELETED */
        if (lc_vmdb_vm_snapshot_update_state(LC_VM_SNAPSHOT_STATE_DELETED,
                                             vm_snapshot->snapshot_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update db error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        VMDRIVER_LOG(LOG_DEBUG, "Delete backup (%s) finish\n",
               vm_snapshot_info.snapshot_label);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        memset(buf, 0, MAX_MSG_BUFFER);
        if ((mlen = lc_vcd_vm_snapshot_del_msg(
                        &vm_info, &vm_snapshot_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "Delete vm snapshot error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_SNAPSHOT_ERRNO_DELETE_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }

        return 0;
    } else {
        VMDRIVER_LOG(LOG_ERR, "The pool ctype error %d\n", pool.ctype);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM_SS, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool ctype error",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__SUCCESS, m_head->seq,
                             TALKER__VM_OPS__SNAPSHOT_DELETE, LC_BUS_QUEUE_TALKER);
    return 0;

error:
    if (vm_snapshot_info.snapshot_id) {
        lc_vmdb_vm_snapshot_update_error(vm_snapshot_info.error,
                                     vm_snapshot_info.snapshot_id);
        lc_vmdb_vm_snapshot_update_state(LC_VM_SNAPSHOT_STATE_COMPLETE,
                                     vm_snapshot_info.snapshot_id);
    }
    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__FAIL, m_head->seq,
                        TALKER__VM_OPS__SNAPSHOT_DELETE, LC_BUS_QUEUE_TALKER);

    return -1;
}

static int lc_vm_snapshot_recovery(struct lc_msg_head *m_head)
{
    struct msg_vm_recovery_opt *vm_snapshot = NULL;
    rpool_t   pool;
    host_t    phost;
    vm_info_t vm_info;
    vm_snapshot_info_t vm_snapshot_info;
    char cmd[MAX_CMD_BUF_SIZE];
    int mlen;
    int ret = 0;
    char buf[MAX_MSG_BUFFER];

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vm_snapshot_info, 0, sizeof(vm_snapshot_info_t));

    /* Get msg info*/
    vm_snapshot = (struct msg_vm_recovery_opt *)(m_head + 1);
    if (NULL == vm_snapshot) {
        VMDRIVER_LOG(LOG_ERR, "vm_snapshot is NULL\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              0, "", "recieve error msg", LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    /* Load snapshot_info from db */
    if (LC_DB_SNAPSHOT_FOUND != lc_vmdb_vm_snapshot_load(&vm_snapshot_info,
                                                          vm_snapshot->snapshot_id)
        || LC_VM_SNAPSHOT_STATE_TO_RECOVERY != vm_snapshot_info.state) {
        VMDRIVER_LOG(LOG_ERR, "snapshot to be recovery not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_snapshot_info.vm_id, "", "snapshot not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load vm_info from db */
    if (LC_DB_VM_FOUND != lc_vmdb_vm_load(&vm_info, vm_snapshot_info.vm_id)) {
        VMDRIVER_LOG(LOG_ERR, "VM to be snapshot not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_snapshot_info.vm_id, "", "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load pool_info from db */
    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, vm_info.host_address);
    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "The host %s not found\n",
                     phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
        goto error;
    }
    vm_info.hostp = &phost;

    if (pool.ctype == LC_POOL_TYPE_XEN) {
        /* Recovery vm snapshot */
        ret = lc_agexec_vm_snapshot_ops(&vm_info, &vm_snapshot_info,
                                        XEN_VM_SNAPSHOT_REVERT);
        if (ret &&
            0 == (ret & (AGEXEC_CPU_GET_ERROR | AGEXEC_MEM_GET_ERROR |
                         AGEXEC_SYS_DISK_GET_ERROR | AGEXEC_USER_DISK_GET_ERROR))) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm snapshot revert is failed", __FUNCTION__);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "revert command error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_RECOVERY_ERROR;
            goto error;
        }
        if (ret & AGEXEC_CPU_GET_ERROR) {
            VMDRIVER_LOG(LOG_ERR, "get vm revert cpu failed");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "get cpu failed",
                                  LC_SYSLOG_LEVEL_WARNING);
        }
        if (ret & AGEXEC_MEM_GET_ERROR) {
            VMDRIVER_LOG(LOG_ERR, "get vm revert memory failed");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "get memory failed",
                                  LC_SYSLOG_LEVEL_WARNING);
        }
        if (ret & AGEXEC_SYS_DISK_GET_ERROR) {
            VMDRIVER_LOG(LOG_ERR, "get vm revert sys_disk failed");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "get sys_disk failed",
                                  LC_SYSLOG_LEVEL_WARNING);
        }
        if (ret & AGEXEC_USER_DISK_GET_ERROR) {
            VMDRIVER_LOG(LOG_ERR, "get vm revert user_disk failed");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "get user_disk failed",
                                  LC_SYSLOG_LEVEL_WARNING);
        }

        /* Update vdi_uuid */
        if (lc_vmdb_vm_update_vdi_info(vm_info.vdi_sys_uuid,
                             "vdi_sys_uuid", vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "update vdi_sys_uuid is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update uuid error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_vm_update_vdi_info(vm_info.vdi_user_uuid,
                             "vdi_user_uuid", vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "update vdi_user_uuid is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update uuid error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        /* Update vcpu_num */
        if (lc_vmdb_vm_update_cpu(vm_info.vcpu_num, vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "update vcpu_num is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update db error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }

        /* Update memory */
        if (lc_vmdb_vm_update_memory(vm_info.mem_size, vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "update mem_size is failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update db error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }

        /* Get and Update vdi_user_size */
        if (0 != strlen(vm_info.vdi_user_uuid)) {
            if (lc_vmdb_vm_update_vdi_size(vm_info.vdi_user_size,
                                 "vdi_user_size", vm_info.vm_id)) {
                VMDRIVER_LOG(LOG_ERR, "update vdi_user_size failed\n");
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                      vm_info.vm_id, vm_info.vm_label, "update db error",
                                      LC_SYSLOG_LEVEL_ERR);
                goto error;
            }
        }

        /*export vm metadata*/
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_EXPORT_STR,
                vm_info.vm_label);
        lc_call_system(cmd, "VM recovery backup", "vm",
                vm_info.vm_label, lc_vmdriver_db_log);

        /* update snapshot state=LC_VM_SNAPSHOT_STATE_COMPLETE */
        if (lc_vmdb_vm_snapshot_update_state(
                    LC_VM_SNAPSHOT_STATE_COMPLETE,
                    vm_snapshot->snapshot_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm snapshot db update failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update db error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        /* update vm state=LC_VM_STATE_STOPPED */
        if (lc_vmdb_vm_update_state(
                    LC_VM_STATE_STOPPED, vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm db update failed\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "update db error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_snapshot_info.error |= LC_SNAPSHOT_ERRNO_DB_ERROR;
            goto error;
        }

        VMDRIVER_LOG(LOG_DEBUG, "recovery (%s) finish\n", vm_snapshot_info.snapshot_label);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        memset(buf, 0, MAX_MSG_BUFFER);
        if ((mlen = lc_vcd_vm_snapshot_revert_msg(
                        &vm_info, &vm_snapshot_info, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "revert vm snapshot error\n");
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "construct vcd msg error",
                                  LC_SYSLOG_LEVEL_ERR);
            vm_info.vm_error |= LC_SNAPSHOT_ERRNO_RECOVERY_ERROR;
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label, "send to vcd error",
                                  LC_SYSLOG_LEVEL_ERR);
            goto error;
        }

        return 0;
    } else {
        VMDRIVER_LOG(LOG_ERR, "pool ctype error %d\n",pool.ctype);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_RECOVERY_VM, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool ctype error",
                              LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__SUCCESS, m_head->seq,
                             TALKER__VM_OPS__SNAPSHOT_REVERT, LC_BUS_QUEUE_TALKER);
    return 0;

error:

    if (vm_snapshot_info.snapshot_id) {
        lc_vmdb_vm_snapshot_update_error(vm_snapshot_info.error,
                                     vm_snapshot_info.snapshot_id);
        lc_vmdb_vm_snapshot_update_state(LC_VM_SNAPSHOT_STATE_COMPLETE,
                                     vm_snapshot_info.snapshot_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm_info.vm_id);
    }
    lc_msg_rollback(NULL, &vm_snapshot_info, TALKER__RESULT__FAIL, m_head->seq,
                       TALKER__VM_OPS__SNAPSHOT_REVERT, LC_BUS_QUEUE_TALKER);
    return -1;
}
static int lc_vm_backup_create(struct lc_msg_head *m_head)
{
    return 0;
}
static int lc_vm_backup_delete(struct lc_msg_head *m_head)
{
    return 0;
}
static int lc_vm_backup_recovery(struct lc_msg_head *m_head)
{
    return 0;
}

static int lc_has_snapshot(vm_info_t *vm_info, vm_snapshot_info_t *vm_backup_info)
{
    if (lc_vmdb_vm_snapshot_load_by_vmid(vm_backup_info, vm_info->vm_id)
        == LC_DB_BACKUP_FOUND) {
        return 1;
    }
    return 0;
}

/*Only Maintainance Tool use it!*/
static int lc_vm_migrate(struct lc_msg_head *m_head)
{
    struct msg_vm_migrate_opt *vm_migrate = NULL;
    int                ret;
    rpool_t            pool;
    host_t             phost;
    host_t             local_phost;
    storage_resource_t sr;
    vm_info_t          vm_info;
    vm_snapshot_info_t   vm_backup_info;
    char               tmp[LC_NAMESIZE];
    int                local_snapshot = 0;

    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&local_phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&sr, 0, sizeof(storage_resource_t));
    memset(&vm_backup_info, 0, sizeof(vm_snapshot_info_t));

    /* Get msg info*/
    vm_migrate = (struct msg_vm_migrate_opt *)(m_head + 1);
    if (NULL == vm_migrate) {
        VMDRIVER_LOG(LOG_ERR, "vm_migrate is NULL\n");
        goto error;
    }

    /* Load vm_info from db */
    if (vm_migrate->vm_type != LC_VM_TYPE_GW) {
        if (LC_DB_BACKUP_FOUND != lc_vmdb_vm_load(&vm_info, vm_migrate->vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm to be migrate not found\n");
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        if (LC_VM_HA_DISK_FLAG & vm_info.vm_flags) {
            VMDRIVER_LOG(LOG_ERR, "vm to be migrate has ha_disk\n");
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
    }

    strcpy(phost.host_address, vm_migrate->des_server);

    /*Load host_device table to get host htype*/
    if (lc_vmdb_host_device_load(&phost, phost.host_address, "htype")
        != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "host device %s not found", phost.host_address);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (vm_info.vm_type == LC_VM_TYPE_VM &&
        phost.host_htype == LC_HOST_HTYPE_KVM) {

        VMDRIVER_LOG(LOG_DEBUG, "vm migrate (%s) finish\n",vm_info.vm_label);
        /* Forward migrate message to lcpd
         * 1. vm should in running state
         * 2. lcpd will update launch_server in database
         */
        m_head->type = LC_MSG_VM_MIGRATE;
        lc_msg_forward((lc_mbuf_t *)m_head);
        return 0;
    }

    /* Load compute_resource from db and update vm_info*/
    if (LC_DB_HOST_FOUND != lc_vmdb_compute_resource_load(&phost, phost.host_address)) {
        VMDRIVER_LOG(LOG_ERR, "host %s not found\n", phost.host_address);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    phost.master = &phost;

    /* Load storage_resource from db and update vm_info*/
    if (LC_DB_STORAGE_FOUND != lc_storagedb_host_device_get_storage_uuid(
                               phost.sr_uuid, MAX_UUID_LEN,
                               phost.host_address, vm_migrate->des_sr_name)) {
        VMDRIVER_LOG(LOG_ERR, "The storage %s not found\n", vm_migrate->des_sr_name);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    /* Load local compute_resource from db and update vm_info*/
    strcpy(local_phost.host_address, vm_info.host_address);
    if (LC_DB_HOST_FOUND !=
        lc_vmdb_compute_resource_load(&local_phost, local_phost.host_address)) {
        VMDRIVER_LOG(LOG_ERR, "host %s not found\n",local_phost.host_address);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    local_phost.master = &local_phost;

    /* Load local storage_resource from db and update vm_info*/
    if (LC_DB_STORAGE_FOUND != lc_storagedb_host_device_get_storage_uuid(
                               local_phost.sr_uuid, MAX_UUID_LEN,
                               local_phost.host_address, vm_info.vdi_sys_sr_name)) {
        VMDRIVER_LOG(LOG_ERR, "The storage %s not found\n",vm_info.vdi_sys_sr_name);
//        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
//        goto error;
    }

    local_snapshot = lc_has_snapshot(&vm_info, &vm_backup_info);
    /* VM migrate */
     if ((LC_VM_STATE_RUNNING == vm_info.vm_state && vm_info.vm_type == LC_VM_TYPE_VM)) {

        /* check vm-migrate process */
        if (vm_info.vm_flags & LC_VM_RECOVERY || vm_info.vm_flags & LC_VM_RETRY) {
            memset(tmp, 0, sizeof(tmp));
            get_param(tmp, LC_NAMESIZE, "ps aux | grep \"vm-migrate.*vm=%s\" | grep -v grep", vm_info.vm_label);
            if (strlen(tmp) > 0) {
                vm_info.vm_error |= LC_VM_ERRNO_MIGRATE_FAILED;
                VMDRIVER_LOG(LOG_ERR, "vm %s migrating\n",vm_info.vm_label);
                goto error;
            }
        }

        /* VM eject_configure_iso */
        vm_info.hostp = &local_phost;
        lc_vm_eject_configure_iso(&vm_info);

        /* VM migrate by xe migrate cli */
        if (local_snapshot) {
            ret = lc_xen_migrate_vm_with_snapshot(vm_info.vm_uuid, &local_phost, &phost);
        } else {
            ret = lc_xen_migrate_vm(vm_info.vm_label, &local_phost, &phost);
        }
        if (ret) {
            if (lc_xen_check_vm_migrate(vm_info.vm_label, &local_phost, &phost)) {
                VMDRIVER_LOG(LOG_ERR, "vm migrate is failed(%d)",ret);
                vm_info.vm_error |= LC_VM_ERRNO_MIGRATE_FAILED;
                goto error;
            }
        }

        /* Get and Update vdi_uuid */
        vm_info.hostp = &phost;
        memset(vm_info.vdi_sys_uuid, 0, MAX_VM_UUID_LEN);
        memset(vm_info.vdi_user_uuid, 0, MAX_VM_UUID_LEN);
        if (lc_xen_get_vdi_uuid(&vm_info)) {
            VMDRIVER_LOG(LOG_ERR, "get vdi-uuid is failed\n");
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        if (strlen(vm_info.vdi_sys_uuid)) {
            if (vm_info.vm_type == LC_VM_TYPE_VM) {
                if (lc_vmdb_vm_update_vdi_info(vm_info.vdi_sys_uuid, "vdi_sys_uuid",
                                            vm_info.vm_id)) {
                    VMDRIVER_LOG(LOG_ERR, "update vdi_sys_uuid is failed\n");
                    vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                    goto error;
                }
            }
        } else {
            VMDRIVER_LOG(LOG_ERR, "vdi_sys_uuid is NULL\n");
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
        if (strlen(vm_info.vdi_user_uuid)) {
            if (lc_vmdb_vm_update_vdi_info(vm_info.vdi_user_uuid, "vdi_user_uuid",
                                           vm_info.vm_id)) {
                VMDRIVER_LOG(LOG_ERR, "update vdi_user_uuid is failed\n");
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
        } else {
            if (vm_info.vm_type == LC_VM_TYPE_VM && vm_info.vdi_user_size) {
                VMDRIVER_LOG(LOG_ERR, "update vdi_user_uuid is NULL\n");
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
        }

        /* Restart vnc */
        vm_info.hostp = &phost;
        lc_start_vnctunnel(&vm_info);

    } else if (LC_VM_STATE_STOPPED == vm_info.vm_state) {

    } else {
        VMDRIVER_LOG(LOG_ERR, "vm state is wrong\n");
        vm_info.vm_error |= LC_VM_ERRNO_MIGRATE_FAILED;
        goto error;
    }

#if 0
    /* Delete origin backup and update vm_backup_v1 local_info */
    memset(&vm_backup_info, 0, sizeof(vm_backup_info_t));
    if (LC_DB_BACKUP_FOUND ==
        lc_vmdb_vm_backup_load_by_vmid(&vm_backup_info, vm_info.vm_id)) {

        /* Delete vm backup */
        if (lc_vm_backup_delete_script(&vm_backup_info)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup delete is failed\n", __FUNCTION__);
            vm_backup_info.error |= LC_BACKUP_ERRNO_DELETE_ERROR;
            goto error;
        }

        /* update backup state=LC_VM_BACKUP_STATE_DELETED */
        if (lc_vmdb_vm_backup_update_state(LC_VM_BACKUP_STATE_DELETED,
                                           vm_backup_info.dbr_id)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup db update failed\n", __FUNCTION__);
            vm_backup_info.error |= LC_BACKUP_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_vm_backup_update_local_info(phost.host_address,
            "resident_server", vm_backup_info.dbr_id)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup db update failed\n", __FUNCTION__);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_vm_backup_update_local_info(phost.username,
            "resident_username", vm_backup_info.dbr_id)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup db update failed\n", __FUNCTION__);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_vm_backup_update_local_info(phost.password,
            "resident_passwd", vm_backup_info.dbr_id)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup db update failed\n", __FUNCTION__);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        if (lc_vmdb_vm_backup_update_local_info(phost.sr_uuid,
            "local_sr_uuid", vm_backup_info.dbr_id)) {
            VMDRIVER_LOG(LOG_ERR, "%s: vm backup db update failed\n", __FUNCTION__);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
    }
#endif

    /* update vm flag */
    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        vm_info.vm_flags &= (~LC_VM_FROZEN_FLAG);
        if (lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id)) {
            VMDRIVER_LOG(LOG_ERR, "vm db update failed\n");
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }
    }

    VMDRIVER_LOG(LOG_DEBUG, "vm migrate (%s) finish\n",vm_info.vm_label);

    /* Forward migrate message to lcpd
     * 1. vm should in running state
     * 2. lcpd will update (gw_)launch_server in database
     */
    if (vm_info.vm_type != LC_VM_TYPE_GW) {
        m_head->type = LC_MSG_VM_MIGRATE;
    }
    lc_msg_forward((lc_mbuf_t *)m_head);

    return 0;

error:

    if (vm_info.vm_type != LC_VM_TYPE_GW) {
        lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);
        vm_info.vm_flags &= (~LC_VM_FROZEN_FLAG);
        vm_info.vm_flags &= (~LC_VM_MIGRATE_FLAG);
        lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    }
    /*FIXME:TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);

    return -1;
}

static int lc_vm_vnc_connect(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    vm_info_t          vm_info;
    rpool_t            pool;
    host_t             master;
    host_t             phost;

    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&pool, 0, sizeof(rpool_t));
    memset(&phost, 0, sizeof(host_t));
    memset(&master, 0, sizeof(host_t));

    mvm = (struct msg_vm_opt *)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {

        VMDRIVER_LOG(LOG_ERR, "vm/vaget to connect vnc not found\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              mvm->vm_id, vm_info.vm_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_vmdb_pool_load(&pool, vm_info.pool_id) != LC_DB_POOL_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "pool %d not found\n",vm_info.pool_id);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    strcpy(phost.host_address, vm_info.host_address);

    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) != LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "host %s not found\n",phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    vm_info.hostp = &phost;

    if (pool.ctype == LC_POOL_TYPE_XEN) {

        if (pool.stype == LC_POOL_TYPE_SHARED) {
            if (lc_vmdb_compute_resource_load(&master, phost.master_address) != LC_DB_HOST_FOUND) {
                VMDRIVER_LOG(LOG_ERR, "host %s not found\n",phost.master_address);
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "host not found",
                              LC_SYSLOG_LEVEL_ERR);
                vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
                goto error;
            }
            phost.master = &master;
        } else {
            phost.master = &phost;
        }

        VMDRIVER_LOG(LOG_DEBUG, "Connect VNC %s, in host %s",
                     vm_info.vm_label, vm_info.hostp->host_address);

        lc_stop_vnctunnel(&vm_info);
        lc_start_vnctunnel(&vm_info);

    } else if (pool.ctype == LC_POOL_TYPE_VMWARE) {

        VMDRIVER_LOG(LOG_DEBUG, "Connect VNC %s, in host %s",
                     vm_info.vm_label, vm_info.hostp->host_address);
        lc_stop_vmware_vnctunnel(&vm_info);
        lc_start_vmware_vnctunnel(&vm_info);

    } else {
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "pool ctype error",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_POOL_ERROR;
        goto error;
    }

    vm_info.vm_flags &= (~LC_VM_VNC_FLAG);
    if (lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id)) {
        VMDRIVER_LOG(LOG_ERR, "vm db update failed\n");
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_CONNECT_VNC, LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label, "update db error",
                              LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    return 0;

error:

    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    /*FIXME: TODO*/
//    lc_msg_rollback((lc_mbuf_t *)m_head);

    return -1;
}

static int lc_vm_start_resume(struct lc_msg_head *m_head)
{
    struct lc_msg_head *m_send;
    lc_mbuf_t m;
    agexec_vm_t *av;
    host_t  phost;
    vm_info_t vm_info;
    int i;
    char buffer[LC_DB_BUF_LEN];
    char err_desc[AGEXEC_ERRDESC_LEN];
    char *perr = NULL;

    av = (agexec_vm_t *)(m_head + 1);

#if 0
    /*reuse start message to migrate system disk*/
    if (av->vdi_sys_size) {
        lc_vm_sys_migrate_resume(m_head);
        return 0;
    }
#endif
    memset(&phost, 0, sizeof(host_t));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&m, 0, sizeof(m));
    memset(err_desc, 0, sizeof(err_desc));

    m.hdr.seq = m_head->seq;
    if (lc_vmdb_vm_load_by_namelable(
                &vm_info, av->vm_name_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;
        m.hdr.type = LC_MSG_VM_START;
        m.hdr.magic = lc_msg_get_path(m.hdr.type, MSG_MG_START);
        m.hdr.length = sizeof(struct msg_vm_opt);

        if (lc_vmdb_vif_loadn_by_vm_id(&vm_info.ifaces[0],
                                       LC_VM_IFACES_MAX,
                                       vm_info.vm_id) != LC_DB_VIF_FOUND) {
            VMDRIVER_LOG(LOG_WARNING, "VM %d interfaces not found",
                         vm_info.vm_id);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label,
                                  "Virtual Interfaces NOT exist in Database",
                                  LC_SYSLOG_LEVEL_WARNING);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

        for (i = 0; i < LC_VM_IFACES_MAX; ++i) {
            if (vm_info.ifaces[i].if_id && vm_info.ifaces[i].if_index == i) {
                /* has data */
                continue;
            }
            VMDRIVER_LOG(LOG_WARNING, "VM %d interfaces not enough",
                         vm_info.vm_id);
            lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                  LC_SYSLOG_OBJECTTYPE_VM,
                                  vm_info.vm_id, vm_info.vm_label,
                                  "Virtual Interfaces NOT enough in Database",
                                  LC_SYSLOG_LEVEL_WARNING);
            vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
            goto error;
        }

    } else {
        VMDRIVER_LOG(LOG_ERR, "VM %s not found\n",
                     av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              0, av->vm_name_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    if (av->result) {
        perr = (char *)((char *)av + AGEXEC_LEN_LABEL + sizeof(uint32_t));
        strncpy(err_desc, perr, AGEXEC_ERRDESC_LEN);
        if (vm_info.vm_type == LC_VM_TYPE_VM) {
            VMDRIVER_LOG(LOG_ERR, "VM %s starts failed", av->vm_name_label);
            if (av->result == AGEXEC_VM_SET_HA_FAILED) {
                memset(buffer, 0, LC_DB_BUF_LEN);
                sprintf(buffer, "HA pool can't restart all VMs after a "
                        "Host failure once starting it");
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                      LC_SYSLOG_OBJECTTYPE_VM,
                                      vm_info.vm_id, vm_info.vm_label,
                                      buffer, LC_SYSLOG_LEVEL_ERR);
            } else {
                lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                                      LC_SYSLOG_OBJECTTYPE_VM,
                                      vm_info.vm_id, vm_info.vm_label,
                                      err_desc, LC_SYSLOG_LEVEL_ERR);
            }
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
        }
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "completing %s", av->vm_name_label);

    strcpy(phost.host_address, vm_info.host_address);

    if (lc_vmdb_compute_resource_load(&phost, phost.host_address) !=
            LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, " host %s not found\n",phost.host_address);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_START_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label,
                              "host not found", LC_SYSLOG_LEVEL_ERR);
        goto error;
    }

    vm_info.hostp = &phost;

    if (vm_info.vm_type == LC_VM_TYPE_GW) {
        for (i = 0; i < LC_PUB_VIF_MAX; ++i) {
            if (vm_info.pub_vif[i].if_state != LC_VIF_STATE_ATTACHED) {
                continue;
            }
            lc_vmdb_vif_update_name(vm_info.pub_vif[i].if_name,
                    vm_info.pub_vif[i].if_id);
            lc_vmdb_vif_update_ofport(vm_info.pub_vif[i].if_port,
                    vm_info.pub_vif[i].if_id);
        }
        for (i = 0; i < LC_L2_MAX; i++) {
            if (vm_info.pvt_vif[i].if_state != LC_VIF_STATE_ATTACHED) {
                continue;
            }
            lc_vmdb_vif_update_name(vm_info.pvt_vif[i].if_name,
                    vm_info.pvt_vif[i].if_id);
            lc_vmdb_vif_update_ofport(vm_info.pvt_vif[i].if_port,
                    vm_info.pvt_vif[i].if_id);
        }
    } else if (vm_info.vm_type == LC_VM_TYPE_VM) {
        for (i = 0; i < LC_VM_IFACES_MAX; i++) {
            if (vm_info.ifaces[i].if_state != LC_VIF_STATE_ATTACHED) {
                continue;
            }
            if (i == LC_VM_IFACE_CTRL) {
                continue;
            }
        }
    }

    //lc_start_vnctunnel(&vm_info);


    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        m_send = lc_get_vm_forward_start_msg(&(m.hdr), &vm_info);
        if (m_send) {
            lc_msg_forward((lc_mbuf_t *)m_send);
            free(m_send);
        }
        lc_vmdb_vm_update_action(LC_VM_ACTION_DONE, vm_info.vm_id);
        lc_vmdb_vm_update_state(LC_VM_STATE_RUNNING, vm_info.vm_id);
    }
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                           TALKER__VM_OPS__VM_START, LC_BUS_QUEUE_TALKER);
#if 0
    /*migrate system disk to this vm*/
    if (lc_migrate_vm_sys_disk(&vm_info)) {
        VMDRIVER_LOG(LOG_ERR, "Migrate VM:(%s) system disk failure",vm_info.vm_label);
    }
#endif
    return 0;

error:

    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    }

    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                        TALKER__VM_OPS__VM_START, LC_BUS_QUEUE_TALKER);
    return -1;
}

static int lc_vm_stop_resume(struct lc_msg_head *m_head)
{
    agexec_vm_t *av;
    vm_info_t vm_info;
    char err_desc[AGEXEC_ERRDESC_LEN];
    char *perr = NULL;

    av = (agexec_vm_t *)(m_head + 1);
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(err_desc, 0, sizeof(err_desc));

    if (lc_vmdb_vm_load_by_namelable(&vm_info,
                                     av->vm_name_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;

    } else {
        VMDRIVER_LOG(LOG_ERR, "VM %s not found\n", av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              0, av->vm_name_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    if (av->result) {
        perr = (char *)((char *)av + AGEXEC_LEN_LABEL + sizeof(uint32_t));
        strncpy(err_desc, perr, AGEXEC_ERRDESC_LEN);
        VMDRIVER_LOG(LOG_ERR, "VM %s stop failed", av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_STOP_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label,
                              err_desc, LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_STOP_ERROR;
        goto error;
    }

    VMDRIVER_LOG(LOG_DEBUG, "completing %s", av->vm_name_label);

    lc_vmdb_vm_update_action(LC_VM_ACTION_DONE, vm_info.vm_id);
    lc_vmdb_vm_update_state(LC_VM_STATE_STOPPED, vm_info.vm_id);

    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                    TALKER__VM_OPS__VM_STOP, LC_BUS_QUEUE_TALKER);
    return 0;

error:
    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                    TALKER__VM_OPS__VM_STOP, LC_BUS_QUEUE_TALKER);
    return -1;
}

static int lc_vm_del_resume(struct lc_msg_head *m_head)
{
    agexec_vm_t *av;
    vm_info_t vm_info;
    char cmd[MAX_CMD_BUF_SIZE];
    char err_desc[AGEXEC_ERRDESC_LEN];
    char *perr = NULL;

    av = (agexec_vm_t *)(m_head + 1);
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(err_desc, 0, sizeof(err_desc));

    if (lc_vmdb_vm_load_by_namelable(&vm_info,
                                     av->vm_name_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;

    } else {
        VMDRIVER_LOG(LOG_ERR, "VM %s not found\n", av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              0, av->vm_name_label, "vm not found",
                              LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    if (av->result) {
        perr = (char *)((char *)av + AGEXEC_LEN_LABEL + sizeof(uint32_t));
        strncpy(err_desc, perr, AGEXEC_ERRDESC_LEN);
        VMDRIVER_LOG(LOG_ERR, "VM %s delete failed", av->vm_name_label);
        lc_vmdriver_db_syslog(LC_SYSLOG_ACTION_DELETE_VM,
                              LC_SYSLOG_OBJECTTYPE_VM,
                              vm_info.vm_id, vm_info.vm_label,
                              err_desc, LC_SYSLOG_LEVEL_ERR);
        vm_info.vm_error |= LC_VM_ERRNO_DELETE_ERROR;
        goto error;
    }

    lc_vmdb_vm_update_state(LC_VM_STATE_DESTROYED, vm_info.vm_id);

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, MAX_CMD_BUF_SIZE, LC_VM_META_DELETE_STR, vm_info.vm_label);
    lc_call_system(cmd, "VM del", "vm", vm_info.vm_label, lc_vmdriver_db_log);

    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                    TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    return 0;

error:
    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                    TALKER__VM_OPS__VM_DELETE, LC_BUS_QUEUE_TALKER);
    return -1;
}

#if 0
static int lc_vm_sys_migrate_resume(struct lc_msg_head *m_head)
{
    agexec_vm_t *av;
    vm_info_t vm_info;
    char err_desc[AGEXEC_ERRDESC_LEN];
    char *perr = NULL;

    av = (agexec_vm_t *)(m_head + 1);

    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(err_desc, 0, sizeof(err_desc));

    if (lc_vmdb_vm_load_by_namelable(
                &vm_info, av->vm_name_label) == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;
    } else {
        VMDRIVER_LOG(LOG_ERR, "VM %s not found\n",
                    av->vm_name_label);
        return -1;
    }

    if (av->result) {
        perr = (char *)((char *)av + AGEXEC_LEN_LABEL + sizeof(uint32_t));
        strncpy(err_desc, perr, AGEXEC_ERRDESC_LEN);
        if (vm_info.vm_type == LC_VM_TYPE_VM) {
            VMDRIVER_LOG(LOG_ERR, "VM %s migrates system disk failed", av->vm_name_label);
            vm_info.vm_error |= LC_VM_ERRNO_START_ERROR;
        }
        goto error;
    }
    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        VMDRIVER_LOG(LOG_DEBUG, "%s system disk uuid=%s",av->vm_name_label, av->vdi_sys_uuid);
        lc_vmdb_vm_update_vdi_info(av->vdi_sys_uuid, "vdi_sys_uuid",
                                                     vm_info.vm_id);
    }
    VMDRIVER_LOG(LOG_DEBUG, "completing %s sysdisk migration",av->vm_name_label);

    return 0;

error:
#if 0
    if (vm_info.vm_type == LC_VM_TYPE_VM) {
        lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*        lc_vmdb_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    }

    lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                        TALKER__VM_OPS__VM_START, LC_BUS_QUEUE_TALKER);
#endif
    return -1;
}
#endif

static int lc_disk_set(struct lc_msg_head *m_head)
{
    struct msg_disk_opt *disk_plug;
    vm_info_t vm_info = {0};
    disk_t disk[MAX_VM_HA_DISK] = {{0}};
    host_t host;
    int op = 0, mlen, i, ret;
    uint32_t vm_id = 0;
    char buf[MAX_MSG_BUFFER];

    memset(&host, 0, sizeof(host_t));
    disk_plug = (struct msg_disk_opt *)(m_head + 1);

    VMDRIVER_LOG(LOG_DEBUG, "Set disk\n");

    for (i=0; i<disk_plug->disk_num; i++) {
        if (lc_vmdb_disk_load_by_id(&disk[i], disk_plug->disk_id[i]) !=  LC_DB_VM_FOUND) {
            VMDRIVER_LOG(LOG_ERR, "LC set disk failure: disk not found\n");
            goto error;
        }
        vm_id = disk[i].vmid;
        if (vm_id != disk[0].vmid) {
            VMDRIVER_LOG(LOG_ERR, "LC set disk failure: vm not only\n");
            goto error;
        }
    }

    if (lc_vmdb_vm_load(&vm_info, vm_id) !=  LC_DB_VM_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "LC set disk failure: vm not found\n");
        goto error;
    }

    if (lc_vmdb_host_device_load(&host, vm_info.host_address, "*") !=  LC_DB_HOST_FOUND) {
        VMDRIVER_LOG(LOG_ERR, "LC set disk failure: host not found\n");
        goto error;
    }

    if (host.host_htype == LC_HOST_HTYPE_XEN) {
        ret = lc_agexec_vm_disk_set(&vm_info, disk_plug->disk_num, disk, m_head->type);
        if (ret & AGEXEC_HA_DISK_ERROR) {
            vm_info.vm_error |= LC_VM_ERRNO_DISK_HA;
        } else {
            vm_info.vm_error &= ~LC_VM_ERRNO_DISK_HA;
            if (m_head->type == LC_MSG_VM_PLUG_DISK) {
                vm_info.vm_flags |= LC_VM_HA_DISK_FLAG;
            } else {
                vm_info.vm_flags &= (~LC_VM_HA_DISK_FLAG);
            }
        }
        vm_info.vm_flags &= (~LC_VM_SET_HADISK_FLAG);
        lc_vmdb_vm_update_flag(&vm_info, vm_info.vm_id);
    } else if (host.host_htype == LC_POOL_TYPE_VMWARE) {
        if (m_head->type == LC_MSG_VM_PLUG_DISK) {
            op |= CLOUDMESSAGE__VMSET_DISK_FLAG__LC_VMWARE_ATTACH_DISK;
        } else {
            op |= CLOUDMESSAGE__VMSET_DISK_FLAG__LC_VMWARE_DETACH_DISK;
        }
        if ((mlen = lc_vcd_vm_set_disk_msg(&vm_info, op,
                        disk_plug->disk_num, disk, buf, m_head->seq)) <= 0) {
            VMDRIVER_LOG(LOG_ERR, "vm set disk error\n");
            goto error;
        }

        if (lc_bus_vmdriver_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER)) {
            return 0;
        }
    }

    VMDRIVER_LOG(LOG_DEBUG, "Set disk finish\n");
    lc_msg_forward((lc_mbuf_t *)m_head);
    lc_vmdb_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
    if (m_head->type == LC_MSG_VM_PLUG_DISK) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                        TALKER__VM_OPS__VM_HADISK_PLUG, LC_BUS_QUEUE_TALKER);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__SUCCESS, m_head->seq,
                      TALKER__VM_OPS__VM_HADISK_UNPLUG, LC_BUS_QUEUE_TALKER);
    }
    return 0;

error:
    if (m_head->type == LC_MSG_VM_PLUG_DISK) {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                     TALKER__VM_OPS__VM_HADISK_PLUG, LC_BUS_QUEUE_TALKER);
    } else {
        lc_msg_rollback(&vm_info, NULL, TALKER__RESULT__FAIL, m_head->seq,
                   TALKER__VM_OPS__VM_HADISK_UNPLUG, LC_BUS_QUEUE_TALKER);
    }
    return -1;
}

static int lc_vm_replace(struct lc_msg_head *m_head)
{
    int ret = 0;
#if 0
    /*reuse deleting vm's message*/
    ret = lc_vm_del(m_head);
    if ((ret != -1) && (ret != LC_POOL_TYPE_VMWARE)) {
        ret = lc_vm_create(m_head);
    }
#endif
    return ret;
}
