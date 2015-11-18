#ifndef _MESSAGE_MSG_VM_H
#define _MESSAGE_MSG_VM_H

#include "../lc_netcomp.h"

#define VM_CHANGE_MASK_DISK_ADD          0x00000001
#define VM_CHANGE_MASK_DISK_DEL          0x00000002
#define VM_CHANGE_MASK_DISK_SYS_RESIZE   0x00000004
#define VM_CHANGE_MASK_DISK_USER_RESIZE  0x00000008
#define VM_CHANGE_MASK_CPU_RESIZE        0x00000010
#define VM_CHANGE_MASK_MEM_RESIZE        0x00000020
#define VM_CHANGE_MASK_IP_MODIFY         0x00000040
#define VM_CHANGE_MASK_BW_MODIFY         0x00000080
#define VM_CHANGE_MASK_HA_DISK           0x00000100
#define VM_CHANGE_MASK_VL2_ID            0x00000200
#define VM_CHANGE_MASK_ALL               0x000000ff

struct msg_vm_opt {
    u32         vnet_id;
    u32         vl2_id;
    u32         vm_id;
    u32         vm_chgmask;
    char        vm_name[LC_NAMESIZE];
    char        vm_passwd[MAX_VM_PASSWD_LEN];
    char        data[0];
};

struct msg_vm_opt_start {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_server[MAX_HOST_ADDRESS_LEN];
    intf_core_t vm_intf;
    intf_core_t vm_ifaces[LC_VM_IFACES_MAX];
};

#define MAX_VM_HA_DISK              2

struct msg_disk_opt {
    u32         disk_num;
    u32         disk_id[MAX_VM_HA_DISK];
};

typedef struct msg_vm_ha_disk {
    u32          userdevice;
    u32          ha_disk_size;
} vm_ha_disk_t;

typedef struct msg_vm_opt_modify {
    u32          cpu;
    u32          mem;
    u32          disk_size;
    u32          vnetid;
    u32          vl2id;
    u32          ha_disk_num;
    char         ip[MAX_HOST_ADDRESS_LEN];
    vm_ha_disk_t ha_disks[0];
} vm_modify_t;

struct msg_vedge_opt {
    u32         vnet_id;
    char        data[0];
};
#define LC_VGATEWAY_IDS_LEN               1024
#define LC_VALVE_IDS_LEN                  LC_VGATEWAY_IDS_LEN

struct msg_vedge_opt_conf_secu {
    char        launch_server[MAX_HOST_ADDRESS_LEN];
    char        pub_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         pub_port;
    char        pub_mac[MAX_VIF_MAC_LEN];
    char        waf_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         waf_port;
    char        waf_mac[MAX_VIF_MAC_LEN];
    char        vnet_ipaddr[MAX_HOST_ADDRESS_LEN];
};

struct msg_vedge_opt_start {
    char        vm_uuid[MAX_VM_UUID_LEN];
    char        vm_server[MAX_HOST_ADDRESS_LEN];
    intf_core_t pub_intf[LC_PUB_VIF_MAX];
    intf_core_t sub_intf[LC_L2_MAX];
    intf_core_t ctrl_intf;
};

struct msg_vedge_opt_switch {
    u32         switch_mode;
};

struct msg_vl2_opt {
    u32         vnet_id;
    u32         vl2_id;
    u32         rack_id;
    u32         vlantag;
};

struct msg_vinterface_opt {
    u32         vinterface_id;
};

struct msg_vinterfaces_opt {
    char        vinterface_ids[MAX_VIF_IDS_LEN];
};

struct msg_vgateway_opt {
    int vnet_id;
    char gw_launch_server[MAX_HOST_ADDRESS_LEN];
    char wans[LC_VGATEWAY_IDS_LEN];
    char lans[LC_VGATEWAY_IDS_LEN];
};

struct msg_valve_opt {
    int vnet_id;
    char gw_launch_server[MAX_HOST_ADDRESS_LEN];
    char wans[LC_VALVE_IDS_LEN];
    char lans[LC_VALVE_IDS_LEN];
};

struct msg_vm_snapshot_opt {
    u32         snapshot_id;
    u32         vm_id;
};

struct msg_vm_recovery_opt {
    u32         snapshot_id;
};

struct msg_vm_backup_opt {
    u32         dbr_id;
};

struct msg_vm_migrate_opt {
    u32         vm_id;

    #define LC_VM_TYPE_VM   1
    #define LC_VM_TYPE_GW   2
    #define LC_VM_TYPE_VMWAF   3
    u32         vm_type;
    char        des_server[MAX_HOST_ADDRESS_LEN];
    char        des_sr_name[LC_NAMESIZE];
};

struct msg_vm_disk_opt {
    u32         disk_num;
    u32         disk_id[MAX_VM_HA_DISK];
};
#endif
