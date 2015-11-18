/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_livegate_api.h
 * Author Name: Xiang Yang
 * Date: 2013-7-4
 *
 * Description: Header of Livegate APIs
 *
 */

#ifndef _LC_LIVEGATE_API_H
#define _LC_LIVEGATE_API_H

#include <inttypes.h>
#include "nxjson.h"

#define LG_API_VERSION            "v2_2_1009"

#define LG_API_PREFIX             "http://%s:20009/api/"
#define LG_API_ADD_TUNNEL         LG_API_PREFIX "createtunnel"
#define LG_API_DEL_TUNNEL         LG_API_PREFIX "deletetunnel"
#define LG_API_TUNNEL_POST        "targetIP=%s"
#define LG_API_SET_TUNNEL_QOS     LG_API_PREFIX "settunnelqos"
#define LG_API_CLEAR_TUNNEL_QOS   LG_API_PREFIX "cleartunnelqos"
#define LG_API_TUNNEL_QOS_POST    "minrate=%u&maxrate=%u"
#define LG_API_SET_VIF_TUNNEL_POLICY         LG_API_PREFIX "setviftunnelpolicy"
#define LG_API_SET_VIF_TUNNEL_POLICY_POST    "vnetid=%u&vl2id=%u&vlantag=%u&vifid=%u&vifmac=%s"
#define LG_API_SET_VL2_TUNNEL_POLICY         LG_API_PREFIX "setvl2tunnelpolicy"
#define LG_API_SET_VL2_TUNNEL_POLICY_POST    "vnetid=%u&vl2id=%u&vlantag=%u"
#define LG_API_CLEAR_VIF_TUNNEL_POLICY       LG_API_PREFIX "clearviftunnelpolicy"
#define LG_API_CLEAR_VIF_TUNNEL_POLICY_POST  "vnetid=%u&vl2id=%u&vifid=%u"
#define LG_API_CLEAR_VL2_TUNNEL_POLICY       LG_API_PREFIX "clearvl2tunnelpolicy"
#define LG_API_CLEAR_VL2_TUNNEL_POLICY_POST  "vnetid=%u&vl2id=%u"
#define LG_API_CONFIG_UPLINK                 LG_API_PREFIX "configuplink"
#define LG_API_CONFIG_UPLINK_POST            "ip=%s&netmask=%s&gateway=%s"
#define LG_API_WEB_SECURITY                "http://%s:20009/v1/ovsnats/"
#define LG_API_ADD_WEB_SECURITY_POST         "{\
                                                   \"BRIDGE\":\"UPLINK\",\
                                                   \"PORT\":%d,\
                                                   \"IP\":\"%s\",\
                                                   \"MAC\":\"%s\",\
                                                   \"TARGET_PORT\":%d,\
                                                   \"TARGET_IP\":\"%s\",\
                                                   \"TARGET_MAC\":\"%s\"}"
#define LG_API_DEL_WEB_SECURITY              LG_API_PREFIX "delwebsecurity"
#define LG_API_DEL_WEB_SECURITY_POST         "UPLINK-%d-%s"

#define LG_API_URL_SIZE           2048
#define LG_API_URL_SIZE_ROUTER    65536
#define LG_API_URL_SIZE_VALVE     LG_API_URL_SIZE_ROUTER
#define LG_API_RESULT_SIZE        512
#define LG_API_BIG_RESULT_SIZE    65536

#define LG_JSON_META_KEY              "meta"
#define LG_JSON_VERSION_KEY           "version"
#define LG_JSON_CODE_KEY              "code"

#define LG_JSON_DATA_KEY              "data"
#define LG_JSON_MESSAGE_KEY           "message"

#define LG_CODE_LABEL(x)   #x

enum LG_RES_CODES {
    LG_CODE_SUCCESS = 0,
    LG_CODE_ERROR,
    LG_CODE_PAYLOAD_TOO_LONG,
    LG_CODE_INVALID_JSON,
    LG_CODE_INVALID_REQ,
    LG_CODE_INVALID_VERSION,
    LG_CODE_INVALID_CODE,
    LG_CODE_CMD_ERROR,

    LG_RES_CODE_COUNT
};

int call_livegate_api(char *url, char *post);

#define LCPD_API_PREFIX          "http://%s:%d/%s/"
#define LCPD_API_CALLBACK        "?callback="
#define LCPD_API_VERSION         "v1"
#define LCPD_API_DEST_IP_RYU     "127.0.0.1"
#define LCPD_API_DEST_PORT       20009
#define LCPD_AGEXEC_DEST_PORT    20016
#define LCPD_TALKER_LISTEN_PORT  20013
#define SDN_CTRL_LISTEN_PORT     20103

#define SDN_API_PREFIX          "http://%s:%d/%s/"
#define SDN_API_CALLBACK        "?callback="
#define SDN_API_VERSION         "v1"

#define SDN_API_DEST_IP_SDNCTRL            "127.0.0.1"
#define SDN_API_SWITCH_PORT                "switch/%s/port/%d/"
#define SDN_API_SWITCH_BOOT_TIME           "switch/%s/boot-time/"
#define SDN_API_CALLBACK_VPORTS            "%s:%d/%s/callbacks/vports/"
#define SDN_API_SWITCH_TUNNEL_ALL          "switch/%s/tunnel/"
#define SDN_API_SWITCH_TUNNEL              "switch/%s/tunnel/%s/"
#define SDN_API_SWITCH_SUBNET              "switch/%s/subnet/%d/"

#define SDN_API_SWITCH_COMMAND             "COMMAND"
#define SDN_API_SWITCH_CMD_CONF_SW         "conf-switchport"
#define SDN_API_SWITCH_CMD_CLOSE_SW        "close-switchport"
#define SDN_API_SWITCH_CMD_BOOT_TIME       "boot-time"
#define SDN_API_SWITCH_TYPE                "SWITCH_TYPE"
#define SDN_API_SWITCH_INFO                "SWITCH_INFO"
#define SDN_API_SWITCH_INFO_IP             "IP"
#define SDN_API_SWITCH_INFO_USERNAME       "USERNAME"
#define SDN_API_SWITCH_INFO_PASSWORD       "PASSWORD"
#define SDN_API_SWITCH_INFO_ENABLE         "ENABLE"
#define SDN_API_SWITCH_CONF_INFO           "SWITCH_CONF_INFO"
#define SDN_API_SWITCH_CONF_INFO_OP        "OPERATION"
#define SDN_API_SWITCH_CONF_INFO_PORT      "SWITCHPORT"
#define SDN_API_SWITCH_CONF_INFO_VLAN      "VLANTAG"
#define SDN_API_SWITCH_CONF_INFO_BW        "BANDWIDTH"
#define SDN_API_SWITCH_CONF_INFO_TRUNK     "TRUNK"
#define SDN_API_SWITCH_CONF_INFO_SRC_MAC   "SRC_MAC"
#define SDN_API_SWITCH_CONF_INFO_SRC_NETS  "SRC_NETS"
#define SDN_API_SWITCH_CONF_INFO_DST_NETS  "DST_NETS"

//RYU
#define LCPD_API_GRE_POST_RYU    "switchs/%s/gres/"
#define LCPD_API_GRE_RYU_BIND_PORT      "BIND_PORT"
#define LCPD_API_GRE_RYU_LOCAL_IP       "LOCAL_IP"
#define LCPD_API_GRE_RYU_LOCAL_MASK     "LOCAL_MASK"
#define LCPD_API_GRE_RYU_LOCAL_GATEWAY  "LOCAL_GATEWAY"
#define LCPD_API_GRE_RYU_LOCAL_CTRL_IP  "LOCAL_CTRL_IP"
#define LCPD_API_GRE_RYU_LOCAL_PASSWD   "LOCAL_PASSWD"
#define LCPD_API_GRE_RYU_REMOTE_IP      "REMOTE_IP"
#define LCPD_API_GRE_RYU_REMOTE_CTRL_IP "REMOTE_CTRL_IP"
#define LCPD_API_GRE_RYU_REMOTE_PASSWD  "REMOTE_PASSWD"
#define LCPD_API_VIF_POST_RYU    "switchs/%s/vifs/"
#define LCPD_API_VIF_RYU_VIF_ID         "VIF_ID"
#define LCPD_API_VIF_RYU_VIF_MAC        "VIF_MAC"
#define LCPD_API_VIF_RYU_VIF_TYPE       "VIF_TYPE"
#define LCPD_API_VIF_RYU_VL2_ID         "VL2_ID"
#define LCPD_API_VIF_RYU_LOCAL_VLAN_ID  "LOCAL_VLAN_VID"
#define LCPD_API_VIF_RYU_CTRL_VLAN_ID   "CTRL_VLAN_VID"
#define LCPD_API_VIF_RYU_DATA_PORT_LIST "DATA_PORT_LIST"
#define LCPD_API_VIF_RYU_CTRL_PORT_LIST "CTRL_PORT_LIST"
#define LCPD_API_VIF_RYU_SWITCH_PORT    "SWITCH_PORT"
#define LCPD_API_VIF_DEL_RYU     "switchs/%s/vifs/%d/"
#define LCPD_API_VL2_POST_RYU    "switchs/%s/vl2s/"
#define LCPD_API_VL2_RYU_VL2_ID         "VL2_ID"
#define LCPD_API_VL2_RYU_LOCAL_VLAN_ID  "LOCAL_VLAN_VID"
#define LCPD_API_VL2_DEL_RYU     "switchs/%s/vl2s/%d/"
//LG
#define LCPD_API_GRE_POST_LG         "tunnels/"
#define LCPD_API_GRE_LG_PROTOCOL             "PROTOCOL"
#define LCPD_API_GRE_LG_PROTOCOL_GRE         "GRE"
#define LCPD_API_GRE_LG_REMOTE_IP            "REMOTE_IP"
#define LCPD_API_TUNNEL_QOS_POST_LG          "bridges/TUNNEL/"
#define LCPD_API_TUNNEL_QOS_LG_NAME          "NAME"
#define LCPD_API_TUNNEL_QOS_LG_NAME_TUNNEL   "TUNNEL"
#define LCPD_API_TUNNEL_QOS_LG_QOS           "QOS"
#define LCPD_API_TUNNEL_QOS_LG_MIN_BANDWIDTH "MIN_BANDWIDTH"
#define LCPD_API_TUNNEL_QOS_LG_MAX_BANDWIDTH "MAX_BANDWIDTH"
#define LCPD_API_UPLINK_PATCH_LG             "bridges/UPLINK/"
#define LCPD_API_UPLINK_PATCH_LG_NAME        "NAME"
#define LCPD_API_UPLINK_PATCH_LG_NAME_UPLINK "UPLINK"
#define LCPD_API_UPLINK_PATCH_LG_IP          "IP"
#define LCPD_API_UPLINK_PATCH_LG_ADDRESS     "ADDRESS"
#define LCPD_API_UPLINK_PATCH_LG_NETMASK     "NETMASK"
#define LCPD_API_UPLINK_PATCH_LG_GATEWAY     "GATEWAY"
#define LCPD_API_VIF_VL2_POST_LG     "tunnel_flows/"
#define LCPD_API_VIF_VL2_LG_SUBNET_ID        "SUBNET_ID"
#define LCPD_API_VIF_VL2_LG_VLANTAG          "VLANTAG"
#define LCPD_API_VIF_VL2_LG_VIF_ID           "VIF_ID"
#define LCPD_API_VIF_VL2_LG_VIF_MAC          "VIF_MAC"
#define LCPD_API_VIF_DEL_LG          "tunnel_flows/%d-%d/"
#define LCPD_API_VL2_DEL_LG          "tunnel_flows/%d-0/"
//TUNNEL SWITCH
#define LCPD_API_TUNNEL_POST_TS      "add-tunnel %s %s %s %s '%s' %s -p %s"
#define LCPD_API_TUNNEL_DELALL_TS    "del-tunnel %s %s %s %s '%s' -a"
#define LCPD_API_VL2_POST_TS         "add-subnet %s %s %s %s '%s' %d %d"
#define LCPD_API_VL2_DEL_TS          "del-subnet %s %s %s %s '%s' %d %d"
#define LCPD_API_BOOTTIME_GET_TS     "boot-time %s %s %s %s '%s'"
#define LCPD_API_CONF_SWITCHPORT_POST_TS     "conf-switchport -o %s -m %s -s %s -d %s %s %s %s %s '%s' %u %s %s"
#define LCPD_API_CLOSE_SWITCHPORT_POST_TS     "close-switchport -o %s %s %s %s %s '%s' %u"

//ROUTER
#define LCPD_API_ROUTER              "routers/"
#define LCPD_API_ROUTER_WAN          "routers/%d/wans/%d/"
#define LCPD_API_ROUTER_LAN          "routers/%d/lans/%d/"

//VALVE
#define LCPD_API_VALVE               "valves/"
#define LCPD_API_VALVE_WAN           "valves/%d/wans/%d/"
#define LCPD_API_VALVE_LAN           "valves/%d/lans/%d/"

#define LCPD_TALKER_LISTEN_IP        "127.0.0.1"
#define LCPD_API_MS_VIF_ATTACH       "micro-segments/%s/interfaces/"
#define LCPD_API_MS_VIF_DETACH       "micro-segments/%s/interfaces/%s/%s/%d/?ignore_db=1"
#define LCPD_API_MS_INSTANCE_TYPE    "INSTANCE_TYPE"
#define LCPD_API_MS_INSTANCE_LCUUID  "INSTANCE_LCUUID"
#define LCPD_API_MS_IF_INDEX         "IF_INDEX"

#define LCPD_API_CONF_SWITCH_ACT_NONE                   "NONE"
#define LCPD_API_CONF_SWITCH_ACT_CONF                   "CONF"

#define LCPD_API_VPORTS                                 "vports/"
#define LCPD_API_VPORTS_STATE                           "STATE"
#define LCPD_API_VPORTS_STATE_ATTACH                    "ATTACH"
#define LCPD_API_VPORTS_STATE_DETACH                    "DETACH"
#define LCPD_API_VPORTS_VIF_ID                          "VIF_ID"
#define LCPD_API_VPORTS_IF_INDEX                        "IF_INDEX"
#define LCPD_API_VPORTS_MAC                             "MAC"
#define LCPD_API_VPORTS_VLANTAG                         "VLANTAG"
#define LCPD_API_VPORTS_VM_LABEL                        "VM_LABEL"
#define LCPD_API_VPORTS_QOS                             "QOS"
#define LCPD_API_VPORTS_QOS_MIN_BANDWIDTH                   "MIN_BANDWIDTH"
#define LCPD_API_VPORTS_QOS_MAX_BANDWIDTH                   "MAX_BANDWIDTH"
#define LCPD_API_VPORTS_BROADCAST_QOS                   "BROADCAST_QOS"
#define LCPD_API_VPORTS_ACL                             "ACL"
#define LCPD_API_VPORTS_ACL_TYPE                            "TYPE"
#define LCPD_API_VPORTS_ACL_TYPE_SRC_MAC                        "SRC_MAC"
#define LCPD_API_VPORTS_ACL_TYPE_SRC_IP                         "SRC_IP"
#define LCPD_API_VPORTS_ACL_TYPE_SRC_DST_IP                     "SRC_DST_IP"
#define LCPD_API_VPORTS_ACL_TYPE_ALL_DROP                       "ALL_DROP"
#define LCPD_API_VPORTS_ACL_ISOLATE_SWITCH                  "ISOLATE_SWITCH"
#define LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_NONE                 "NONE"
#define LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_OPEN                 "OPEN"
#define LCPD_API_VPORTS_ACL_ISOLATE_SWITCH_CLOSE                "CLOSE"
#define LCPD_API_VPORTS_ACL_SRC_IPS                         "SRC_IPS"
#define LCPD_API_VPORTS_ACL_DST_IPS                         "DST_IPS"
#define LCPD_API_VPORTS_ACL_IPS_ADDRESS                         "ADDRESS"
#define LCPD_API_VPORTS_ACL_IPS_NETMASK                         "NETMASK"

#define LCPD_API_CALLBACK_VPORTS              "%s:%d/%s/callbacks/vports/"

#define LCPD_API_MASTER_CTRL_IPS              "master-ctrl-ips/"
#define LCPD_API_MASTER_CTRL_IPS_MODULE       "MODULE"
#define LCPD_API_MASTER_CTRL_IPS_DOMAIN       "DOMAIN"
#define LCPD_API_MASTER_CTRL_IPS_IP           "IP"

// talker
#define TALKER_API_PREFIX                     "http://%s:%d/%s/"
#define TALKER_API_VERSION                    "v1"
#define TALKER_API_VL2VLAN                    "vl2-vlans/"
#define TALKER_API_VL2VLAN_VL2ID              "VL2_ID"
#define TALKER_API_VL2VLAN_RACKID             "RACK_ID"
#define TALKER_API_VL2VLAN_VLAN_REQ           "VLAN_REQUEST"
#define TALKER_API_VL2VLAN_FORCE              "FORCE"
#define TALKER_API_VL2VLAN_VLAN_TAG           "VLAN_TAG"

#define LCPD_JSON_OPT_KEY        "OPT_STATUS"
#define LCPD_JSON_DESC_KEY       "DESCRIPTION"
#define LCPD_JSON_DATA_KEY       "DATA"
#define LCPD_JSON_TYPE_KEY       "TYPE"
#define LCPD_JSON_VIF_ID_KEY     "VIF_ID"

#define LCPD_CODE_LABEL(x)      #x

#define LCSNF_JSON_OPT_KEY       "OPT_STATUS"
#define LCSNF_JSON_DESC_KEY      "DESCRIPTION"
#define LCSNF_JSON_DATA_KEY      "DATA"
#define LCSNF_JSON_TYPE_KEY      "TYPE"

#define LCSNF_CODE_LABEL(x)     #x

#define LCMON_JSON_OPT_KEY       "OPT_STATUS"
#define LCMON_JSON_DESC_KEY      "DESCRIPTION"
#define LCMON_JSON_DATA_KEY      "DATA"
#define LCMON_JSON_TYPE_KEY      "TYPE"

#define LCMON_CODE_LABEL(x)     #x

enum LCPD_CODES {
    SUCCESS = 0,
    FAIL,
    ERROR,
    INVALID_JSON,
};

int dump_json_msg(const nx_json *root, char *msg_buf, int buf_len);
int lcpd_call_api(char *url, int method, char *data, char *res);
int lcsnf_call_api(char *url, int method, char *data);
int lcmon_call_api(char *url, int method, char *data);
int call_system_output(char *raw_cmd, char *result, int result_buf_len);
int vmwaf_ovsnat_call_api(char *url, int method, char *data);
int parse_json_msg_get_mac(char *mac, void *post);
int parse_json_msg_get_boot_time(uint64_t *btime, void *post);
#endif /* _LC_LIVEGATE_API_H */
