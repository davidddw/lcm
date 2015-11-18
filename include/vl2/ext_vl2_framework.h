#ifndef __EXT_VL2_FRAMEWORK_H__
#define __EXT_VL2_FRAMEWORK_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include "lc_agexec_msg.h"
#include "vl2/ext_vl2_db.h"

// Shared policy and resource tables
#define VL2_POLICY_TABLE  "policy"
#define VL2_INTERF_TABLE  "vinterface"
#define VL2_NETSRC_TABLE  "network_resource"

// VL2 table entries
#define VL2_POLICY_ID_ENTRY  "id"
#define VL2_POLICY_TE_ENTRY  "type"
#define VL2_POLICY_GN_ENTRY  "grain"
#define VL2_POLICY_OT_ENTRY  "object"
#define VL2_POLICY_CT_ENTRY  "component"
#define VL2_POLICY_AP_ENTRY  "app"
#define VL2_POLICY_GP_ENTRY  "pgroup"
#define VL2_POLICY_IX_ENTRY  "pindex"
#define VL2_POLICY_ST_ENTRY  "socket"
#define VL2_POLICY_AE_ENTRY  "attribute"
#define VL2_POLICY_CN_ENTRY  "content"
#define VL2_POLICY_SE_ENTRY  "state"
#define VL2_POLICY_DN_ENTRY  "description"
#define VL2_INTERF_ID_ENTRY  "id"
#define VL2_INTERF_NE_ENTRY  "name"
#define VL2_INTERF_PH_ENTRY  "ofport"
#define VL2_INTERF_DL_ENTRY  "mac"
#define VL2_INTERF_NW_ENTRY  "ip"
#define VL2_INTERF_NK_ENTRY  "netmask"
#define VL2_INTERF_VD_ENTRY  "vlantag"
#define VL2_INTERF_BW_ENTRY  "bandw"
#define VL2_INTERF_VI_ENTRY  "device_id"
#define VL2_INTERF_NI_ENTRY  "subnet_id"
#define VL2_NETSRC_TS_ENTRY  "int_patch_port"
#define VL2_NETSRC_TP_ENTRY  "tunnel_ip"
#define VL2_TUNPOL_ID_ENTRY  "id"
#define VL2_TUNPOL_FI_ENTRY  "vifid"
#define VL2_TUNPOL_UP_ENTRY  "uplink_ip"
#define VL2_TUNPOL_DT_ENTRY  "vdevicetype"
#define VL2_TUNPOL_DI_ENTRY  "vdeviceid"
#define VL2_TUNPOL_VI_ENTRY  "vl2id"
#define VL2_TUNPOL_VT_ENTRY  "vlantag"
#define VL2_TUNPOL_FC_ENTRY  "vifmac"
#define VL2_RACTUN_LU_ENTRY  "local_uplink_ip"
#define VL2_RACTUN_RU_ENTRY  "remote_uplink_ip"
#define VL2_NETDEC_ID_ENTRY  "id"
#define VL2_NETDEC_RI_ENTRY  "rackid"
#define VL2_NETDEC_IP_ENTRY  "mip"
#define VL2_NETDEC_BR_ENTRY  "brand"
#define VL2_NETDEC_TI_ENTRY  "tunnel_ip"
#define VL2_NETDEC_TN_ENTRY  "tunnel_netmask"
#define VL2_NETDEC_TG_ENTRY  "tunnel_gateway"
#define VL2_NETDEC_UN_ENTRY  "username"
#define VL2_NETDEC_PW_ENTRY  "password"
#define VL2_NETDEC_EN_ENTRY  "enable"
#define VL2_NETDEC_BT_ENTRY  "boot_time"
#define VL2_NETDEC_UUID_ENTRY "lcuuid"
#define VL2_NETDEC_DOMAIN_ENTRY "domain"


// The constant with maximum length corresponding to each field
// of the table entry
#define VL2_MAX_POLICY_ID_VALUE  "4294967295"
#define VL2_MAX_POLICY_TE_VALUE  "2"
#define VL2_MAX_POLICY_GN_VALUE  "3"
#define VL2_MAX_POLICY_AP_VALUE  "255"
#define VL2_MAX_POLICY_GP_VALUE  "255"
#define VL2_MAX_POLICY_IX_VALUE  "4294967295"
#define VL2_MAX_POLICY_ST_VALUE  "tcp:255.255.255.255:65535"
#define VL2_MAX_POLICY_SE_VALUE  "-255"
#define VL2_MAX_INTERF_ID_VALUE  "4294967295"
#define VL2_MAX_INTERF_NE_VALUE  "\"vif4294967295.4\""
#define VL2_MAX_INTERF_PH_VALUE  "4294967295"
#define VL2_MAX_INTERF_DL_VALUE  "FF:FF:FF:FF:FF:FF"
#define VL2_MAX_INTERF_NW_VALUE  "255.255.255.255"
#define VL2_MAX_INTERF_VD_VALUE  "4294967295"
#define VL2_MAX_INTERF_VI_VALUE  "4294967295"
#define VL2_MAX_INTERF_NI_VALUE  "4294967295"
#define VL2_MAX_INTERF_ST_VALUE  "4294967295"
#define VL2_MAX_NETSRC_TS_VALUE  "4294967295"
#define VL2_MAX_NETSRC_TP_VALUE  "255.255.255.255"
#define VL2_MAX_COOKIE_VALUE     "0x0123456789ABCDEF"
#define VL2_MAX_RACKS            256

#define VL2_ANY_POLICY_ID_VALUE  "id is not null"

// Maximum length for each fields of the table entry
#define VL2_MAX_POLICY_ID_LEN  ( sizeof(VL2_MAX_POLICY_ID_VALUE) )
#define VL2_MAX_POLICY_TE_LEN  ( sizeof(VL2_MAX_POLICY_TE_VALUE) )
#define VL2_MAX_POLICY_GN_LEN  ( sizeof(VL2_MAX_POLICY_GN_VALUE) )
#define VL2_MAX_POLICY_OT_LEN  32
#define VL2_MAX_POLICY_CT_LEN  32
#define VL2_MAX_POLICY_AP_LEN  ( sizeof(VL2_MAX_POLICY_AP_VALUE) )
#define VL2_MAX_POLICY_GP_LEN  ( sizeof(VL2_MAX_POLICY_GP_VALUE) )
#define VL2_MAX_POLICY_IX_LEN  ( sizeof(VL2_MAX_POLICY_IX_VALUE) )
#define VL2_MAX_POLICY_ST_LEN  ( sizeof(VL2_MAX_POLICY_ST_VALUE) )
#define VL2_MAX_POLICY_AE_LEN  48
#define VL2_MAX_POLICY_CN_LEN  256
#define VL2_MAX_POLICY_SE_LEN  ( sizeof(VL2_MAX_POLICY_SE_VALUE) )
#define VL2_MAX_POLICY_DN_LEN  256
#define VL2_MAX_INTERF_ID_LEN  ( sizeof(VL2_MAX_INTERF_ID_VALUE) )
#define VL2_MAX_INTERF_NE_LEN  ( sizeof(VL2_MAX_INTERF_NE_VALUE) )
#define VL2_MAX_INTERF_PH_LEN  ( sizeof(VL2_MAX_INTERF_PH_VALUE) )
#define VL2_MAX_INTERF_DL_LEN  ( sizeof(VL2_MAX_INTERF_DL_VALUE) )
#define VL2_MAX_INTERF_NW_LEN  ( sizeof(VL2_MAX_INTERF_NW_VALUE) )
#define VL2_MAX_INTERF_VD_LEN  ( sizeof(VL2_MAX_INTERF_VD_VALUE) )
#define VL2_MAX_INTERF_BW_LEN  24
#define VL2_MAX_INTERF_VI_LEN  ( sizeof(VL2_MAX_INTERF_VI_VALUE) )
#define VL2_MAX_INTERF_NI_LEN  ( sizeof(VL2_MAX_INTERF_NI_VALUE) )
#define VL2_MAX_INTERF_SB_LEN  ( sizeof(VL2_MAX_INTERF_ST_VALUE) )
#define VL2_MAX_NETSRC_TS_LEN  ( sizeof(VL2_MAX_NETSRC_TS_VALUE) )
#define VL2_MAX_NETSRC_TP_LEN  ( sizeof(VL2_MAX_NETSRC_TP_VALUE) )
#define VL2_MAX_COOKIE_LEN     ( sizeof(VL2_MAX_COOKIE_VALUE) )
#define VL2_MAX_FORMAT_LEN     256
#define VL2_MAX_RACK_ID_LEN    12
#define VL2_MAX_NETMASK_LEN    4

// Maximum length for all combinations
// *VL-->value_list, CD-->conditions, OP-->operations, EP-->extend_ptr
#define VL2_MAX_FWK_VL_LEN  ( sizeof(VL2_POLICY_POLICY_VL_FORMAT) \
    + VL2_MAX_POLICY_TE_LEN + VL2_MAX_POLICY_GN_LEN + VL2_MAX_POLICY_OT_LEN + \
    + VL2_MAX_POLICY_CT_LEN + VL2_MAX_POLICY_AP_LEN + VL2_MAX_POLICY_GP_LEN + \
    + VL2_MAX_POLICY_IX_LEN + VL2_MAX_POLICY_ST_LEN + VL2_MAX_POLICY_AE_LEN + \
    + VL2_MAX_POLICY_CN_LEN + VL2_MAX_POLICY_SE_LEN )
#define VL2_MAX_FWK_CD_LEN  1024
#define VL2_MAX_FWK_OP_LEN  ( sizeof(VL2_POLICY_POLYCN_OP_FORMAT) \
    + VL2_MAX_POLICY_CN_LEN + VL2_MAX_POLICY_SE_LEN )
#define VL2_MAX_FWK_EP_LEN  ( VL2_MAX_POLICY_CN_LEN )

// The combination of table fields
// CL-->Column, VL-->Value, CD-->Condition
#define VL2_POLICY_POLICY_CL_FORMAT  "type,grain,object,component,app,pgroup," \
                                     "pindex,socket,attribute,content,state"
#define VL2_POLICY_POLICY_VL_FORMAT  "\"%d\",\"%d\",\"%s\",\"%s\",\"%d\",\"%d\"," \
                                     "\"%d\",\"%s\",\"%s\",\"%s\",\"%d\""
#define VL2_POLICY_STATUS_CD_FORMAT  "state=\"%d\" or state=\"%d\" or "\
                                     "state=\"%d\" or state=\"%d\""
#define VL2_POLICY_POLYID_CD_FORMAT  "id=\"%d\""
#define VL2_POLICY_POLYCN_CD_FORMAT  "type=\"%d\" and grain=\"%d\" " \
                                     "and component=\"%s\" and app=\"%d\" " \
                                     "and pgroup=\"%d\" and socket=\"%s\" " \
                                     "and attribute=\"%s\""
#define VL2_POLICY_POLYTE_CD_FORMAT  "type=\"%d\" and app=\"%d\""
#define VL2_POLICY_POLYCT_CD_FORMAT  "component=\"%s\" and app=\"%d\""
#define VL2_POLICY_VERIFY_CD_FORMAT  "type=\"%d\" and component=\"%s\" " \
                                     "and app=\"%d\" and pgroup=\"%d\" " \
                                     "and socket=\"%s\" and state!=\"%d\" and state!=\"%d\""
#define VL2_INTERF_INTFID_CD_FORMAT  "id=\"%s\""
#define VL2_INTERF_INTFDL_CD_FORMAT  "mac=\"%s\""
#define VL2_INTERF_INTFDN_CD_FORMAT  "mac=\"%s\" and name=\"%s\""
#define VL2_NETSRC_SERVER_CD_FORMAT  "ip=\"%s\""
#define VL2_POLICY_POLYCN_OP_FORMAT  "content=\"%s\",state=\"%d\""
#define VL2_POLICY_POLYSE_OP_FORMAT  "state=\"%d\",description=\"%s\""
#define VL2_INTERF_OFPORT_OP_FORMAT  "name=\"%s\",ofport=\"%s\""
#define VL2_OF_COOKIE_DOMAIN_FORMAT  "0x%08x%02x%02x%04x"
#define VL2_OF_INPORT_DOMAIN_FORMAT  "in_port=%s"
#define VL2_OF_DL_SRC_DOMAIN_FORMAT  "dl_src=%s"
#define VL2_OF_NW_DST_DOMAIN_FORMAT  "nw_dst=%s"
#define VL2_OF_NW_DST_NETMSK_FORMAT  "nw_dst=%s/%s"
#define VMWAF_WEB_SERVER_CD_FORMAT   "pub_ipaddr=\"%s\" and pub_port=%d"
#define VMWAF_PORT_MAP_CD_FORMAT     "vnetid=%d"
#define VL2_OF_DL_DST_DOMAIN         "dl_dst="
#define VL2_OF_NW_SRC_DOMAIN         "nw_src="
#define VL2_OF_COOKIE_DOMAIN         "cookie="
#define VL2_OF_PRIORITY_DOMAIN       "priority="
#define VL2_OF_IDLE_TIMEOUT_DOMAIN   "idle_timeout="
#define VL2_OF_TABLE_DOMAIN          "table="
#define VL2_OF_BRIDGE_ID_DOMAIN      "bridge_id="
#define VL2_OF_DL_TYPE_DOMAIN        "dl_type="
#define VL2_OF_NW_PROTO_DOMAIN       "nw_proto="
#define VL2_OF_TP_SRC_DOMAIN         "tp_src="
#define VL2_OF_TP_DST_DOMAIN         "tp_dst="
#define VL2_OF_TUN_ID_DOMAIN         "tun_id="
#define VL2_OF_ACTION_DOMAIN         "actions="

// OVS
#define VL2_OVS_XX_SSCANF_FORMAT  "tcp:%[^:]"
#define VL2_OVS_DB_SOCKET_FORMAT  "tcp:%s"
#define VL2_TUN_BR_SOCKET_FORMAT  "tcp:%s"
#define VL2_INT_BR_SOCKET_FORMAT  "tcp:%s"

// Basic variable macro definition for common return value and debug level
#define _VL2_TRUE_        0
#define _VL2_FALSE_      -1
#define _VL2_EQUAL_       0
#define _VL2_YES_         1
#define _VL2_NO_          0
#define _VL2_FIRST_       1
#define _VL2_LAST_       -1
#define _VL2_SYS_DEBUG_  LOG_DEBUG
#define _VL2_DEV_DEBUG_  LOG_INFO
// For the maximum number of policies to be enforced simultaneously
#define VL2_MAX_IDS_NUM  256

#define VL2_MAX_CMD_LEN  256

// Basic function macro definition for system log within debug level
#define VL2_SYSLOG(level, format, ...)  \
    if (level <= _VL2_SYS_DEBUG_) { \
        syslog(level, "(tid=%lu) " format, pthread_self(), ##__VA_ARGS__); \
    }
#define VL2_DEVLOG(level, format, ...)  \
    if (level <= _VL2_DEV_DEBUG_) { \
        syslog(level, "(tid=%lu) " format, pthread_self(), ##__VA_ARGS__); \
    }

// Macros corresponding to policy entries
#define POLICY_TYPE_OVSDB       0x01
#define POLICY_TYPE_OVSFLOW     0x02

#define POLICY_GRAIN_VM         0x01
#define POLICY_GRAIN_VLAN       0x02
#define POLICY_GRAIN_SERVER     0x03

#define POLICY_APP_TUNNEL       0x01
#define POLICY_APP_AUTH         0x02
#define POLICY_APP_VLAN         0x03
#define POLICY_APP_QOS          0x04
#define POLICY_APP_ACL          0x05
#define POLICY_APP_ISO          0x06

#define POLICY_TABLE_TUNNEL     0x00
#define POLICY_TABLE_AUTH       0x00
#define POLICY_TABLE_ACL        ( POLICY_TABLE_AUTH + 1 )
#define POLICY_TABLE_ISO        0x00

#define POLICY_GROUP_GRE            0x01
#define POLICY_GROUP_VLANTAG        0x02
#define POLICY_GROUP_BWRATE         0x03
#define POLICY_GROUP_BWBURST        0x04
#define POLICY_GROUP_IPSPOOF        0x05
#define POLICY_GROUP_ARPSPOOF       0x06
#define POLICY_GROUP_ANYSPOOF       0x07
#define POLICY_GROUP_DEFSPOOF       0x08
#define POLICY_GROUP_KERNAL         0x09
#define POLICY_GROUP_PASS_SRCSSH    0x0a
#define POLICY_GROUP_PASS_DSTSSH    0x0b
#define POLICY_GROUP_PASS_SRCDSK    0x0c
#define POLICY_GROUP_PASS_DSTDSK    0x0d

#define POLICY_STATE_ADD_SYN     1
#define POLICY_STATE_DEL_SYN     3
#define POLICY_STATE_MOD_SYN     5
#define POLICY_STATE_CCL_SYN     7
#define POLICY_STATE_ADD_ERR    -1
#define POLICY_STATE_DEL_ERR    -3
#define POLICY_STATE_MOD_ERR    -5
#define POLICY_STATE_CCL_ERR    -7
#define POLICY_STATE_ADD_FIN     2
#define POLICY_STATE_DEL_FIN     4
#define POLICY_STATE_MOD_FIN     6
#define POLICY_STATE_CCL_FIN     8

#define OVS_PORT_INSERT         0x01
#define OVS_PORT_DELETE         0x02
#define OVS_PORT_MODIFY         0x03

typedef struct policy_s {
    int  id;
    int  type;
    int  grain;
    char object[VL2_MAX_POLICY_OT_LEN];
    char component[VL2_MAX_POLICY_CT_LEN];
    int  app;
    int  group;
    int  index;
    char socket[VL2_MAX_POLICY_ST_LEN];
    char attribute[VL2_MAX_POLICY_AE_LEN];
    char content[VL2_MAX_POLICY_CN_LEN];
    int  state;
    char description[VL2_MAX_POLICY_DN_LEN];
} policy_t;

typedef struct device_s {
    char tunnel_ip[VL2_MAX_NETSRC_TP_LEN];
    char tunnel_name[VL2_MAX_INTERF_NE_LEN];
    char interface_ofport[VL2_MAX_INTERF_NE_LEN];
    char flow_content[VL2_MAX_POLICY_CN_LEN];
} device_t;

typedef struct tunnel_policy_s {
    int  id;
    int  vifid;
    char uplink_ip[VL2_MAX_INTERF_NW_LEN];
    int  vdevicetype;
    int  vdeviceid;
    int  vl2id;
    int  vlantag;
    char vifmac[VL2_MAX_INTERF_DL_LEN];
}tunnel_policy_t;

typedef struct vl2_tunnel_policy_s {
    int  id;
    char uplink_ip[VL2_MAX_INTERF_NW_LEN];
    int  vl2id;
    int  vlantag;
}vl2_tunnel_policy_t;

#define LC_DOMAIN_LEN                64
typedef struct vmwaf_port_map_info_s {
    int         id;
    int         state;
    int         pre_state;
    int         flag;
    int         web_server_confid;
    int         vnetid;
    char        wan_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         wan_port;
    char        wan_mac[MAX_VIF_MAC_LEN];
    char        vnet_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         vnet_port;
    char        vm_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         vm_port;
    char        domain[LC_DOMAIN_LEN];
} vmwaf_port_map_info_t;

typedef struct vmwaf_web_conf_info_s {
    int         id;
    int         state;
    int         pre_state;
    int         flag;
    int         errorno;
    char        description[LC_NAMEDES];
    int         vnetid;
    char        server_name[MAX_VM_UUID_LEN];
    int         service_type;
    char        pub_ipaddr[MAX_HOST_ADDRESS_LEN];
    int         pub_port;
    char        pub_mac[MAX_VIF_MAC_LEN];
    char        domain_name[LC_NAMEDES];
    int         weblog_state;
    int         vmwafid;
    char        domain[LC_DOMAIN_LEN];
} vmwaf_web_conf_info_t;

// Functions open for outer call
int enforce_policy();

#endif /* __EXT_VL2_FRAMEWORK_H__ */
