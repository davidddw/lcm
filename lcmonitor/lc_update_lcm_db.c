/*
 * Copyright 2012 yunshan.net.cn
 * All rights reserved
 * Author: Lai Yuan laiyuan@yunshan.net.cn
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "lc_db.h"
#include "lc_db_vm.h"
#include "lc_lcm_api.h"
#include "lc_update_lcm_db.h"

#define LCM_IP_LEN           50
#define LCC_RUNNING_MODE     19
#define LCM_HOST_PUBLIC_IP   20

int LCMON_LCM_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *LCMON_LCM_LOG_LEVEL_P = &LCMON_LCM_LOG_DEFAULT_LEVEL;

char *LCMON_LCM_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define LCMON_LCM_LOG(level, format, ...)  \
    if(level <= *LCMON_LCM_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), LCMON_LCM_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

char *lcm_server = NULL;

static int get_lcm_process(void *lcmip, char *field, char *value)
{
    if (strcmp(field, "value") == 0) {
        strncpy((char*)lcmip, value, LCM_IP_LEN);
    }
    return 0;
}

static int lc_get_lcm_from_system_config(char *lcmip)
{
    char condition[LC_DB_BUF_LEN];
    int result;
    int id = LCM_HOST_PUBLIC_IP;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    result = lc_db_table_load((void *)lcmip, "sys_configuration", "*", condition, get_lcm_process);
    if (result == LC_DB_COMMON_ERROR) {
        LCMON_LCM_LOG(LOG_ERR, "%s: error=%d\n", __FUNCTION__, result);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LCMON_LCM_LOG(LOG_ERR, "%s: system config table is NULL\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

static int lc_get_running_mode_from_system_config(char *lcmip)
{
    char condition[LC_DB_BUF_LEN];
    int result;
    int id = LCC_RUNNING_MODE;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    result = lc_db_table_load((void *)lcmip, "sys_configuration", "*", condition, get_lcm_process);
    if (result == LC_DB_COMMON_ERROR) {
        LCMON_LCM_LOG(LOG_ERR, "%s: error=%d\n", __FUNCTION__, result);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LCMON_LCM_LOG(LOG_ERR, "%s: system config table is NULL\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int get_lcm_server()
{
    char running_mode[256];
    lcm_server = malloc(LCM_IP_LEN);
    memset(lcm_server, 0, LCM_IP_LEN);
    lc_get_running_mode_from_system_config(running_mode);
    if (strcmp(running_mode, "hierarchical") == 0) {
        lc_get_lcm_from_system_config(lcm_server);
        LCMON_LCM_LOG(LOG_DEBUG, "%s: lcm server ip address is %s.\n", __FUNCTION__,lcm_server);
    }
    return 0;
}

int lc_update_lcm_vmdb_state(int vmstate, uint32_t vmid)
{
    if (NULL == lcm_server)
    {
        get_lcm_server();
    }
    if(strlen(lcm_server) > 0)
    {
        lcmvm_t vminfo;
        vm_info_t tmpvminfo;
        lc_vmdb_vm_load(&tmpvminfo,vmid);
        vminfo.vmstate = vmstate;
        vminfo.vmid = tmpvminfo.vm_id;
        vminfo.vmerrno = tmpvminfo.vm_error;
        vminfo.vmflag = tmpvminfo.vm_flags;
        url_update_lcm_vm_db(&vminfo);
        LCMON_LCM_LOG(LOG_DEBUG, "%s: vm state is updated to lcm: state=%u,id=%u,errorno=%u,flag=%u.\n", \
               __FUNCTION__, vminfo.vmstate, vminfo.vmid, vminfo.vmerrno, vminfo.vmflag);
    }
    return 0;
}

int lc_update_lcm_vmdb_state_by_host(char *hostip)
{
    int i = 0, vm_number = 0, code = 0;
    vm_query_info_t vm_query_set[MAX_VM_PER_HOST];
    memset(vm_query_set, 0, sizeof(vm_query_info_t)*MAX_VM_PER_HOST);

    if (hostip == NULL) {
        LCMON_LCM_LOG(LOG_ERR, "%s: hostip is NULL.\n", __FUNCTION__);
        return 1;
    }

    code = lc_get_vm_by_server(vm_query_set, sizeof(vm_query_info_t), &vm_number,
                                                     hostip, 0, MAX_VM_PER_HOST);
    if (code == LC_DB_VM_NOT_FOUND) {
        LCMON_LCM_LOG(LOG_DEBUG, "%s vm not found on host %s.\n", __FUNCTION__, hostip);
    }
    if (vm_number) {
        for (i = 0; i < vm_number; i ++) {
             lc_update_lcm_vmdb_state(vm_query_set[i].pw_state, vm_query_set[i].vm_id);
        }
    }
    return 0;
}

int lc_update_lcm_vgate_state(int vgtatestate, uint32_t vgateid)
{
    if (NULL == lcm_server)
    {
        get_lcm_server();
    }
    lcmvdc_t vgateinfo;
    url_update_lcm_vgate_db(&vgateinfo);
    return 0;
}

int lc_update_lcm_sharedsrdb(int disk_used, char *sr_name, char *lcm_ip, char *domain)
{
    char cmd[MAX_CMD_BUF_SIZE];
    if (!strlen(lcm_ip) || !strlen(domain)) {
        return -1;
    }
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, MAX_CMD_BUF_SIZE,
             "curl -X POST " \
             "-k https://%s/sharedstorageresource/set -d " \
             "\"data={" \
             "\\\"sr_name\\\":\\\"%s\\\", " \
             "\\\"disk_used\\\":%d, " \
             "\\\"domain\\\":\\\"%s\\\"" \
             "}\" 2>/dev/null",
             lcm_ip,
             sr_name,
             disk_used,
             domain
    );
    LCMON_LCM_LOG(LOG_DEBUG, "%s: [%s]", __FUNCTION__, cmd);
    system(cmd);
    return 0;
}
