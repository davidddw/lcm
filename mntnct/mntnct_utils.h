/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: mntnct_utils.c
 * Author Name: Xiang Yang
 * Date: 2013-7-18
 *
 * Description: utils for mntnct
 */

#ifndef _MNTNCT_UTILS_H
#define _MNTNCT_UTILS_H

#include "nxjson.h"

#define MNTNCT_DOMAIN_LEN                64
#define MNTNCT_CMD_LEN                   512
#define MNTNCT_CMD_RESULT_LEN            1024
#define MNTNCT_BIG_CMD_RESULT_LEN        65536
#define CURL_BUFFER_SIZE                 4096

#define MNTNCT_VL2_STATE_FINISH       2
#define MNTNCT_VL2_STATE_ABNORMAL     3

#define ANY_LCUUID  "ffffffff-ffff-ffff-ffff-ffffffffffff"
#define NONE_LCUUID "00000000-0000-0000-0000-000000000000"

enum API_METHOD_CODES {
    API_METHOD_POST = 0,
#define API_METHOD_POST_CMD "POST"
    API_METHOD_PUT,
#define API_METHOD_PUT_CMD "PUT"
    API_METHOD_DEL,
#define API_METHOD_DEL_CMD "DELETE"
    API_METHOD_GET,
#define API_METHOD_GET_CMD "GET"
    API_METHOD_PATCH
#define API_METHOD_PATCH_CMD "PATCH"
};

#define MNTNCT_CODE_LABEL(x)       #x
#define MNTNCT_API_PREFIX          "http://%s:%d/%s/"
#define MNTNCT_API_VERSION         "v1"
#define MNTNCT_API_DEST_IP         "127.0.0.1"
#define MNTNCT_API_DEST_PORT       20013
#define MNTNCT_JSON_OPT_KEY        "OPT_STATUS"
#define MNTNCT_JSON_DESC_KEY       "DESCRIPTION"

#define MNTNCT_API_DOMAIN          "DOMAIN"

#define MNTNCT_API_DOMAIN_POST     "domains/"
#define MNTNCT_API_DOMAIN_DELETE   "domains/%s/"
#define MNTNCT_API_DOMAIN_PATCH    "domains/%s/"
#define MNTNCT_API_DOMAIN_PARAM_NAME     "PARAM_NAME"
#define MNTNCT_API_DOMAIN_VALUE    "VALUE"
#define MNTNCT_API_DOMAIN_NAME     "NAME"
#define MNTNCT_API_DOMAIN_IP       "IP"
#define MNTNCT_API_DOMAIN_ROLE     "ROLE"
#define MNTNCT_API_DOMAIN_PUBLICIP       "PUBLIC_IP"

#define MNTNCT_API_IP_POST         "ips/"
#define MNTNCT_API_IP_IP           "IP"
#define MNTNCT_API_IP_NETMASK      "NETMASK"
#define MNTNCT_API_IP_GATEWAY      "GATEWAY"
#define MNTNCT_API_IP_POOL_NAME    "POOL_NAME"
#define MNTNCT_API_IP_PRODUCT_SPEC_LCUUID    "PRODUCT_SPECIFICATION_LCUUID"
#define MNTNCT_API_IP_ISP          "ISP"
#define MNTNCT_API_IP_VLANTAG      "VLANTAG"

#define MNTNCT_API_ISP_PUT         "isps/%s/"
#define MNTNCT_API_ISP_PUT_C       "isps/"
#define MNTNCT_API_ISP_NAME        "NAME"
#define MNTNCT_API_ISP_BANDWIDTH   "BANDWIDTH"
#define MNTNCT_API_ISP_DOMAIN      "DOMAIN"
#define MNTNCT_API_ISP_ISP         "ISP"

#define MNTNCT_API_VLAN_PATCH       "vlan/"
#define MNTNCT_API_VLAN_VL2_LCUUID  "VL2_LCUUID"
#define MNTNCT_API_VLAN_RACK_LCUUID "RACK_LCUUID"
#define MNTNCT_API_VLAN_VLANTAG     "VLANTAG"

#define MNTNCT_API_CTRL_IP_POST     "sys/ip-ranges/"
#define MNTNCT_API_CTRL_IP_TYPE     "TYPE"
#define MNTNCT_API_CTRL_IP_IP_MIN   "IP_MIN"
#define MNTNCT_API_CTRL_IP_IP_MAX   "IP_MAX"
#define MNTNCT_API_CTRL_IP_NETMASK  "NETMASK"
#define MNTNCT_API_CTRL_IP_DOMAIN   "DOMAIN"

#define MNTNCT_HWDEV_INTF_STATE_ATTACHED    1
#define MNTNCT_HWDEV_INTF_STATE_DETACHED    2
#define MNTNCT_API_HWDEV_POST               "third-party-devices/"
#define MNTNCT_API_HWDEV_DELETE             "third-party-devices/%s/"
#define MNTNCT_API_HWDEV_INTF_POST          "third-party-devices/%s/interfaces/"
#define MNTNCT_API_HWDEV_INTF_PATCH         "third-party-devices/%s/interfaces/%s/"
#define MNTNCT_API_HWDEV_INTF_DELETE        "third-party-devices/%s/interfaces/%s/"
#define MNTNCT_API_HWDEV_IF_TYPE            "IF_TYPE"
#define MNTNCT_API_HWDEV_IF_SUBTYPE         "SUBTYPE"
#define MNTNCT_API_HWDEV_IF_STATE           "STATE"
#define MNTNCT_API_HWDEV_USERID             "USERID"
#define MNTNCT_API_HWDEV_NAME               "NAME"
#define MNTNCT_API_HWDEV_DOMAIN             "DOMAIN"
#define MNTNCT_API_HWDEV_RACK_LCUUID        "RACK_LCUUID"
#define MNTNCT_API_HWDEV_SNMP_USER_NAME     "SNMP_USER_NAME"
#define MNTNCT_API_HWDEV_SNMP_USER_PASSWD   "SNMP_USER_PASSWD"
#define MNTNCT_API_HWDEV_SNMP_COMMUNITY     "SNMP_COMMUNITY"
#define MNTNCT_API_HWDEV_TYPE               "TYPE"
#define MNTNCT_API_HWDEV_ROLE               "ROLE"
#define MNTNCT_API_HWDEV_PRODUCT_SPECIFICATION  "PRODUCT_SPECIFICATION_LCUUID"
#define MNTNCT_API_HWDEV_BRAND              "BRAND"
#define MNTNCT_API_HWDEV_MGMT_IP            "MGMT_IP"
#define MNTNCT_API_HWDEV_USER_NAME          "USER_NAME"
#define MNTNCT_API_HWDEV_USER_PASSWD        "USER_PASSWD"
#define MNTNCT_API_HWDEV_ORDER_ID           "ORDER_ID"
#define MNTNCT_API_HWDEV_POOL_ID            "POOL_ID"
#define MNTNCT_API_HWDEV_IF_INDEX           "IF_INDEX"
#define MNTNCT_API_HWDEV_IF_MAC             "MAC"
#define MNTNCT_API_HWDEV_IF_SPEED           "SPEED"
#define MNTNCT_API_HWDEV_NETWORK_DEVICE_LCUUID           "NETWORK_DEVICE_LCUUID"
#define MNTNCT_API_HWDEV_NETWORK_DEVICE_PORT             "NETWORK_DEVICE_PORT"

#define MNTNCT_API_SERVICE_REGISTER_POST        "service-registrations/"
#define MNTNCT_API_SERVICE_VENDOR               "SERVICE_VENDOR"
#define MNTNCT_API_SERVICE_TYPE                 "SERVICE_TYPE"
#define MNTNCT_API_SERVICE_IP                   "SERVICE_IP"
#define MNTNCT_API_SERVICE_PORT                 "SERVICE_PORT"
#define MNTNCT_API_SERVICE_USER_NAME            "SERVICE_USER_NAME"
#define MNTNCT_API_SERVICE_USER_PASSWD          "SERVICE_USER_PASSWD"
#define MNTNCT_API_CONTROL_IP                   "CONTROL_IP"
#define MNTNCT_API_CONTROL_PORT                 "CONTROL_PORT"
#define MNTNCT_API_SERVICE_REGISTER_DELETE      "service-registrations/%s/"

#define MNTNCT_API_VM_CTRL_IP_POST        "resources/vm-ctrl-ips/?domain=%s"
#define MNTNCT_API_VM_CTRL_IP_ADDRESS     "IP"
#define MNTNCT_API_VM_CTRL_IP_NETMASK     "NETMASK"

#define MNTNCT_API_VM_VNC_TPORT_POST      "resources/vm-vnc-tports/"
#define MNTNCT_API_VM_VNC_TPORT           "TPORT"

void chomp(char *str);
int get_param(char *value, int size, char *get_str, ...);
int call_system_output(char *raw_cmd, char *result, int result_buf_len);
char *get_domain();
void str_replace(char *str, char dst, char ori);
int get_masklen(char *netmask);
int generate_uuid(char *buffer, int buflen);
int dump_json_msg(const nx_json *root, char *msg_buf, int buf_len);
int call_curl_api(char *url, int method, char *data, char *userp);
int acquire_mntnct_queue_lock(void);
int release_mntnct_queue_lock(void);
int dump_json_msg(const nx_json *root, char *msg_buf, int buf_len);

#endif
