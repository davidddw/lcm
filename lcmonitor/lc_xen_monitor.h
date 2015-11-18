/*
 * Copyright 2012 yunshan network
 * All rights reserved
 * Monitor the state of Hosts and VMs
 * Author:
 * Date:
 */
#ifndef LC_XEN_MONITOR_H
#define LC_XEN_MONITOR_H

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <xen/api/xen_all.h>

#include "lc_mon.h"
#include "lc_queue.h"
#include "vm/vm_host.h"
#include "lc_db.h"
#include "lc_db_vm.h"
#include "lc_db_host.h"
#include "lc_db_storage.h"
#include "lc_db_res_usg.h"
#include "lc_agent_msg.h"
#include "lc_mon_qctrl.h"

#define LC_MONITOR_HOST_STATE_THRESHOLD   3
#define LC_MONITOR_VM_STATE_THRESHOLD   1
#define LC_MONITOR_POSTMAN_NO_FLAG 0        /*used for alarm*/
#define LC_MONITOR_POSTMAN_NOTIFY_FLAG 1    /*used for notify*/

#define MONITOR_EMAIL_ADDRESS                  6000
#define MONITOR_HOST_CPU_DURATION              6001
#define MONITOR_HOST_CPU_WARNING_USAGE         6002
#define MONITOR_HOST_MEM_DURATION              6003
#define MONITOR_HOST_MEM_WARNING_USAGE         6004
#define MONITOR_HOST_NETWORK_DURATION          6005
#define MONITOR_HOST_NETWORK_WARNING_USAGE     6006
#define MONITOR_HOST_WARNING_INTERVAL          6007
#define MONITOR_VM_WARNING_INTERVAL            6008
#define MONITOR_VM_NETWORK_DURATION            6009
#define MONITOR_VM_NETWORK_WARNING_USAGE       6010
#define MONITOR_VM_DISK_DURATION               6011
#define MONITOR_VM_DISK_WARNING_USAGE          6012
#define MONITOR_VM_CPU_DURATION                6013
#define MONITOR_VM_CPU_WARNING_USAGE           6014
#define MONITOR_VM_MEM_DURATION                6015
#define MONITOR_VM_MEM_WARNING_USAGE           6016
#define MONITOR_HOST_DISK_DURATION             6017
#define MONITOR_HOST_DISK_WARNING_USAGE        6018

#define LC_MONITOR_CONFIG_START_ID 6000
#define LC_MONITOR_CONFIG_END_ID 6018

#define EMAIL_FORMAT "/usr/local/livecloud/script/report_sendmail.sh -t %s -s '%s' -g '%s'"
#define EMAIL_CC_FORMAT "/usr/local/livecloud/script/report_sendmail.sh -t %s -c %s -s '%s' -g '%s'"
#define EMAIL_BODY_FORMAT_LEN  50

/*Only skip to monitor some error numbers
 * when vm state is exception*/
#define  LC_MON_VM_ERRNO_SET 0xFCFFFE7E
#define  LC_MON_GW_ERRNO_SET 0xFF37FE00

#define  LC_MON_NFS_UNREACHABLE         1
#define  LC_MON_NFS_SERVICE_UNAVAILABLE 2
#define  LC_MON_TEMPLATE_VDI_UNPLUG     3
#define  LC_MON_TEMPLATE_VHD_DELETED    4
#define  LC_MON_INVALID_NFS_CLIENT      5
#define  LC_MON_XENSERVER_UNREACHABLE   10
#define  LC_MON_XENSERVER_XAPI_DOWN     11

/*
XEN vm state:
0: Haltd;
1: Paused;
2: Running;
3: Suspended
4: Undefined
*/
#define VM_PW_STATE_HALTED 0
#define VM_PW_STATE_PAUSED 1
#define VM_PW_STATE_RUNNING 2
#define VM_PW_STATE_SUSPENDED 3
#define VM_PW_STATE_UNDEFINED 4
#define VM_PW_STATE_ERROR     5

#define VMWARE_HOST_STATE_CONNECTED   3

static inline
int lc_get_vm_current_state(char *state, int type)
{
    if (strcmp(state, "running") == 0) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_RUNNING:LC_VEDGE_STATE_RUNNING);
    } else if (strcmp(state, "halted") == 0 ) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_STOPPED:LC_VEDGE_STATE_STOPPED);
    } else if (strcmp(state, "nonexistence") == 0 ) {
        return ((type == LC_VM_TYPE_VM)?
               LC_VM_STATE_ERROR:LC_VEDGE_STATE_ERROR);
    }
    return ((type == LC_VM_TYPE_VM)?
            LC_VM_STATE_STOPPED:LC_VEDGE_STATE_STOPPED);
}


static inline
int lc_convert_xen_vm_state(int value)
{
    switch(value) {
    case LC_VM_STATE_RUNNING:
        return VM_PW_STATE_RUNNING;
    case LC_VM_STATE_STOPPED:
        return VM_PW_STATE_HALTED;
    case LC_VM_STATE_SUSPENDED:
        return VM_PW_STATE_PAUSED;
    }
    return VM_PW_STATE_UNDEFINED;
}

static inline
int lc_xen_vm_convert_controller_state(int value)
{
    switch(value) {
    case VM_PW_STATE_RUNNING:
        return LC_VM_STATE_RUNNING;
    case VM_PW_STATE_HALTED:
        return LC_VM_STATE_STOPPED;  /*Halted */
    case VM_PW_STATE_SUSPENDED:
    case VM_PW_STATE_PAUSED:
        return LC_VM_STATE_SUSPENDED;
    }
    return LC_VM_STATE_ERROR;
}


static inline
int lc_convert_xen_vedge_state(int value)
{
    switch(value) {
    case LC_VEDGE_STATE_RUNNING:
        return VM_PW_STATE_RUNNING;
    case LC_VEDGE_STATE_STOPPED:
        return VM_PW_STATE_HALTED;
    }
    return VM_PW_STATE_UNDEFINED;
}

static inline
int lc_xen_vedge_convert_controller_state(int value)
{
    switch(value) {
    case VM_PW_STATE_RUNNING:
        return LC_VEDGE_STATE_RUNNING;
    case VM_PW_STATE_HALTED:
    case VM_PW_STATE_SUSPENDED:
        return LC_VEDGE_STATE_STOPPED;
    }
    return LC_VEDGE_STATE_ERROR;
}

static inline
int lc_monitor_care_host_state(int state)
{
    if ((state == LC_CR_STATE_COMPLETE) ||
        (state == LC_CR_STATE_ERROR)) {
        return 1;
    }
    return 0;
}
#if 0
static inline
int lc_monitor_no_care_vmstate_compulsory(int state)
{
    if ((state == LC_VM_STATE_TO_CREATE) ||
        (state == LC_VM_STATE_COMPLETE)  ||
        (state == LC_VM_STATE_TO_START) ||
        (state == LC_VM_STATE_TO_SUSPEND) ||
        (state == LC_VM_STATE_TO_RESUME) ||
        (state == LC_VM_STATE_TO_STOP) ||
        (state == LC_VM_STATE_MODIFY) ||
        (state == LC_VM_STATE_TO_DESTROY) ||
        (state == LC_VM_STATE_DESTROYED)) {
        /*monitor doesn't take care of these transient state processed by LCM*/
        return 1;
    }
    return 0;
}

static inline
int lc_monitor_no_care_vmstate_optional(int state)
{
    if ((state == LC_VM_STATE_TO_SUSPEND) ||
        (state == LC_VM_STATE_TO_RESUME) ||
        (state == LC_VM_STATE_TO_STOP) ||
        (state == LC_VM_STATE_MODIFY) ||
        (state == LC_VM_STATE_TO_DESTROY) ||
        (state == LC_VM_STATE_DESTROYED)) {
        /*monitor doesn't take care of these transient state processed by LCM*/
        return 1;
    }
    return 0;
}
#endif

static inline
int lc_monitor_skip_vmerrno(uint32_t vm_error)
{
    if (vm_error & LC_MON_VM_ERRNO_SET) {
        return 1;
    }
    return 0;
}

static inline
int lc_monitor_skip_gwerrno(uint32_t vm_error)
{
    if (vm_error & LC_MON_GW_ERRNO_SET) {
        return 1;
    }
    return 0;
}

static inline
int lc_monitor_care_vmstate(int state)
{
    if ((state == LC_VM_STATE_RUNNING) ||
/*        (state == LC_VM_STATE_TMP) ||*/
        (state == LC_VM_STATE_ERROR)  ||
/*        (state == LC_VM_STATE_DESTROYED)  ||*/
        (state == LC_VM_STATE_STOPPED)) {
        return 1;
    }
    return 0;
}

static inline
int lc_monitor_care_gwstate(int state)
{
    if ((state == LC_VEDGE_STATE_RUNNING) ||
        (state == LC_VEDGE_STATE_STOPPED) ||
 /*       (state == LC_VEDGE_STATE_TMP) ||*/
        (state == LC_VEDGE_STATE_ERROR) /*||
        (state == LC_VEDGE_STATE_DESTROYED)*/) {
        return 1;
    }
    return 0;
}

extern
int get_vm_actual_vdi_size(uint64_t *number, char *num);

extern
int get_vm_actual_memory(uint32_t *number, char *num);

extern
int get_cpu_num(int *number, char *num);

extern
int get_vm_cpu_usage (char *result, char *usage);

extern xen_session *
xen_create_session(char *server_url, char *username, char *password);


extern int
lc_monitor_check_bottom_half(intptr_t data_ptr);

bool lc_host_health_check(char *host_address, xen_session *session);
int lc_get_pool_by_index (rpool_t *pool_info, uint32_t pool_index);

int lc_get_compute_resource (host_t *host_info, char *ip);

int send_host_disconnect_email(int type, monitor_host_t *hostp,
        vm_info_t *vm_info, char *EMAIL_FORMAT_msg, int level, int flag);

int send_nsp_fail_email(int type, monitor_host_t *hostp,
        vm_info_t *vm_info, char *EMAIL_FORMAT_msg, int level, int flag);

int lc_mon_get_config_info(monitor_host_t *pmon_host, int is_host);

#endif /*LC_XEN_MONITOR_H*/

