/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_lcmg_api.h
 * Author Name: Xiang Yang
 * Date: 2013-8-15
 *
 * Description: Header of LCMG APIs
 *
 */

#ifndef _LC_LCMG_API_H
#define _LC_LCMG_API_H

#define LCMG_API_PREFIX      "curl -s --connect-timeout 10 -k -X POST \"https://%s/bdbadapter/"
#define LCMG_API_UPDATE_VM_LAUNCH_SERVER \
    LCMG_API_PREFIX "updatevm\" -d \"data={\\\"id\\\":%d, \\\"launch_server\\\":\\\"%s\\\"}\""
#define LCMG_API_UPDATE_VGW_LAUNCH_SERVER \
    LCMG_API_PREFIX "updatevgw\" -d \"data={\\\"id\\\":%d, \\\"gw_launch_server\\\":\\\"%s\\\"}\""

#define LCMG_API_PUBLIC_URL                "http://%s:20009/api/"
#define LCMG_API_OFS_URL                   "http://127.0.0.1:20009/"\
                                              "v1/switchs/%s/ports/"
#define LCMG_API_OFS_PORT_URL              "http://127.0.0.1:20009/"\
                                              "v1/switchs/%s/pifs/"
#define LCMG_API_OFS_FLOW_URL              "http://127.0.0.1:20009/"\
                                              "v1/switchs/%s/flowentries/"
#define LCMG_API_NSP_VGW_URL               "http://127.0.0.1:20013/"\
                                              "v1/vgateways/%s/"
#define LCMG_API_NSP_VALVE_URL             "http://127.0.0.1:20013/"\
                                              "v1/valves/%s/"
#define LCMG_API_NSP_STAT_URL              "http://%s:20009/"\
                                              "v1/nsp-stats/?lcc_talker_ip=%s"
#define LCMG_API_NSP_STAT_RT_URL           "http://%s:20009/"\
                                              "v1/nsp-stats/?realtime=true/"
#define LCMG_API_LB_STAT_URL               "http://%s:20016/v1/stats/lbs/"
#define LCMG_API_LB_BK_VM_HEALTH_STATE_URL "http://%s:20016/v1/vagent/%s/lb-listeners/health-state/"
#define LCMG_API_SWITCH_LOAD_URL           "http://127.0.0.1:20103/"\
                                              "v1/switch/%s/rates/"


#define LCMG_API_ADD_VMWAF_NAT             "addvmwafnat"
#define LCMG_API_DEL_VMWAF_NAT             "delvmwafnat"
#define LCMG_API_ADD_PUBIP                 "addpubipaddr"
#define LCMG_API_SET_GW                    "setdefaultgw"
#define LCMG_API_GET_GWMODE                "getvgwmode"
#define LCMG_API_SET_MASTER_MANUAL         "setmastermanual"
#define LCMG_API_SET_BACKUP_MANUAL         "setbackupmanual"
#define LCMG_API_SET_AUTO                  "setauto"
#define LCMG_API_PUBLIC_POST               "json={%s}"
#define LCMG_API_ADD_PUBIP_POST            "\"pubif%d\":\"%d %s\""
#define LCMG_API_SET_GW_POST               "\"defaultgw\":\"%s\""
#define LCMG_API_ADD_VMWAF_NAT_POST        "json={\"vnetctrlip\":\"%s\",\
                                                  \"vnetctrlport\":\"%d\",\
                                                  \"vmip\":\"%s\",\
                                                  \"vmport\":\"%d\"}"
#define LCMG_API_DEL_VMWAF_NAT_POST        "json={\"vnetctrlip\":\"%s\",\
                                                  \"vnetctrlport\":\"%d\"}"

#define LCMG_API_INIT_OFS_PORT_POST      "{\"CTRL_PORT_LIST\":\"%s\",\
                                              \"DATA_PORT_LIST\":\"%s\",\
                                              \"PUB_PORT_LIST\":\"%s\",\
                                              \"CTRL_VLAN_VID\":%d}"

#define LCMG_API_OFS_PORT_CONFIG_POST      "{\"PIF_MAC\":\"%s\",\
                                              \"SWITCH_PORT\":\"%s\",\
                                              \"PIF_TYPE\":\"%s\",\
                                              \"PIF_ID\":%d,\
                                              \"CTRL_VLAN_VID\":%d}"

#define LCMG_API_NSP_VGW_SERVER_POST      "{\"GW_LAUNCH_SERVER\":\"%s\"}"


#define LCMG_API_URL_SIZE                  512
#define LCMG_API_RESULT_SIZE               512
#define LCMG_API_BIG_RESULT_SIZE           65536
#define LCMG_API_SUPER_BIG_RESULT_SIZE     ( 1 << 20 )
#define LCMG_API_POST_SIZE                 1024
#define LCMG_API_SUCCESS_RESULT            "VG_CODE_SUCCESS"
#define LCMG_API_RESULT_HASTATE_MASTER     "master"
#define LCMG_API_RESULT_HASTATE_BACKUP     "backup"
#define LCMG_API_RESULT_HASTATE_FAULT      "fault"
#define LCMG_API_RESULT_BK_VM_STATE_UP     "UP"
#define LCMG_API_RESULT_BK_VM_STATE_DOWN   "DOWN"
#define LCMG_API_RESULT_BK_VM_STATE_MAINT  "MAINT"

int call_lcmg_api(char *url, char *result, int result_len);
int call_curl_add_pubip(char *ctrl_ip, char *ip_post[], int ip_num);
int call_curl_set_gw(char *ctrl_ip, char *gw);
int call_curl_get_hastate(char *ctrl_ip, int *hastate);
int call_curl_set_master_manual(char *ctrl_ip);
int call_curl_set_backup_manual(char *ctrl_ip);
int call_curl_set_auto(char *ctrl_ip);
int call_curl_add_vmwaf_nat(char *ctrl_ip, char *nat_post);
int call_curl_del_vmwaf_nat(char *ctrl_ip, char *nat_post);
int call_curl_init_ofs_port_confg(char *ctrl_ip, char *ofs_post);
int call_curl_delete_all_flow_table(char *ctrl_ipt);
int call_curl_ofs_port_confg(char *ctrl_ip, char *ofs_post);
int call_curl_patch_all_nsp_vgws(char *lcuuid, char *nsp_post);
int call_curl_patch_all_nsp_valves(char *lcuuid, char *nsp_post);
int call_curl_get_nsp_stat(char *host_ip, char *master_ctrl_ip, char *data);
int call_curl_get_nsp_stat_rt(char *host_ip, char *data);
int call_curl_get_lb_stat(char *host_ip, char *data);
int call_curl_get_switch_load(char *switch_mip, char *data);
int call_curl_get_bk_vm_health(char *host_ip, char *ctrl_ip, char *data);
int call_url_post_vl2_vlan(uint32_t *p_vlantag,
        uint32_t vl2id, uint32_t rackid, uint32_t vlan_request, uint32_t force);

#endif /* _LC_LCMG_API_H */

