#ifndef __EXT_VL2_HANDLER_H__
#define __EXT_VL2_HANDLER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include "lc_db_net.h"
#include "vl2/ext_vl2_db.h"
#include "vl2/ext_vl2_framework.h"
#include "message/msg_vm.h"

// VL2 tables
#define WEB_SECU_CONF_TABLE  "vmwaf_web_server_conf"
#define PORT_MAP_TABLE       "vmwaf_port_map"

#define TABLE_TUNNEL_POLICY                         "tunnel_policy"
#define TABLE_TUNNEL_POLICY_COLUMN_VIFID            "vifid"
#define TABLE_TUNNEL_POLICY_COLUMN_VL2ID            "vl2id"
#define TABLE_TUNNEL_POLICY_COLUMN_VLANTAG          "vlantag"
#define TABLE_TUNNEL_POLICY_COLUMN_VIFMAC           "vifmac"
#define TABLE_TUNNEL_POLICY_COLUMN_UPLINK_IP        "uplink_ip"
#define TABLE_TUNNEL_POLICY_COLUMN_VDEVICETYPE      "vdevicetype"
#define TABLE_TUNNEL_POLICY_COLUMN_VDEVICEID        "vdeviceid"

#define TABLE_VL2                                   "vl2"
#define TABLE_VL2_COLUMN_ID                         "id"
#define TABLE_VL2_COLUMN_VLANID                     "vlan_id"
#define TABLE_VL2_COLUMN_VNETID                     "vnetid"
#define TABLE_VL2_COLUMN_NET_TYPE                   "net_type"

#define TABLE_VL2_VLAN                              "vl2_vlan"
#define TABLE_VL2_VLAN_COLUMN_VL2ID                 "vl2id"
#define TABLE_VL2_VLAN_COLUMN_RACKID                "rackid"
#define TABLE_VL2_VLAN_COLUMN_VLANTAG               "vlantag"

#define TABLE_VL2_NET                              "vl2_net"
#define TABLE_VL2_NET_COLUMN_ID                    "id"
#define TABLE_VL2_NET_COLUMN_NETMASK               "netmask"
#define TABLE_VL2_NET_COLUMN_PREFIX                "prefix"
#define TABLE_VL2_NET_COLUMN_NETINDEX              "net_index"
#define TABLE_VL2_NET_COLUMN_ISP                   "isp"
#define TABLE_VL2_NET_COLUMN_VL2ID                 "vl2id"

#define TABLE_COMPUTE_RESOURCE                      "compute_resource"
#define TABLE_COMPUTE_RESOURCE_COLUMN_UPLINK_IP     "uplink_ip"
#define TABLE_COMPUTE_RESOURCE_COLUMN_IP            "ip"
#define TABLE_COMPUTE_RESOURCE_COLUMN_RACKID        "rackid"
#define TABLE_COMPUTE_RESOURCE_COLUMN_DOMAIN        "domain"

#define TABLE_COMPUTE_RESOURCE_COLUMN_STATE_COMPLETE 4
#define TABLE_COMPUTE_RESOURCE_COLUMN_TYPE_NSP      3
#define TABLE_COMPUTE_RESOURCE_COLUMN_TYPE_XCP      1

#define TABLE_UPLINK_QOS                            "uplink_qos"
#define TABLE_UPLINK_QOS_COLUMN_UPLINK_IP           "uplink_ip"
#define TABLE_UPLINK_QOS_COLUMN_TUNNEL_MIN_RATE     "tunnel_min_rate"
#define TABLE_UPLINK_QOS_COLUMN_TUNNEL_MAX_RATE     "tunnel_max_rate"

#define TUNNEL_QOS_MIN_RATE_DEF  500
#define TUNNEL_QOS_MAX_RATE_DEF  700

#define TABLE_RACK_TUNNEL                           "rack_tunnel"
#define TABLE_RACK_TUNNEL_COLUMN_ID                 "id"
#define TABLE_RACK_TUNNEL_COLUMN_LOCAL_UPLINK_IP    "local_uplink_ip"
#define TABLE_RACK_TUNNEL_COLUMN_REMOTE_UPLINK_IP   "remote_uplink_ip"
#define TABLE_RACK_TUNNEL_COLUMN_SWITCH_IP          "switch_ip"

#define TABLE_IP_RESOURCE                           "ip_resource"
#define TABLE_IP_RESOURCE_COLUMN_VIFID              "vifid"
#define TABLE_IP_RESOURCE_COLUMN_IP                 "ip"
#define TABLE_IP_RESOURCE_COLUMN_MASK               "netmask"
#define TABLE_IP_RESOURCE_COLUMN_GATEWAY            "gateway"
#define TABLE_IP_RESOURCE_COLUMN_VIFID              "vifid"
#define TABLE_IP_RESOURCE_COLUMN_VDCID              "vdcid"
#define TABLE_IP_RESOURCE_COLUMN_IFINDEX            "ifid"
#define TABLE_IP_RESOURCE_COLUMN_ISPID              "isp"
#define TABLE_IP_RESOURCE_COLUMN_VLANTAG            "vlantag"

#define TABLE_VINTERFACE                            "vinterface"
#define TABLE_VINTERFACE_COLUMN_BANDW               "bandw"
#define TABLE_VINTERFACE_COLUMN_CEIL_BANDW          "ceil_bandw"
#define TABLE_VINTERFACE_COLUMN_BROADC_BANDW        "broadc_bandw"
#define TABLE_VINTERFACE_COLUMN_BROADC_CEIL_BANDW   "broadc_ceil_bandw"
#define TABLE_VINTERFACE_COLUMN_IFINDEX             "ifindex"
#define TABLE_VINTERFACE_COLUMN_SUBNETID            "subnetid"
#define TABLE_VINTERFACE_COLUMN_ID                  "id"
#define TABLE_VINTERFACE_COLUMN_VLANTAG             "vlantag"
#define TABLE_VINTERFACE_COLUMN_MAC                 "mac"
#define TABLE_VINTERFACE_COLUMN_DEVICETYPE          "devicetype"
#define TABLE_VINTERFACE_COLUMN_DEVICEID            "deviceid"
#define TABLE_VINTERFACE_COLUMN_IP                  "ip"
#define TABLE_VINTERFACE_COLUMN_GW                  "gateway"
#define TABLE_VINTERFACE_COLUMN_VL2ID               "subnetid"
#define TABLE_VINTERFACE_COLUMN_STATE               "state"
#define TABLE_VINTERFACE_COLUMN_IFTYPE              "iftype"
#define TABLE_VINTERFACE_COLUMN_LCUUID              "lcuuid"
#define TABLE_VINTERFACE_COLUMN_NETDEVICE_LCUUID    "netdevice_lcuuid"
#define TABLE_VINTERFACEIP                          "vinterface_ip"
#define TABLE_VINTERFACEIP_COLUMN_ID                "id"
#define TABLE_VINTERFACEIP_COLUMN_IP                "ip"
#define TABLE_VINTERFACEIP_COLUMN_ISP               "isp"
#define TABLE_VINTERFACEIP_COLUMN_NETMASK           "netmask"
#define TABLE_VINTERFACEIP_COLUMN_GATEWAY           "gateway"
#define TABLE_VINTERFACEIP_COLUMN_VL2ID             "vl2id"
#define TABLE_VINTERFACEIP_COLUMN_NETINDEX          "net_index"
#define TABLE_VINTERFACEIP_COLUMN_LCUUID            "lcuuid"
#define TABLE_VINTERFACEIP_COLUMN_VIFID             "vifid"
#define TABLE_VINTERFACEIP_COLUMN_SUBTYPE           "subtype"

#define IFTYPE_VGATEWAY_WAN     3
#define IFTYPE_VGATEWAY_LAN     4
#define DEVICETYPE_VM           1
#define DEVICETYPE_VGATEWAY     5
#define ROLE_VGATEWAY           7
#define ROLE_VALVE              11

#define VIF_DEVICE_TYPE_VM      1
#define VIF_DEVICE_TYPE_VGW     2
#define VIF_DEVICE_TYPE_HWDEV   3
#define VIF_DEVICE_TYPE_VGATEWAY   5
#define VIF_STATE_ATTACHED      1
#define VIF_TYPE_CTRL           1
#define VIF_TYPE_SERVICE        2
#define VIF_TYPE_WAN            3
#define VIF_TYPE_LAN            4
#define VIF_SUBTYPE_SERVICE_USER       0x1
#define VIF_SUBTYPE_SERVICE_PROVIDER   0x2
#define VIF_SUBTYPE_L2_INTERFACE       0x4



#define TABLE_VINTERFACE_IP                         "vinterface_ip"
#define TABLE_VINTERFACE_IP_COLUMN_ID               "id"
#define TABLE_VINTERFACE_IP_COLUMN_VIFID            "vifid"
#define TABLE_VINTERFACE_IP_COLUMN_IP               "ip"
#define TABLE_VINTERFACEIP_IP_COLUMN_NETMASK        "netmask"

#define TABLE_VL2_TUNNEL_POLICY                     "vl2_tunnel_policy"
#define TABLE_VL2_TUNNEL_POLICY_COLUMN_ID           "id"
#define TABLE_VL2_TUNNEL_POLICY_COLUMN_VL2ID        "vl2id"
#define TABLE_VL2_TUNNEL_POLICY_COLUMN_VLANTAG      "vlantag"
#define TABLE_VL2_TUNNEL_POLICY_COLUMN_UPLINK_IP    "uplink_ip"

#define TABLE_VNET                                  "vnet"
#define TABLE_VNET_COLUMN_ID                        "id"
#define TABLE_VNET_COLUMN_VDCID                     "vdcid"
#define TABLE_VNET_COLUMN_CTRLIFID                  "ctrl_ifid"
#define TABLE_VNET_COLUMN_LAUNCH_SERVER             "gw_launch_server"
#define TABLE_VNET_COLUMN_STATE                     "state"
#define TABLE_VNET_COLUMN_ROLE                      "role"
#define TABLE_VNET_COLUMN_SPECIFICATION             "specification"
#define TABLE_VNET_COLUMN_CONN_MAX                  "conn_max"
#define TABLE_VNET_COLUMN_NEW_CONN_PER_SEC          "new_conn_per_sec"

#define TABLE_VM                                    "vm"
#define TABLE_VM_COLUMN_ID                          "id"
#define TABLE_VM_COLUMN_LAUNCH_SERVER               "launch_server"
#define TABLE_VM_COLUMN_DOMAIN                      "domain"
#define TABLE_VM_COLUMN_LABEL                       "label"

#define TABLE_RACK                                  "rack"
#define TABLE_RACK_COLUMN_ID                        "id"
#define TABLE_RACK_COLUMN_TORSWITCH_TYPE            "switch_type"
#define TABLE_RACK_COLUMN_DOMAIN                    "domain"

enum {
    TORSWITCH_TYPE_ETHERNET = 0,
    TORSWITCH_TYPE_OPENFLOW,
    TORSWITCH_TYPE_TUNNEL,
};

enum {
    NETWORK_DEVICE_TYPE_VDEV = 0x1000,
    NETWORK_DEVICE_TYPE_VDEV_WAN = 0x1001,
    NETWORK_DEVICE_TYPE_VDEV_LAN = 0x1002,
    NETWORK_DEVICE_TYPE_VDEV_CONTROL = 0x1004,
    NETWORK_DEVICE_TYPE_VDEV_SERVICE = 0x1008,

    NETWORK_DEVICE_TYPE_HWDEV = 0x2000,
    NETWORK_DEVICE_TYPE_HWDEV_WAN  = 0x2001,
    NETWORK_DEVICE_TYPE_HWDEV_LAN = 0x2002,
    NETWORK_DEVICE_TYPE_HWDEV_CONTROL = 0x2004,
    NETWORK_DEVICE_TYPE_HWDEV_SERVICE = 0x2008
};

#define TABLE_HOST_DEVICE                           "host_device"
#define TABLE_HOST_DEVICE_COLUMN_TYPE               "type"
#define TABLE_HOST_DEVICE_COLUMN_IP                 "ip"
#define TABLE_HOST_DEVICE_COLUMN_UPLINK_IP          "uplink_ip"
#define TABLE_HOST_DEVICE_COLUMN_UPLINK_MASK        "uplink_netmask"
#define TABLE_HOST_DEVICE_COLUMN_UPLINK_GATEWAY     "uplink_gateway"
#define TABLE_HOST_DEVICE_COLUMN_PASSWD             "user_passwd"

#define TABLE_HOST_DEVICE_COLUMN_TYPE_VM     1
#define TABLE_HOST_DEVICE_COLUMN_TYPE_VGW    2

#define TABLE_HOST_PORT_CONFIG                      "host_port_config"
#define TABLE_HOST_PORT_CONFIG_COLUMN_NAME          "name"
#define TABLE_HOST_PORT_CONFIG_COLUMN_HOST_IP       "host_ip"
#define TABLE_HOST_PORT_CONFIG_COLUMN_TORSWITCH_PORT "torswitch_port"
#define TABLE_HOST_PORT_CONFIG_COLUMN_TORSWITCH_ID  "torswitch_id"
#define TABLE_HOST_PORT_CONFIG_COLUMN_TYPE          "type"

#define HOST_PORT_CONFIG_TYPE_CTRL  0
#define HOST_PORT_CONFIG_TYPE_DATA  1

#define TABLE_NETWORK_DEVICE                        "network_device"
#define TABLE_NETWORK_DEVICE_COLUMN_ID              "id"
#define TABLE_NETWORK_DEVICE_COLUMN_MIP             "mip"
#define TABLE_NETWORK_DEVICE_COLUMN_RACKID          "rackid"
#define TABLE_NETWORK_DEVICE_COLUMN_PUBLIC_PORT0    "uplink_port0"
#define TABLE_NETWORK_DEVICE_COLUMN_PUBLIC_PORT1    "uplink_port1"
#define TABLE_NETWORK_DEVICE_COLUMN_CTRL_PORT_LIST  "crl_plane_port_list"
#define TABLE_NETWORK_DEVICE_COLUMN_DATA_PORT_LIST  "data_plane_port_list"
#define TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_IP       "tunnel_ip"
#define TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_NETMASK  "tunnel_netmask"
#define TABLE_NETWORK_DEVICE_COLUMN_TUNNEL_GATEWAY  "tunnel_gateway"
#define TABLE_NETWORK_DEVICE_COLUMN_USERNAME        "username"
#define TABLE_NETWORK_DEVICE_COLUMN_PASSWD          "password"
#define TABLE_NETWORK_DEVICE_COLUMN_BRAND           "brand"
#define TABLE_NETWORK_DEVICE_COLUMN_BOOT_TIME       "boot_time"
#define TABLE_NETWORK_DEVICE_COLUMN_DOMAIN          "domain"
#define TABLE_NETWORK_DEVICE_COLUMN_LCUUID          "lcuuid"
#define TABLE_NETWORK_DEVICE_COLUMN_ENABLE          "enable"

#define OFS_MAX_PUBLIC_PORT     2

#define TABLE_SYS_CONFIGURATION                     "sys_configuration"
#define TABLE_SYS_CONFIGURATION_COLUMN_PARAM_NAME   "param_name"
#define TABLE_SYS_CONFIGURATION_COLUMN_VALUE        "value"

#define TABLE_DOMAIN_CONFIGURATION                  "domain_configuration"
#define TABLE_DOMAIN_CONFIGURATION_COLUMN_PARAM_NAME "param_name"
#define TABLE_DOMAIN_CONFIGURATION_COLUMN_VALUE     "value"
#define TABLE_DOMAIN_CONFIGURATION_COLUMN_DOMAIN    "domain"

#define TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_VLAN       "ctrl_plane_vlan"
#define TABLE_DOMAIN_CONFIGURATION_OFS_SERV_VLAN       "serv_plane_vlan"
#define TABLE_DOMAIN_CONFIGURATION_OFS_CTRL_BANDWIDTH  "ctrl_plane_bandwidth"
#define TABLE_DOMAIN_CONFIGURATION_OFS_SERV_BANDWIDTH  "serv_plane_bandwidth"
#define TABLE_DOMAIN_CONFIGURATION_TUNNEL_PROTOCOL     "tunnel_protocol"
#define TABLE_DOMAIN_CONFIGURATION_VM_CTRL_IP_MIN      "vm_ctrl_ip_min"
#define TABLE_DOMAIN_CONFIGURATION_VM_CTRL_IP_MAX      "vm_ctrl_ip_max"
#define TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MIN "controller_ctrl_ip_min"
#define TABLE_DOMAIN_CONFIGURATION_CTRLLER_CTRL_IP_MAX "controller_ctrl_ip_max"
#define TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MIN      "service_provider_ip_min"
#define TABLE_DOMAIN_CONFIGURATION_SERVICE_IP_MAX      "service_provider_ip_max"
#define TABLE_DOMAIN_CONFIGURATION_SERVICE_VM_IP_MIN   "vm_service_ip_min"
#define TABLE_DOMAIN_CONFIGURATION_SERVICE_VM_IP_MAX   "vm_service_ip_max"

#define TABLE_THIRD_PARTY_DEVICE                    "third_party_device"
#define TABLE_THIRD_PARTY_DEVICE_COLUMN_RACK_NAME   "rack_name"

// VL2 table entries
#define VL2_NETS_VI_ENTRY  "view_id"
#define VL2_NETS_NI_ENTRY  "network_id"
#define VL2_NETS_PN_ENTRY  "port_number"
#define VL2_POTS_VI_ENTRY  VL2_NETS_VI_ENTRY
#define VL2_POTS_NI_ENTRY  VL2_NETS_NI_ENTRY
#define VL2_POTS_PI_ENTRY  "port_id"
#define VL2_POTS_PS_ENTRY  "port_status"
#define VL2_POTS_ID_ENTRY  "interface_id"
#define VL2_POTS_SP_ENTRY  "server_ip"
#define VL2_POTS_TD_ENTRY  "tunnel_id"
#define VL2_POTS_VD_ENTRY  "vlan_id"

// The constant with maximum length corresponding to each field
// of the table entry
#define VL2_MAX_NETS_VI_VALUE  "4294967295"
#define VL2_MAX_NETS_NI_VALUE  "4294967295"
#define VL2_MAX_NETS_PN_VALUE  "4294967295"
#define VL2_MAX_POTS_VI_VALUE  VL2_MAX_NETS_VI_VALUE
#define VL2_MAX_POTS_NI_VALUE  VL2_MAX_NETS_NI_VALUE
#define VL2_MAX_POTS_PI_VALUE  "4294967295"
#define VL2_MAX_POTS_PS_VALUE  "1"
#define VL2_MAX_POTS_ID_VALUE  "4294967295"
#define VL2_MAX_POTS_SP_VALUE  "255.255.255.255"
#define VL2_MAX_POTS_TD_VALUE  VL2_MAX_NETS_VI_VALUE
#define VL2_MAX_POTS_VD_VALUE  VL2_MAX_NETS_NI_VALUE

// The predefined constant for specially chosen fields
// DEF-->Default, ACT-->Active, ANY-->Don't care
#define VL2_DEF_POTS_PS_VALUE  "0"
#define VL2_ACT_POTS_PS_VALUE  "1"
#define VL2_DEF_POTS_TD_VALUE  "0"
#define VL2_ANY_POTS_TD_VALUE  "tunnel_id is not null"

// Maximum length for each fields of the table entry
#define VL2_MAX_NETS_VI_LEN  ( sizeof(VL2_MAX_NETS_VI_VALUE) )
#define VL2_MAX_NETS_NI_LEN  ( sizeof(VL2_MAX_NETS_NI_VALUE) )
#define VL2_MAX_NETS_PN_LEN  ( sizeof(VL2_MAX_NETS_PN_VALUE) )
#define VL2_MAX_POTS_VI_LEN  ( sizeof(VL2_MAX_POTS_VI_VALUE) )
#define VL2_MAX_POTS_NI_LEN  ( sizeof(VL2_MAX_POTS_NI_VALUE) )
#define VL2_MAX_POTS_PI_LEN  ( sizeof(VL2_MAX_POTS_PI_VALUE) )
#define VL2_MAX_POTS_PS_LEN  ( sizeof(VL2_MAX_POTS_PS_VALUE) )
#define VL2_MAX_POTS_ID_LEN  ( sizeof(VL2_MAX_POTS_ID_VALUE) )
#define VL2_MAX_POTS_SP_LEN  ( sizeof(VL2_MAX_POTS_SP_VALUE) )
#define VL2_MAX_POTS_TD_LEN  ( sizeof(VL2_MAX_POTS_TD_VALUE) )
#define VL2_MAX_POTS_VD_LEN  ( sizeof(VL2_MAX_POTS_VD_VALUE) )
#define VL2_MAX_DB_BUF_LEN   512

// Maximum length for all combinations
// *VL-->value_list, CD-->conditions, OP-->operations, EP-->extend_ptr
#define VL2_MAX_APP_VL_LEN  ( sizeof(VL2_POTS_11110000_VL_FORMAT) \
    + VL2_MAX_POTS_VI_LEN + VL2_MAX_POTS_NI_LEN + VL2_MAX_POTS_PI_LEN \
    + VL2_MAX_POTS_PS_LEN )
#define VL2_MAX_APP_CD_LEN  ( sizeof(VL2_POTS_11110000_CD_FORMAT) \
    + VL2_MAX_POTS_VI_LEN + VL2_MAX_POTS_NI_LEN + VL2_MAX_POTS_PI_LEN \
    + VL2_MAX_POTS_PS_LEN )
#define VL2_MAX_APP_OP_LEN  ( sizeof(VL2_POTS_00011111_OP_FORMAT) \
    + VL2_MAX_POTS_PS_LEN + VL2_MAX_POTS_ID_LEN + VL2_MAX_POTS_SP_LEN \
    + VL2_MAX_POTS_TD_LEN + VL2_MAX_POTS_VD_LEN )
#define VL2_MAX_APP_EP_LEN  ( VL2_MAX_POTS_SP_LEN )

// The combination of table fields for ports(_version)
// CL-->Column, VL-->Value, CD-->Condition, OP-->Operation
// 1st bit-->view_id, 2nd bit-->network_id, 3rd bit-->port_id,
// 4th bit-->port_status, 5th bit-->interface_id,
// 6th bit-->server_ip, 7th bit-->tunnel_id, 8th bit-->vlan_id
// 1-->Choose corresponding entry value, 0-->Ignore corresponding entry value
// X-->Choose the opponent of corresponding entry value
#define VL2_POTS_11110000_CL_FORMAT  "view_id,network_id,port_id,port_status"
#define VL2_POTS_11110000_VL_FORMAT  "\"%s\",\"%s\",\"%s\",\"%s\""
#define VL2_POTS_11000000_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\""
#define VL2_POTS_11100000_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\" " \
                                     "and port_id=\"%s\""
#define VL2_POTS_11110000_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\" " \
                                     "and port_id=\"%s\" and port_status=\"%s\""
#define VL2_POTS_11010000_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\" " \
                                     "and port_status=\"%s\""
#define VL2_POTS_11000100_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\" " \
                                     "and server_ip=\"%s\""
#define VL2_POTS_11000X00_CD_FORMAT  "view_id=\"%s\" and network_id=\"%s\" " \
                                     "and server_ip!=\"%s\""
#define VL2_POTS_00000110_CD_FORMAT  "server_ip=\"%s\" and tunnel_id=\"%s\""
#define VL2_POTS_00001000_CD_FORMAT  "interface_id=\"%s\""
#define VL2_POTS_00000100_CD_FORMAT  "server_ip=\"%s\""
#define VL2_POTS_00000010_CD_FORMAT  "tunnel_id=\"%s\""
#define VL2_POTS_00011111_OP_FORMAT  "port_status=\"%s\",interface_id=\"%s\"," \
                                     "server_ip=\"%s\",tunnel_id=\"%s\",vlan_id=\"%s\""
#define VL2_POTS_000XXXXX_OP_FORMAT  "port_status=\"%s\",interface_id=%s," \
                                     "server_ip=%s,tunnel_id=%s,vlan_id=%s"

// For the maximum number of servers where a single subnet may distributes
#define VL2_MAX_SES_NUM  65536
// For the maximum number of VMs where a single server may distributes
#define VL2_MAX_LCGS_NUM 4
#define VL2_MAX_VGWS_NUM 8
#define VL2_MAX_RACKS_NUM 8
#define VL2_MAX_VIF_NUM  128
#define VNET_MAX_VL2_NUM 5
#define VIF_MAX_IP_NUM   128
#define VMWAF_MAX_WEB_SERV_NUM   256
#define RACK_MAX_VIF     8192
#define RACK_MAX_VL2     256
#define RACK_MAX_SERVER_NUM 32
#define RACK_MAX_VLAN_NUM  4096
#define SERVER_MAX_VM_NUM  200
#define HOST_PASSWD_MAX_LEN 64
#define HOST_PORT_NAME_MAX_LEN 64
#define HOST_PORT_DATA_TYPE     1
#define OFS_MAX_PORT_LIST_LEN   512
#define HOST_MAX_PORT_NUM       8
#define ADD_CALL_TYPE           1
#define DEL_CALL_TYPE           2
#define PORT_ID_LEN             16
#define ONE_TRILLION_BANDWIDTH  1048576
#define TUNNEL_PROTOCOL_MAX_LEN 16
#define SWITCH_USER_NAME_PASSWD_MAX_LEN   64
#define LCUUID_LEN                        64
#define RACK_MAX_SWITCH_NUM 2
#define VL2_CMD_RESULT_SIZE     4096
#define TORSWITCH_MAX_RACKTUNNEL_NUM 64
#define TORSWITCH_MAX_IP_SEG    2048
#define LC_VALVE_SPEC_LEN       256

// For padding parameters
#define PADDING          "0"

#define AUTH_FLAG_SRC_IP_SPOOF     0x1

typedef struct attr_s {
    char view_id[VL2_MAX_POTS_VI_LEN];
    char network_id[VL2_MAX_POTS_NI_LEN];
    char port_id[VL2_MAX_POTS_PI_LEN];
    char port_status[VL2_MAX_POTS_PS_LEN];
    char interface_id[VL2_MAX_POTS_ID_LEN];
    char server_ip[VL2_MAX_POTS_SP_LEN];
    char tunnel_id[VL2_MAX_POTS_TD_LEN];
    char vlan_id[VL2_MAX_POTS_VD_LEN];
} attr_t;

typedef struct torswitch_s {
    int id;
    int rackid;
    char name[SWITCH_USER_NAME_PASSWD_MAX_LEN];
    char ip[VL2_MAX_INTERF_NW_LEN];
    char username[SWITCH_USER_NAME_PASSWD_MAX_LEN];
    char passwd[SWITCH_USER_NAME_PASSWD_MAX_LEN];
    char enable[SWITCH_USER_NAME_PASSWD_MAX_LEN];
    char brand[SWITCH_USER_NAME_PASSWD_MAX_LEN];
    char tunnel_ip[VL2_MAX_INTERF_NW_LEN];
    char tunnel_netmask[VL2_MAX_INTERF_NW_LEN];
    char tunnel_gateway[VL2_MAX_INTERF_NW_LEN];
    uint64_t boot_time;
    int public_port[OFS_MAX_PUBLIC_PORT];
    char domain[LCUUID_LEN];
    char lcuuid[LCUUID_LEN];
} torswitch_t;

typedef struct rack_network_s {
    char id[VL2_MAX_RACK_ID_LEN];
    int torswitch_type;
    torswitch_t torswitch[RACK_MAX_SWITCH_NUM];
    char gw_hosts[VL2_MAX_LCGS_NUM][VL2_MAX_INTERF_NW_LEN];
} rack_network_t;

typedef struct rack_tunnel_s {
    char local_uplink_ip[VL2_MAX_INTERF_NW_LEN];
    char remote_uplink_ip[VL2_MAX_INTERF_NW_LEN];
} rack_tunnel_t;

enum API_METHOD_CODES {
    API_METHOD_POST = 0,
    API_METHOD_PUT,
    API_METHOD_DEL,
    API_METHOD_GET,
    API_METHOD_PATCH
};

struct msg_torswitch_opt {
    char ip[VL2_MAX_INTERF_NW_LEN];
    int state;
};

// Funcvtions open for outer call
int config_gateway_network(host_t *compute_resource);
int config_gateway_uplink(host_t *compute_resource);

int enable_isolate(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *server_ip,
    uint32_t br_id);
int disable_isolate(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *server_ip,
    uint32_t br_id);
int enable_auth(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *if_index,
    char *server_ip,
    uint32_t br_id,
    uint32_t flag,
    int devicetype,
    int switchport);
int disable_auth(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *server_ip,
    uint32_t br_id,
    int devicetype,
    int switchport);
int enable_vlan(
    char *interface_id,
    char *server_ip,
    char *vlan_id,
    int devicetype,
    int switchport);
int disable_vlan(
    char *interface_id,
    char *server_ip,
    int devicetype,
    int switchport);
int enable_qos(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *server_ip,
    char *bandwidth,
    int devicetype,
    int switchport);
int disable_qos(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id,
    char *server_ip,
    int devicetype,
    int switchport);
int add_vl2_tunnel_policies(char *interface_id);
int del_vl2_tunnel_policies(char *interface_id);
int get_rack_of_vdevice(char *vdevice_id, bool is_vgw, char *rackid);
int get_rack_of_hwdev(char *hwdev_id, char *rackid);

int release_tunnel_policies(
    char *network_id,
    char *interface_id,
    char *server_ip,
    int mark);
int release_isolate_policies(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id);
int release_auth_policies(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id);
int release_vlan_policies(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id);
int release_qos_policies(char *view_id,
    char *network_id,
    char *port_id,
    char *interface_id);
int config_vm_vif_to_switch(char *switch_ip, int switch_id, char *launch_server, int vmid, int vl2id, int call_type);
int pack_vl2_ryu_post(char *buf, int buf_len,
                             int vl2_id,
                             int local_vlan_id);

int pack_vgateway_update(char *buf, int buf_len,
                     char *router_id,
                     int wan_count,
                     vinterface_info_t *wansinfo,
                     int lan_count,
                     vinterface_info_t *lansinfo,
                     int conn_max,
                     int new_conn_per_sec,
                     int role);
int pack_vgateway_wan_update(char *buf, int buf_len,
                             vinterface_info_t *wansinfo);
int pack_vgateway_lan_update(char *buf, int buf_len,
                             vinterface_info_t *lansinfo);
#endif /* __EXT_VL2_HANDLER_H__ */
