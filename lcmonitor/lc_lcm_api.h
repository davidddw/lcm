/*
 * Copyright 2012 yunshan.net.cn
 * All rights reserved
 * Author: Lai Yuan laiyuan@yunshan.net.cn
 */
#ifndef __LC_LCM_API_H_INCLUDED__
#define __LC_LCM_API_H_INCLUDED__

#include "sys/queue.h"
#include "framework/basic_types.h"

#define LC_DOMAIN_SIZE                       64
/*int
lcm_transmsg(int to, void* msg);
*/

struct lcm_message {
    struct lcm_msghdr {
        int        flags;
        int        eventid;
        int        cmd;
        int        status;
        void*      handler_req;
        void*      handler_resp;
        int        lcmcmdseq;
    } header;
    int            id;
    int            bitmasks;
};

typedef struct lcm_message lcm_message_t;

#define LCM_MGM_SOCKPATH        "/tmp/.lcm_mgm_unix"
#define LCM_APP_SOCKPATH        "/tmp/.lcm_app_unix"
#define LCMD_SHM_ID             "/usr/local/livecloud/conf/lcmconf"

/*
 * Define LCM Command Error types and information
 * Information should shorten than 64 chars
 * Should be used by header.status
 */
#define LCM_CMD_INFO_LEN                       512
#define LCM_CMD_STATUS_SUCCESS                 0

/*
 * Control lcm commands returns
 */
#define LCM_CMD_CONTROL_ERR_BASE               0
#define LCM_CMD_CONTROL_ERR_FORMAT             1
#define LCM_CMD_CONTROL_ERR_NOAPP              2
#define LCM_CMD_CONTROL_ERR_CONF               3
#define LCM_CMD_CONTROL_ERR_CMDFORMAT          4
#define LCM_CMD_CONTROL_ERR_CMD_NOEXIST        5
#define LCM_CMD_CONTROL_ERR_UNKONW             6
/*
 * extern const char* control_ret[];
 */

/*
 * Device/host lcm commands returns
 */
#define LCM_CMD_DEVICE_HOST_ERR_TIMEOUT       1
#define LCM_CMD_DEVICE_HOST_ERR_DATABASE      2
#define LCM_CMD_DEVICE_HOST_ERR_DISCONN       4
/*
 * extern const char* device_host_errno[];
 */
/*
 * Resourcepool lcm error
 */
#define LCM_CMD_POOL_ERR_TIMEOUT              1
/*
 * extern const char* pool_errno[];
 */

/*
 * Resourcepool/compute lcm error
 */
#define LCM_CMD_POOL_CR_ERR_TIMEOUT           0x1
#define LCM_CMD_POOL_CR_ERR_DISCONN           0x2
#define LCM_CMD_POOL_CR_ERR_INVALID           0x4
#define LCM_CMD_POOL_CR_ERR_DATABASE          0x8
#define LCM_CMD_POOL_CR_ERR_LCDISCONN         0x10
#define LCM_CMD_POOL_CR_ERR_INFO              0x20
#define LCM_CMD_POOL_CR_ERR_SHARED_PUSH       0x40
#define LCM_CMD_POOL_CR_ERR_SHARED_PULL       0x80
/*
 * extern const char* pool_cr_errno[];
 */

/*
 * Resourcepool/storage lcm error
 */
#define LCM_CMD_POOL_SR_ERR_TIMEOUT           0x1
#define LCM_CMD_POOL_SR_ERR_DATABASE          0x2
#define LCM_CMD_POOL_SR_ERR_LCDISCONN         0x4
#define LCM_CMD_POOL_SR_ERR_INFO              0x8
/*
 * extern const char* pool_sr_errno[];
 */

/*
 * Resourcepool/network lcm error
 */
#define LCM_CMD_POOL_NR_ERR_TIMEOUT           0x1
#define LCM_CMD_POOL_NR_ERR_DATABASE          0x2
#define LCM_CMD_POOL_NR_ERR_LCDISCONN         0x4
#define LCM_CMD_POOL_NR_ERR_NETWORK           0x8
#define LCM_CMD_POOL_NR_ERR_INFO              0x10
/*
 * extern const char* pool_nr_errno[];
 */

/*
 * Cloud lcm commands errno
 */
#define LCM_CMD_CLOUD_ERR_TIMEOUT             1
/*
 * extern const char* cloud_errno[];
 */

/*
 * Cloud/DC lcm commands errno
 */
#define LCM_CMD_CLOUD_VDC_ERR_TIMEOUT          0x1
#define LCM_CMD_CLOUD_VDC_ERR_RES2             0x2
#define LCM_CMD_CLOUD_VDC_ERR_RES3             0x4
#define LCM_CMD_CLOUD_VDC_ERR_RES4             0x8
#define LCM_CMD_CLOUD_VDC_ERR_RES5             0x10
#define LCM_CMD_CLOUD_VDC_ERR_RES6             0x20
#define LCM_CMD_CLOUD_VDC_ERR_RES7             0x40
#define LCM_CMD_CLOUD_VDC_ERR_DATABASE         0x80
#define LCM_CMD_CLOUD_VDC_ERR_POOLTYPE         0x100
#define LCM_CMD_CLOUD_VDC_ERR_INOEXIST         0x200
#define LCM_CMD_CLOUD_VDC_ERR_EXIST            0x400
#define LCM_CMD_CLOUD_VDC_ERR_TNOEXIST         0x800
#define LCM_CMD_CLOUD_VDC_ERR_SCRIPT           0x1000
#define LCM_CMD_CLOUD_VDC_ERR_COMMAND          0x2000
#define LCM_CMD_CLOUD_VDC_ERR_MIGRATE          0x4000
#define LCM_CMD_CLOUD_VDC_ERR_ISTATE           0x8000
#define LCM_CMD_CLOUD_VDC_ERR_NO_DISK          0x10000
#define LCM_CMD_CLOUD_VDC_ERR_NO_MEMORY        0x20000
/*
 * extern const char* cloud_vdc_errno[];
 */
/*
 * Cloud/vl2 lcm commands returns
 */
#define LCM_CMD_CLOUD_VL2_ERR_TIMEOUT          0x1
#define LCM_CMD_CLOUD_VL2_ERR_DATABASE         0x2
#define LCM_CMD_CLOUD_VL2_ERR_LCDISCONN        0x4
#define LCM_CMD_CLOUD_VL2_ERR_NETWORK          0x8
#define LCM_CMD_CLOUD_VL2_ERR_VLANID           0x10
#define LCM_CMD_CLOUD_VL2_ERR_VMWARE           0x20
#define LCM_CMD_CLOUD_VL2_ERR_AGEXEC           0x40
/*
 * extern const char* cloud_vl2_errno[];
 */
/*
 * Cloud/vm lcm commands returns
 */
#define LCM_CMD_CLOUD_VM_ERR_TIMEOUT           0x1
#define LCM_CMD_CLOUD_VM_ERR_DISK_ADD          0x2
#define LCM_CMD_CLOUD_VM_ERR_DISK_DEL          0x4
#define LCM_CMD_CLOUD_VM_ERR_DISK_SYS_RESIZE   0x8
#define LCM_CMD_CLOUD_VM_ERR_DISK_USER_RESIZE  0x10
#define LCM_CMD_CLOUD_VM_ERR_CPU_RESIZE        0x20
#define LCM_CMD_CLOUD_VM_ERR_MEM_RESIZE        0x40
#define LCM_CMD_CLOUD_VM_ERR_DATABASE          0x80
#define LCM_CMD_CLOUD_VM_ERR_POOLTYPE          0x100
#define LCM_CMD_CLOUD_VM_ERR_INOEXIST          0x200
#define LCM_CMD_CLOUD_VM_ERR_EXIST             0x400
#define LCM_CMD_CLOUD_VM_ERR_TNOEXIST          0x800
#define LCM_CMD_CLOUD_VM_ERR_SCRIPT            0x1000
#define LCM_CMD_CLOUD_VM_ERR_COMMAND           0x2000
#define LCM_CMD_CLOUD_VM_ERR_MIGRATE           0x4000
#define LCM_CMD_CLOUD_VM_ERR_ISTATE            0x8000
#define LCM_CMD_CLOUD_VM_ERR_NO_DISK           0x10000
#define LCM_CMD_CLOUD_VM_ERR_BW_MODIFY         0x20000
#define LCM_CMD_CLOUD_VM_ERR_NO_MEMORY         0x40000
/*
 * extern const char* cloud_vm_errno[];
 */

/*
 * Backup snapshot commands returns
 */
#define LCM_CMD_BACKUP_SNAPSHOT_ERR_TIMEOUT           0x1
#define LCM_CMD_BACKUP_SNAPSHOT_ERR_DATABASE          0x2
#define LCM_CMD_BACKUP_SNAPSHOT_ERR_CREATE            0x4
#define LCM_CMD_BACKUP_SNAPSHOT_ERR_DELETE            0x8
#define LCM_CMD_BACKUP_SNAPSHOT_ERR_RECOVERY          0x10
/*
 * extern const char* snapshot_errno[];
 */

#define LCM_CMD_BACKUP_REMOTE_ERR_TIMEOUT           0x1
#define LCM_CMD_BACKUP_REMOTE_ERR_DATABASE          0x2
#define LCM_CMD_BACKUP_REMOTE_ERR_LOCAL             0x4
#define LCM_CMD_BACKUP_REMOTE_ERR_REMOTE            0x8
#define LCM_CMD_BACKUP_REMOTE_ERR_RECOVERY          0x10
#define LCM_CMD_BACKUP_REMOTE_ERR_DELETE            0x20
#define LCM_CMD_BACKUP_REMOTE_ERR_NODISK            0x40
/*
 * extern const char* backup_errno[];
 */

/*
 * Promt definition
 */

typedef struct lcmvm {
    SLIST_ENTRY(lcmvm) chain;
    /*
     * vm infor
     */
    u32                  vmid;
    u32                  vmstate;
    u32                  vmvcloudid;
    u32                  vmvnetid;
    u32                  vmvl2id;
    u32                  vmpoolid;
    u32                  vmerrno;
    u32                  vmflag;
    u32                  vmvdiusersize;
    u32                  vmmemsize;
    u32                  vmvcpunum;
    char                 domain[LC_DOMAIN_SIZE];
}lcmvm_t;

typedef struct lcmvdc {
    SLIST_ENTRY(lcmvdc) chain;
    /*
     * vnet infor
     */
    u32                  vdcid;
    u32                  vdcstate;
    u32                  vdcvcloudid;
    u32                  vdcpoolid;
    u32                  vdcerrno;
    u32                  vdcflag;
    char                 domain[LC_DOMAIN_SIZE];
} lcmvdc_t;

extern char *lcm_server;

int url_update_lcm_vgate_db(lcmvdc_t *vdc);
int url_update_lcm_vm_db(lcmvm_t *vm);
#endif
