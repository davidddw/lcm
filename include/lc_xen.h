#ifndef _LC_XEN_H
#define _LC_XEN_H

#include "lc_global.h"
#include "lc_agexec_msg.h"

#define XEN_VM_PARAM_SET_STR    "xe vm-param-set uuid=%s %s=%s"
#define XEN_VM_PARAM_VCPU       "VCPUs-at-startup"
#define XEN_VM_PARAM_VCPU_MAX   "VCPUs-max"
#define XEN_VM_PARAM_MEM_SMIN   "memory-static-min"
#define XEN_VM_PARAM_MEM_DMIN   "memory-dynamic-min"
#define XEN_VM_PARAM_MEM_DMAX   "memory-dynamic-max"
#define XEN_VM_PARAM_MEM_SMAX   "memory-static-max"

#define XEN_VM_SNAPSHOT_CREATE   0
#define XEN_VM_SNAPSHOT_DESTROY  1
#define XEN_VM_SNAPSHOT_REVERT   2

#define LC_MAX_CURL_LEN   40
typedef struct thread_url_s {
    pthread_t tid;
    char url[LC_MAX_CURL_LEN];
} thread_url_t;

thread_url_t thread_url[MAX_THREAD];

#ifndef _LC_MONITOR_
/*
XEN vm state:
0: Haltd;
1: Paused;
2: Running;
3: Suspended
4: Undefined
*/
#define LC_VM_PW_STATE_HALTED 0
#define LC_VM_PW_STATE_PAUSED 1
#define LC_VM_PW_STATE_RUNNING 2
#define LC_VM_PW_STATE_SUSPENDED 3
#define LC_VM_PW_STATE_UNDEFINED 4
#define LC_HOST_ETH_MAX_COUT 6
void xen_common_init(void);
int lc_xen_test_init();

int lc_check_host_device(host_t *phost);

int lc_nsp_get_host_name(host_t *host);

int lc_start_vnctunnel(vm_info_t *vm_info);
int lc_stop_vnctunnel(vm_info_t *vm_info);
int lc_start_vmware_vnctunnel(vm_info_t *vm_info);
int lc_stop_vmware_vnctunnel(vm_info_t *vm_info);
int lc_start_vm_by_import(vm_info_t *vm_info, host_t *phost);
int lc_start_vm_by_vminfo(vm_info_t *vm_info, int msg_type, uint32_t seqid);
int lc_migrate_vm_sys_disk(vm_info_t *vm_info);
int lc_vm_add_configure_iso(vm_info_t *vm_info);
int lc_vm_insert_configure_iso(vm_info_t *vm_info);
int lc_vm_eject_configure_iso(vm_info_t *vm_info);
int lc_host_vm_add_disk (vm_info_t *vm_info, struct msg_req_drv_vm_add_disk *vdi_add);
int lc_host_vm_remove_disk (vm_info_t *vm_info, struct msg_req_drv_vm_remove_disk *vdi);
int lc_host_vm_resize_disk (vm_info_t *vm_info, struct msg_req_drv_vm_resize_disk *vdi);

int lc_get_host_if_type(host_t *host);
int lc_get_host_mac_info(host_t *host);
int lc_get_host_device_info(host_t *host);

int lc_del_network_storage(host_t *phost, char *sr_uuid);
int lc_del_local_storage(host_t *phost);

int lc_xen_reconfigure_vm_cpu(host_t * host, vm_info_t *vm_info, int cpu);
int lc_xen_reconfigure_vm_memory(host_t * host, vm_info_t *vm_info, int memory);
int lc_xen_reconfigure_vm_by_template(host_t *host, vm_info_t *vm_info);

int lc_get_vm_state(xen_session *session, char *vmname);
bool lc_host_health_check(char *host_address, xen_session *session);

int lc_request_xen_add_qos (char *server_addr, char *vif, char *mac,
        char *uuid, uint32_t qos_capacity, uint32_t qos_burst);
int lc_request_xen_add_iface (char *server_addr, char *vif, char *phy_if,
        int vlan_id, char *mac, char *vm_uuid);
int lc_request_xen_del_mon (char *mac, char *uuid, char *server_addr);
int lc_request_xen_add_mon (char *mac, char *uuid, int vlan_id,
        char *server_addr);
int lc_vm_snapshot_uninstall(host_t *phost, vm_snapshot_info_t *snapshot_info);
int lc_vm_snapshot_take(vm_info_t *vm_info, vm_snapshot_info_t *snapshot_info);
int lc_vm_snapshot_revert(host_t *phost, vm_snapshot_info_t *snapshot_info);
int lc_vm_backup_create_script(char *vm_label, host_t *phost, host_t *peerhost);
int lc_vm_withsp_backup_create_script(char *vm_label, char *sp_label, host_t *phost, host_t *peerhost);
int lc_vm_backup_delete_script(vm_backup_info_t *backup_info);
int lc_vm_backup_restore_script(vm_info_t *vm_info);
int lc_xen_migrate_vm_with_snapshot(char *vm_uuid, host_t *phost, host_t *peerhost);
int lc_xen_migrate_vm(char *vm_label, host_t *phost, host_t *peerhost);
int lc_xen_check_vm_migrate(char *vm_label, host_t *phost, host_t *peerhost);
int lc_xen_get_vdi_uuid(vm_info_t *vm_info);
int lc_xen_get_host_uuid(host_t *host);
int lc_agexec_vm_uninstall(vm_info_t *vm_info);
int lc_agexec_vm_create(vm_info_t *vm_info, uint32_t seqid);
int lc_agexec_vm_ops(vm_info_t *vm_info, int msg_type, uint32_t seqid);
int lc_agexec_vm_snapshot_ops(vm_info_t *vm_info, vm_snapshot_info_t *snapshot_info, int action);
int lc_agexec_vm_backup_remote(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                               vm_backup_info_t *vm_backup_info);
int lc_agexec_vm_recovery(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                          vm_backup_info_t *vm_backup_info);
int lc_agexec_vm_backup_delete(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                               vm_backup_info_t *vm_backup_info);
int lc_agexec_vm_backup_chkdisk(vm_info_t *vm_info, host_t *phost, host_t *peerhost);
int lc_agexec_vm_migrate_local(vm_info_t *vm_info, host_t *phost, host_t *peerhost);
int lc_agexec_vm_migrate_remote(vm_info_t *vm_info, host_t *phost, host_t *peerhost,
                               vm_backup_info_t *vm_backup_info);
int lc_agexec_get_host_info(host_t *phost);
int lc_agexec_vm_stop(vm_info_t *vm_info);
int lc_agexec_vm_modify(vm_info_t *vm_info, int disk_num, disk_t disks[]);
int lc_agexec_vm_disk_set(vm_info_t *vm_info, int disk_num, disk_t disks[], int type);
extern int
xen_state_get_host_cpu_info(xen_session *session, char **result, int *cpu_number, xen_host host);

extern int
xen_state_get_host_mem_total(xen_session *session, int *result, xen_host host);
#endif /*_LC_MONITOR_*/
#endif
