#ifndef _LC_DB_ERRNO_H
#define _LC_DB_ERRNO_H

/* host device */
#define LC_HOST_ERRNO_TIMEOUT            0x00000001
#define LC_HOST_ERRNO_DB_ERROR           0x00000002
#define LC_HOST_ERRNO_CON_ERROR          0x00000004
#define LC_HOST_ERRNO_AGENT_ERROR        0x00000008
#define LC_HOST_ERRNO_PWD_ERROR          0x00000010
#define LC_HOST_ERRNO_BR_ERROR           0x00000020

/* compute resource */
#define LC_CR_ERRNO_TIMEOUT              0x00000001
#define LC_CR_ERRNO_CONN_FAILURE         0x00000002
/*connect successfully, but has trouble*/
#define LC_CR_ERRNO_CONN_TROUBLE         0x00000004
#define LC_CR_ERRNO_DB_ERROR             0x00000008
#define LC_CR_ERRNO_POOL_ERROR           0x00000010
#define LC_CR_ERRNO_GET_INFO_ERROR       0x00000020
#define LC_CR_ERRNO_SLAVE_JOIN_ERROR     0x00000040
#define LC_CR_ERRNO_SLAVE_EJECT_ERROR    0x00000080

/* storage resource */
#define LC_SR_ERRNO_TIMEOUT              0x00000001
#define LC_SR_ERRNO_DB_ERROR             0x00000002
#define LC_SR_ERRNO_CON_ERROR            0x00000004
#define LC_SR_ERRNO_POOL_ERROR           0x00000008
#define LC_SR_ERRNO_GET_INFO_ERROR       0x00000010

/* network resource */
#define LC_NR_ERRNO_TIMEOUT              0x00000001
#define LC_NR_ERRNO_DB_ERROR             0x00000002
#define LC_NR_ERRNO_CON_ERROR            0x00000004
#define LC_NR_ERRNO_NETWORK_MODE_ERROR   0x00000008
#define LC_NR_ERRNO_GET_INFO_ERROR       0x00000010
#define LC_NR_ERRNO_RACK_TUNNEL_ERROR    0x00000020

/* VM errno */
#define LC_VM_ERRNO_TIMEOUT              0x00000001
#define LC_VM_ERRNO_DISK_ADD             0x00000002
#define LC_VM_ERRNO_DISK_DEL             0x00000004
#define LC_VM_ERRNO_DISK_SYS_RESIZE      0x00000008
#define LC_VM_ERRNO_DISK_USER_RESIZE     0x00000010
#define LC_VM_ERRNO_CPU_RESIZE           0x00000020
#define LC_VM_ERRNO_MEM_RESIZE           0x00000040
#define LC_VM_ERRNO_DB_ERROR             0x00000080
#define LC_VM_ERRNO_POOL_ERROR           0x00000100
#define LC_VM_ERRNO_NO_IMPORT_VM         0x00000200
#define LC_VM_ERRNO_VM_EXISTS            0x00000400
#define LC_VM_ERRNO_NO_TEMPLATE          0x00000800
#define LC_VM_ERRNO_IMPORT_OP_FAILED     0x00001000
#define LC_VM_ERRNO_INSTALL_OP_FAILED    0x00002000
#define LC_VM_ERRNO_MIGRATE_FAILED       0x00004000
#define LC_VM_ERRNO_IMPORT_VM_STATE_ERR  0x00008000
#define LC_VM_ERRNO_NO_DISK              0x00010000
#define LC_VM_ERRNO_BW_MODIFY            0x00020000
#define LC_VM_ERRNO_NO_MEMORY            0x00040000
#define LC_VM_ERRNO_NO_CPU               0x00080000
#define LC_VM_ERRNO_NO_RESOURCE          0x00100000
#define LC_VM_ERRNO_URL_ERROR            0x00200000
#define LC_VM_ERRNO_CREATE_ERROR         0x00400000
#define LC_VM_ERRNO_DELETE_ERROR         0x00800000
#define LC_VM_ERRNO_START_ERROR          0x01000000
#define LC_VM_ERRNO_STOP_ERROR           0x02000000
#define LC_VM_ERRNO_NOEXISTENCE          0x04000000
#define LC_VM_ERRNO_NETWORK              0x08000000
#define LC_VM_ERRNO_DISK_HA              0x10000000
#define LC_VM_ERRNO_CTRLIP_ERROR         0x20000000

/* VEDGE errno */
#define LC_VEDGE_ERRNO_TIMEOUT           0x00000001
#define LC_VEDGE_ERRNO_NO_IP             0x00000040
#define LC_VEDGE_ERRNO_DB_ERROR          0x00000080
#define LC_VEDGE_ERRNO_POOL_ERROR        0x00000100
#define LC_VEDGE_ERRNO_NO_IMPORT_VM      0x00000200
#define LC_VEDGE_ERRNO_VM_EXISTS         0x00000400
#define LC_VEDGE_ERRNO_NO_TEMPLATE       0x00000800
#define LC_VEDGE_ERRNO_IMPORT_OP_FAILED  0x00001000
#define LC_VEDGE_ERRNO_INSTALL_OP_FAILED 0x00002000
#define LC_VEDGE_ERRNO_MIGRATE_FAILED    0x00004000
#define LC_VEDGE_ERRNO_IMPORT_STATE_ERR  0x00008000
#define LC_VEDGE_ERRNO_NO_DISK           0x00010000
#define LC_VEDGE_ERRNO_NO_MEMORY         0x00020000
#define LC_VEDGE_ERRNO_NO_CPU            0x00040000
#define LC_VEDGE_ERRNO_URL_ERROR         0x00080000
#define LC_VEDGE_ERRNO_CREATE_ERROR      0x00100000
#define LC_VEDGE_ERRNO_DELETE_ERROR      0x00200000
#define LC_VEDGE_ERRNO_START_ERROR       0x00400000
#define LC_VEDGE_ERRNO_STOP_ERROR        0x00800000
#define LC_VEDGE_ERRNO_NOEXISTENCE       0x01000000
#define LC_VEDGE_ERRNO_NETWORK           0x02000000
#define LC_VEDGE_ERRNO_CTRLIP_ERROR      0x04000000
#define LC_VEDGE_ERRNO_HA_ERROR          0x08000000
#define LC_VEDGE_ERRNO_SWITCH_ERROR      0x10000000
#define LC_VEDGE_ERRNO_ADD_SECU_ERROR    0x20000000
#define LC_VEDGE_ERRNO_DEL_SECU_ERROR    0x40000000

/* vl2 */
#define LC_VL2_ERRNO_TIMEOUT             0x00000001
#define LC_VL2_ERRNO_DB_ERROR            0x00000002
#define LC_VL2_ERRNO_POOL_MODE_ERROR     0x00000004
#define LC_VL2_ERRNO_NETWORK_MODE_ERROR  0x00000008
#define LC_VL2_ERRNO_NO_VLAN_ID          0x00000010
#define LC_VL2_ERRNO_VMWARE_ERROR        0x00000020
#define LC_VL2_ERRNO_AGEXEC_ERROR        0x00000040

/* VM Snapshot errno */
#define LC_SNAPSHOT_ERRNO_TIMEOUT        0x00000001
#define LC_SNAPSHOT_ERRNO_DB_ERROR       0x00000002
#define LC_SNAPSHOT_ERRNO_CREATE_ERROR   0x00000004
#define LC_SNAPSHOT_ERRNO_DELETE_ERROR   0x00000008
#define LC_SNAPSHOT_ERRNO_RECOVERY_ERROR 0x00000010

/* VM Backup errno */
#define LC_BACKUP_ERRNO_TIMEOUT          0x00000001
#define LC_BACKUP_ERRNO_DB_ERROR         0x00000002
#define LC_BACKUP_ERRNO_LOCAL_ERROR      0x00000004
#define LC_BACKUP_ERRNO_REMOTE_ERROR     0x00000008
#define LC_BACKUP_ERRNO_RECOVERY_ERROR   0x00000010
#define LC_BACKUP_ERRNO_DELETE_ERROR     0x00000020
#define LC_BACKUP_ERRNO_NO_DISK_ERROR    0x00000040

#endif