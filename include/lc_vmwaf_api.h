/*
 * Copyright (c) 2012-2014 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_vmwaf_api.h
 * Author Name: Zhiming Zhang
 * Date: 2014-1-14
 *
 * Description: Header of VM WAF APIs
 *
 */

#ifndef _LC_VMWAF_API_H
#define _LC_VMWAF_API_H

#include <sys/types.h>

#define VMWAF_API_RESULT_SIZE        1024
#define VMWAF_API_URL_SIZE           512
#define VMWAF_API_DATA_SIZE          2048
#define VMWAF_API_COOKIE_SIZE        2048
#define VMWAF_API_BIG_RESULT_SIZE    65536
#define VMWAF_API_SUCCESS_RESULT     "200"
#define VMWAF_API_USERNAME           "admin"
#define VMWAF_API_PASSWD             "nsfocus"
#define VMWAF_API_VERSION            "v1"

//#define VMWAF_API_PREFIX                  "http://%s:%s@%s:8999/rest/%s/"
#define VMWAF_API_PREFIX                    "http://admin:nsfocus@%s:8999/rest/v1/"
#define VMWAF_API_STATION_GROUPS            VMWAF_API_PREFIX "clouds"
#define VMWAF_API_STATION_GROUP             VMWAF_API_PREFIX "clouds/%s_%d"
#define VMWAF_API_STATION                   VMWAF_API_PREFIX "clouds/%s_%d/website"
#define VMWAF_API_STATIONS                  VMWAF_API_PREFIX "clouds/%s_%d/website?ws=%s"

/*
{\
  \"SITE_LIST\": [\
    {\
      \"NAME\": \"'%s'\",\
      \"ACTIVE\": \"true\",\
      \"IP\": \"%s\",\
      \"PORT\": \"%s\",\
      \"DESC\": \"livecloud station\",\
      \"WEBACCESS_ENABLED\": \"false\",\
      \"HOST\": [\
          {\
            \"DOMAIN\": \"%s\",\
            \"SSL\": \"false\",\
            \"SERVER\": \
            [\
              {\
                \"REAL_IP\": \"%s\",\
                \"REAL_PORT\": \"%s\"\
              },\
              {\
                \"REAL_IP\": \"%s\",\
                \"REAL_PORT\": \"%s\"\
              }\
            ]\
          }\
      ]\
    }\
  ]\
}
*/
#define VMWAF_API_STATIONS_START \
"{\
  \"SITE_LIST\": [\
    {\
      \"NAME\": \"%s\",\
      \"ACTIVE\": \"true\",\
      \"IP\": \"%s\",\
      \"PORT\": \"%d\",\
      \"DESC\": \"livecloud station\",\
      \"WEBACCESS_ENABLED\": \"false\",\
      \"HOST\": [\
          {\
            \"DOMAIN\": \"%s\",\
            \"SSL\": \"false\",\
            \"SERVER\": \
            ["
#define VMWAF_API_STATIONS_BODY \
             "{\
                \"REAL_IP\": \"%s\",\
                \"REAL_PORT\": \"%d\"\
              }"
#define VMWAF_API_STATIONS_END \
           "]\
          }\
      ]\
    }\
  ]\
}"

#define VMWAF_API_SET_DEFAULT_GW     VMWAF_API_PREFIX "interfaces/gateway/default"
#define VMWAF_API_DEL_TUNNEL         VMWAF_API_PREFIX "deletetunnel"
#define VMWAF_API_TUNNEL_POST        "targetIP=%s"
#define VMWAF_API_SET_TUNNEL_QOS     VMWAF_API_PREFIX "settunnelqos"
#define VMWAF_API_CLEAR_TUNNEL_QOS   VMWAF_API_PREFIX "cleartunnelqos"
#define VMWAF_API_TUNNEL_QOS_POST    "minrate=%u&maxrate=%u"
#define VMWAF_API_SET_VIF_TUNNEL_POLICY         VMWAF_API_PREFIX "setviftunnelpolicy"
#define VMWAF_API_SET_VIF_TUNNEL_POLICY_POST    "vnetid=%u&vl2id=%u&vlantag=%u&vifid=%u&vifmac=%s"
#define VMWAF_API_SET_VL2_TUNNEL_POLICY         VMWAF_API_PREFIX "setvl2tunnelpolicy"
#define VMWAF_API_SET_VL2_TUNNEL_POLICY_POST    "vnetid=%u&vl2id=%u&vlantag=%u"
#define VMWAF_API_CLEAR_VIF_TUNNEL_POLICY       VMWAF_API_PREFIX "clearviftunnelpolicy"
#define VMWAF_API_CLEAR_VIF_TUNNEL_POLICY_POST  "vnetid=%u&vl2id=%u&vifid=%u"
#define VMWAF_API_CLEAR_VL2_TUNNEL_POLICY       VMWAF_API_PREFIX "clearvl2tunnelpolicy"
#define VMWAF_API_CLEAR_VL2_TUNNEL_POLICY_POST  "vnetid=%u&vl2id=%u"
#define VMWAF_API_CONFIG_UPLINK                 VMWAF_API_PREFIX "configuplink"
#define VMWAF_API_CONFIG_UPLINK_POST            "ip=%s&netmask=%s&gateway=%s"

#define VMWAF_JSON_META_KEY              "meta"
#define VMWAF_JSON_VERSION_KEY           "version"
#define VMWAF_JSON_CODE_KEY              "code"

#define VMWAF_JSON_DATA_KEY              "data"
#define VMWAF_JSON_MESSAGE_KEY           "message"

#define VMWAF_CODE_LABEL(x)   #x

enum VMWAF_RES_CODES {
    VMWAF_CODE_SUCCESS = 0,
    VMWAF_CODE_ERROR,
    VMWAF_CODE_PAYLOAD_TOO_LONG,
    VMWAF_CODE_INVALID_JSON,
    VMWAF_CODE_INVALID_REQ,
    VMWAF_CODE_INVALID_VERSION,
    VMWAF_CODE_INVALID_CODE,
    VMWAF_CODE_CMD_ERROR,

    VMWAF_RES_CODE_COUNT
};

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

int call_vmwaf_curl_api(char *url, int method, char *data, char *cookie, result_process fp, char *echo);


#endif /* _LC_LIVEGATE_API_H */
