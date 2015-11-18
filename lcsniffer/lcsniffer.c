/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-20
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/resource.h>

#include "lc_agent_msg.h"
#include "lc_mongo_db.h"

#include "lc_snf.h"
#include "lc_db.h"
#include "lcs_socket.h"
#include "lcs_utils.h"
#include "lcs_usage.h"
#include "ovs_dfi.h"
#include "ovs_rsync.h"
#include "ovs_process.h"
#include "lcs_usage_api.h"
#include "framework/list.h"

extern struct list_head head_vm_server_load;
extern struct list_head head_vm_load;

extern pthread_mutex_t mutex_vm;
extern pthread_cond_t  cond_vm;
extern pthread_mutex_t mutex_vmser;
extern pthread_cond_t  cond_vmser;

extern int debug_lcsnfd, debug_dfi;

lcsnf_domain_t lcsnf_domain;

char *SNFLOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};
lc_snf_sknob_t *lc_snf_knob = NULL;

static int run_as_daemon(void)
{
    pid_t pid;
    int rv, fd;

    SNF_SYSLOG(LOG_DEBUG, "%s() before run\n", __FUNCTION__);
    pid = fork();
    if (pid < 0) {
        rv = -ECHILD;
        goto err;
    }
    if (pid > 0) {
        exit(0);
    }
    SNF_SYSLOG(LOG_DEBUG, "%s() after  run\n", __FUNCTION__);

    pid = setsid();
    if (pid < -1) {
        rv = -ESRCH;
        goto err;
    }

    if ((chdir("/")) < 0) {
        rv = errno;
        return -1;
    }

    fd = open("/dev/null", O_RDWR, 0);
    if (fd <= STDERR_FILENO) {
        rv = -ENOENT;
        goto err;
    } else {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
    umask(0027);

    SNF_SYSLOG(LOG_INFO, "%s() succeeded\n", __FUNCTION__);
    return 0;

err:
    SNF_SYSLOG(LOG_ERR, "%s() failed (rv=%d)\n", __FUNCTION__, rv);
    return rv;
}

static void sig_process(int sign_no)
{
    if (sign_no == SIGQUIT) {
        SNF_SYSLOG(LOG_INFO, "Receive SIGQUIT, close socket\n");
    } else if (sign_no == SIGTERM) {
        SNF_SYSLOG(LOG_INFO, "Receive SIGTERM, close socket\n");
    } else {
        return;
    }

    lcsnf_sock_exit();
    lcs_g_msg_destroy();

    return;
}

static void print_help(void)
{
    static const char *help =
        "Usage: lcsnfd [OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -v, --version      display the version and exit\n"
        " -d, --daemon       running as a daemon (default)\n"
        " -g, --gstart       graceful start\n"
        " -l, --log          log level\n"
        " --debug-lcsnfd     open debug info for lcsnfd\n"
        " --debug-dfi        open debug info for dfi kernel and agent\n";

    fprintf(stderr, "%s", help);
    return;
}

static int lc_snf_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    sprintf(cmd, "touch "LC_SNF_SHM_ID);
    system(cmd);

    shmkey = ftok(LC_SNF_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        printf("%s:SHM already existing, re-create it %d", __FUNCTION__,shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            printf("%s:Remove existing SHM error\n",__FUNCTION__);
        }
    }

    shmid = shmget(shmkey, sizeof(lc_snf_sknob_t), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        printf("Create shared memory for sniffer %d", shmid);
    } else {
        printf("Create shared memory for sniffer error!");
        return -1;
    }

    if ((lc_snf_knob = (lc_snf_sknob_t *)shmat(
                    shmid, (const void*)0, 0)) == (void *)-1) {
        printf("%s:shmat error:%s\n", __FUNCTION__,strerror(errno));
        return -1;
    }

    memset(lc_snf_knob, 0, sizeof(lc_snf_sknob_t));

    return 0;

}

static int parse_cmd_line(int argc, char *const argv[])
{
    lc_snf_sknob_t knob = {0};
    int option;
    const char *optstr = "hgl:vd?";
    struct option longopts[] = {
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"gstart", no_argument, NULL, 'g'},
        {"log", required_argument, NULL, 'l'},
        {"debug-lcsnfd", no_argument, NULL, '0'},
        {"debug-dfi", no_argument, NULL, '1'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (option) {
            case 'd':
                knob.lc_opt.lc_flags |= LC_FLAG_DAEMON;
                break;
            case 'v':
                knob.lc_opt.lc_flags |= LC_FLAG_VERSION;
                break;
            case 'g':
                break;
            case 'l':
                knob.lc_opt.log_level = atol(optarg);
                knob.lc_opt.lc_flags |= LC_FLAG_LOGSET;
                break;
            case '0':
                debug_lcsnfd = 1;
                break;
            case '1':
                debug_dfi = 1;
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
        fprintf(stderr, "LCC_SNIFFER_VERSION: %s\nXEN_AGENT_VERSION: %#X\n",
                VERSION, AGENT_VERSION_MAGIC);
        return -1;
    }

    /* help */
    if (knob.lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
        return -1;
    }

    if (lc_snf_sknob_init() == -1) {
        return -1;
    }
    memcpy(lc_snf_knob, &knob, sizeof(lc_snf_sknob_t));

    if (lc_snf_knob->lc_opt.lc_flags & LC_FLAG_LOGSET) {
        SNF_SYSLOG(LOG_INFO, "%s: Set log level to %d.\n",
                __FUNCTION__, lc_snf_knob->lc_opt.log_level);
    } else {
        lc_snf_knob->lc_opt.log_level = LOG_INFO;
        SNF_SYSLOG(LOG_INFO, "%s: Set log level to default (%d).\n",
                __FUNCTION__, lc_snf_knob->lc_opt.log_level);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    pthread_t t;
    const rlim_t kStackSize = PROCESS_STACK_SIZE;   // min stack size = 64 Mb
    struct rlimit rl;
    int result;
    const uint32_t mongo_apps =
         (1 << MONGO_APP_HOST_USAGE) |
         (1 << MONGO_APP_VM_USAGE) |
         (1 << MONGO_APP_VGW_USAGE) |
         (1 << MONGO_APP_TRAFFIC_USAGE) |
         (1 << MONGO_APP_HWDEV_USAGE);

    if (argc < 2) {
        print_help();
        exit(-1);
    }
    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    result = getrlimit(RLIMIT_STACK, &rl);
    SNF_SYSLOG(LOG_DEBUG, "getrlimit returned result = %d, stacksize=%ld, maxzise=%ld\n",
        result, (long)rl.rlim_cur, (long)rl.rlim_max);
    if (result == 0)
    {
        if (rl.rlim_cur < kStackSize)
        {
            SNF_SYSLOG(LOG_DEBUG, "set stack size to %ld\n", (long)kStackSize);
            rl.rlim_cur = kStackSize;
            rl.rlim_max = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
            {
                 SNF_SYSLOG(LOG_ERR, "setrlimit returned result = %d\n", result);
            }
        }
    }

    if (run_as_daemon() < 0) {
        exit(-1);
    }
    result = getrlimit(RLIMIT_STACK, &rl);
    SNF_SYSLOG(LOG_DEBUG, "getrlimit returned result = %d, stacksize=%ld, maxzise=%ld\n",
        result, (long)rl.rlim_cur, (long)rl.rlim_max);

    while (lcs_get_domain_from_config()) {
        SNF_SYSLOG(LOG_ERR, "%s: please config /usr/local/livecloud/conf/livecloud.conf.\n", __FUNCTION__);
        sleep(5);
    }

    /* init mysql db */
    if (lc_db_init("localhost", "root", "security421",
                "livecloud", "0", LC_DB_VERSION)) {
        SNF_SYSLOG(LOG_ERR, "%s: db init error.\n", __FUNCTION__);
        exit(-1);
    }

    if (lc_db_master_init(lcsnf_domain.mysql_master_ip, "root",
        "security421", "livecloud", "20130", LC_DB_VERSION) != 0) {
        SNF_SYSLOG(LOG_ERR, "lc_db_master_init error\n");
        exit(-1);
    }

    /* init socket */
    if (lcsnf_sock_init() < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: socket init error.\n", __FUNCTION__);
        exit(-1);
    }

    /* signals */
    signal(SIGTERM, sig_process);
    signal(SIGQUIT, sig_process);
    lcs_g_msg_init();

    list_init(&head_vm_load);
    list_init(&head_vm_server_load);

    if (mongo_db_init("127.0.0.1", "20011", "root",
                "security421", "livecloud", LC_DB_VERSION, mongo_apps) < 0) {
        SNF_SYSLOG(LOG_ERR, "%s: mongo_db_init error\n", __FUNCTION__);
        exit(1);
    }

    /* start ovs-dfi thread */

    if (pthread_create(&t, NULL, ovs_dfi_thread, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create ovs-dfi thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    if (pthread_create(&t, NULL, api_thread, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create api thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    /* start usage thread */
    if (pthread_create(&t, NULL, usage_socket_thread, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create vif-traffic socket thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    if (pthread_create(&t, NULL, usage_handle_thread, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create vif-traffic handle thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    /* start db insertion thread */
    if (pthread_create(&t, NULL, list_vm_server_insert, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create vm server insert thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    if (pthread_create(&t, NULL, list_vm_insert, NULL) != 0) {
        SNF_SYSLOG(LOG_DEBUG, "%s: create vm insert thread thread error.\n",
                __FUNCTION__);
        exit(-1);
    }

    pthread_join(t, NULL);

    pthread_mutex_destroy(&mutex_vm);
    pthread_cond_destroy(&cond_vm);
    pthread_mutex_destroy(&mutex_vmser);
    pthread_cond_destroy(&cond_vmser);
    return 0;
}
