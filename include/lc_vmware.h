#ifndef _LC_VMWARE_H
#define _LC_VMWRAE_H


#include "vm/vm_host.h"
#include "cloudmessage.pb-c.h"

#define LC_VMWARE_UPDATE_SR_SYS         1
#define LC_VMWARE_UPDATE_SR_DISK        2
#define LC_VMWARE_UPDATE_INTF_MAC       4

int lc_vcd_vm_add_msg(vm_info_t *vm, host_t *host, vcdc_t *vcdc, void* buf, uint32_t seqid);
int lc_vcd_vm_del_msg(vm_info_t* vm, void *buf, uint32_t seqid);
int lc_vcd_vm_start_msg(vm_info_t *vm, void* buf, uint32_t seqid);
int lc_vcd_vm_shutdown_msg(vm_info_t *vm, void* buf, uint32_t seqid);
int lc_vcd_vm_pause_msg(vm_info_t *vm, void* buf);
int lc_vcd_vm_resume_msg(vm_info_t *vm, void* buf);
int lc_vcd_vm_update_msg(vm_info_t* vm, uint32_t op,
        int disk_num, disk_t disks[], vcdc_t *vcdc, void *buf, uint32_t seqid);
int lc_vcd_hosts_info_msg(host_t *host, void *buf);
int lc_vcd_vm_snapshot_add_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seqid);
int lc_vcd_vm_snapshot_del_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seqid);
int lc_vcd_vm_snapshot_revert_msg(vm_info_t *vm, vm_snapshot_info_t *ss, void *buf, uint32_t seqid);
int lc_vcd_vm_set_disk_msg(vm_info_t* vm, uint32_t op,
        int disk_num, disk_t disks[], void *buf, uint32_t seq);

int lc_vcd_vm_add_reply_msg(Cloudmessage__VMAddResp *resp,
        vm_info_t *vm);
int lc_vcd_vm_update_reply_msg(Cloudmessage__VMUpdateResp *resp,
        vm_info_t* vm, int *disk_num, disk_t disks[]);
int lc_vcd_vm_ops_reply_msg(Cloudmessage__VMOpsResp *resp,
        vm_info_t *vm);
int lc_vcd_vm_del_reply_msg(Cloudmessage__VMDeleteResp *resp,
        vm_info_t *vm);
int lc_vcd_vm_snapshot_add_reply_msg(Cloudmessage__VMSnapshotAddResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss);
int lc_vcd_vm_snapshot_del_reply_msg(Cloudmessage__VMSnapshotDelResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss);
int lc_vcd_vm_snapshot_revert_reply_msg(Cloudmessage__VMSnapshotRevertResp *resp,
        vm_info_t *vm, vm_snapshot_info_t *ss);
int lc_vcd_hosts_info_reply_msg(Cloudmessage__HostsInfoResp *resp,
        host_t *host);
int lc_vcd_vm_set_disk_reply_msg(Cloudmessage__VMSetDiskResp *resp,
        vm_info_t* vm);


#endif
