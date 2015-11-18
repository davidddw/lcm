#ifndef _LC_NETCOMPONENT_H
#define _LC_NETCOMPONENT_H

#include <sys/types.h>
#include <sys/socket.h>

#include "framework/types.h"

#define LC_NAMESIZE                        80
#define LC_NAMEDES                         256
#define LC_IMAGEPATH                       256
#define LC_UUID_LEN                        64
#define LC_DB_EX_HOST_LEN                  128
#define MAX_GW_ADDR_LEN                    16
#define MAX_VIF_MAC_LEN                    18
#define MAX_VM_ADDRESS_LEN                 20
#define MAX_HOST_ADDRESS_LEN               20
#define MAX_VIF_NAME_LEN                   16
#define MAX_VM_UUID_LEN                    64
#define LCM_MGM_SOCKPATH                   "/tmp/.lcm_mgm_unix"
#define LCM_APP_SOCKPATH                   "/tmp/.lcm_app_unix"
#define MAX_ID_LEN                         16
#define LC_HA_DISK_NUM                     1
#define MAX_VM_PASSWD_LEN                  16
#define MAX_VIF_PER_HOST                   1600
#define MAX_UINT_VALUE                     "4294967295"
#define MAX_UINT_VALUE_LEN                 10
#define MAX_VIF_IDS_LEN                    MAX_VIF_PER_HOST * MAX_UINT_VALUE_LEN

#define OVS_INT_PATCH_PORT      "patch-tunl-data"

#define LC_DAY_SEC_NUM                     24*60*60

#define TABLE_VNET_FLAG_SWITCH             0x400
#define TABLE_VNET_STATE_TEMP              0
#define TABLE_VNET_STATE_EXCEPTION         3
#define TABLE_VNET_STATE_RUNNING           7
#define TABLE_VM_STATE_TEMP                0
#define TABLE_VM_STATE_RUNNING             4

#define TABLE_VNET_SWITCH_MODE_AUTO        1
#define TABLE_VNET_SWITCH_MODE_MANUAL      2
#define TABLE_VNET_HASTATE_MASTER          1
#define TABLE_VNET_HASTATE_BACKUP          2
#define TABLE_VNET_HASTATE_FAULT           3

enum {
    SE_OS_TYPE_BASE = 0,
    SE_OS_TYPE_WIN7,
    SE_OS_TYPE_FC,
    SE_OS_TYPE_UBUNTU,
}SE_OS_TYPE;

#define LC_VM_IFACE_ACL_MAX   8
#define LC_PUB_VIF_MAX  3
#define LC_L2_MAX  3
#define LC_CTRL_VIF_MAX 1
#define LC_VM_IFACE_CSTM 3
#define LC_VM_IFACE_MGMT 4
#define LC_VM_IFACE_SERV 5
#define LC_VM_IFACE_CTRL 6
#define LC_VM_IFACES_MAX 7
typedef struct {
    char    import[LC_IMAGEPATH];
    char    svrip[LC_NAMESIZE];
    char    svruser[LC_NAMESIZE];
    char    svrpwd[LC_NAMESIZE];
    u32     rack;
    u32     slot;
    u32     switchport;
}launchvm_t;

typedef struct {
    char    import[LC_IMAGEPATH];
    u32     rack;
    u32     slot;
    struct {
        char  ifname[LC_NAMESIZE];
        u32   switchport;
    }infs[LC_L2_MAX];
}launch_t;

typedef struct {
    struct memory {
        u16     msize;
        u16     gsize;
    }m;
    struct disk {
        u16     type;
        u16     reserve;
        u16     gsize;
        u16     tsize;
    }d;
}storage_t;

typedef struct {
    u16     type;
    u16     corenum;
}cpu_t;

typedef struct {
    char                    name[LC_NAMESIZE];
    struct ether_addr       mac;
    struct ipv4_addr        v4addr;
    struct ipv4_addr        gwaddr;
    u8                      masklen;
    u8                      dhcp;
    u8                      attnetid;
    u8                      ofport;
    u16                     bindvlan;
    u32                     status;
    u64                     bandwidth;
}interface_t;

/*
 VM OS Type

 Windows 7                      10001
 Windows server2008             10002
 Fedora 14                      20001
 CentOS 5.5                     20002
 SUSE 11                        20003
 Ubuntu 11.10 32bits            20004
 Ubuntu Server 11.10 64bits     20005
*/

typedef struct {
    u32         vm_id;
    char        vmname[LC_NAMESIZE];
    char        vmtemplate[LC_NAMESIZE];
    char        launch_server[MAX_HOST_ADDRESS_LEN];
    #define LC_VM_FLAGS_FROM_TEMPLATE   0x0001
    u16         flags;
    u16         reserve;
    u16         state;
    u16         os;
    storage_t   storage;
    cpu_t       cpu;
    interface_t intf;
    launchvm_t  linfo;
    u32         vnetid;
    u32         vl2id;
}vm_t;

typedef struct {
    u32         id;
#define LC_VL2_STATE_TEMP         0
#define LC_VL2_STATE_CREATING     1
#define LC_VL2_STATE_FINISH       2
#define LC_VL2_STATE_ABNORMAL     3
#define LC_VL2_STATE_MODIFY       4
#define LC_VL2_STATE_DELETING     5
#define LC_VL2_STATE_DELETED      6
    u32         state;
#define LC_VL2_NET_TYPE_OVS       1
#define LC_VL2_NET_TYPE_VMPG      2
    u32         err;
#define LC_VL2_NET_TYPE_CTRL      1
    u32         net_type;
    u32         vnetid;
    u32         vcloudid;
    u32         vlan_request;
    u32         portnum;
    char        name[LC_NAMESIZE];
    char        desc[LC_NAMEDES];
    char        pg_id[LC_NAMESIZE];
} vl2_t;

typedef struct {
    u32         id;
    u32         vl2id;
    u32         rackid;
    u32         vlantag;
} vl2_vlan_t;

typedef struct {
    u8          enable;
    u8          intfsnum;
    #define VEDGE_FLAGS_IMPORT   0x0001
    u16         flags;
    interface_t pub_intf[LC_PUB_VIF_MAX];
    interface_t intfs[LC_L2_MAX];
    launchvm_t  linfo;
}vedge_t;

typedef struct intf_s {
    u32        if_id;
    char       if_name[MAX_VIF_NAME_LEN];
    u32        if_index;
#define LC_VIF_STATE_ATTACHED            1
#define LC_VIF_STATE_DETACHED            2
#define LC_VIF_STATE_ABNORMAL            3
#define LC_VIF_STATE_ATTACHING           4
#define LC_VIF_STATE_DETACHING           5
    u32        if_state;
    u32        if_flag;
    u32        if_errno;
    u32        if_eventid;
    char       if_ip[MAX_VM_ADDRESS_LEN];
    char       if_netmask[MAX_VM_ADDRESS_LEN];
#define LC_VIF_IFTYPE_CTRL               1
#define LC_VIF_IFTYPE_SERVICE            2
#define LC_VIF_IFTYPE_WAN                3
#define LC_VIF_IFTYPE_LAN                4
    u32        if_type;
    u32        subtype;
    char       if_gateway[MAX_VM_ADDRESS_LEN];
    char       if_mac[MAX_VIF_MAC_LEN];
    u32        if_mask;
    u64        if_bandw;
    u32        if_port;
    u32        if_subnetid;
    u32        if_vlan;
#define LC_VIF_DEVICE_TYPE_VGATEWAY      5
#define LC_VIF_DEVICE_TYPE_HWDEV         3
#define LC_VIF_DEVICE_TYPE_GW            2
#define LC_VIF_DEVICE_TYPE_VM            1
    u32        if_devicetype;
    u32        if_deviceid;
    char       if_portid[LC_NAMESIZE];
    char       if_pgid[LC_NAMESIZE];
    char       if_netdevice_lcuuid[LC_UUID_LEN];
    char       if_domain[LC_NAMESIZE];
    char       if_lcuuid[LC_UUID_LEN];
} intf_t;

typedef struct intf_core_s {
    u32        if_id;
    u32        if_state;
    u64        if_bandw;
    u32        if_subnetid;
    u32        if_vlan;
    u32        if_index;
} intf_core_t;

typedef struct intf_ip_s {
    char ip[MAX_VM_ADDRESS_LEN];
    char netmask[MAX_VM_ADDRESS_LEN];
} intf_ip_t;

static inline int copy_intf_to_intf_core(
        intf_core_t *dst, const intf_t *src, int n_intf)
{
    int i;
    if (!src || !dst) {
        return -1;
    }
    for (i = 0; i < n_intf; ++i, ++src, ++dst) {
       dst->if_id = src->if_id;
       dst->if_state = src->if_state;
       dst->if_bandw = src->if_bandw;
       dst->if_subnetid = src->if_subnetid;
       dst->if_vlan  = src->if_vlan;
       dst->if_index = src->if_index;
    }
    return 0;
}

typedef struct {
    u32         vnet_id;
    char        opaque_id[LC_NAMESIZE];
    char        vnet_name[LC_NAMESIZE];
    char        launch_server[MAX_HOST_ADDRESS_LEN];
    u32         vl2num;
    u32         poolid;
    intf_core_t pub_intf[LC_PUB_VIF_MAX];
    intf_core_t pvt_intf[LC_L2_MAX];
    char        lcuuid[LC_UUID_LEN];
    u32         role;
}vnet_t;

typedef struct {
    char        svrip[LC_NAMESIZE];
    char        svruser[LC_NAMESIZE];
    char        svrpwd[LC_NAMESIZE];
}svr_t;

struct msg_hwdev_opt {
    u32         vnet_id;
    u32         vl2_id;
    u32         vl2_type;
    u32         hwdev_id;
    char        data[0];
};

struct msg_hwdev_opt_start {
    u32         switch_id;
    u32         switch_port;
    intf_core_t hwdev_iface;
};

#define IP_ADDR_WIDTH 32
/* In theory, PREFIX_MAX is 2*IP_ADDR_WIDTH-2 */
#define PREFIX_MAX 62
typedef struct ip_prefix_s {
    char address[16];
    char mask[16];
} ip_prefix_t;

#endif /* _LC_NETCOMPONENT_H */
