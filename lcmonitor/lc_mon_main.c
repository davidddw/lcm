/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_main.c
 * Port from lc_vm_main.c
 * Date: 2012-11-26
 *
 * Description: monitor the state and resource usage
 *                    of Host and VM/Vedge
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include "lc_bus.h"
#include "lc_db.h"
#include "lc_mon.h"
#include "lc_queue.h"
#include "lc_xen.h"
#include "lc_mon_qctrl.h"
#include "lc_agent_msg.h"
#include "lc_db_log.h"
#include "lc_utils.h"
#include "lc_mon_nsp.h"
#include "lc_mon_rados.h"

lc_mon_sknob_t *lc_mon_knob;
pthread_mutex_t lc_dpq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_cond = PTHREAD_COND_INITIALIZER;
extern pthread_mutex_t lc_dpq_host_mutex;
extern pthread_cond_t lc_dpq_host_cond;
extern pthread_mutex_t lc_dpq_vm_mutex;
extern pthread_cond_t lc_dpq_vm_cond;
pthread_mutex_t lc_dpq_nfs_chk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_dpq_nfs_chk_cond = PTHREAD_COND_INITIALIZER;
extern pthread_mutex_t lc_dpq_vgw_ha_mutex;
extern pthread_cond_t lc_dpq_vgw_ha_cond;
extern pthread_mutex_t lc_dpq_bk_vm_health_mutex;
extern pthread_cond_t lc_dpq_bk_vm_health_cond;

lc_dpq_host_queue_t lc_host_data_queue;
lc_dpq_vm_queue_t lc_vm_data_queue;

lcmon_domain_t lcmon_domain;
struct config lcmon_config;

int LCMON_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *LCMON_LOG_LEVEL_P = &LCMON_LOG_DEFAULT_LEVEL;

char *LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

int lc_mon_parase_config()
{
    FILE *fp = NULL;
    char buf[MAX_LOG_MSG_LEN] = {0};
    char *s = NULL;
    char *key, *value;
    int flag = 0;

#define FIELD_CEPH_SWITCH_THRESHOLD 0x0001
#define FIELD_CEPH_CLUSTER_NAME     0x0010
#define FIELD_CEPH_USER_NAME        0x0100
#define FIELD_CEPH_CONF_FILE        0x1000
#define FIELD_CEPH_ALL              (FIELD_CEPH_SWITCH_THRESHOLD | \
                                     FIELD_CEPH_CLUSTER_NAME | \
                                     FIELD_CEPH_USER_NAME | \
                                     FIELD_CEPH_CONF_FILE)
    int ceph_field_flag = 0;

    fp = fopen(LC_MON_CONFIG_FILE, "r");
    if (NULL == fp) {
        LCMON_LOG(LOG_ERR, "%s error open config file", __FUNCTION__);
        return 1;
    }

    while (fgets(buf, MAX_LOG_MSG_LEN, fp) != NULL) {
        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }
        if (buf[0] == '[') {
            for (s = buf + strlen(buf) - 1; *s == ']' ||  *s == ' ' ||  *s == '\n'; s--) {
                *s = '\0';
            }
            for (s = buf; *s == '[' || *s == ' '; s++) {
                //do nothing
            }
            if (strcmp(s, LC_MON_SIGN) && strcmp(s, LC_MON_GLOBAL_SIGN)) {
                flag = 0;
            } else {
                flag = 1;
            }
            continue;
        }

        if (!flag) {
            continue;
        }

        key = strtok(buf, "=");
        for (s = key + strlen(key) - 1; *s == ' '; s--) {
            *s = '\0';
        }
        for (; *key == ' '; key++) {
            //do nothing
        }

        value = strtok(NULL, "=");
        for (s = value + strlen(value) - 1; *s == ' ' ||  *s == '\n'; s--) {
            *s = '\0';
        }
        for (; *value == ' '; value++) {
            //do nothing
        }

        if (!strcmp(key, LC_MON_KEY_DOMAIN_LCUUID)) {
            strncpy(lcmon_domain.lcuuid, value,
                    sizeof(lcmon_domain.lcuuid));

        } else if (!strcmp(key, LC_MON_KEY_LOCAL_CTRL_IP)) {
            strncpy(lcmon_domain.local_ctrl_ip, value,
                    sizeof(lcmon_domain.local_ctrl_ip));

        } else if (!strcmp(key, LC_MON_KEY_MYSQL_MASTER_IP)) {
            strncpy(lcmon_domain.mysql_master_ip, value,
                    sizeof(lcmon_domain.mysql_master_ip));

        } else if (strcmp(key, LC_MON_KEY_CEPH_RADOS_MONITOR) == 0) {
            if (strcmp(value, "enabled") == 0) {
                lcmon_config.ceph_rados_monitor_enabled = 1;
            } else {
                lcmon_config.ceph_rados_monitor_enabled = 0;
            }

        } else if (strcmp(key, LC_MON_KEY_CEPH_SWITCH_THRESHOLD) == 0) {
            lcmon_config.ceph_switch_threshold = atoi(value);
            ceph_field_flag |= FIELD_CEPH_SWITCH_THRESHOLD;

        } else if (strcmp(key, LC_MON_KEY_CEPH_CLUSTER_NAME) == 0) {
            strncpy(lcmon_config.ceph_cluster_name, value,
                    sizeof lcmon_config.ceph_cluster_name);
            ceph_field_flag |= FIELD_CEPH_CLUSTER_NAME;

        } else if (strcmp(key, LC_MON_KEY_CEPH_USER_NAME) == 0) {
            strncpy(lcmon_config.ceph_user_name, value,
                    sizeof lcmon_config.ceph_user_name);
            ceph_field_flag |= FIELD_CEPH_USER_NAME;

        } else if (strcmp(key, LC_MON_KEY_CEPH_CONF_FILE) == 0) {
            strncpy(lcmon_config.ceph_conf_file, value,
                    sizeof lcmon_config.ceph_conf_file);
            ceph_field_flag |= FIELD_CEPH_CONF_FILE;

        }
    }

    fclose(fp);
    if (lcmon_config.ceph_rados_monitor_enabled &&
            ceph_field_flag != FIELD_CEPH_ALL) {
        if (!(ceph_field_flag & FIELD_CEPH_SWITCH_THRESHOLD)) {
            LCMON_LOG(LOG_ERR, "No ceph_switch_threshold in conf");
        }
        if (!(ceph_field_flag & FIELD_CEPH_CLUSTER_NAME)) {
            LCMON_LOG(LOG_ERR, "No ceph_cluster_name in conf");
        }
        if (!(ceph_field_flag & FIELD_CEPH_USER_NAME)) {
            LCMON_LOG(LOG_ERR, "No ceph_user_name in conf");
        }
        if (!(ceph_field_flag & FIELD_CEPH_CONF_FILE)) {
            LCMON_LOG(LOG_ERR, "No ceph_conf_file in conf");
        }
        return 1;
    }
    if (!lcmon_domain.lcuuid[0]) {
        LCMON_LOG(LOG_ERR, "No domain.lcuuid in conf");
        return 1;
    }
    if (!strcmp(lcmon_domain.lcuuid, LC_MON_DOMAIN_LCUUID_DEFAULT)) {
        LCMON_LOG(LOG_ERR, "Wrong domain.lcuuid in conf");
        return 1;
    }
    return 0;
}

int lcmon_log_level_init()
{
    int ret = 0;
    int shmid;
    key_t shmkey;
    char cmd[100];

    LCMON_LOG(LOG_INFO, "%s Enter\n", __FUNCTION__);
    sprintf(cmd, "touch "LC_MON_SYS_LEVEL_SHM_ID);
    system(cmd);
    shmkey = ftok(LC_MON_SYS_LEVEL_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        LCMON_LOG(LOG_DEBUG, "SHM already existing, re-create it %d", shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            LCMON_LOG(LOG_ERR, "Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        LCMON_LOG(LOG_DEBUG, "Create shared memory for lcmon %d", shmid);
    } else {
        LCMON_LOG(LOG_ERR, "Create shared memory for lcmon error!");
        ret = -1;
        goto error;
    }

    if ((LCMON_LOG_LEVEL_P = (int *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        LCMON_LOG(LOG_ERR, "shmat error:%s\n",strerror(errno));
        ret = -1;
        goto error;
    }

    *LCMON_LOG_LEVEL_P = LCMON_LOG_DEFAULT_LEVEL;
error:
    LCMON_LOG(LOG_DEBUG, "End\n");
    return ret;
}

void print_help(void)
{
    static const char *help =
        "Usage: vmdriver [OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -v, --version      display the version and exit\n"
        " -d, --daemon       running as a daemon (default)\n"
        " -f, --foreground   running foreground\n"
        " -g, --gstart       graceful learning\n"
        " -l, --log          log level\n"
        " -t, --thread       processing thread number\n"
        " -a, --address      controller ip address\n";

    printf("%s", help);
    return;
}

static int lc_handle_dpq_data(int type, int htype, void *pdata)
{
    lc_data_host_t *phostdata = NULL;
    lc_data_vm_t *pvmdata = NULL;
    dpq_obj_t *msg_obj = NULL;

    if (htype == LC_POOL_TYPE_XEN) {
        msg_obj = &dpq_q_xen_msg[type];
        if (msg_obj) {
            switch (type) {
            case DPQ_TYPE_HOST_INFO:
                phostdata = (lc_data_host_t *)pdata;
                msg_obj->dpq_func((void *)&phostdata->u.host, phostdata->htype);
                break;
            case DPQ_TYPE_VM_INFO:
                pvmdata = (lc_data_vm_t *)pdata;
                msg_obj->dpq_func((void *)pvmdata, pvmdata->htype);
                break;
            }
        }
    } else if (htype == LC_POOL_TYPE_VMWARE) {
        msg_obj = &dpq_q_vmware_msg[type];
        if (msg_obj) {
            switch (type) {
            case DPQ_TYPE_HOST_INFO:
                phostdata = (lc_data_host_t *)pdata;
                msg_obj->dpq_func((void *)&phostdata->u.vmware_host, phostdata->htype);
                break;
            case DPQ_TYPE_VM_INFO:
                pvmdata = (lc_data_vm_t *)pdata;
                msg_obj->dpq_func((void *)&pvmdata->u_vm.vmware_vm, pvmdata->htype);
                break;
            }
        }
    } else if (htype == LC_POOL_TYPE_UNKNOWN) {
        msg_obj = &dpq_q_vm_check_msg[DPQ_TYPE_VM_CHECK];
        if (msg_obj) {
            pvmdata = (lc_data_vm_t *)pdata;
            msg_obj->dpq_func((void *)pvmdata, pvmdata->htype);
        }
    }
    return 0;
}

int lc_mon_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "/bin/touch "LC_MON_SHM_ID);
    system(cmd);

    shmkey = ftok(LC_MON_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        printf("%s:SHM already existing, re-create it %d", __FUNCTION__,shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            printf("%s:Remove existing SHM error\n",__FUNCTION__);
        }
    }

    shmid = shmget(shmkey, sizeof(lc_mon_sknob_t), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        printf("Create shared memory for monitor %d", shmid);
    } else {
        printf("Create shared memory for monitor error!");
        return -1;
    }

    if ((lc_mon_knob = (lc_mon_sknob_t *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        printf("%s:shmat error:%s\n", __FUNCTION__,strerror(errno));
        return -1;
    }

    memset(lc_mon_knob, 0, sizeof(lc_mon_sknob_t));

    return 0;

}

int lc_daemon(void )
{
    int fd;

    switch (fork()) {
        case -1:
            /* Fork failed! */
            return -1;

        case 0:
            /* Daemon process */
            break;

        default:
            /* Valid PID returned, exit the parent process. */
            exit(EXIT_SUCCESS);
            break;
    }

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    if (setsid() == -1) {
        return -1;
    }

    if ((chdir("/")) < 0) {
        LCMON_LOG(LOG_ERR, "Change working directory error\n");
        return -1;
    }

    /* Close out the standard file descriptors */
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }

    return 0;
}

/* Return -1 on error, 0 on success */
int parse_cmd_line(int argc, char *const argv[])
{
    lc_mon_sknob_t knob = {0};
    int option;
    const char *optstr = "hgl:t:vfd?";
    struct option longopts[] = {
        {"thread", required_argument, NULL, 't'},
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"foreground", no_argument, NULL, 'f'},
        {"gstart", no_argument, NULL, 'g'},
        {"log", required_argument, NULL, 'l'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (option) {
            case 't':
                knob.lc_opt.lc_flags |= LC_FLAG_THREAD;
                knob.lc_opt.thread_num = LC_MAX_THREAD_NUM - 1;/*atol(optarg);*/
                break;
            case 'd':
                knob.lc_opt.lc_flags |= LC_FLAG_DAEMON;
                break;
            case 'v':
                knob.lc_opt.lc_flags |= LC_FLAG_VERSION;
                break;
            case 'f':
                knob.lc_opt.lc_flags |= LC_FLAG_FOREGROUND;
                break;
            case 'g':
                break;
            case 'l':
                knob.lc_opt.log_level = atol(optarg);
                knob.lc_opt.log_on = 1;
                knob.lc_opt.lc_flags |= LC_FLAG_LOGSET;
                break;
            case '?':
                knob.lc_opt.lc_flags |= LC_FLAG_HELP;
                break;
            default:
                knob.lc_opt.lc_flags |= LC_FLAG_HELP;
                break;
        }
    }

    /* version */
    if (knob.lc_opt.lc_flags & LC_FLAG_VERSION) {
        fprintf(stderr, "LCC_MONITOR_VERSION: %s\nXEN_AGENT_VERSION: %#X\n",
                VERSION, AGENT_VERSION_MAGIC);
        return -1;
    }

    /* help */
    if (knob.lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
        return -1;
    }

    if (lc_mon_sknob_init() == -1) {
        return -1;
    }
    memcpy(lc_mon_knob, &knob, sizeof(lc_mon_sknob_t));

    if (lc_mon_knob->lc_opt.lc_flags & LC_FLAG_LOGSET) {
        printf("Set log level to %d\n", lc_mon_knob->lc_opt.log_level);
    } else {
        printf("Set log level to default level (LOG_WARNING)\n");
        lc_mon_knob->lc_opt.log_level = LOG_WARNING;
        lc_mon_knob->lc_opt.log_on = 1;
    }

    /* set thread number */
    if (lc_mon_knob->lc_opt.lc_flags & LC_FLAG_THREAD) {
        if (lc_mon_knob->lc_opt.thread_num > LC_MAX_THREAD_NUM) {
            printf("Thread# exceed max thread# (%d)\n", LC_MAX_THREAD_NUM);
            return -1;
        }
    } else {
        lc_mon_knob->lc_opt.thread_num = LC_DEFAULT_THREAD_NUM;
    }

    return 0;
}

#define LOG_PATH "/var/tmp/log_switch"
void *lc_mon_tsk_req_thread()
{
    int i;
    lc_data_t *pdata;
    FILE *fp = NULL;
    char buf[10];
    char *p = NULL, *q = NULL;
    char *pbuf = NULL;

    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    for (i = 0; i < MAX_THREAD; i++) {
        if (thread_url[i].tid == 0) {
            thread_url[i].tid = pthread_self();
            break;
        }
    }
    while (1) {

        memset(buf, 0, 10);
        fp = fopen(LOG_PATH, "r");
        if(fp){
           fread(buf, 10, 1, fp);
           fclose(fp);
           fp = NULL;
           pbuf = buf;
           p = strsep(&pbuf, "#");
           q = strsep(&p, ",");
           lc_mon_knob->lc_opt.log_level = atoi(p);
           lc_mon_knob->lc_opt.log_on = atoi(q);
        }
        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_cond, &lc_dpq_mutex);

        if (!lc_is_dpq_empty(&lc_data_queue)) {
            pdata = (lc_data_t *)lc_dpq_dq(&lc_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_mutex);

        /* process recevied message here */
        if (pdata) {
            if (pdata->data_ptr) {
                lc_mon_msg_handler(pdata->data_ptr);
            }
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_mutex);
            if (!lc_is_dpq_empty(&lc_data_queue)) {
                pdata = (lc_data_t *)lc_dpq_dq(&lc_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_mutex);
            /* process recevied message here */
            if (pdata) {
                if (pdata->data_ptr) {
                    lc_mon_msg_handler(pdata->data_ptr);
                }
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_vm_stats_thread()
{
    lc_data_vm_t *pdata;

    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_vm_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_vm_cond, &lc_dpq_vm_mutex);

        if (!lc_is_vm_dpq_empty(&lc_vm_data_queue)) {
            pdata = (lc_data_vm_t *)lc_vm_dpq_dq(&lc_vm_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_vm_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_handle_dpq_data(DPQ_TYPE_VM_INFO, pdata->htype, (void *)pdata);
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_vm_mutex);
            if (!lc_is_vm_dpq_empty(&lc_vm_data_queue)) {
                pdata = (lc_data_vm_t *)lc_vm_dpq_dq(&lc_vm_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_vm_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_vm_mutex);
            /* process recevied message here */
            if (pdata) {
                lc_handle_dpq_data(DPQ_TYPE_VM_INFO, pdata->htype, (void *)pdata);
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_vgw_ha_thread()
{
    lc_data_vgw_ha_t *pdata;
    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_vgw_ha_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_vgw_ha_cond, &lc_dpq_vgw_ha_mutex);

        if (!lc_is_vgw_ha_dpq_empty(&lc_vgw_ha_data_queue)) {
            pdata = (lc_data_vgw_ha_t *)lc_vgw_ha_dpq_dq(&lc_vgw_ha_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_vgw_ha_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_mon_vgw_ha_handler(pdata);
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_vgw_ha_mutex);
            if (!lc_is_vgw_ha_dpq_empty(&lc_vgw_ha_data_queue)) {
                pdata = (lc_data_vgw_ha_t *)lc_vgw_ha_dpq_dq(&lc_vgw_ha_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_vgw_ha_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_vgw_ha_mutex);
            /* process recevied message here */
            if (pdata) {
                lc_mon_vgw_ha_handler(pdata);
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_bk_vm_health_thread()
{
    lc_data_bk_vm_health_t *pdata;
    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_bk_vm_health_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_bk_vm_health_cond, &lc_dpq_bk_vm_health_mutex);

        if (!lc_is_bk_vm_health_dpq_empty(&lc_bk_vm_health_data_queue)) {
            pdata = (lc_data_bk_vm_health_t *)lc_bk_vm_health_dpq_dq(&lc_bk_vm_health_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_bk_vm_health_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_mon_bk_vm_health_handler(pdata);
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_bk_vm_health_mutex);
            if (!lc_is_bk_vm_health_dpq_empty(&lc_bk_vm_health_data_queue)) {
                pdata = (lc_data_bk_vm_health_t *)
                    lc_bk_vm_health_dpq_dq(&lc_bk_vm_health_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_bk_vm_health_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_bk_vm_health_mutex);
            /* process recevied message here */
            if (pdata) {
                lc_mon_bk_vm_health_handler(pdata);
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_nfs_chk_thread()
{
    lc_data_nfs_chk_t *pdata;
    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_nfs_chk_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_nfs_chk_cond, &lc_dpq_nfs_chk_mutex);

        if (!lc_is_nfs_chk_dpq_empty(&lc_nfs_chk_data_queue)) {
            pdata = (lc_data_nfs_chk_t *)lc_nfs_chk_dpq_dq(&lc_nfs_chk_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_nfs_chk_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_mon_nfs_chk_handler(pdata);
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_nfs_chk_mutex);
            if (!lc_is_nfs_chk_dpq_empty(&lc_nfs_chk_data_queue)) {
                pdata = (lc_data_nfs_chk_t *)lc_nfs_chk_dpq_dq(&lc_nfs_chk_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_nfs_chk_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_nfs_chk_mutex);
            /* process recevied message here */
            if (pdata) {
                lc_mon_nfs_chk_handler(pdata);
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_host_stats_thread()
{
    lc_data_host_t *pdata;

    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        pdata = NULL;
        pthread_mutex_lock(&lc_dpq_host_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_dpq_host_cond, &lc_dpq_host_mutex);

        if (!lc_is_host_dpq_empty(&lc_host_data_queue)) {
            pdata = (lc_data_host_t *)lc_host_dpq_dq(&lc_host_data_queue);
        }
        pthread_mutex_unlock(&lc_dpq_host_mutex);

        /* process recevied message here */
        if (pdata) {
            lc_handle_dpq_data(DPQ_TYPE_HOST_INFO, pdata->htype, (void *)pdata);
            free(pdata);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_dpq_host_mutex);
            if (!lc_is_host_dpq_empty(&lc_host_data_queue)) {
                pdata = (lc_data_host_t *)lc_host_dpq_dq(&lc_host_data_queue);
            } else {
                pthread_mutex_unlock(&lc_dpq_host_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_dpq_host_mutex);
            /* process recevied message here */
            if (pdata) {
                lc_handle_dpq_data(DPQ_TYPE_HOST_INFO, pdata->htype, (void *)pdata);
                free(pdata);
            }
        }
    }
    pthread_exit(NULL);
}

void *lc_mon_nsp_state_thread()
{
    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCMON_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    /* TODO: lookup the db table pool_v2_2 to find the list of NSPs in distinct pools
     * NOTE: pool type to indicate NSP, pool id to indicate NSP cluster

     * TODO: send the heartbeat info to the target NSPs
     * NOTE:

     * TODO: handle the replies of each target NSP
     * NOTE:

     * TODO: according to the comparison with the state in db, send msg to lcpd to
     * handle the failed NSPs
     * NOTE: the msg should contain the pool id and the host id
     */
    lc_nsp_state_monitor();

    pthread_exit(NULL);
}

int lc_mon_thread_create(int num)
{
    int ret, i, success = 0;
    pthread_t *thread;
    int remained_thread_num = 0;
    int thread_num = 0;

    thread = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (!thread) {
        LCMON_LOG(LOG_ERR, "Create thread buffer failed!");
        return -1;
    }

    memset(thread, 0, num * sizeof(thread));
    for (i = 0; i < LC_MON_TSK_REQ_PTHREAD_1; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_tsk_req_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
    for (i = 0; i < LC_MON_NSP_PTHREAD_1; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_nsp_state_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
    for (i = 0; i < LC_MON_HOST_PTHREAD_2; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_host_stats_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
    for (i = 0; i < LC_MON_HA_CHECK_PTHREAD_2; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_vgw_ha_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
    for (i = 0; i < LC_MON_BK_VM_CHECK_PTHREAD_3; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_bk_vm_health_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
    if (lcmon_config.ceph_rados_monitor_enabled) {
        for (i = 0; i < LC_MON_RADOS_PTHREAD_1; i++) {
            LC_MON_CNT_INC(lc_mon_thread_create);
            if ((ret = pthread_create(thread, NULL, lc_rados_monitor_thread, NULL)) != 0) {
                 LCMON_LOG(LOG_ERR, "Create thread failed!");
                 LC_MON_CNT_INC(lc_mon_thread_create_failed);
            } else {
                 LCMON_LOG(LOG_INFO, "Create thread successfully");
                 LC_MON_CNT_INC(lc_mon_thread_create_success);
                 success++;
            }
            thread++;
            ++thread_num;
        }
    }
#if 0
    for (i = 0; i < LC_MON_NFS_CHECK_PTHREAD_1; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_nfs_chk_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
         ++thread_num;
    }
#endif
    remained_thread_num = num - thread_num;
    for (i = 0; i < remained_thread_num; i++) {
         LC_MON_CNT_INC(lc_mon_thread_create);
         if ((ret = pthread_create(thread, NULL, lc_mon_vm_stats_thread, NULL)) != 0) {
              LCMON_LOG(LOG_ERR, "Create thread failed!");
              LC_MON_CNT_INC(lc_mon_thread_create_failed);
         } else {
              LCMON_LOG(LOG_INFO, "Create thread successfully");
              LC_MON_CNT_INC(lc_mon_thread_create_success);
              success++;
         }
         thread++;
    }
    if (!success) {
        LCMON_LOG(LOG_ERR, "No thread created!");
        return -1;
    }

    return 0;
}

void lc_mon_timer_handler(int signal)
{
    lc_monitor_check_top_half();
}

void lc_mon_timer_init(void)
{
    struct itimerval val;

    val.it_value.tv_sec = MON_RESOURCE_TIMER;
    val.it_value.tv_usec = 0;
    val.it_interval.tv_sec = MON_RESOURCE_TIMER;
    val.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL,&val,NULL);
    signal(SIGALRM, lc_mon_timer_handler);

    return;
}



#include <stdio.h>
#include <stdlib.h>


void sig_process(int sign_no)
{
    int data_fd;
    if (sign_no == SIGQUIT) {
        LCMON_LOG(LOG_INFO, "Receive SIGQUIT, close socket\n");
    } else if (sign_no == SIGTERM) {
        LCMON_LOG(LOG_INFO, "Receive SIGTERM, close socket\n");
    } else {
        return;
    }
    data_fd = get_data_sock_fd();
    if (data_fd > 0) {
        shutdown(data_fd, SHUT_RD);
        close(data_fd);
    }
    return;
}

static int clear_db_vm_action()
{
    char buffer[200];
    int ret;

    memset(buffer, 0, 200);
    sprintf(buffer, "mysql -uroot -psecurity421 -D livecloud -e \"update vm_v%s set action=0\"", LC_DB_VERSION);
    ret = lc_call_system(buffer, "Monitor", "", "", lc_monitor_db_log);

    if (!ret) {
        LCMON_LOG(LOG_INFO, "Clear action flag successfully");
    } else {
        LCMON_LOG(LOG_INFO, "Clear action flag failed");
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int g_start = 0;

    (void)lcmon_log_level_init();
    LCMON_LOG(LOG_DEBUG, "argc=%d\n", argc);

    if (argc < 2) {
        print_help();
        exit(-1);
    }

    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    if (lc_mon_knob->lc_opt.lc_flags & LC_FLAG_DAEMON) {
        LCMON_LOG(LOG_INFO, "Start LC as a daemon...");
        lc_daemon();
    }

    while (lc_mon_parase_config()) {
        LCMON_LOG(LOG_ERR, "%s: please config /usr/local/livecloud/conf/livecloud.conf.\n", __FUNCTION__);
        sleep(5);
    }

    lc_dpq_init(&lc_data_queue);
    lc_host_dpq_init(&lc_host_data_queue);
    lc_vm_dpq_init(&lc_vm_data_queue);
    lc_vgw_ha_dpq_init(&lc_vgw_ha_data_queue);
    lc_nfs_chk_dpq_init(&lc_nfs_chk_data_queue);
    lc_bk_vm_health_dpq_init(&lc_bk_vm_health_data_queue);

    if (lc_db_init("localhost", "root", "security421",
        "livecloud", "0", LC_DB_VERSION) != 0) {
        LCMON_LOG(LOG_INFO, "lc_db_init error\n");
    }

    if (lc_db_master_init(lcmon_domain.mysql_master_ip, "root",
        "security421", "livecloud", "20130", LC_DB_VERSION) != 0) {
        LCMON_LOG(LOG_INFO, "lc_db_master_init error\n");
    }

    clear_db_vm_action();

    for (i = 0; i < MAX_THREAD; i++) {
        thread_url[i].tid = 0;
        memset(thread_url[i].url, 0, LC_MAX_CURL_LEN);
    }
    lc_db_thread_init();
    lc_db_master_thread_init();
    if (lc_mon_thread_create(lc_mon_knob->lc_opt.thread_num) < 0) {
        LCMON_LOG(LOG_ERR, "Create thread failure, thread num=%d\n",
               lc_mon_knob->lc_opt.thread_num);
        exit(-1);
    }

    if (lc_mon_knob->lc_opt.lc_flags & LC_FLAG_GSTART) {
        LCMON_LOG(LOG_INFO, "Start LC as in graceful restart mode...");
        g_start = 1;
    }

    xen_common_init();
    if (lc_sock_init() != 0) {
        exit(-1);
    }

    if (lc_mon_init_bus() != 0) {
        exit(-1);
    }

    signal(SIGTERM, sig_process);
    signal(SIGQUIT, sig_process);

    lc_mon_timer_init();
    lc_main_process();
    return 0;
}

int lc_send_msg(lc_mbuf_t *msg)
{
#if 0
    int nsend, len;
    int ret = 0;
    lc_mbuf_hdr_t *hdr = (lc_mbuf_hdr_t *)msg;;

    LCMON_LOG(LOG_INFO, "%s msg send out: dir=%08x type=%d len=%d seq=%d \n",
               __FUNCTION__, hdr->direction, hdr->type, hdr->length, hdr->seq);
    if (!msg) {
        return -1;
    }

    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.magic) {
        case MSG_MG_MON2CHK:

            nsend = sendto(lc_sock.sock_rcv_chk, msg, len, 0,
                (struct sockaddr *)&sa_mon_snd_chk, sizeof(struct sockaddr_un));
            if (nsend < 0) {
                LCMON_LOG(LOG_ERR, "%s err \n", __FUNCTION__);
                ret = -1;
            }
            break;
        default:
            LCMON_LOG(LOG_ERR, "%s err \n", __FUNCTION__);
            return -1;

    }
    return ret;
#endif
    return 0;
}

int lc_publish_msg(lc_mbuf_t *msg)
{
    int len;
    int ret = 0;
    lc_mbuf_hdr_t *hdr = (lc_mbuf_hdr_t *)msg;;

    LCMON_LOG(LOG_INFO, "%s msg send out: dir=%08x type=%d len=%d seq=%d \n",
               __FUNCTION__, hdr->direction, hdr->type, hdr->length, hdr->seq);
    if (!msg) {
        return -1;
    }

    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.magic) {
        case MSG_MG_MON2DRV:
            ret = lc_bus_lcmond_publish_unicast((char *)msg, len,
                    LC_BUS_QUEUE_VMDRIVER);
            if (ret) {
                LCMON_LOG(LOG_ERR, "%s err magic=0x%8x\n",
                        __FUNCTION__, msg->hdr.magic);
            }
            break;

        default:
            LCMON_LOG(LOG_ERR, "%s maggic error.\n", __FUNCTION__);
            return -1;

    }
    return ret;
}
