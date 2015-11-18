#include "cloudmessage.pb-c.h"
#include "vm/vm_host.h"
#include "lc_netcomp.h"
#include "lc_db.h"
#include "lc_db_vm.h"
#include "lc_db_pool.h"
#include "lc_db_errno.h"
#include "lc_header.h"

static void vl2_id_to_pg_name(int id, char *pg_name, int size)
{
    memset(pg_name, 0, size);
    snprintf(pg_name, size, LC_VCD_PG_PREFIX "%06d", id);
}

int lc_vcd_vm_add_msg(vm_info_t *vm, host_t *host, vcdc_t *vcdc, void* buf, uint32_t seqid)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMAddReq vm_add = CLOUDMESSAGE__VMADD_REQ__INIT;
    Cloudmessage__IntfInfo intf[1 + LC_L2_MAX];
    Cloudmessage__LaunchInfo launch_info = CLOUDMESSAGE__LAUNCH_INFO__INIT;
    char iso[LC_NAMESIZE];

    char pg_name[1 + LC_L2_MAX][LC_NAMESIZE];

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    vm_add.vm_name = vm->vm_label;
    vm_add.vm_template = vm->vm_template;
    vm_add.vm_cpunum = vm->vcpu_num;
    vm_add.vm_memory = vm->mem_size;
    vm_add.vm_sys_disk_ds_name = vm->vdi_sys_sr_name;
    vm_add.vm_sys_disk_size = vm->vdi_sys_size;
    vm_add.vm_user_disk_ds_name = vm->vdi_user_sr_name;
    vm_add.vm_user_disk_size = vm->vdi_user_size;
    vm_add.os_type = vm->vm_os;
    vm_add.vnc_port = vm->tport;
    memset(iso, 0, sizeof iso);
    snprintf(iso, LC_NAMESIZE, "/nfsiso/%s.iso", vm->vm_label);
    vm_add.iso_path = iso;

    if (vm->vm_type == LC_VM_TYPE_VM) {
        vm_add.n_intf = 1;
    } else if (vm->vm_type == LC_VM_TYPE_GW) {
        vm_add.n_intf = 1 + LC_L2_MAX;
    } else {
        VMDRIVER_LOG(LOG_ERR, "vm %s type error %08x\n",
                     vm->vm_label, vm->vm_type);
        return -1;
    }
    vm_add.intf = (Cloudmessage__IntfInfo **)malloc(
            vm_add.n_intf * sizeof(Cloudmessage__IntfInfo *));
    if (!vm_add.intf) {
        VMDRIVER_LOG(LOG_ERR, "Malloc error.\n");
        return -1;
    }
    cloudmessage__intf_info__init(&intf[0]);
    intf[0].mac = "";
    intf[0].v4addr = vm->pub_vif[0].if_ip;
    intf[0].gwaddr = vm->pub_vif[0].if_gateway;
    intf[0].mask = vm->pub_vif[0].if_netmask;
    memset(pg_name[0], 0, LC_NAMESIZE);
    vl2_id_to_pg_name(vm->pub_vif[0].if_subnetid,
            pg_name[0], LC_NAMESIZE);
    intf[0].portgroup_name = pg_name[0];
    intf[0].if_index = vm->pub_vif[0].if_index;
    intf[0].flag = CLOUDMESSAGE__INTF_INFO__INTF_FLAG__LC_INTF_STATIC;
    intf[0].bandwidth = vm->pub_vif[0].if_bandw;
    vm_add.intf[0] = &intf[0];

    launch_info.serverip = host->host_address;
    launch_info.username = host->username;
    launch_info.password = host->password;
    vm_add.launch_info = &launch_info;

    msg.vm_add_request = &vm_add;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seqid;
    fill_message_header(&hdr, buf);


    free(vm_add.intf);
    return hdr_len + msg_len;
}

int lc_vcd_vm_add_reply_msg(Cloudmessage__VMAddResp *resp, vm_info_t *vm)
{
    int r, i;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    if (!r) {
        if (resp->n_intf > 0) {
            strncpy(vm->pub_vif[0].if_mac,
                    resp->intf[0]->mac, MAX_VIF_MAC_LEN - 1);
            vm->pub_vif[0].if_index = resp->intf[0]->if_index;
            for (i = 1; i < resp->n_intf; ++i) {
                strncpy(vm->pvt_vif[i - 1].if_mac,
                        resp->intf[i]->mac, MAX_VIF_MAC_LEN - 1);
                vm->pvt_vif[i - 1].if_index = resp->intf[i]->if_index;
            }
        }
    }

    return r;
}

int lc_vcd_vm_del_msg(vm_info_t* vm, void *buf, uint32_t seqid)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMDeleteReq vm_del = CLOUDMESSAGE__VMDELETE_REQ__INIT;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    vm_del.vm_name = vm->vm_label;

    msg.vm_del_request = &vm_del;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seqid;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_del_reply_msg(Cloudmessage__VMDeleteResp *resp,
        vm_info_t *vm)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);

    return r;
}

int lc_vcd_vm_update_msg(vm_info_t* vm, uint32_t op,
        int disk_num, disk_t disks[], vcdc_t *vcdc, void *buf, uint32_t seq)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMUpdateReq vm_update = CLOUDMESSAGE__VMUPDATE_REQ__INIT;
    Cloudmessage__IntfInfo iface = CLOUDMESSAGE__INTF_INFO__INIT;
    Cloudmessage__IntfInfo *piface = &iface;
    Cloudmessage__VMHAdisk disk_tmp = CLOUDMESSAGE__VMHADISK__INIT;
    Cloudmessage__VMHAdisk ha_disk[MAX_VM_HA_DISK];
    Cloudmessage__VMHAdisk *pha_disk[MAX_VM_HA_DISK] = {0};
    char pg_name[LC_NAMESIZE];
    int i;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    vm_update.vm_name = vm->vm_label;
    vm_update.flag = op;
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_SYS_DISK) {
        vm_update.vm_sys_disk_ds_name = vm->vdi_sys_sr_name;
        vm_update.has_vm_sys_disk_size = 1;
        vm_update.vm_sys_disk_size = vm->vdi_sys_size;
    }
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_USER_DISK) {
        vm_update.vm_user_disk_ds_name = vm->vdi_user_sr_name;
        vm_update.has_vm_user_disk_size = 1;
        vm_update.vm_user_disk_size = vm->vdi_user_size;
    }
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_INTF) {
        vm_update.n_intf = 1;
        iface.mac = vm->pub_vif[0].if_mac;
        iface.v4addr = vm->pub_vif[0].if_ip;
        iface.gwaddr = vm->pub_vif[0].if_gateway;
        iface.mask = vm->pub_vif[0].if_netmask;
        memset(pg_name, 0, LC_NAMESIZE);
        vl2_id_to_pg_name(vm->pub_vif[0].if_subnetid,
                pg_name, LC_NAMESIZE);
        iface.portgroup_name = pg_name;
        iface.if_index = vm->pub_vif[0].if_index;
        iface.flag = CLOUDMESSAGE__INTF_INFO__INTF_FLAG__LC_INTF_STATIC;
        iface.bandwidth = vm->pub_vif[0].if_bandw;
        vm_update.intf = &piface;
    } else {
        vm_update.n_intf = 0;
    }
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_CPU_NUM) {
        vm_update.has_vm_cpu_num = 1;
        vm_update.vm_cpu_num = vm->vcpu_num;
    }
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_MEMORY_SIZE) {
        vm_update.has_vm_memory_size = 1;
        vm_update.vm_memory_size = vm->mem_size;
    }
    if (op & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_HA_DISK) {
        vm_update.n_ha_disk = disk_num;
        for (i=0; i<disk_num; i++) {
            ha_disk[i] = disk_tmp;
            ha_disk[i].userdevice = disks[i].userdevice;
            ha_disk[i].disk_size = disks[i].size;
            if (disks[i].vdi_uuid[0]) {
                ha_disk[i].disk_uuid = disks[i].vdi_uuid;
            }
            ha_disk[i].disk_ds_name = disks[i].vdi_sr_name;
            pha_disk[i] = &ha_disk[i];
        }
        vm_update.ha_disk = pha_disk;
    } else {
        vm_update.n_ha_disk = 0;
    }

    msg.vm_update_request = &vm_update;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_update_reply_msg(Cloudmessage__VMUpdateResp *resp,
        vm_info_t* vm, int *disk_num, disk_t disks[])
{
    int i,r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    vm->action = 0;

    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_SYS_DISK) {
        vm->action |= VM_CHANGE_MASK_DISK_SYS_RESIZE;
        vm->vdi_sys_size = resp->vm_sys_disk_size;
    }
    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_USER_DISK) {
        vm->action |= VM_CHANGE_MASK_DISK_USER_RESIZE;
        vm->vdi_user_size = resp->vm_user_disk_size;
    }
    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_INTF) {
        vm->action |= VM_CHANGE_MASK_BW_MODIFY;
        vm->pub_vif[0].if_bandw = resp->intf[0]->bandwidth;
    }
    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_CPU_NUM) {
        vm->action |= VM_CHANGE_MASK_CPU_RESIZE;
        vm->vcpu_num = resp->vm_cpu_num;
    }
    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_MEMORY_SIZE) {
        vm->action |= VM_CHANGE_MASK_MEM_RESIZE;
        vm->mem_size = resp->vm_memory_size;
    }
    if (resp->flag & CLOUDMESSAGE__VMUPDATE_FLAG__LC_VMWARE_UPDATE_HA_DISK) {
        vm->action |= VM_CHANGE_MASK_HA_DISK;
        for (i=0; i<resp->n_ha_disk; i++) {
            disks[i].userdevice = resp->ha_disk[i]->userdevice;
            disks[i].size = resp->ha_disk[i]->disk_size;
            if (resp->ha_disk[i]->disk_uuid) {
                strcpy(disks[i].vdi_uuid, resp->ha_disk[i]->disk_uuid);
                strcpy(disks[i].vdi_name, resp->ha_disk[i]->disk_name);
            }
        }
        *disk_num = resp->n_ha_disk;
    }

    return r;
}

static int lc_vcd_vm_ops_msg(vm_info_t* vm, uint32_t op, void *buf, uint32_t seq)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMOpsReq vm_ops = CLOUDMESSAGE__VMOPS_REQ__INIT;
    char iso[LC_NAMESIZE];

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    vm_ops.vm_name = vm->vm_label;
    vm_ops.ops = op;
    if (op == CLOUDMESSAGE__VMOPS__LC_VM_START) {
        sprintf(iso, "/nfsiso/%s.iso", vm->vm_label);
        vm_ops.iso_path = iso;
    }

    msg.vm_ops_request = &vm_ops;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_start_msg(vm_info_t *vm, void* buf, uint32_t seq)
{
    return lc_vcd_vm_ops_msg(vm, CLOUDMESSAGE__VMOPS__LC_VM_START, buf, seq);
}

int lc_vcd_vm_shutdown_msg(vm_info_t *vm, void* buf, uint32_t seq)
{
    return lc_vcd_vm_ops_msg(vm, CLOUDMESSAGE__VMOPS__LC_VM_SHUTDOWN, buf, seq);
}

int lc_vcd_vm_pause_msg(vm_info_t *vm, void* buf)
{
    return lc_vcd_vm_ops_msg(vm, CLOUDMESSAGE__VMOPS__LC_VM_PAUSE, buf, 0);
}

int lc_vcd_vm_resume_msg(vm_info_t *vm, void* buf)
{
    return lc_vcd_vm_ops_msg(vm, CLOUDMESSAGE__VMOPS__LC_VM_RESUME, buf, 0);
}

int lc_vcd_vm_ops_reply_msg(Cloudmessage__VMOpsResp *resp,
        vm_info_t *vm)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);

    if (!r) {
        switch (resp->ops) {
        case CLOUDMESSAGE__VMOPS__LC_VM_START:
            vm->vm_state = LC_VM_STATE_RUNNING;
            break;
        case CLOUDMESSAGE__VMOPS__LC_VM_SHUTDOWN:
            vm->vm_state = LC_VM_STATE_STOPPED;
            break;
        case CLOUDMESSAGE__VMOPS__LC_VM_PAUSE:
            vm->vm_state = LC_VM_STATE_SUSPENDED;
            break;
        case CLOUDMESSAGE__VMOPS__LC_VM_RESUME:
            vm->vm_state = LC_VM_STATE_RUNNING;
            break;
        default:
            break;
        }
    }

    return r;
}

int lc_vcd_hosts_info_msg(host_t *host, void *buf)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__HostsInfoReq host_info = CLOUDMESSAGE__HOSTS_INFO_REQ__INIT;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    host_info.host_address = host->host_address;

    msg.host_info_request = &host_info;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_hosts_info_reply_msg(Cloudmessage__HostsInfoResp *resp,
        host_t *host)
{
    int r, i, n_storage = 0, n_hastorage = 0;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(host->host_address, resp->host_address,
            MAX_HOST_ADDRESS_LEN - 1);

    if (!r) {
        host->vcpu_num = resp->cpu_num;
        strncpy(host->cpu_info, resp->cpu_info,
                LC_HOST_CPU_INFO - 1);
        host->mem_total = resp->mem_total;
        strncpy(host->host_name, resp->host_name,
                MAX_HOST_NAME_LEN - 1);
        if (resp->n_storage_info > LC_MAX_SR_NUM) {
            VMDRIVER_LOG(LOG_ERR, "too many SRs\n");
        }
        for (i = 0; i < resp->n_storage_info && i < LC_MAX_SR_NUM; ++i) {
            if (strncmp(resp->storage_info[i]->ds_name, "livecloud_ha", 12) == 0) {
                strncpy(host->ha_storage[n_hastorage].name_label, resp->storage_info[i]->ds_name,
                        MAX_SR_NAME_LEN - 1);
                strncpy(host->ha_storage[n_hastorage].uuid, resp->storage_info[i]->uuid,
                        MAX_UUID_LEN - 1);
                if (resp->storage_info[i]->storage_type ==
                        CLOUDMESSAGE__STORAGE_INFO__STORAGE_TYPE__LC_STORAGE_SHARED) {
                    host->ha_storage[n_hastorage].is_shared = 1;
                } else {
                    host->ha_storage[n_hastorage].is_shared = 0;
                }
                host->ha_storage[n_hastorage].disk_total = resp->storage_info[i]->disk_total;
                host->ha_storage[n_hastorage].disk_used =
                    host->storage[n_hastorage].disk_total - resp->storage_info[i]->disk_free;
                n_hastorage++;
            } else {
                strncpy(host->storage[n_storage].name_label, resp->storage_info[i]->ds_name,
                        MAX_SR_NAME_LEN - 1);
                strncpy(host->storage[n_storage].uuid, resp->storage_info[i]->uuid,
                        MAX_UUID_LEN - 1);
                if (resp->storage_info[i]->storage_type ==
                        CLOUDMESSAGE__STORAGE_INFO__STORAGE_TYPE__LC_STORAGE_SHARED) {
                    host->storage[n_storage].is_shared = 1;
                } else {
                    host->storage[n_storage].is_shared = 0;
                }
                host->storage[n_storage].disk_total = resp->storage_info[i]->disk_total;
                host->storage[n_storage].disk_used =
                    host->storage[n_storage].disk_total - resp->storage_info[i]->disk_free;
                n_storage++;
            }
            host->n_storage = n_storage;
            host->n_hastorage = n_hastorage;
        }
    }

    return r;
}

int lc_vcd_vm_snapshot_add_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seq)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMSnapshotAddReq snap_add = CLOUDMESSAGE__VMSNAPSHOT_ADD_REQ__INIT;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    snap_add.vm_name = vm->vm_label;
    snap_add.vm_snapshot_name = ss->snapshot_label;
    snap_add.flag = CLOUDMESSAGE__VMSNAPSHOT_ADD_REQ__VMSNAPSHOT_ADD_FLAG__LC_VM_SNAPSHOT_ADD_REPLACE_OLD;

    msg.vm_snapshot_add_request = &snap_add;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_snapshot_add_reply_msg(Cloudmessage__VMSnapshotAddResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    strncpy(ss->snapshot_label, resp->vm_snapshot_name, MAX_VM_NAME_LEN - 1);

    return r;
}

int lc_vcd_vm_snapshot_del_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seq)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMSnapshotDelReq snap_del = CLOUDMESSAGE__VMSNAPSHOT_DEL_REQ__INIT;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    snap_del.vm_name = vm->vm_label;
    snap_del.vm_snapshot_name = ss->snapshot_label;

    msg.vm_snapshot_del_request = &snap_del;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_snapshot_del_reply_msg(Cloudmessage__VMSnapshotDelResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    strncpy(ss->snapshot_label, resp->vm_snapshot_name,
            MAX_VM_NAME_LEN - 1);

    return r;
}

int lc_vcd_vm_snapshot_revert_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seqid)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMSnapshotRevertReq snap_revert = CLOUDMESSAGE__VMSNAPSHOT_REVERT_REQ__INIT;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    snap_revert.vm_name = vm->vm_label;
    snap_revert.vm_snapshot_name = ss->snapshot_label;

    msg.vm_snapshot_revert_request = &snap_revert;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seqid;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_snapshot_revert_reply_msg(Cloudmessage__VMSnapshotRevertResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    strncpy(ss->snapshot_label, resp->vm_snapshot_name,
            MAX_VM_NAME_LEN - 1);

    return r;
}

int lc_vcd_vm_set_disk_msg(vm_info_t* vm, uint32_t op,
        int disk_num, disk_t disks[], void *buf, uint32_t seq)
{
    header_t hdr;
    Cloudmessage__Message msg = CLOUDMESSAGE__MESSAGE__INIT;
    Cloudmessage__VMSetDiskReq set_disk = CLOUDMESSAGE__VMSET_DISK_REQ__INIT;
    Cloudmessage__VMHAdisk disk_tmp = CLOUDMESSAGE__VMHADISK__INIT;
    Cloudmessage__VMHAdisk ha_disk[MAX_VM_HA_DISK];
    Cloudmessage__VMHAdisk *pha_disk[MAX_VM_HA_DISK] = {0};
    int i;

    int hdr_len = get_message_header_pb_len();
    int msg_len;

    set_disk.vm_name = vm->vm_label;
    set_disk.flag = op;
    set_disk.n_ha_disk = disk_num;
    for (i=0; i<disk_num; i++) {
        ha_disk[i] = disk_tmp;
        ha_disk[i].userdevice = disks[i].userdevice;
        ha_disk[i].disk_size = disks[i].size;
        ha_disk[i].disk_uuid = disks[i].vdi_uuid;
        ha_disk[i].disk_name = disks[i].vdi_name;
        ha_disk[i].disk_ds_name = disks[i].vdi_sr_name;
        pha_disk[i] = &ha_disk[i];
    }
    set_disk.ha_disk = pha_disk;

    msg.vm_disk_request = &set_disk;
    cloudmessage__message__pack(&msg, buf + hdr_len);
    msg_len = cloudmessage__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    return hdr_len + msg_len;
}

int lc_vcd_vm_set_disk_reply_msg(Cloudmessage__VMSetDiskResp *resp,
        vm_info_t* vm)
{
    int r;

    if (!resp) {
        return 1;
    }
    r = resp->result;
    strncpy(vm->vm_label, resp->vm_name, MAX_VM_NAME_LEN - 1);
    vm->vm_flags = 0;

    if (resp->flag & CLOUDMESSAGE__VMSET_DISK_FLAG__LC_VMWARE_ATTACH_DISK) {
        vm->vm_flags |= LC_VM_HA_DISK_FLAG;
    }

    return r;
}
