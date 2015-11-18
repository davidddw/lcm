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
#include <syslog.h>
#include <pthread.h>

#include "lc_lcm_api.h"

#define URL_LOG_HEADER          "URL: "

/* POST define */
#define URL_PUBLIC_POST         "https://%s/"
#define URL_UPDATE_CLOUD_POST   "bdbadapter/updatecloud"
#define URL_UPDATE_VGW_POST     "bdbadapter/updatevgw"
#define URL_UPDATE_VM_POST      "bdbadapter/updatevm"
#define URL_UPDATE_VL2_POST     "bdbadapter/updatevl2"
#define URL_UPDATE_VIF_POST     "bdbadapter/updatevinterface"
#define URL_UPDATE_BACKUP_POST  "bdbadapter/updatebackup"

/* DATA define */
#define URL_UPDATE_DATA         "\"data={\\\"id\\\":%d, \\\"state\\\":%d, \\\"errno\\\":%d, \\\"flag\\\":%d}\""
#define URL_UPDATE_VM_DATA      "\"data={\\\"id\\\":%d, \\\"state\\\":%d, \\\"errno\\\":%d, \\\"flag\\\":%d, "\
                                "\\\"domain\\\":\\\"%s\\\", \\\"poolid\\\":%d, \\\"launch_server\\\":\\\"%s\\\", "\
                                "\\\"exclude_server\\\":\\\"%s\\\"}\""
#define URL_MODIFY_VM_DATA      "\"data={\\\"id\\\":%d, \\\"vdi_user_size\\\":%d, \\\"mem_size\\\":%d, \\\"vcpu_num\\\":%d}\""
#define URL_MODIFY_VIF_DATA     "\"data={\\\"id\\\":%d, \\\"ip\\\":\\\"%s\\\", \\\"bandw\\\":%d}\""
#define URL_UPDATE_VGW_DATA     "\"data={\\\"id\\\":%d, \\\"state\\\":%d, \\\"errno\\\":%d, \\\"flag\\\":%d, "\
                                "\\\"domain\\\":\\\"%s\\\", \\\"gw_poolid\\\":%d, \\\"gw_launch_server\\\":\\\"%s\\\", "\
                                "\\\"exclude_server\\\":\\\"%s\\\"}\""
#define URL_UPDATE_BACKUP_DATA  "\"data={\\\"id\\\":%d, \\\"state\\\":%d, \\\"errno\\\":%d}\""
#define URL_DEL_CLOUD_DATA      "bdbadapter/deletecloud?id=%d"
#define URL_DEL_VM_DATA         "bdbadapter/deletevm?id=%d"
#define URL_DEL_VL2_DATA        "bdbadapter/deletevl2?id=%d"
#define URL_DEL_VIF_DATA        "bdbadapter/deletevinterface?id=%d"
#define URL_DEL_BACKUP_DATA     "bdbadapter/deletebackup?id=%d"
#define URL_UPDATE_VM_VNC_DATA  "bdbadapter/updatevmvnc?id=%d"
#define URL_UPDATE_VGW_VNC_DATA "bdbadapter/updatevgwvnc?id=%d"

int LCMON_API_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *LCMON_API_LOG_LEVEL_P = &LCMON_API_LOG_DEFAULT_LEVEL;

char *LCMON_API_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define LCMON_API_LOG(level, format, ...)  \
    if(level <= *LCMON_API_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), LCMON_API_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

int url_update_lcm_vgate_db(lcmvdc_t *vdc)
{
    int  ret = 0;
    char cmd[LCM_CMD_INFO_LEN];

    if (NULL == vdc) {
        LCMON_API_LOG(LOG_ERR, "LCMd:"URL_LOG_HEADER" %s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(cmd, 0, LCM_CMD_INFO_LEN);
    sprintf(cmd, "curl -X POST -k "URL_PUBLIC_POST URL_UPDATE_VGW_POST" -d "URL_UPDATE_DATA,
            lcm_server, vdc->vdcid, vdc->vdcstate, vdc->vdcerrno, vdc->vdcflag);
    LCMON_API_LOG(LOG_DEBUG, "LCMd:"URL_LOG_HEADER"Update_vdc :%s", cmd);

    ret = system(cmd);
    if (0 != ret) {
        LCMON_API_LOG(LOG_ERR, "LCMd:"URL_LOG_HEADER"%s: system cmd error", __FUNCTION__);
        return -1;
    }

error:
    return ret;
}

int url_update_lcm_vm_db(lcmvm_t *vm)
{
    int  ret = 0;
    char cmd[LCM_CMD_INFO_LEN];

    if (NULL == vm) {
        LCMON_API_LOG(LOG_ERR, "LCMd:"URL_LOG_HEADER" %s Input Para is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(cmd, 0, LCM_CMD_INFO_LEN);
    sprintf(cmd, "curl -X POST -k "URL_PUBLIC_POST URL_UPDATE_VM_POST" -d "URL_UPDATE_DATA,
            lcm_server, vm->vmid, vm->vmstate, vm->vmerrno, vm->vmflag);
    LCMON_API_LOG(LOG_DEBUG, "LCMd:"URL_LOG_HEADER"Update_vm :%s", cmd);

    ret = system(cmd);
    if (0 != ret) {
        LCMON_API_LOG(LOG_ERR, "LCMd:"URL_LOG_HEADER"%s: system cmd error", __FUNCTION__);
        return -1;
    }

error:
    return ret;
}

