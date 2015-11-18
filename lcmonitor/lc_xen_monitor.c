/*
 * Copyright 2012 yunshan network
 * All rights reserved
 * Monitor the state of Hosts and VMs
 * Author:
 * Date:
 */

#include <string.h>
#include "lc_xen_monitor.h"
#include "lc_db_errno.h"
#include "lc_db_log.h"
#include "lc_mon_vmware.h"
#include "lc_update_lcm_db.h"
#include "lc_db_sys.h"
#include "lc_utils.h"
#include "nxjson.h"
#include "lc_lcmg_api.h"
#include "lc_livegate_api.h"

#include "lc_header.h"
#include "lc_postman.h"
#include <ctype.h>
#include "talker.pb-c.h"

extern pthread_mutex_t lc_dpq_mutex;
/*extern pthread_mutex_t lc_host_msg_mutex;*/
extern pthread_cond_t lc_dpq_cond;
extern pthread_mutex_t lc_dpq_nfs_chk_mutex;
extern pthread_cond_t lc_dpq_nfs_chk_cond;
extern pthread_mutex_t lc_dpq_vgw_ha_mutex;
extern pthread_cond_t lc_dpq_vgw_ha_cond;
extern pthread_mutex_t lc_dpq_bk_vm_health_mutex;
extern pthread_cond_t lc_dpq_bk_vm_health_cond;
#define LC_UPDATE_VM_STATE  "mysql -uroot -psecurity421 -D livecloud -e" \
                            "\"update vm_v%s set action=0,state=9," \
                            "cpu_start_time=0,check_start_time=0 " \
                            "where state>1 and state<11 and launch_server=\'%s\'\""
#define LC_UPDATE_VGW_STATE  "mysql -uroot -psecurity421 -D livecloud -e" \
                            "\"update vnet_v%s set action=0,state=9, " \
                            "cpu_start_time=0,check_start_time=0 " \
                            "where state=7 and gw_launch_server=\'%s\'\""

#define LC_INCLUDE_TMP_STATE 0
#define LC_EXCLUDE_TMP_STATE 1

static const char *vmwaf_lookup_table[] =
{
    "NONE",
    "RUNNING",
    "EXCEPTION",
    "HALTED"
};

static const char *vm_lookup_table[] =
{
    "Temp",
    "Creating",
    "Complete",
    "Starting",
    "RUNNING",
    "Suspending",
    "Paused",
    "Resuming",
    "Stopping",
    "HALTED",
    "Modify",
    "Error",
    "Destroying",
    "DESTROYED"
};

static const char *gw_lookup_table[] =
{
    "Temp",
    "Creating",
    "Complete",
    "Error",
    "Modify",
    "Destroying",
    "Starting",
    "RUNNING",
    "Stopping",
    "HALTED",
    "DESTROYED"
};

const char *
lc_vmwaf_state_to_string(int val)
{
    return vmwaf_lookup_table[val];
}

const char *
lc_vm_state_to_string(int val)
{
    return vm_lookup_table[val];
}

const char *
lc_gw_state_to_string(int val)
{
    return gw_lookup_table[val];
}
const char *lc_vwaf_state_to_string(int val)
{
    return vmwaf_lookup_table[val];
}

const char *
lc_state_to_string(int val, int type)
{
    if (type == LC_VM_TYPE_VM) {
        return lc_vm_state_to_string(val);
    } else {
        return lc_gw_state_to_string(val);
    }
}

inline char* strlupper( char* str )
{
    char* orig = str;
    for ( ; *str != '\0'; str++ ) {
         *str = toupper(*str);
    }
    return orig;
}

#define NOTIFY_ONE_OBJ 1
static int lc_monitor_notify(uint32_t id, int type, int state)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__NotifyProactive *notify = NULL;
    Talker__NotifyBundleProactive notify_bundle = TALKER__NOTIFY_BUNDLE_PROACTIVE__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len = 0, i = 0, count = NOTIFY_ONE_OBJ;
    char buf[MAX_MSG_BUFFER];

    memset(buf, 0, MAX_MSG_BUFFER);

    notify_bundle.n_bundle = count;
    notify_bundle.bundle =
            (Talker__NotifyProactive **)malloc(count * sizeof(Talker__NotifyProactive *));
    if (!notify_bundle.bundle) {
        return -1;
    }
    notify = (Talker__NotifyProactive *)malloc(count * sizeof(Talker__NotifyProactive));
    if (!notify) {
        free(notify_bundle.bundle);
        return -1;
    }
    memset(notify, 0, count * sizeof(Talker__NotifyProactive));
    for (i = 0; i < count; i ++) {
         talker__notify_proactive__init(&notify[i]);
         notify[i].id = id;
         notify[i].type = type;
         notify[i].state = state;
         notify[i].launch_server = NULL;
         notify[i].has_id = 1;
         notify[i].has_type = 1;
         notify[i].has_state = 1;
         notify_bundle.bundle[i] = &notify[i];
    }

    msg.notify_bundle = &notify_bundle;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    fill_message_header(&hdr, buf);

    free(notify);
    free(notify_bundle.bundle);
    lc_bus_lcmond_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    return 0;
}

static int lc_monitor_notify_bundle(char *host_address)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__NotifyProactive *notify = NULL;
    Talker__NotifyBundleProactive notify_bundle = TALKER__NOTIFY_BUNDLE_PROACTIVE__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len = 0, i = 0, vm_number = 0, vnet_number = 0;
    char buf[MAX_MSG_BUFFER];
    vm_query_info_t vm_query_set[MAX_VM_PER_HOST];
    vnet_query_info_t vnet_query_set[MAX_VM_PER_HOST];

    memset(vm_query_set, 0, sizeof(vm_query_info_t)*MAX_VM_PER_HOST);
    memset(vnet_query_set, 0, sizeof(vnet_query_info_t)*MAX_VM_PER_HOST);
    memset(buf, 0, MAX_MSG_BUFFER);

    lc_get_vm_by_server(vm_query_set, sizeof(vm_query_info_t),
                            &vm_number, host_address, LC_EXCLUDE_TMP_STATE, MAX_VM_PER_HOST);
    if (!vm_number) {
        lc_get_vnet_by_server(vnet_query_set, sizeof(vnet_query_info_t),
                            &vnet_number, host_address, LC_EXCLUDE_TMP_STATE, MAX_VM_PER_HOST);
    }
    if (vm_number) {
        notify_bundle.n_bundle = vm_number;
        notify_bundle.bundle =
            (Talker__NotifyProactive **)malloc(vm_number * sizeof(Talker__NotifyProactive *));
        if (!notify_bundle.bundle) {
            return -1;
        }
        notify = (Talker__NotifyProactive *)malloc(vm_number * sizeof(Talker__NotifyProactive));
        if (!notify) {
            free(notify_bundle.bundle);
            return -1;
        }
        memset(notify, 0, vm_number * sizeof(Talker__NotifyProactive));
        for (i = 0; i < vm_number; i ++) {
             talker__notify_proactive__init(&notify[i]);
             notify[i].id = vm_query_set[i].vm_id;
             notify[i].type = TALKER__RESOURCE_TYPE__VM_RESOURCE;
             notify[i].state = TALKER__RESOURCE_STATE__RESOURCE_DOWN;
             notify[i].launch_server = NULL;
             notify[i].has_id = 1;
             notify[i].has_type = 1;
             notify[i].has_state = 1;
             notify_bundle.bundle[i] = &notify[i];
        }
    } else if (vnet_number) {
        notify_bundle.n_bundle = vnet_number;
        notify_bundle.bundle =
            (Talker__NotifyProactive **)malloc(vnet_number * sizeof(Talker__NotifyProactive *));
        if (!notify_bundle.bundle) {
            return -1;
        }
        notify = (Talker__NotifyProactive *)malloc(vnet_number * sizeof(Talker__NotifyProactive));
        if (!notify) {
            free(notify_bundle.bundle);
            return -1;
        }
        memset(notify, 0, vnet_number * sizeof(Talker__NotifyProactive));
        for (i = 0; i < vnet_number; i ++) {
             talker__notify_proactive__init(&notify[i]);
             notify[i].id = vnet_query_set[i].vm_id;
             notify[i].type = TALKER__RESOURCE_TYPE__VGW_RESOURCE;
             notify[i].state = TALKER__RESOURCE_STATE__RESOURCE_DOWN;
             notify[i].launch_server = NULL;
             notify[i].has_id = 1;
             notify[i].has_type = 1;
             notify[i].has_state = 1;
             notify_bundle.bundle[i] = &notify[i];
        }
    } else {
        return 0;
    }

    msg.notify_bundle = &notify_bundle;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    fill_message_header(&hdr, buf);

    free(notify);
    free(notify_bundle.bundle);
    lc_bus_lcmond_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_TALKER);
    return 0;
}

static int nfs_server_is_available(char *cmd, char *message, char *server_ip, char *ip, char *os)
{
    int result = 0;
    result = lc_call_system(cmd, "", "", "", NULL);
    if (result) {
        switch(result) {
             case LC_MON_NFS_UNREACHABLE:
                snprintf (message, MAX_LOG_MSG_LEN, "Network Unreachable to NFS server %s on %s",
                                                                                  server_ip, ip);
                break;
             case LC_MON_NFS_SERVICE_UNAVAILABLE:
                snprintf (message, MAX_LOG_MSG_LEN, "Invalid NFS Service [%s] on %s",
                                                                      server_ip, ip);
                break;
             case LC_MON_TEMPLATE_VDI_UNPLUG:
                snprintf (message, MAX_LOG_MSG_LEN, "%s's VHD is unplugged on %s",
                                                                          os, ip);
                LCMON_LOG(LOG_WARNING, "%s: %s", __FUNCTION__, message);
                result = 0;
                break;
             case LC_MON_TEMPLATE_VHD_DELETED:
                snprintf (message, MAX_LOG_MSG_LEN, "[%s]%s's VHD file was deleted illegally on NFS server %s",
                                                                                            ip, os, server_ip);
                LCMON_LOG(LOG_WARNING, "%s: %s", __FUNCTION__, message);
                result = 0;
                break;
             case LC_MON_INVALID_NFS_CLIENT:
                snprintf (message, MAX_LOG_MSG_LEN, "Invalid NFS client %s to NFS server %s",
                                                                              ip, server_ip);
                break;
             default:
                LCMON_LOG(LOG_ERR, "Connect NFS unknown error[%s]", cmd);
                result = 0;
                break;
        }
    }
    return result;
}

static int send_email_msg_optional(int type, monitor_host_t *hostp,
        vm_info_t *vm_info, char *email_msg, int level, int flag,
        int option)
{
    header_t hdr;
    char cmd[MAX_CMD_BUF_SIZE];
    time_t current;
    char subject[50];
    char mail_buf[MAX_CMD_BUF_SIZE];
    int is_admin = 0;
    send_request_t send_req;
    char *to = NULL, *objtype = NULL;
    int mlen = sizeof(mail_buf);
    int hlen = 0;
    int ret = 0;
    int addrlen = 0;

    if (vm_info) {
        if (vm_info->vm_type == LC_VM_TYPE_VM) {
            objtype = LC_SYSLOG_OBJECTTYPE_VM;
        } else if(vm_info->vm_type == LC_VM_TYPE_GW){
            objtype = LC_SYSLOG_OBJECTTYPE_VGW;
        }else{
            objtype = LC_SYSLOG_OBJECTTYPE_VMWAF;
        }
        lc_monitor_db_syslog("", objtype, vm_info->vm_id,
                    vm_info->vm_label, email_msg, level);
    } else {
        lc_monitor_db_syslog("", LC_SYSLOG_OBJECTTYPE_HOST,
                         hostp->cr_id, hostp->host_address,
                                         email_msg, level);
    }

    if (type == POSTMAN__RESOURCE__HOST) {
        to = hostp->email_address;
    } else if (type == POSTMAN__RESOURCE__VM) {
        to = vm_info->email_address;
    }else if(type == POSTMAN__RESOURCE__VMWAF){
        to = vm_info->email_address;
    }
    if (NULL == to) {
        return 0;
    }

    memset(subject, 0, 50);
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&send_req, 0, sizeof(send_request_t));
    memset(mail_buf, 0, sizeof(mail_buf));

    current = time(NULL);
    send_req.policy.priority = POSTMAN__PRIORITY__URGENT;
    if (option) {
        send_req.policy.aggregate_id = POSTMAN__AGGREGATE_ID__AGG_NO_AGGREGATE;
        send_req.policy.min_event_interval = 0;
    } else {
        send_req.policy.aggregate_id = POSTMAN__AGGREGATE_ID__AGG_ALARM_EVENT;
        send_req.policy.aggregate_window = 10;
        send_req.policy.min_event_interval = 86400;
    }
    send_req.policy.min_dest_interval = 0;
    send_req.policy.issue_timestamp = current;
    if (type == POSTMAN__RESOURCE__HOST) {
        if (level == LC_SYSLOG_LEVEL_WARNING) {
            if (option) {
                send_req.id.event_type = POSTMAN__EVENT__NSP_FAIL;
            } else {
                send_req.id.event_type = POSTMAN__EVENT__HOST_DISCONN;
            }
        } else {
            if (option) {
                send_req.id.event_type = POSTMAN__EVENT__NSP_FAIL_OVER;
            } else {
                send_req.id.event_type = POSTMAN__EVENT__HOST_DISCONN_OVER;
            }
        }
        send_req.id.resource_type = POSTMAN__RESOURCE__HOST;
        send_req.id.resource_id = hostp->cr_id;
    } else if (type == POSTMAN__RESOURCE__VM) {
        send_req.id.event_type = POSTMAN__EVENT__VM_STATE_CHG;
        send_req.id.resource_type = POSTMAN__RESOURCE__VM;
        send_req.id.resource_id = vm_info->vm_id;
    }else if(type == POSTMAN__RESOURCE__VMWAF){
        send_req.id.event_type = POSTMAN__EVENT__VM_STATE_CHG;
        send_req.id.resource_type = POSTMAN__RESOURCE__VMWAF;
        send_req.id.resource_id = vm_info->vm_id;
    }
    strcpy(send_req.mail.from, hostp->from);
    strcpy(send_req.mail.to, to);
    if (level == LC_SYSLOG_LEVEL_WARNING) {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__ALARM_MAIL;
    } else {
        send_req.mail.mail_type = POSTMAN__MAIL_TYPE__NOTIFY_MAIL;
    }
    if (type == POSTMAN__RESOURCE__HOST) {
        if (option) {
            strcpy(send_req.mail.head_template, "head_notify_nsp_ha_to_admin");
            strcpy(send_req.mail.tail_template, "tail_notify_nsp_ha_to_admin");
            strcpy(send_req.mail.resource_type, "NSP");
        } else {
            strcpy(send_req.mail.head_template, "head_alarm_to_admin");
            strcpy(send_req.mail.tail_template, "tail_alarm_to_admin");
            strcpy(send_req.mail.resource_type, "Host");
        }
        strcpy(send_req.mail.resource_name, hostp->host_address);
        strcpy(subject, "Controller Message");
        strcpy(send_req.mail.subject, subject);
    } else if (type == POSTMAN__RESOURCE__VM) {
        if (flag == LC_MONITOR_POSTMAN_NO_FLAG){
            strcpy(send_req.mail.head_template, "head_alarm_to_user");
            strcpy(send_req.mail.tail_template, "tail_alarm_to_user");
            strcpy(send_req.mail.resource_type, "VM");
            strcpy(send_req.mail.resource_name, vm_info->vm_name);
            strcpy(subject, "State Alarm Message");
            strcpy(send_req.mail.subject, subject);
        }
        else{
            strcpy(send_req.mail.head_template,"head_notify_to_user");
            strcpy(send_req.mail.tail_template,"tail_notify_to_user");
            strcpy(send_req.mail.resource_type,"VM");
            strcpy(send_req.mail.resource_name,vm_info->vm_name);
            strcpy(subject, "State Notification Message");
            strcpy(send_req.mail.subject, subject);
        }
    }else if (type == POSTMAN__RESOURCE__VMWAF) {
        if (flag == LC_MONITOR_POSTMAN_NO_FLAG){
            strcpy(send_req.mail.head_template, "head_alarm_to_user");
            strcpy(send_req.mail.tail_template, "tail_alarm_to_user");
            strcpy(send_req.mail.resource_type, "VMWAF");
            strcpy(send_req.mail.resource_name, vm_info->vm_name);
            strcpy(subject, "State Alarm Message");
            strcpy(send_req.mail.subject, subject);
        }
        else{
            strcpy(send_req.mail.head_template,"head_notify_to_user");
            strcpy(send_req.mail.tail_template,"tail_notify_to_user");
            strcpy(send_req.mail.resource_type,"VMWAF");
            strcpy(send_req.mail.resource_name,vm_info->vm_name);
            strcpy(subject, "State Notification Message");
            strcpy(send_req.mail.subject, subject);
        }
    }
    if (hostp->email_enable) {
        if (0 == strcmp(to, hostp->email_address)) {
            is_admin = 1;
        } else {
            strcpy(send_req.mail.cc, hostp->email_address);
        }
    }
    if (hostp->email_address_ccs[0]) {
        addrlen = strlen(send_req.mail.cc);
        snprintf(send_req.mail.cc + addrlen,
                 MAIL_ADDR_LEN - addrlen,",%s", hostp->email_address_ccs);
    }

    strncpy(send_req.mail.alarm_message, email_msg, LC_EMAIL_ALARM_MESSAGE_LEN);
    send_req.mail.alarm_begin = current;

    /* publish send_request to postman */

    hlen = get_message_header_pb_len();
    mlen = construct_send_request(&send_req, mail_buf + hlen, mlen - hlen);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = mlen;
    hdr.seq = 0;
    fill_message_header(&hdr, mail_buf);

    ret = lc_bus_lcmond_publish_unicast(mail_buf, mlen + hlen, LC_BUS_QUEUE_POSTMAND);
    if (ret) {
        LCMON_LOG(LOG_ERR, "%s error send mail to postman", __FUNCTION__);
    }

    return 0;
}

int send_host_disconnect_email(int type, monitor_host_t *hostp,
        vm_info_t *vm_info, char *email_msg, int level, int flag)
{
    return send_email_msg_optional(type, hostp,
            vm_info, email_msg, level, flag, 0);
}

int send_nsp_fail_email(int type, monitor_host_t *hostp,
        vm_info_t *vm_info, char *email_msg, int level, int flag)
{
    return send_email_msg_optional(type, hostp,
            vm_info, email_msg, level, flag, 1);
}

int lc_monitor_connect_host_failure(monitor_host_t *hostp)
{
    char message[100];
    char buffer[200];

    memset(buffer, 0, 200);
    memset(message, 0, 100);
    /*When host connection failed, monitor will change the state of compute_resource table to error
           regardless of the previous state is transient or not*/
    if (hostp->host_state != LC_CR_STATE_ERROR) {
        hostp->host_state = LC_CR_STATE_ERROR;
    }
    if (hostp->error_number != LC_CR_ERRNO_CONN_FAILURE) {
        hostp->error_number = LC_CR_ERRNO_CONN_FAILURE;
    }
    sprintf(message, "Failed to connect Host %s", hostp->host_address);
    send_host_disconnect_email(POSTMAN__RESOURCE__HOST, hostp, NULL, message,
            LC_SYSLOG_LEVEL_WARNING, LC_MONITOR_POSTMAN_NO_FLAG);
    if (hostp->health_check >= LC_MONITOR_HOST_STATE_THRESHOLD) {
        /*The host disconnect and update the state of vm and vnet to Halted*/
        lc_monitor_notify_bundle(hostp->host_address);
        sprintf(buffer, LC_UPDATE_VM_STATE, LC_DB_VERSION, hostp->host_address);
        lc_call_system(buffer, "Monitor", "", "", NULL);
        memset(buffer, 0, 200);
        sprintf(buffer, LC_UPDATE_VGW_STATE, LC_DB_VERSION, hostp->host_address);
        lc_call_system(buffer, "Monitor", "", "", NULL);
        if (hostp->health_check == LC_MONITOR_HOST_STATE_THRESHOLD) {
            /*Only show out once*/
            memset(message, 0, 100);
            sprintf(message, "Stopping all VMs or vGWs because we lost the host");
            lc_monitor_db_syslog("", LC_SYSLOG_OBJECTTYPE_HOST,
                             hostp->cr_id, hostp->host_address,
                             message, LC_SYSLOG_LEVEL_WARNING);
        }
    }
    return LC_CR_STATE_ERROR;
}

int lc_monitor_host_disconnected(monitor_host_t *hostp)
{
    host_t host;
    int ret = 0;

    memset(&host, 0, sizeof(host_t));
    lc_vmdb_compute_resource_load_by_col(&host,
           hostp->host_address, "health_state");
    if (host.health_check ++ >= LC_MONITOR_HOST_STATE_THRESHOLD) {
        ret = 1;
    }
    lc_vmdb_master_compute_resource_update_health_check(host.health_check,
                                              hostp->host_address);
    hostp->health_check = host.health_check;
    return ret;
}

int lc_mon_get_config_info(monitor_host_t *pmon_host, int is_host)
{
    monitor_config_t mon_cfg;

    memset(&mon_cfg, 0, sizeof(monitor_config_t));
    lc_get_config_info_from_system_config(&mon_cfg);

    if (is_host) {
        pmon_host->host_cpu_threshold = mon_cfg.host_cpu_threshold;
        pmon_host->host_cpu_warning_usage = mon_cfg.host_cpu_warning_usage;
        pmon_host->host_disk_threshold = mon_cfg.host_disk_threshold;
        pmon_host->host_disk_warning_usage = mon_cfg.host_disk_warning_usage;
        pmon_host->host_check_interval = mon_cfg.host_warning_interval;
    } else {
        pmon_host->vm_cpu_threshold = mon_cfg.vm_cpu_threshold;
        pmon_host->vm_cpu_warning_usage = mon_cfg.vm_cpu_warning_usage;
        pmon_host->vm_disk_threshold = mon_cfg.vm_disk_threshold;
        pmon_host->vm_disk_warning_usage = mon_cfg.vm_disk_warning_usage;
        pmon_host->vm_check_interval = mon_cfg.vm_warning_interval;
    }
    if (mon_cfg.email_address[0]) {
        strcpy(pmon_host->email_address, mon_cfg.email_address);
        if (!strlen(mon_cfg.from)) {
            strcpy(pmon_host->from, "stats@yunshan.net.cn");
        } else {
            strcpy(pmon_host->from, mon_cfg.from);
        }
        pmon_host->email_enable = 1;
    } else {
        pmon_host->email_enable = 0;
    }
    if (mon_cfg.email_address_ccs[0]) {
        strcpy(pmon_host->email_address_ccs, mon_cfg.email_address_ccs);
    }

    return 0;

}

int lc_monitor_get_vm_res_usg_fields(vm_result_t *pvm, monitor_host_t *phost, vm_info_t *vm_info, int htype)
{
    return 0;
}

static int vgw_name_process(void *vgw_name, char *field, char *value)
{
    char *name = (char *)vgw_name;
    if (strcmp(field, "name") == 0) {
        strcpy(name, value);
    }
    return 0;
}

int lc_get_vgw_name_from_fdb(char *vgw_name, int id)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    return lc_db_table_load((void *)vgw_name, "fdb_vgw", "name", condition, vgw_name_process);
}
int lc_get_vmwaf_current_state(char *state)
{
    if (strcmp(state, "running") == 0) {
        return LC_VMWAF_STATE_RUNING;
    } else if (strcmp(state, "halted") == 0 ) {
        return LC_VMWAF_STATE_HELTED;
    }
    return LC_VMWAF_STATE_EXCEPTION;
}

int lc_monitor_vm_state(vm_result_t *pvm, monitor_host_t *phost, char *host_address)
{
    char message[MAX_LOG_MSG_LEN];
    vm_info_t vm_info;
    vmwaf_query_info_vm_t vmwaf_info;
    int lcstate = 0;
    int vmstate = 0;
    int code = 0;
    int care_state = 0;
    int real_state = 0;
    char vgw_name[MAX_VM_NAME_LEN];
    int htype = phost->htype;
    int flag = LC_MONITOR_POSTMAN_NO_FLAG;
    int iRet = 0;
    memset(message, 0, sizeof(message));
    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&vmwaf_info, 0, sizeof(vmwaf_query_info_vm_t));

    vm_info.vm_type = LC_VM_TYPE_VM;
    code = lc_get_vm_from_db_by_namelable(&vm_info, pvm->vm_label);
    if (code == LC_DB_VM_FOUND) {
        vm_info.vm_type = LC_VM_TYPE_VM;
    } else {
        code = lc_get_vedge_from_db_by_namelable(&vm_info, pvm->vm_label);
        if (code == LC_DB_VM_FOUND) {
            vm_info.vm_type = LC_VM_TYPE_GW;
        } else {
            code = lc_db_get_vmwaf_by_launch_server(&vmwaf_info, host_address);
            if(code == LC_DB_VMWAF_FOUND){
                if (!strcmp(pvm->vm_label,vmwaf_info.label)){
                    LCMON_LOG(LOG_DEBUG,"pvm->vm_label is : %s ### vmwaf_info.label is %s \r\n",pvm->vm_label,vmwaf_info.label);
                    vm_info.vm_type= LC_VM_TYPE_VMWAF;
                    vm_info.user_id = vmwaf_info.userid;
                    vm_info.vm_id = vmwaf_info.id;
                    memcpy(vm_info.vm_name,vmwaf_info.label,sizeof(vmwaf_info.label));
                    LCMON_LOG(LOG_DEBUG,"vm_info.vm_type = %d", vm_info.vm_type);
                }
            }else{
                    code = LC_DB_VMWAF_NOT_FOUND;
            }
        }
    }
    if (code) {
        if (strlen(pvm->vm_label)) {
            LCMON_LOG(LOG_DEBUG, "The VM %s is %s, but it was lost by controller",
                                                    pvm->vm_label, pvm->state);
        }
        return 0;
    }
    if(vm_info.vm_type == LC_VM_TYPE_VMWAF){
        /*TODO(ZZM) update the state of vmwaf and send alarm Email to livecloud administrator */
        LCMON_LOG(LOG_DEBUG,"The vmwaf real status is :%s\n",pvm->state);
        LCMON_LOG(LOG_DEBUG,"The vmwaf last status is :%d\n",vmwaf_info.state);
        real_state = lc_get_vmwaf_current_state(pvm->state);
        lcstate = vmwaf_info.state;
        /*state changed of vwaf and send Email to livecloud administrator, write real_state to vwaf_v2_2*/
        if(real_state != lcstate){
            iRet = lc_db_master_vmwaf_update_state_by_launch_server(real_state,host_address);
            if (iRet != 0){
                LCMON_LOG(LOG_ERR,"Update vmwaf state fail! iRet = %d\n",iRet);
            }else{
                LCMON_LOG(LOG_DEBUG,"Update vmwaf state success.Curret state is %d\n",real_state);
            }
            /*send Email to livecloud administrator*/
            if(real_state == LC_VMWAF_STATE_RUNING ){
                sprintf(message,"The state changed from %s to %s",lc_vmwaf_state_to_string(lcstate),strlupper(pvm->state));
                flag = LC_MONITOR_POSTMAN_NOTIFY_FLAG;
            }
            else{
                sprintf(message, "Its current state is %s, and the previous state is %s",
                        strlupper(pvm->state),lc_vmwaf_state_to_string(lcstate));
            }

            if (strlen(message)) {
                lc_get_email_address_from_user_db(vm_info.email_address, vm_info.user_id);
                send_host_disconnect_email(POSTMAN__RESOURCE__VMWAF, phost, &vm_info, message,
                        LC_SYSLOG_LEVEL_WARNING, flag);
            }
            return 0;

        }else{
            ;
        }

    } else{
        lc_get_email_address_from_user_db(vm_info.email_address, vm_info.user_id);
        /*Check if the vm's launch server has been changed*/
        if (htype == LC_POOL_TYPE_VMWARE) {
            if (0 != strcmp(vm_info.host_address, host_address)) {
                snprintf (message, MAX_LOG_MSG_LEN, "The VM %s is migrated to Host %s because Host %s is in trouble",
                                                                  pvm->vm_label, host_address, vm_info.host_address);
                #if 0
                send_host_disconnect_email(POSTMAN__RESOURCE__VM, phost, &vm_info, message,
                        LC_SYSLOG_LEVEL_WARNING, flag);
                #endif
                memset(message, 0, sizeof(message));
    #if 0
                /*Update the launch server*/
                if (vm_info.vm_type == LC_VM_TYPE_VM) {
                    lc_vmdb_vm_update_launchserver(host_address, vm_info.vm_id);
                } else {
                    lc_vmdb_vnet_update_vedge_server(host_address, vm_info.vm_id);
                }
    #endif
            }
        } else if (htype == LC_POOL_TYPE_XEN){
            if (0 != strcmp(vm_info.host_address, phost->host_address)) {
                snprintf (message, MAX_LOG_MSG_LEN, "The VM %s is migrated to Host %s because Host %s is in trouble",
                                                           pvm->vm_label, phost->host_address, vm_info.host_address);
                #if 0
                send_host_disconnect_email(POSTMAN__RESOURCE__VM, phost, &vm_info, message,
                        LC_SYSLOG_LEVEL_WARNING, flag);
                #endif
                memset(message, 0, sizeof(message));
            }
        }
        vmstate = lcstate = vm_info.vm_state;
        if (vm_info.vm_type == LC_VM_TYPE_VM) {
            care_state = lc_monitor_care_vmstate(lcstate);
            if (LC_POOL_TYPE_VMWARE == htype) {
                if (lcstate == LC_VM_STATE_TMP) {
                    LCMON_LOG(LOG_DEBUG, "Skipping the TEMP state of VM %s on host %s",
                                               vm_info.vm_name, vm_info.host_address);
                    return 0;
                }
            }
        } else {
            care_state = lc_monitor_care_gwstate(lcstate);
            if (LC_POOL_TYPE_VMWARE == htype) {
                if (lcstate == LC_VEDGE_STATE_TMP) {
                    LCMON_LOG(LOG_DEBUG, "Skipping the TEMP state of vGW %s on host %s",
                                                vm_info.vm_name, vm_info.host_address);
                    return 0;
                }
            }
        }
        real_state = lc_get_vm_current_state(pvm->state, vm_info.vm_type);

        LCMON_LOG(LOG_DEBUG, "Monitor the state of VM %s on host %s",
                           vm_info.vm_name, vm_info.host_address);
        if (vmstate != real_state && care_state) {
            /*Don't care of the inconsistent state if it is set by livecloud*/
            if (vm_info.action == LC_VM_ACTION_DONE) {
                LCMON_LOG(LOG_DEBUG, "Skip the previous state of VM %s is %s"
                                  ", current state is %s", vm_info.vm_name,
                              lc_state_to_string(lcstate, vm_info.vm_type),
                                                    strlupper(pvm->state));
                return 0;
            }
            if (vm_info.vm_type == LC_VM_TYPE_VM) {
                if (real_state == LC_VM_STATE_ERROR
                    && vmstate != LC_VM_STATE_TMP) {
                        real_state = LC_VM_STATE_STOPPED;
                        vm_info.vm_error |= LC_VM_ERRNO_NOEXISTENCE;
                        sprintf(message, "The %s VM %s is NOT existed",
                            lc_vm_state_to_string(lcstate), vm_info.vm_name);
                } else {
                    if ((real_state == LC_VM_STATE_ERROR)
                        && (vmstate == LC_VM_STATE_TMP)) {
                        return 0;
                    }
                    if ((vmstate == LC_VM_STATE_ERROR)
                        && (lc_monitor_skip_vmerrno(vm_info.vm_error))) {
                        return 0;
                    }
                    vm_info.vm_error = 0;
                    if(real_state == LC_VM_STATE_RUNNING ){
                        lc_monitor_notify(vm_info.vm_id, TALKER__RESOURCE_TYPE__VM_RESOURCE,
                                                       TALKER__RESOURCE_STATE__RESOURCE_UP);
                        sprintf(message,"The state changed from %s to %s",lc_vm_state_to_string(lcstate),strlupper(pvm->state));
                        flag = LC_MONITOR_POSTMAN_NOTIFY_FLAG;
                    }
                    else{
                        lc_monitor_notify(vm_info.vm_id, TALKER__RESOURCE_TYPE__VM_RESOURCE,
                                                     TALKER__RESOURCE_STATE__RESOURCE_DOWN);
                        sprintf(message, "Its current state is %s, and the previous state is %s",
                                strlupper(pvm->state),lc_vm_state_to_string(lcstate));
                    }

                }
                lc_vmdb_master_vm_update_state(real_state, vm_info.vm_id);
                lc_vmdb_master_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
            } else {
                if (real_state == LC_VEDGE_STATE_ERROR
                    && vmstate != LC_VEDGE_STATE_TMP) {
                        real_state = LC_VEDGE_STATE_STOPPED;
                        vm_info.vm_error |= LC_VEDGE_ERRNO_NOEXISTENCE;
                        sprintf(message, "The %s vGW %s is NOT existed",
                           lc_gw_state_to_string(lcstate), vm_info.vm_name);
                } else {
                    if ((real_state == LC_VEDGE_STATE_ERROR)
                        && (vmstate == LC_VEDGE_STATE_TMP)) {
                        return 0;
                    }
                    if ((vmstate == LC_VEDGE_STATE_ERROR)
                        && (lc_monitor_skip_gwerrno(vm_info.vm_error))) {
                        return 0;
                    }
                    vm_info.vm_error = 0;
                    if(real_state == LC_VEDGE_STATE_RUNNING){
                        lc_monitor_notify(vm_info.vm_id, TALKER__RESOURCE_TYPE__VGW_RESOURCE,
                                                        TALKER__RESOURCE_STATE__RESOURCE_UP);
                        sprintf(message,"The state changed from %s to %s",lc_vm_state_to_string(lcstate),strlupper(pvm->state));
                        flag = LC_MONITOR_POSTMAN_NOTIFY_FLAG;
                    }
                    else{
                        lc_monitor_notify(vm_info.vm_id, TALKER__RESOURCE_TYPE__VGW_RESOURCE,
                                                      TALKER__RESOURCE_STATE__RESOURCE_DOWN);
                        sprintf(message, "Its current state is %s, and the previous state is %s",
                                strlupper(pvm->state),lc_vm_state_to_string(lcstate));
                    }
                }
                lc_vmdb_master_vedge_update_state(real_state, vm_info.vm_id);
                lc_vmdb_master_vedge_update_errno(vm_info.vm_error, vm_info.vm_id);
                memset(vgw_name, 0, MAX_VM_NAME_LEN);
                if (lc_get_vgw_name_from_fdb(vgw_name, vm_info.vm_id) == 0) {
                    memset(vm_info.vm_name, 0, MAX_VM_NAME_LEN);
                    strcpy(vm_info.vm_name, vgw_name);
                }
            }
        } else {
            /*if the state is consistent or the state is error, update to 0*/
            if (vm_info.action == LC_VM_ACTION_DONE) {
                if (vm_info.vm_type == LC_VM_TYPE_VM) {
                    lc_vmdb_master_vm_update_action(LC_VM_ACTION_CLEAR, vm_info.vm_id);
                } else {
                    lc_vmdb_master_vnet_update_action(LC_VM_ACTION_CLEAR, vm_info.vm_id);
                }
            }
        }

        if (real_state == LC_VM_STATE_RUNNING
            || real_state == LC_VEDGE_STATE_RUNNING) {
            lc_monitor_get_vm_res_usg_fields(pvm, phost, &vm_info, htype);
        } else {
            vm_info.cpu_start_time = 0;
            vm_info.disk_start_time = 0;
            vm_info.check_start_time = 0;
            if (vm_info.vm_type == LC_VM_TYPE_VM) {
                lc_vmdb_master_vm_update_time(&vm_info);
            } else {
                lc_vmdb_master_vedge_update_time(&vm_info);
            }
        }
        #if 0
        if (strlen(message)) {
            send_host_disconnect_email(POSTMAN__RESOURCE__VM, phost, &vm_info, message,
                    LC_SYSLOG_LEVEL_WARNING, flag);
        }
        #endif
    }
    return 0;
}
#if 0
static int lc_monitor_check_remote_template_state(template_os_info_t *p, int count, char *ip)
{
    char cmd[MAX_CMD_BUF_SIZE];
    char message[MAX_LOG_MSG_LEN];
    host_t    host;
    monitor_host_t mon_host;
    int result = 0, i = 0;
    lc_data_nfs_chk_t *pchk_data = NULL;


    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&host, 0, sizeof(host_t));
    memset(message, 0, sizeof(message));
    memset(&mon_host, 0, sizeof(monitor_host_t));

    lc_mon_get_config_info(&mon_host, 1);

    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&host, ip) != LC_DB_HOST_FOUND) {
        LCMON_LOG(LOG_ERR, "%s: host %s not found",
                __FUNCTION__, ip);
        return -1;
    }

    mon_host.cr_id = host.cr_id;
    snprintf(cmd, MAX_CMD_BUF_SIZE, LC_XEN_CHECK_TEMPLATE_STR, ip);
    result = lc_call_system(cmd, "", "", "", NULL);
    if (result) {
        switch(result) {
             case LC_MON_XENSERVER_XAPI_DOWN:
                snprintf (message, MAX_LOG_MSG_LEN, "XAPI works abnormally on XenServer %s",
                                                                                        ip);
                LCMON_LOG(LOG_WARNING, "%s", message);
/*                send_host_disconnect_email(POSTMAN__RESOURCE__HOST, &mon_host, NULL, message,
                                                        LC_SYSLOG_LEVEL_WARNING);*/
                break;
             default:
                LCMON_LOG(LOG_ERR, "Check NFS status unknown error[%s]", cmd);
                result = 0;
                break;
        }
        return 0;
    }
    for (i = 0; i < count; i ++) {

        if (lc_nfs_chk_dpq_len(&lc_nfs_chk_data_queue) > MAX_DATAQ_LEN) {
            break;
        }

        pchk_data = (lc_data_nfs_chk_t *)malloc(sizeof(lc_data_nfs_chk_t));
        if (!pchk_data) {
            break;
        }
        memset(pchk_data, 0, sizeof(lc_data_nfs_chk_t));
        strcpy(pchk_data->host_addr, ip);
        strcpy(pchk_data->server_ip, p[i].server_ip);
        strcpy(pchk_data->vm_template, p[i].vm_template);
        pchk_data->state = p[i].state;
        pchk_data->htype = p[i].htype;

        pthread_mutex_lock(&lc_dpq_nfs_chk_mutex);
        lc_nfs_chk_dpq_eq(&lc_nfs_chk_data_queue, pchk_data);
        pthread_cond_signal(&lc_dpq_nfs_chk_cond);
        pthread_mutex_unlock(&lc_dpq_nfs_chk_mutex);
    }
    return 0;
}
#endif
static int lc_monitor_request_resource(char *server, uint32_t htype, uint32_t type)
{
    int vm_number = 0, vm_count = 0, mlen = 0;
    int vnet_number= 0, vnet_count = 0;
    int vmwaf_number = 0, vmwaf_count = 0;
    int i = 0, remote_template_count = 0;
    int k = 0, res = 0;
    vm_query_info_t vm_query_set[MAX_VM_PER_HOST];
    vnet_query_info_t vnet_query_set[MAX_VM_PER_HOST];
    agent_vm_vif_id_t *vm_set = NULL, *free_vm_set = NULL;
    intf_t intf_info;
    char buf[MAX_MSG_BUFFER];
    vmwaf_query_info_vm_t vmwaf_info;

    memset(&vmwaf_info, 0, sizeof(vmwaf_query_info_vm_t));
    memset(vm_query_set, 0, sizeof(vm_query_info_t)*MAX_VM_PER_HOST);
    memset(vnet_query_set, 0, sizeof(vnet_query_info_t)*MAX_VM_PER_HOST);

    if (type == LC_HOST_ROLE_SERVER) {
        lc_get_vm_by_server(vm_query_set, sizeof(vm_query_info_t), &vm_number,
                               server, LC_INCLUDE_TMP_STATE, MAX_VM_PER_HOST);
        if (vm_number == MAX_VM_PER_HOST) {
            LCMON_LOG(LOG_WARNING, "VMs MAYBE exceed the max value %d on host %s",
                                                      MAX_VM_PER_HOST, server);
        }
    } else if (type == LC_HOST_ROLE_GATEWAY) {
        lc_get_vnet_by_server(vnet_query_set, sizeof(vnet_query_info_t), &vnet_number,
                                       server, LC_INCLUDE_TMP_STATE, MAX_VM_PER_HOST);
        if (vnet_number == MAX_VM_PER_HOST) {
            LCMON_LOG(LOG_WARNING, "vGWs number MAYBE exceed the max value %d on host %s",
                                                              MAX_VM_PER_HOST, server);
        }
        res = lc_db_get_vmwaf_by_launch_server(&vmwaf_info, server);
        if( res == LC_DB_VMWAF_FOUND){
            vmwaf_number = 1;
        }
    }

    if (vm_number) {
        vm_set = malloc(sizeof(agent_vm_vif_id_t)*vm_number);
        if (!vm_set) {
            return -1;
        }
#if 0
        ptemplate_info = malloc(sizeof(template_os_info_t)*vm_number);
        if (!ptemplate_info) {
            free(vm_set);
            return -1;
        }
        memset(ptemplate_info, 0, sizeof(template_os_info_t)*vm_number);
#endif
        memset(vm_set, 0, sizeof(agent_vm_vif_id_t)*vm_number);
        free_vm_set = vm_set;

        for (i = 0; i < vm_number; i ++) {
             vm_query_info_t *pvm_query = &vm_query_set[i];
             /*vm/vgw's name label MUST not be overlapped*/
             if (!strlen(pvm_query->vm_label)) {
                 continue;
             }
             strcpy(vm_set->vm_label, pvm_query->vm_label);
             vm_set->vm_id = pvm_query->vm_id;
             vm_set->vm_res_type = VM_RES_ALL;
             vm_set->vif_num = LC_VM_MAX_VIF;
             for (k = 0; k < vm_set->vif_num; k ++) {
                 memset(&intf_info, 0, sizeof(intf_t));
                 lc_vmdb_vif_load_by_id(&intf_info, k, LC_VM_TYPE_VM, pvm_query->vm_id);
                 vm_set->vifs[k].vlan_tag = intf_info.if_vlan;
                 strcpy(vm_set->vifs[k].mac, intf_info.if_mac);
             }
             vm_set ++;
             vm_count ++;
#if 0
             /*XEN/VMWare share the same NFS server, so it's enough to
              * only check xen vm*/
             /* V3.3 support local template */
             if (htype == LC_POOL_TYPE_XEN) {
                 memset(&template_info, 0, sizeof(template_os_info_t));
                 skip = 0;
                 if (lc_get_remote_template_info(&template_info,
                             pvm_query->vm_template) != LC_DB_VM_FOUND) {
                     continue;
                 }
                 if (strlen(template_info.server_ip) == 0) {
                     continue;
                 }
                 for (j = 0; j < remote_template_count; j ++) {
                     if (strcmp(template_info.server_ip, ptemplate_info[j].server_ip) == 0) {
                         skip = 1;
                         break;
                     }
                 }
                 if (!skip) {
                     strcpy(ptemplate_info[j].server_ip, template_info.server_ip);
                     strcpy(ptemplate_info[j].vm_template, template_info.vm_template);
                     ptemplate_info[j].htype = template_info.htype;
                     ptemplate_info[j].state = template_info.state;
                     remote_template_count ++;
                 }
             }
#endif
        }
        if (htype == LC_POOL_TYPE_VMWARE) {
            memset(buf, 0, MAX_MSG_BUFFER);
            if ((mlen = lc_vcd_vm_stats_msg(free_vm_set, vm_count, buf)) > 0) {
                lc_bus_lcmond_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER);
            }
        } else if (htype == LC_POOL_TYPE_XEN || htype == LC_POOL_TYPE_KVM) {
            lc_agent_request_vm_monitor(free_vm_set, vm_count, server);
        }
        if (free_vm_set) {
            free(free_vm_set);
        }
    }
    remote_template_count = 0;
    if (vnet_number) {
        vm_set = malloc(sizeof(agent_vm_vif_id_t)*vnet_number);
        if (!vm_set) {
            return -1;
        }
#if 0
        ptemplate_info = malloc(sizeof(template_os_info_t)*vnet_number);
        if (!ptemplate_info) {
            free(vm_set);
            return -1;
        }
        memset(ptemplate_info, 0, sizeof(template_os_info_t)*vnet_number);
#endif
        memset(vm_set, 0, sizeof(agent_vm_vif_id_t)*vnet_number);
        free_vm_set = vm_set;

        for (i = 0; i < vnet_number; i ++) {
             vnet_query_info_t *pvm_query = &vnet_query_set[i];
             /*vm/vgw's name label MUST not be overlapped*/
             if (!strlen(pvm_query->vm_label)) {
                 continue;
             }
             strcpy(vm_set->vm_label, pvm_query->vm_label);
             vm_set->vm_res_type |= VM_RES_STATE | VM_RES_CPU_NUM | VM_RES_CPU_USAGE;
             vm_set->vif_num = LC_VM_MAX_VIF;
             for (k = 0; k < vm_set->vif_num; k ++) {
                 /*load the  mac and vlantag*/
                 memset(&intf_info, 0, sizeof(intf_t));
                 lc_vmdb_vif_load_by_id(&intf_info, k, LC_VM_TYPE_GW, pvm_query->vm_id);
                 vm_set->vifs[k].vlan_tag = intf_info.if_vlan;
                 strcpy(vm_set->vifs[k].mac, intf_info.if_mac);
             }

             vm_set ++;
             vnet_count ++;

#if 0
             /*XEN/VMWare share the same NFS server, so it's enough to
              * only check xen vm*/
             /* V3.3 support local template */
             if (htype == LC_POOL_TYPE_XEN) {
                 memset(&template_info, 0, sizeof(template_os_info_t));
                 skip = 0;
                 if (lc_get_remote_template_info(&template_info,
                             pvm_query->vm_template) != LC_DB_VM_FOUND) {
                     continue;
                 }
                 for (j = 0; j < remote_template_count; j ++) {
                     if (strcmp(template_info.server_ip, ptemplate_info[j].server_ip) == 0) {
                         skip = 1;
                         break;
                     }
                 }
                 if (!skip) {
                     strcpy(ptemplate_info[j].server_ip, template_info.server_ip);
                     strcpy(ptemplate_info[j].vm_template, template_info.vm_template);
                     ptemplate_info[j].htype = template_info.htype;
                     ptemplate_info[j].state = template_info.state;
                     remote_template_count ++;
                 }
             }
#endif
        }
        if (htype == LC_POOL_TYPE_VMWARE) {
            memset(buf, 0, MAX_MSG_BUFFER);
            if ((mlen = lc_vcd_vm_stats_msg(free_vm_set, vnet_count, buf)) > 0) {
                lc_vcd_request_vm_monitor(lc_sock.sock_rcv_vcd, buf, mlen);
            }
        } else if (htype == LC_POOL_TYPE_XEN) {
            lc_agent_request_vm_monitor(free_vm_set, vnet_count, server);
        }
        if (free_vm_set) {
            free(free_vm_set);
        }
    }

    if (vmwaf_number) {
        vm_set = malloc(sizeof(agent_vm_vif_id_t)*vmwaf_number);
        if (!vm_set) {
            return -1;
        }
        memset(vm_set, 0, sizeof(agent_vm_vif_id_t)*vmwaf_number);

        /*vm/vgw's name label MUST not be overlapped*/
        if (strlen(vmwaf_info.label)) {
            strcpy(vm_set->vm_label, vmwaf_info.label);
            vm_set->vm_res_type |= VM_RES_STATE | VM_RES_CPU_NUM | VM_RES_CPU_USAGE;
            vm_set->vif_num = 0;
            vmwaf_count ++;
            if (htype == LC_POOL_TYPE_XEN) {
                lc_agent_request_vm_monitor(vm_set, vmwaf_count, server);
            }
            if (vm_set) {
                free(vm_set);
            }
        }
    }
    return 0;
}

static int lc_monitor_check_host_state(monitor_host_t *hostp)
{
    char msg[50];
    char message[100];
    int  host_state = 0;
    char host_address[MAX_HOST_ADDRESS_LEN];
    char buf[MAX_MSG_BUFFER];
    int mlen = 0;
    time_t current = time(NULL);
    time_t pre_time = hostp->stats_time;

    memset(message,0,100);
    memset(host_address, 0, MAX_HOST_ADDRESS_LEN);
    memset(buf, 0, MAX_MSG_BUFFER);

    if ( hostp->htype == LC_POOL_TYPE_XEN ||
        (hostp->htype == LC_POOL_TYPE_KVM && hostp->type == LC_HOST_ROLE_SERVER)) {
        if (lc_monitor_host_disconnected(hostp)) {
            lc_monitor_connect_host_failure(hostp);
            lc_vmdb_master_compute_resource_state_multi_update(hostp->host_state,
                                                               hostp->error_number,
                                                               hostp->health_check,
                                                               hostp->host_address);
            /*Also update the state of host_device */
            lc_vmdb_master_host_device_state_update(LC_HOST_STATE_ERROR, hostp->host_address);
            lc_monitor_notify(hostp->cr_id, TALKER__RESOURCE_TYPE__COMPUTE_RESOUCE,
                                            TALKER__RESOURCE_STATE__RESOURCE_DOWN);
            hostp->host_cpu_start_time = 0;
            hostp->host_disk_start_time = 0;
            hostp->host_check_start_time = 0;
            lc_vmdb_master_compute_resource_time_update(hostp->host_cpu_start_time,
                                                        hostp->host_disk_start_time,
                                                        hostp->host_check_start_time,
                                                        hostp->host_address);
            /*Still try to send request to detect the connection*/
            lc_agent_request_host_monitor(hostp->host_address, hostp->htype, 0, NULL, 0);
            if (hostp) {
                free(hostp);
            }
            return -1;
        }
        strcpy(host_address, hostp->host_address);
        lc_vmdb_load_compute_resource_state(&host_state, host_address);
        if (lc_monitor_care_host_state(host_state)
            && (host_state == LC_CR_STATE_ERROR)) {
            memset(msg, 0, 50);
            lc_vmdb_master_compute_resource_state_multi_update(LC_CR_STATE_COMPLETE,
                                                         0, 0, host_address);
            lc_vmdb_master_compute_resource_update_health_check(0, host_address);
            /*Also update the state of host_device */
            lc_vmdb_master_host_device_state_update(LC_HOST_STATE_COMPLETE, host_address);
            lc_monitor_notify(hostp->cr_id, TALKER__RESOURCE_TYPE__COMPUTE_RESOUCE,
                                              TALKER__RESOURCE_STATE__RESOURCE_UP);
            sprintf(msg, "Host %s comes back.", host_address);
            send_host_disconnect_email(POSTMAN__RESOURCE__HOST, hostp, NULL, msg,
                    LC_SYSLOG_LEVEL_INFO, LC_MONITOR_POSTMAN_NO_FLAG);
        }

        lc_agent_request_host_monitor(hostp->host_address, hostp->htype, 0, NULL, 0);
    } else if (hostp->htype == LC_POOL_TYPE_VMWARE) {

        if ((mlen = lc_vcd_hosts_info_msg(hostp->host_address, buf)) <= 0) {
            LCMON_LOG(LOG_ERR, "%s: get host info  error from vcd", __FUNCTION__);
            if (hostp) {
                free(hostp);
            }
            return -1;
        }
        lc_bus_lcmond_publish_unicast(buf, mlen, LC_BUS_QUEUE_VMADAPTER);
    }
    if (0 == pre_time) {
        pre_time = current;
    }
    if (current - pre_time >= MON_VM_RESOURCE_TIMER) {
        lc_monitor_request_resource(hostp->host_address, hostp->htype, hostp->type);
        lc_vmdb_master_compute_resource_stats_time_update(current, hostp->host_address);
    } else {
        lc_vmdb_master_compute_resource_stats_time_update(pre_time, hostp->host_address);
    }
    if (hostp) {
        free(hostp);
    }
    return 0;
}

int lc_monitor_check_top_half()
{
    int host_number = 0;
    int ha_vnet_num = 0;
    int lb_num = 0;
    int i = 0;
    host_t host_set[MAX_HOST_NUM];
    vgw_ha_query_info_t vgw_set[MAX_VGW_NUM];
    lb_query_info_t lb_set[MAX_LB_NUM];
    monitor_host_t *pmon_host = NULL;
    lc_data_vgw_ha_t *pvgw_ha_data = NULL;
    lc_data_bk_vm_health_t *pbk_vm_health_data = NULL;

    memset(host_set, 0, sizeof(host_t)*MAX_HOST_NUM);

    lc_get_all_compute_resource(host_set, sizeof(host_t), &host_number);

    for (i = 0; i < host_number; i ++) {
        rpool_t pool;
        host_t *phost = &host_set[i];

        memset(&pool, 0, sizeof(rpool_t));
        if (lc_get_pool_by_index(&pool, phost->pool_index) != LC_DB_POOL_FOUND) {
            LCMON_LOG(LOG_DEBUG, "%s: ignore the host %s because it can't belong to any resource pool yet",
                            __FUNCTION__, phost->host_address);
            continue;
        }

        if ((pool.ctype != LC_POOL_TYPE_XEN)
                && (pool.ctype != LC_POOL_TYPE_VMWARE)
                && (pool.ctype != LC_POOL_TYPE_KVM)) {
            LCMON_LOG(LOG_DEBUG, "%s: Ignore the host %s because it is based on unknown hypervisor",
                             __FUNCTION__, phost->host_address);
            continue;
        }

        if (!lc_monitor_care_host_state(phost->host_state)) {
            LCMON_LOG(LOG_DEBUG, "%s: ignore the host %s because its state %d is not valid",
                             __FUNCTION__, phost->host_address, phost->host_state);
            continue;
        }

        if (phost->host_flag & VM_HOST_INUSE) {
            lc_data_t *pdata = NULL;

            /*Load host_device table to get host type*/
            if (lc_vmdb_host_device_load(phost, phost->host_address, "type")
                != LC_DB_HOST_FOUND) {
                LCMON_LOG(LOG_ERR, "host %s not found", phost->host_address);
                continue;
            }
            pmon_host = (monitor_host_t *)malloc(sizeof(monitor_host_t));
            if (NULL == pmon_host) {
                LCMON_LOG(LOG_ERR, "%s: monitor host alloc failed",__FUNCTION__);
                break;
            }
            memset(pmon_host, 0 , sizeof(monitor_host_t));
            pmon_host->htype = pool.ctype;
            pmon_host->stype = pool.stype;
            pmon_host->pool_id = pool.rpool_index;
            pmon_host->role = phost->role;
            pmon_host->type = phost->host_role;
            pmon_host->host_state = phost->host_state;
            pmon_host->error_number = phost->error_number;
            pmon_host->health_check = phost->health_check;
            pmon_host->stats_time = phost->stats_time;

            strcpy(pmon_host->host_address, phost->host_address);
            strcpy(pmon_host->username, phost->username);
            strcpy(pmon_host->password, phost->password);
            pmon_host->cr_id = phost->cr_id;
            lc_mon_get_config_info(pmon_host, 1);

            if (lc_dpq_len(&lc_data_queue) > MAX_DATAQ_LEN) {
                LC_MON_CNT_INC(lc_mon_data_drop);
                free(pmon_host);
                break;
            }

            LC_MON_CNT_INC(lc_mon_data_in);
            pdata = (lc_data_t *)malloc(sizeof(lc_data_t));
            if (!pdata) {
                LC_MON_CNT_INC(lc_buff_alloc_error);
                free(pmon_host);
                break;
            }
            memset(pdata, 0, sizeof(lc_data_t));
            pdata->data_ptr = (intptr_t)pmon_host;

            pthread_mutex_lock(&lc_dpq_mutex);
            LC_MON_CNT_INC(lc_mon_data_enqueue);
            lc_dpq_eq(&lc_data_queue, pdata);
            pthread_cond_signal(&lc_dpq_cond);
            pthread_mutex_unlock(&lc_dpq_mutex);
        }
    }

    memset(vgw_set, 0, sizeof(vgw_ha_query_info_t)*MAX_VGW_NUM);
    lc_get_ha_vnet_ctl_ip_by_domain(vgw_set, sizeof(vgw_ha_query_info_t), &ha_vnet_num, MAX_VGW_NUM, lcmon_domain.lcuuid);

    for (i = 0; i < ha_vnet_num; i ++) {

        if (lc_vgw_ha_dpq_len(&lc_vgw_ha_data_queue) > MAX_DATAQ_LEN) {
            break;
        }

        pvgw_ha_data = (lc_data_vgw_ha_t *)malloc(sizeof(lc_data_vgw_ha_t));
        if (!pvgw_ha_data) {
            break;
        }
        memset(pvgw_ha_data, 0, sizeof(lc_data_vgw_ha_t));
        if (strlen(vgw_set[i].ctl_ip)) {
            strcpy(pvgw_ha_data->vgw_ctl_addr, vgw_set[i].ctl_ip);
        }
        if (strlen(vgw_set[i].label)) {
            strcpy(pvgw_ha_data->label, vgw_set[i].label);
        }
        pvgw_ha_data->ha_state = vgw_set[i].ha_state;
        pvgw_ha_data->id = vgw_set[i].vnet_id;

        pthread_mutex_lock(&lc_dpq_vgw_ha_mutex);
        lc_vgw_ha_dpq_eq(&lc_vgw_ha_data_queue, pvgw_ha_data);
        pthread_cond_signal(&lc_dpq_vgw_ha_cond);
        pthread_mutex_unlock(&lc_dpq_vgw_ha_mutex);
    }

    memset(lb_set, 0, sizeof(lb_query_info_t)*MAX_LB_NUM);
    lc_get_lb_ctl_ip_by_domain(lb_set, sizeof(lb_query_info_t), &lb_num, MAX_LB_NUM, lcmon_domain.lcuuid);

    for (i = 0; i < lb_num; i ++) {

        if (lc_bk_vm_health_dpq_len(&lc_bk_vm_health_data_queue) > MAX_DATAQ_LEN) {
            break;
        }

        pbk_vm_health_data = (lc_data_bk_vm_health_t *)malloc(sizeof(lc_data_bk_vm_health_t));
        if (!pbk_vm_health_data) {
            break;
        }
        memset(pbk_vm_health_data, 0, sizeof(lc_data_bk_vm_health_t));
        if (strlen(lb_set[i].ctl_ip)) {
            strcpy(pbk_vm_health_data->lb_ctl_addr, lb_set[i].ctl_ip);
        }
        if (strlen(lb_set[i].label)) {
            strcpy(pbk_vm_health_data->lb_label, lb_set[i].label);
        }
        if (strlen(lb_set[i].lcuuid)) {
            strcpy(pbk_vm_health_data->lb_lcuuid, lb_set[i].lcuuid);
        }
        if (strlen(lb_set[i].server)) {
            strcpy(pbk_vm_health_data->lb_server, lb_set[i].server);
        }

        pthread_mutex_lock(&lc_dpq_bk_vm_health_mutex);
        lc_bk_vm_health_dpq_eq(&lc_bk_vm_health_data_queue, pbk_vm_health_data);
        pthread_cond_signal(&lc_dpq_bk_vm_health_cond);
        pthread_mutex_unlock(&lc_dpq_bk_vm_health_mutex);
    }
    return 0;
}

int lc_monitor_check_bottom_half(intptr_t data_ptr)
{
    lc_monitor_check_host_state((monitor_host_t *)data_ptr);
    return 0;
}

int host_dpq_handler(void *pdata, int htype)
{
    monitor_host_t mon_host;
    host_t host;
    rpool_t pool;
    int host_state = 0;
    char msg[50];
    int r = 0, i = 0, j = 0, k = 0;
    int disk_total = 0, disk_usage = 0, disk_type = 0;
    msg_host_info_response_t *phost = NULL;
    msg_host_info_response_t vmware_host;
    host_info_response_t *phost_info = NULL;
    vmware_msg_reply_t *pvmware_host_info = NULL;
    vmware_host_info_t host_info;
    char sr_name[SR_NAME_LEN];
    char sr_usage[AGENT_DISK_SIZE_LEN];
    char domain[LC_NAMESIZE];
    char mode[LC_NAMESIZE];
    char lcm_ip[LC_NAMESIZE];
    int n_sr;
    int maintained_state = 0;

    memset(&mon_host, 0, sizeof(monitor_host_t));
    memset(&host, 0, sizeof(host_t));
    memset(domain, 0, sizeof(domain));
    memset(mode, 0, sizeof(mode));
    memset(lcm_ip, 0, sizeof(lcm_ip));
    memset(&pool, 0, sizeof(rpool_t));

    lc_mon_get_config_info(&mon_host, 1);
    if (htype == LC_POOL_TYPE_XEN) {
        phost_info = (host_info_response_t *)pdata;
        phost = (msg_host_info_response_t *)&phost_info->host_info;
        if (lc_get_compute_resource(&host, phost_info->host_addr) != LC_DB_HOST_FOUND) {
            return -1;
        }
        lc_vmdb_master_compute_resource_update_health_check(0, host.host_address);
        if (lc_get_pool_by_index(&pool, host.pool_index) != LC_DB_POOL_FOUND) {
            return -1;
        }
        /*only check the maintenance state in shared pool*/
        if (pool.stype == LC_POOL_TYPE_SHARED) {
            maintained_state = ntohl(phost->state);
            if (maintained_state == 0) {
                /*Normal mode*/
                if (host.host_state == LC_CR_STATE_MAINTAINED) {
#if 0
                    lc_vmdb_master_compute_resource_state_update(LC_CR_STATE_COMPLETE,
                                                            host.host_address);
                    lc_vmdb_master_host_device_state_update(LC_HOST_STATE_COMPLETE,
                                                         host.host_address);
#endif
                    LCMON_LOG(LOG_INFO, "Host %s changed from Maintenance to Normal state",
                                                                     host.host_address);
                }
            } else {
                /*Maintenace mode*/
                if ((host.host_state == LC_CR_STATE_COMPLETE) ||
                    (host.host_state == LC_CR_STATE_ERROR)) {
#if 0
                    lc_vmdb_master_compute_resource_state_update(LC_CR_STATE_MAINTAINED,
                                                              host.host_address);
                    lc_vmdb_master_host_device_state_update(LC_HOST_STATE_MAINTAINED,
                                                           host.host_address);
#endif
                    LCMON_LOG(LOG_INFO, "Host %s changed from Normal to Maintenance state",
                                                                     host.host_address);
                }
            }
        }
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        pvmware_host_info = (vmware_msg_reply_t *)pdata;
        memset(&host_info, 0, sizeof(vmware_host_info_t));
        memset(&vmware_host, 0, sizeof(msg_host_info_response_t));
        r = lc_vcd_host_usage_reply_msg(&host_info, (void *)pvmware_host_info->data, pvmware_host_info->len);
        free(pvmware_host_info->data);
        if (r) {
            LCMON_LOG(LOG_ERR, "%s: Get host reply info error", __FUNCTION__);
            return -1;
        }
        if (lc_get_compute_resource(&host, host_info.host_address) != LC_DB_HOST_FOUND) {
            if (host_info.sr_name) {
                free(host_info.sr_name);
            }
            if (host_info.sr_disk_total) {
                free(host_info.sr_disk_total);
            }
            if (host_info.sr_usage) {
                free(host_info.sr_usage);
            }
            return -1;
        }
        /*Firstly, check the host state*/
        mon_host.host_state = host.host_state;
        mon_host.error_number = host.error_number;
        mon_host.health_check = host.health_check;
        strcpy(mon_host.host_address, host.host_address);
        host_state = host_info.host_state;
        if (host_state != VMWARE_HOST_STATE_CONNECTED) {
            lc_monitor_connect_host_failure(&mon_host);
            lc_vmdb_master_compute_resource_state_multi_update(mon_host.host_state,
                                                               mon_host.error_number,
                                                               mon_host.health_check,
                                                               mon_host.host_address);
            /*Also update the state of host_device */
            lc_vmdb_master_host_device_state_update(LC_HOST_STATE_ERROR, mon_host.host_address);
            lc_monitor_notify(host.cr_id, TALKER__RESOURCE_TYPE__COMPUTE_RESOUCE,
                                           TALKER__RESOURCE_STATE__RESOURCE_DOWN);
            lc_vmdb_master_compute_resource_time_update(0, 0, 0, mon_host.host_address);

            if (host_info.sr_name) {
                free(host_info.sr_name);
            }
            if (host_info.sr_disk_total) {
                free(host_info.sr_disk_total);
            }
            if (host_info.sr_usage) {
                free(host_info.sr_usage);
            }
            return -1;
        }
        lc_vmdb_load_compute_resource_state(&host_state, mon_host.host_address);
        if (lc_monitor_care_host_state(host_state)
            && (host_state == LC_CR_STATE_ERROR)) {
            /* FIXME ugly hard code */
            memset(msg, 0, 50);
            lc_vmdb_master_compute_resource_state_multi_update(LC_CR_STATE_COMPLETE,
                                                         0, 0, mon_host.host_address);
            /*Also update the state of host_device */
            lc_vmdb_master_host_device_state_update(LC_HOST_STATE_COMPLETE, mon_host.host_address);
            lc_monitor_notify(host.cr_id, TALKER__RESOURCE_TYPE__COMPUTE_RESOUCE,
                                            TALKER__RESOURCE_STATE__RESOURCE_UP);
            sprintf(msg, "Host %s comes back.", mon_host.host_address);
            send_host_disconnect_email(POSTMAN__RESOURCE__HOST, &mon_host, NULL, msg,
                    LC_SYSLOG_LEVEL_INFO, LC_MONITOR_POSTMAN_NO_FLAG);
        }
        vmware_host.curr_time = time(NULL);
        vmware_host.cpu_count = host_info.cpu_count;
        strcpy(vmware_host.cpu_usage, host_info.cpu_usage);
        /*FIXME: Just display the first storage info*/
/*        strncpy(vmware_host.sr_usage, host_info.sr_usage, AGENT_DISK_SIZE_LEN - 1);*/
        mon_host.disk_total = host_info.sr_disk_total[0];
        phost = &vmware_host;
    }

    mon_host.host_cpu_start_time = host.cpu_start_time;
    mon_host.host_disk_start_time = host.disk_start_time;
    mon_host.host_check_start_time = host.check_start_time;
    if (0 == mon_host.disk_total) {
        mon_host.disk_total = host.disk_total;
    }
    mon_host.mem_total = host.mem_total;
    mon_host.cr_id = host.cr_id;
    strcpy(mon_host.cpu_info, host.cpu_info);
    strcpy(mon_host.host_address, host.host_address);

    /*FIXME: load the state from db again to workaround PR331 */
    lc_vmdb_load_compute_resource_state(&host_state, mon_host.host_address);
    if (lc_monitor_care_host_state(host_state)) {
/*
        lc_vmdb_compute_resource_usage_update(mon_host.cpu_usage,
                                              mon_host.cpu_used,
                                              mon_host.mem_usage,
                                              mon_host.disk_usage,
                                              mon_host.host_address);
*/
        if (htype == LC_POOL_TYPE_VMWARE) {

            lc_sysdb_get_running_mode(mode, LC_NAMESIZE);
            lc_sysdb_get_domain(domain, LC_NAMESIZE);
            lc_sysdb_get_lcm_ip(lcm_ip, LC_NAMESIZE);

            for (i = 0; i < host_info.sr_count; i ++) {

                 memset(sr_name, 0, SR_NAME_LEN);
                 memset(sr_usage, 0, AGENT_DISK_SIZE_LEN);

                 strncpy(sr_name, host_info.sr_name + j, SR_NAME_LEN - 1);
                 strncpy(sr_usage, host_info.sr_usage + k, AGENT_DISK_SIZE_LEN - 1);
                 disk_total = host_info.sr_disk_total[i];
                 disk_usage = atoll(sr_usage)/LC_GB;
                 /*FIXME: On demand to prefix ha_ to name HA disk*/
                 if (strncasecmp(sr_name, HA_DISK_NAME_PREFIX,
                                  HA_DISK_NAME_PREFIX_LEN) != 0) {
                     lc_storagedb_master_sr_usage_update_by_name(disk_total, disk_usage,
                                                                 0, sr_name, host.rack_id);
                 } else {
                     lc_storagedb_master_ha_sr_usage_update_by_name(disk_total, disk_usage,
                                                                    0, sr_name, host.rack_id);
                 }
                 if (strlen(mode) && strcmp(mode, "hierarchical") == 0) {
                     lc_update_lcm_sharedsrdb(disk_usage, sr_name, lcm_ip, domain);
                 }
                 j += SR_NAME_LEN;
                 k += AGENT_DISK_SIZE_LEN;
            }
            free(host_info.sr_name);
            free(host_info.sr_disk_total);
            free(host_info.sr_usage);
        } else if (htype == LC_POOL_TYPE_XEN || htype == LC_POOL_TYPE_KVM) {
            n_sr = ntohl(phost->n_sr);
            if (n_sr > MAX_SR_NUM) {
                LCMON_LOG(LOG_DEBUG, "%s: SR total:%d, Max SR: %d", __FUNCTION__, n_sr, MAX_SR_NUM);
                n_sr = MAX_SR_NUM;
            }
            for (i = 0; i < n_sr; i ++) {
                disk_total = ntohl(phost->sr_usage[i].disk_total);
                disk_usage = ntohl(phost->sr_usage[i].disk_used);
                disk_type = ntohl(phost->sr_usage[i].type);
                if (disk_type == SR_DISK_TYPE_HA) {
                    lc_storagedb_master_ha_sr_usage_update(disk_total, disk_usage, 0,
                                                           phost->sr_usage[i].uuid);
                } else {
                    lc_storagedb_master_sr_usage_update(disk_total, disk_usage, 0,
                                                        phost->sr_usage[i].uuid);
                }
            }
        }
        lc_vmdb_master_compute_resource_time_update(mon_host.host_cpu_start_time,
                                                    mon_host.host_disk_start_time,
                                                    mon_host.host_check_start_time,
                                                    mon_host.host_address);
    }
    return 0;
}

int vm_dpq_handler(void *datap, int htype)
{
    int i = 0, r =0;
    vm_result_t *pvm = NULL;
    vm_result_t vmware_vm;
    vm_result_t *freevm = NULL;
    lc_data_vm_t *pdata = NULL;
    int count = 0;
    monitor_host_t mon_host;
    vmware_msg_reply_t *pvmware_vm_info = NULL;
    vcd_vm_stats_t vm_stats[MAX_VM_PER_HOST];

    memset(&mon_host, 0, sizeof(monitor_host_t));

    lc_mon_get_config_info(&mon_host, 0);
    mon_host.htype = htype;
    pdata = (lc_data_vm_t *)datap;
    if (htype == LC_POOL_TYPE_XEN) {
        count = pdata->vm_cnt;
        strcpy(mon_host.host_address, pdata->host_addr);
        pvm = pdata->u_vm.vm;
        freevm = pvm;
        for (i = 0; i < count; i ++) {
             lc_monitor_vm_state(pvm, &mon_host, pdata->host_addr);
             pvm ++;
        }
        free(freevm);
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        memset(vm_stats, 0, sizeof(vcd_vm_stats_t) * MAX_VM_PER_HOST);
        pvmware_vm_info = (vmware_msg_reply_t *)pdata;
        r = lc_vcd_vm_stats_reply_msg(vm_stats, &count, pvmware_vm_info->data,
                                       pvmware_vm_info->len, MAX_VM_PER_HOST);
        free(pvmware_vm_info->data);
        if (r) {
            LCMON_LOG(LOG_ERR, "%s: Get vm reply info error", __FUNCTION__);
            return -1;
        }
        for (i = 0; i < count; i ++) {
             memset(&vmware_vm, 0, sizeof(vm_result_t));
             vmware_vm.curr_time = time(NULL);
             strcpy(vmware_vm.vm_label, vm_stats[i].vm_name);
             strcpy(vmware_vm.cpu_usage, vm_stats[i].cpu_usage);
             if (vm_stats[i].vm_state == LC_VM_STATE_RUNNING) {
                 strcpy(vmware_vm.state, "running");
             } else if (vm_stats[i].vm_state == LC_VM_STATE_STOPPED) {
                 strcpy(vmware_vm.state, "halted");
             }
             lc_monitor_vm_state(&vmware_vm, &mon_host,
                             vm_stats[i].host_address);
        }
    }
    return 0;
}

static int lc_msg_forward(lc_mbuf_t *msg)
{
    int magic = lc_msg_get_path(msg->hdr.type, msg->hdr.magic);

    if (magic == -1) {
        LCMON_LOG(LOG_ERR, "%s: find next module from %x error", __FUNCTION__, msg->hdr.magic);
        return -1;
    } else if (magic == MSG_MG_END) {
        LCMON_LOG(LOG_DEBUG, "%s: msg %d complete", __FUNCTION__, msg->hdr.type);
        return 0;
    } else {
        LCMON_LOG(LOG_DEBUG, "%s: msg %d forwarding", __FUNCTION__, msg->hdr.type);
        msg->hdr.magic = magic;
        lc_publish_msg(msg);
        return 0;
    }
}

static int lc_msg_rollback(vm_info_t *vm_info, uint32_t result, uint32_t seq, int ops)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VmOpsReply vm_ops = TALKER__VM_OPS_REPLY__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];

    memset(buf, 0, MAX_MSG_BUFFER);

    if (vm_info) {
        vm_ops.id = vm_info->vm_id;
        vm_ops.err = vm_info->vm_error;
        if (LC_VM_TYPE_VM == vm_info->vm_type) {
            vm_ops.type = TALKER__VM_TYPE__VM;
        } else if (LC_VM_TYPE_GW == vm_info->vm_type) {
            vm_ops.type = TALKER__VM_TYPE__VGW;
        }
    }
    vm_ops.result = result;
    vm_ops.ops = ops;
    vm_ops.has_id = vm_ops.has_err = 1;
    vm_ops.has_type = vm_ops.has_result = 1;
    vm_ops.has_ops = 1;

    msg.vm_ops_reply = &vm_ops;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__LCMOND;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = seq;
    fill_message_header(&hdr, buf);

    lc_bus_lcmond_publish_unicast(buf, (hdr_len + msg_len), LC_BUS_QUEUE_LCRMD);
    return 0;
}

static int lc_vm_check(struct lc_msg_head *m_head)
{
    struct msg_vm_opt *mvm;
    vm_info_t vm_info;
    template_os_info_t template_info;
    char cmd[MAX_CMD_BUF_SIZE];
    char message[MAX_LOG_MSG_LEN];
    host_t    host;
    rpool_t pool;

    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&template_info, 0, sizeof(template_os_info_t));
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&host, 0, sizeof(host_t));
    memset(message, 0, sizeof(message));
    mvm = (struct msg_vm_opt*)(m_head + 1);

    if (lc_vmdb_vm_load(&vm_info, mvm->vm_id) != LC_DB_VM_FOUND) {
        LCMON_LOG(LOG_ERR, "[Monitor]vm %s to be created not found",
                                               vm_info.vm_label);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    if (vm_info.vm_flags & (LC_VM_INSTALL_IMPORT | LC_VM_THIRD_HW_FLAG)) {
        goto finish;
    }
    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&host, vm_info.host_address) != LC_DB_HOST_FOUND) {
        LCMON_LOG(LOG_ERR, "%s: host %s not found",
                __FUNCTION__, host.host_address);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    vm_info.hostp = &host;
    memset(&pool, 0, sizeof(rpool_t));
    if (lc_get_pool_by_index(&pool, host.pool_index) != LC_DB_POOL_FOUND) {
        LCMON_LOG(LOG_ERR, "%s: pool %d not found",
                __FUNCTION__, host.pool_index);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }

    if (lc_get_template_info(&template_info, vm_info.vm_template) != LC_DB_VM_FOUND) {
        if (pool.ctype != LC_POOL_TYPE_XEN) {
            /*only check the nfs state to XEN HOST server*/
            goto finish;
        }
        LCMON_LOG(LOG_ERR, "[Monitor] vm %s's template %s not be found",
                              vm_info.vm_label, vm_info.vm_template);
        vm_info.vm_error |= LC_VM_ERRNO_DB_ERROR;
        goto error;
    }
    if (template_info.state == LC_TEMPLATE_STATE_OFF) {
        LCMON_LOG(LOG_ERR, "[Monitor]vm %s's template %s is not available",
                                 vm_info.vm_label, vm_info.vm_template);
        vm_info.vm_error |= LC_VM_ERRNO_NO_TEMPLATE;
        goto error;
    }
#if 0
    /*If it is NFS server, need to check the state*/
    if (template_info.htype == LC_TEMPLATE_OS_XEN) {
        if (template_info.type == LC_TEMPLATE_OS_REMOTE) {
            snprintf(cmd, MAX_CMD_BUF_SIZE, LC_XEN_CHECK_TEMPLATE_STR,
                 vm_info.hostp->host_address, vm_info.hostp->username,
                 vm_info.hostp->password, template_info.server_ip, vm_info.vm_template);
            result = nfs_server_is_available(cmd, message, template_info.server_ip,
                                        vm_info.host_address, vm_info.vm_template);
            if ((result) && (result != LC_MON_XENSERVER_UNREACHABLE)) {
                lc_monitor_db_log("", message, LOG_WARNING);
                lc_update_master_template_os_state(LC_TEMPLATE_STATE_OFF, vm_info.vm_template);
                vm_info.vm_error |= LC_VM_ERRNO_NO_TEMPLATE;
                goto error;
            }
        }
    }
#endif
finish:
    lc_msg_forward((lc_mbuf_t *)m_head);
    return 0;

error:

    lc_vmdb_master_vm_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_master_vm_update_state(LC_VM_STATE_ERROR, vm_info.vm_id);*/
    lc_msg_rollback(&vm_info, TALKER__RESULT__FAIL, m_head->seq,
                                     TALKER__VM_OPS__VM_CREATE);
    return -1;
}

static int lc_vedge_check(struct lc_msg_head *m_head)
{
    struct msg_vedge_opt *mvedge;
    vm_info_t vm_info;
    template_os_info_t template_info;
    char cmd[MAX_CMD_BUF_SIZE];
    char message[MAX_LOG_MSG_LEN];
    host_t    host;

    memset(&vm_info, 0, sizeof(vm_info_t));
    memset(&template_info, 0, sizeof(template_os_info_t));
    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&host, 0, sizeof(host_t));
    memset(message, 0, sizeof(message));
    mvedge = (struct msg_vedge_opt *)(m_head + 1);

    if (lc_vmdb_vedge_load(&vm_info, mvedge->vnet_id) != LC_DB_VEDGE_FOUND) {

        LCMON_LOG(LOG_ERR, "%s: vedge to be created not found\n",
                __FUNCTION__);
        vm_info.vm_error |= LC_VEDGE_ERRNO_DB_ERROR;
        goto error;
    }
    if (vm_info.vm_flags & (LC_VM_INSTALL_IMPORT | LC_VNET_THIRD_HW_FLAG)) {
        goto finish;
    }

    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&host, vm_info.host_address) != LC_DB_HOST_FOUND) {
        LCMON_LOG(LOG_ERR, "%s: host %s not found",
                __FUNCTION__, host.host_address);
        vm_info.vm_error |= LC_VEDGE_ERRNO_DB_ERROR;
        goto error;
    }
    vm_info.hostp = &host;

    if (lc_get_template_info(&template_info, vm_info.vm_template) != LC_DB_VM_FOUND) {
        LCMON_LOG(LOG_ERR, "[Monitor] vgw %s's template %s not be found",
                               vm_info.vm_label, vm_info.vm_template);
        vm_info.vm_error |= LC_VEDGE_ERRNO_DB_ERROR;
        goto error;
    }
    if (template_info.state == LC_TEMPLATE_STATE_OFF) {
        LCMON_LOG(LOG_ERR, "[Monitor]vgw %s's template %s is not available",
                                  vm_info.vm_label, vm_info.vm_template);
        vm_info.vm_error |= LC_VEDGE_ERRNO_NO_TEMPLATE;
        goto error;
    }
#if 0
    /*If it is NFS server, need to check the state*/
    if (template_info.htype == LC_TEMPLATE_OS_XEN) {
        if (template_info.type == LC_TEMPLATE_OS_REMOTE) {
            snprintf(cmd, MAX_CMD_BUF_SIZE, LC_XEN_CHECK_TEMPLATE_STR,
                 vm_info.hostp->host_address, vm_info.hostp->username,
                    vm_info.hostp->password, template_info.server_ip, vm_info.vm_template);
            result = nfs_server_is_available(cmd, message, template_info.server_ip,
                                        vm_info.host_address, vm_info.vm_template);
            if ((result) && (result != LC_MON_XENSERVER_UNREACHABLE)) {
                lc_monitor_db_log("", message, LOG_WARNING);
                lc_update_master_template_os_state(LC_TEMPLATE_STATE_OFF, vm_info.vm_template);
                vm_info.vm_error |= LC_VEDGE_ERRNO_NO_TEMPLATE;
                goto error;
            }
        }
    }
#endif
finish:
    lc_msg_forward((lc_mbuf_t *)m_head);
    return 0;

error:

    lc_vmdb_master_vedge_update_errno(vm_info.vm_error, vm_info.vm_id);
/*    lc_vmdb_master_vedge_update_state(LC_VEDGE_STATE_ERROR, vm_info.vm_id);*/
    lc_msg_rollback(&vm_info, TALKER__RESULT__FAIL, m_head->seq,
                                     TALKER__VM_OPS__VM_CREATE);
    return -1;

}

static int vm_ops_check_handler(struct lc_msg_head *m_head)
{
    switch (m_head->type) {
    case LC_MSG_VM_ADD:
         return lc_vm_check(m_head);
    case LC_MSG_VEDGE_ADD:
         return lc_vedge_check(m_head);
    }
    return 0;
}

int vm_dpq_ops_check_handler(void *datap, int htype)
{
    lc_data_vm_t *pdata = NULL;
    struct lc_msg_t *msg = NULL;
    pdata = (lc_data_vm_t *)datap;
    msg = (struct lc_msg_t *)pdata->u_vm.vm_ops;
    if (msg) {
        vm_ops_check_handler((struct lc_msg_head *)msg);
        free(msg);
    }
    return 0;
}

int lc_mon_vgw_ha_handler(lc_data_vgw_ha_t *pdata)
{
    lc_data_vgw_ha_t *pvgw = NULL;
    int hastate = 0, state = 0;
    pvgw = pdata;

    if (call_curl_get_hastate(pvgw->vgw_ctl_addr, &hastate)) {
        LCMON_LOG(LOG_ERR, "%s: Get vGW ha state failed", __FUNCTION__);
    }

    if (hastate != pvgw->ha_state) {
        lc_update_master_ha_vnet_hastate(hastate, pvgw->label);
        switch (hastate) {
        case LC_VM_HASTATE_MASTER:
             state = TALKER__RESOURCE_STATE__RESOURCE_MASTER;
             break;
        case LC_VM_HASTATE_BACKUP:
             state = TALKER__RESOURCE_STATE__RESOURCE_BACKUP;
             break;
        case LC_VM_HASTATE_FAULT:
             state = TALKER__RESOURCE_STATE__RESOURCE_FAULT;
             break;
        default:
             state = TALKER__RESOURCE_STATE__RESOURCE_UNKNOWN;
             break;
        }
        lc_monitor_notify(pvgw->id, TALKER__RESOURCE_TYPE__VGW_HA_RESOURCE, state);
        LCMON_LOG(LOG_WARNING, "%s: the HA state of vGW %s is changed from %d to %d",
                              __FUNCTION__, pvgw->label, pvgw->ha_state, hastate);
    }
    return 0;
}

int lc_mon_bk_vm_health_handler(lc_data_bk_vm_health_t *pdata)
{
    lc_data_bk_vm_health_t *pbk_vms = NULL;
    lb_listener_t listener_info;
    lb_bk_vm_t bk_vm_info;
    char listener_name[LC_NAMESIZE];
    char bk_vm_name[LC_NAMESIZE];
    int bk_vm_health_state = 0, state = 0;
    char result[LCMG_API_SUPER_BIG_RESULT_SIZE];
    const nx_json *data = NULL, *item = NULL, *json = NULL;
    const nx_json *item_j = NULL, *json_j = NULL;
    int i, j;
    pbk_vms = pdata;

    //get bk_vm health_state by api
    memset(result, 0, LCMG_API_SUPER_BIG_RESULT_SIZE * sizeof(char));
    if (call_curl_get_bk_vm_health(pbk_vms->lb_server, pbk_vms->lb_ctl_addr,
                                   result)) {
        LCMON_LOG(LOG_ERR, "%s: call_curl_get_bk_vm_health failed\n",
               __FUNCTION__);
        return -1;
    }

    data = nx_json_get(*((const nx_json **)result), LCMON_JSON_DATA_KEY);
    if (data->type == NX_JSON_NULL) {
        LCMON_LOG(LOG_ERR, "%s: invalid json recvived, no data\n",
               __FUNCTION__);
        return -1;
    }

    //analysis api result
    i = 0;
    do {
        item = nx_json_item(data, i);
        if (item->type == NX_JSON_NULL) {
            break;
        }
        json = nx_json_get(item, "NAME");
        memset(listener_name, 0, LC_NAMESIZE);
        strcpy(listener_name, json->text_value);
        json = nx_json_get(item, "BK_VMS");
        if (json->type == NX_JSON_NULL) {
            break;
        }
        i++;

        //get listener_info by name & lb_lcuuid
        memset(&listener_info, 0, sizeof(lb_listener_t));
        if (LC_DB_VM_FOUND != lc_vmdb_lb_listener_load_by_name(&listener_info,
                                                               listener_name,
                                                               pbk_vms->lb_lcuuid)) {
            LCMON_LOG(LOG_ERR, "%s: lb %s listener %s not found",
                   __FUNCTION__, pbk_vms->lb_label, listener_name);
            continue;
        }

        j = 0;
        do {
            item_j = nx_json_item(json, j);
            if (item_j->type == NX_JSON_NULL) {
                break;
            }

            json_j = nx_json_get(item_j, "NAME");
            memset(bk_vm_name, 0, LC_NAMESIZE);
            strcpy(bk_vm_name, json_j->text_value);
            json_j = nx_json_get(item_j, "HEALTH_STATE");
            j++;

            //get bk_vm_info by name & lb_listener_lcuuid & lb_lcuuid
            memset(&bk_vm_info, 0, sizeof(lb_bk_vm_t));
            if (LC_DB_VM_FOUND != lc_vmdb_lb_bk_vm_load_by_name(&bk_vm_info,
                                                                bk_vm_name,
                                                                listener_info.lcuuid,
                                                                pbk_vms->lb_lcuuid)) {
                LCMON_LOG(LOG_ERR, "%s: lb %s listener %s bk_vm %s not found",
                       __FUNCTION__, pbk_vms->lb_label, listener_name, bk_vm_name);
                continue;
            } else {
                if (bk_vm_info.health_state == LC_BK_VM_HEALTH_STATE_NO_CHECK) {
                    LCMON_LOG(LOG_DEBUG, "%s: lb %s listener %s health_check is off",
                           __FUNCTION__, pbk_vms->lb_label, listener_name);
                    continue;
                }
            }

            //convert health_state
            if (strstr(json_j->text_value, LCMG_API_RESULT_BK_VM_STATE_UP)) {
                bk_vm_health_state = LC_BK_VM_HEALTH_STATE_UP;
            } else if (strstr(json_j->text_value, LCMG_API_RESULT_BK_VM_STATE_DOWN)) {
                bk_vm_health_state = LC_BK_VM_HEALTH_STATE_DOWN;
            } else if (strstr(json_j->text_value, LCMG_API_RESULT_BK_VM_STATE_MAINT)) {
                bk_vm_health_state = LC_BK_VM_HEALTH_STATE_DOWN;
            } else {
                LCMON_LOG(LOG_ERR, "%s: health_state err[%s]\n",
                       __FUNCTION__, json_j->text_value);
                continue;
            }

            //update health_state
            if (bk_vm_health_state != bk_vm_info.health_state) {
                lc_update_master_lb_bk_vm_health_state(bk_vm_health_state, bk_vm_info.lcuuid);
                if (LC_BK_VM_HEALTH_STATE_DOWN == bk_vm_health_state) {
                     state = TALKER__RESOURCE_STATE__RESOURCE_DOWN;
                } else {
                     state = TALKER__RESOURCE_STATE__RESOURCE_UP;
                }
                lc_monitor_notify(bk_vm_info.id, TALKER__RESOURCE_TYPE__BK_VM_HEALTH_RESOURCE, state);
                LCMON_LOG(LOG_WARNING,
                       "%s: the health state of lb %s listener %s bk_vm %s is changed from %d to %d",
                       __FUNCTION__, pbk_vms->lb_label, listener_name, bk_vm_name,
                       bk_vm_info.health_state, bk_vm_health_state);
            }

        } while(1);
    } while(1);

    return 0;
}

int lc_mon_nfs_chk_handler(lc_data_nfs_chk_t *pdata)
{
    char cmd[MAX_CMD_BUF_SIZE];
    char message[MAX_LOG_MSG_LEN];
    host_t    host;
    monitor_host_t mon_host;
    int result = 0;
    lc_data_nfs_chk_t *pchk = pdata;


    memset(cmd, 0, MAX_CMD_BUF_SIZE);
    memset(&host, 0, sizeof(host_t));
    memset(message, 0, sizeof(message));
    memset(&mon_host, 0, sizeof(monitor_host_t));
    return 0;

    lc_mon_get_config_info(&mon_host, 1);

    /* Load compute_resource from db and update vm_info*/
    if (lc_vmdb_compute_resource_load(&host, pchk->host_addr) != LC_DB_HOST_FOUND) {
        LCMON_LOG(LOG_ERR, "%s: host %s not found",
                __FUNCTION__, pchk->host_addr);
        return 0;
    }

    mon_host.cr_id = host.cr_id;
    if (pchk->htype == LC_TEMPLATE_OS_XEN) {
        snprintf(cmd, MAX_CMD_BUF_SIZE, LC_XEN_CON_TEMPLATE_STR,
                                 host.host_address, host.username,
               host.password, pchk->server_ip, pchk->vm_template);
        result = nfs_server_is_available(cmd, message, pchk->server_ip, pchk->host_addr, pchk->vm_template);
        if (result) {
            if((result != LC_MON_XENSERVER_UNREACHABLE)
                && (pchk->state == LC_TEMPLATE_STATE_ON)) {
                lc_update_master_template_os_state_by_ip(LC_TEMPLATE_STATE_OFF, pchk->server_ip);
                send_host_disconnect_email(POSTMAN__RESOURCE__HOST, &mon_host, NULL, message,
                        LC_SYSLOG_LEVEL_WARNING, LC_MONITOR_POSTMAN_NO_FLAG);
            }
        } else {
            if(pchk->state == LC_TEMPLATE_STATE_OFF) {
               lc_update_master_template_os_state_by_ip(LC_TEMPLATE_STATE_ON, pchk->server_ip);
               snprintf (message, MAX_LOG_MSG_LEN, "NFS server %s is available on %s",
                                                    pchk->server_ip, pchk->host_addr);
               send_host_disconnect_email(POSTMAN__RESOURCE__HOST, &mon_host, NULL, message,
                       LC_SYSLOG_LEVEL_INFO, LC_MONITOR_POSTMAN_NOTIFY_FLAG);
            }
        }
    }

    return 0;
}
