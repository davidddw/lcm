#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/shm.h>

#include "lc_healthcheck.h"
#include "lc_queue.h"

lc_chk_sknob_t *lc_chk_knob = NULL;
pthread_mutex_t lc_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_msg_cond = PTHREAD_COND_INITIALIZER;
extern lc_msg_t *lc_msgq_dq(lc_msg_queue_t *msgq);

void print_help(void)
{
    static const char *help =
        "Usage: healthcheck [OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -v, --version      display the version and exit\n"
        " -d, --daemon       running as a daemon (default)\n"
        " -f, --foreground   running foreground\n"
        " -l, --log          log level\n"
        " -i, --interval     duration between two messages\n"
        " -t, --threshold    msgs lost before graceful restart\n";

    printf("%s", help);
    return;
}

int lc_chk_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "/bin/touch "LC_CHK_SHM_ID);
    system(cmd);

    shmkey = ftok(LC_CHK_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        printf("%s:SHM already existing, re-create it %d", __FUNCTION__, shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            printf("%s:Remove existing SHM error\n",__FUNCTION__);
        }
    }

    shmid = shmget(shmkey, sizeof(lc_chk_sknob_t), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        syslog(LOG_INFO,"%s:Create a shared memory segment %d", __FUNCTION__, shmid);
    } else {
        syslog(LOG_ERR, "%s:Create a shared memory error! shmid=%d",__FUNCTION__, shmid);
        return -1;
    }

    if ((lc_chk_knob = (lc_chk_sknob_t *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        printf("%s:shmat error:%s\n", __FUNCTION__,strerror(errno));
        return -1;
    }

    memset(lc_chk_knob, 0, sizeof(lc_chk_sknob_t));


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
    int option;
    const char *optstr = "hi:t:l:vfd?";
    struct option longopts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"threshold", required_argument, NULL, 't'},
        {"log", required_argument, NULL, 'l'},
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"foreground", no_argument, NULL, 'f'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (option) {
            case 'i':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_INTERVAL;
                lc_chk_knob->lc_opt.check_interval = atol(optarg);
                break;
            case 't':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_THRESHOLD;
                lc_chk_knob->lc_opt.fail_threshold = atol(optarg);
                break;
            case 'l':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_LOGLEVEL;
                lc_chk_knob->lc_opt.log_level = atol(optarg);
                break;
            case 'd':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_DAEMON;
                break;
            case 'v':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_VERSION;
                break;
            case 'f':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_FOREGROUND;
                break;
            case '?':
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_HELP;
                break;
            default:
                lc_chk_knob->lc_opt.lc_flags |= LC_FLAG_HELP;
                break;
        }
    }

    /* version */
    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_VERSION) {
        printf("Live Cloud, version %s\n", VERSION);
        return -1;
    }

    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_INTERVAL) {
        printf("Set heartbeat interval to %d\n", lc_chk_knob->lc_opt.check_interval);
    } else {
        printf("Set heartbeat interval to default 4s\n");
        lc_chk_knob->lc_opt.check_interval = 4;
    }

    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_THRESHOLD) {
        printf("Set fail threshold to %d\n", lc_chk_knob->lc_opt.fail_threshold);
    } else {
        printf("Set fail threshold to default 10\n");
        lc_chk_knob->lc_opt.fail_threshold = 10;
    }

    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_LOGLEVEL) {
        printf("Set log level to %d\n", lc_chk_knob->lc_opt.log_level);
    } else {
        printf("Set log level to default level (4)\n");
        lc_chk_knob->lc_opt.log_level = 4;
    }

    /* help */
    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
        return -1;
    }

    return 0;
}

void *lc_chk_thread()
{

    lc_msg_t *msg;

    while (1) {

        msg = NULL;
        pthread_mutex_lock(&lc_msg_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_msg_cond, &lc_msg_mutex);

        if (!lc_is_msgq_empty(&lc_data_queue)) {
            msg = (lc_msg_t *)lc_msgq_dq(&lc_data_queue);
        }
        pthread_mutex_unlock(&lc_msg_mutex);

        /* process recevied message here */
        if (msg) {
            chk_msg_handler((struct lc_msg_head *)msg);
            LC_LOG(LOG_INFO, "type=%d magic=%x, len=%d\n",
                       msg->mbuf.hdr.type, msg->mbuf.hdr.magic, msg->mbuf.hdr.length);
            free(msg);
        }

        /* is there any messages still available in message queue? */
        while (1) {
            pthread_mutex_lock(&lc_msg_mutex);
            if (!lc_is_msgq_empty(&lc_data_queue)) {
                msg = (lc_msg_t *)lc_msgq_dq(&lc_data_queue);
            } else {
                pthread_mutex_unlock(&lc_msg_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_msg_mutex);
            /* process recevied message here */
            if (msg) {
                chk_msg_handler((struct lc_msg_head *)msg);
                LC_LOG(LOG_DEBUG, "type=%d magic=%x\n", msg->mbuf.hdr.type,
                        msg->mbuf.hdr.magic);
                free(msg);
            }
        }
    }
    pthread_exit(NULL);
}

int lc_chk_thread_create()
{
    pthread_t *thread;

    thread = (pthread_t *)malloc(sizeof(pthread_t));
    if (!thread) {
        LC_LOG(LOG_ERR, "Create thread buffer failed!");
        return -1;
    }

    memset(thread, 0, sizeof(thread));

    if (pthread_create(thread, NULL, lc_chk_thread, NULL) != 0) {
        LC_LOG(LOG_ERR, "Create thread failed!");
    } else {
        LC_LOG(LOG_INFO, "Create thread successfully");
    }

    return 0;
}

void graceful_restart()
{
    int ret;
    LC_LOG(LOG_INFO, "%s: killing processes...", __FUNCTION__);
    ret = system(LC_KILL_ALL);
    LC_LOG(LOG_INFO, "killing livecloud daemons %s\n", ret == 0 ? "ok" : "failed");
    LC_LOG(LOG_INFO, "%s: restarting...", __FUNCTION__);

    ret = system(LC_START_LCMD);
    LC_LOG(LOG_INFO, "killing lcmd %s\n", ret == 0 ? "ok" : "failed");
    ret = system(LC_START_LCPD);
    LC_LOG(LOG_INFO, "killing lcpd %s\n", ret == 0 ? "ok" : "failed");
    ret = system(LC_START_VMDRIVER);
    LC_LOG(LOG_INFO, "killing vmdriver %s\n", ret == 0 ? "ok" : "failed");

}

void graceful_restart_lcmond()
{
    int ret;
    LC_LOG(LOG_INFO, "%s: restarting...", __FUNCTION__);

    ret = system(LC_START_MONITOR);
    LC_LOG(LOG_INFO, "restart lcmond %s\n", ret == 0 ? "ok" : "failed");

}

void lc_heartbeat()
{
    if (LC_HB_CNT_GET(lcm)      >= lc_chk_knob->lc_opt.fail_threshold ||
        LC_HB_CNT_GET(kernel)   >= lc_chk_knob->lc_opt.fail_threshold ||
        LC_HB_CNT_GET(vmdriver) >= lc_chk_knob->lc_opt.fail_threshold) {

        LC_LOG(LOG_ERR, "%s: daemon not responding\n", __FUNCTION__);
        graceful_restart();
        LC_HB_CNT_RST(lcm);
        LC_HB_CNT_RST(kernel);
        LC_HB_CNT_RST(vmdriver);
        LC_HB_CNT_RST(monitor);

        return;
    }

    if (LC_HB_CNT_GET(monitor) >= lc_chk_knob->lc_opt.fail_threshold) {
        LC_LOG(LOG_ERR, "%s: lcmond not responding\n", __FUNCTION__);
        graceful_restart_lcmond();
        LC_HB_CNT_RST(monitor);

        return;
    }
    lc_send_lcm_heartbeat();
    lc_send_kernel_heartbeat();
    lc_send_vmdriver_heartbeat();
    lc_send_monitor_heartbeat();

}

void lc_timer_handler(int signal)
{
     //LC_LOG(LOG_INFO, "LC timer \n");
     lc_heartbeat();
     LC_LOG(LOG_INFO, "%d, %d, %d\n",
             LC_HB_CNT_GET(lcm),
             LC_HB_CNT_GET(kernel),
             LC_HB_CNT_GET(vmdriver));
}

void lc_timer_init(void)
{
    struct itimerval val;

    val.it_value.tv_sec = lc_chk_knob->lc_opt.check_interval;
    val.it_value.tv_usec = 0;
    val.it_interval.tv_sec = lc_chk_knob->lc_opt.check_interval;
    val.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL,&val,NULL);
    signal(SIGALRM, lc_timer_handler);

    return;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        exit(-1);
    }

    if (lc_chk_sknob_init() == -1) {
        exit(-1);
    }

    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    if (lc_chk_knob->lc_opt.lc_flags & LC_FLAG_DAEMON) {
        LC_LOG(LOG_INFO, "Start LC as a daemon...");
        lc_daemon();
    }

    if (lc_chk_thread_create() < 0) {
        LC_LOG(LOG_ERR, "Create thread failure\n");
        exit(-1);
    }

    lc_msgq_init(&lc_data_queue);
    lc_sock_init();
    graceful_restart();
    lc_timer_init();

    lc_main_process();

    return 0;
}
