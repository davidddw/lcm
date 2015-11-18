#include <sys/ipc.h>
#include <sys/shm.h>
#include <curl/curl.h>
#include "lc_bus.h"
#include "lc_db.h"
#include "lc_db_net.h"
#include "lc_kernel.h"
#include "lc_queue.h"
#include "lc_agent_msg.h"
#include "lc_agexec_msg.h"
#include "lc_livegate_api.h"
#include "lc_kernel_mon.h"

lc_ker_sknob_t *lc_ker_knob = NULL;
pthread_mutex_t lc_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_msg_cond = PTHREAD_COND_INITIALIZER;
extern lc_msg_t *lc_msgq_dq(lc_msg_queue_t *msgq);
int LCPD_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *LCPD_LOG_LEVEL_P = &LCPD_LOG_DEFAULT_LEVEL;
char master_ctrl_ip[16];
char local_ctrl_ip[16];

char* LCPD_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

int lc_ker_parase_config()
{
    FILE *fp = NULL;
    char buf[MAX_LOG_MSG_LEN] = {0};
    char *s = NULL;
    char *key, *value;
    int flag = 0;

    fp = fopen(LC_KER_CONFIG_FILE, "r");
    if (NULL == fp) {
        LCPD_LOG(LOG_ERR, "%s error open config file", __FUNCTION__);
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
            if (strcmp(s, LC_KER_GLOBAL_SIGN)) {
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

        if (!strcmp(key, LC_KER_KEY_MASTER_CTRL_IP)) {
            strncpy(master_ctrl_ip, value, sizeof(master_ctrl_ip));
        }
        if (!strcmp(key, LC_KER_KEY_LOCAL_CTRL_IP)) {
            strncpy(local_ctrl_ip, value, sizeof(local_ctrl_ip));
        }
    }

    if (!strcmp(master_ctrl_ip, "127.0.0.1")) {
        strncpy(master_ctrl_ip, local_ctrl_ip, sizeof(master_ctrl_ip));
    }

    fclose(fp);
    return 0;
}

int lcpd_log_level_init()
{
    int ret = 0;
    int shmid;
    key_t shmkey;

    LCPD_LOG(LOG_INFO, "Enter\n");

    shmkey = ftok(LC_LCPD_SYS_LEVEL_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        LCPD_LOG(LOG_DEBUG, "SHM already existing, re-create it %d", shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            LCPD_LOG(LOG_ERR, "Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        LCPD_LOG(LOG_DEBUG, "Create shared memory for vmdriver %d", shmid);
    } else {
        LCPD_LOG(LOG_ERR, "Create shared memory for vmdriver error!");
        ret = -1;
        goto error;
    }

    if ((LCPD_LOG_LEVEL_P = (int *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        LCPD_LOG(LOG_ERR, "shmat error:%s\n",strerror(errno));
        ret = -1;
        goto error;
    }

    *LCPD_LOG_LEVEL_P = LCPD_LOG_DEFAULT_LEVEL;
error:
    LCPD_LOG(LOG_DEBUG, "End\n");
    return ret;
}

void print_help(void)
{
    static const char *help =
        "Usage: lcpd [OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -v, --version      display the version and exit\n"
        " -d, --daemon       running as a daemon (default)\n"
        " -f, --foreground   running foreground\n"
        " -c, --config       read config from file\n"
        " -d, --debug        debug on\n"
        " -t, --thread       processing thread number\n";

    printf("%s", help);
    return;
}

int load_running_mode_from_db()
{
    int running_mode = lc_db_get_sys_running_mode();

    if (running_mode == LCC_SYS_RUNNING_MODE_INDEPENDENT) {
        lc_ker_knob->lc_opt.lc_flags |= LC_FLAG_CONTROL_MODE_LCC;
    } else if (running_mode == LCC_SYS_RUNNING_MODE_HIERARCHICAL) {
        lc_ker_knob->lc_opt.lc_flags |= LC_FLAG_CONTROL_MODE_LCM;
    } else {
        fprintf(stderr, "Running mode error: %d\n", running_mode);
        return -1;
    }

    return 0;
}

int lc_ker_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "touch "LC_KER_SHM_ID);
    system(cmd);

    shmkey = ftok(LC_KER_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        LCPD_LOG(LOG_DEBUG,"SHM already existing, re-create it %d",shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            LCPD_LOG(LOG_DEBUG,"Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(lc_ker_sknob_t), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        LCPD_LOG(LOG_INFO,"Create a shared memory segment %d",shmid);
    } else {
        LCPD_LOG(LOG_ERR, "Create a shared memory error! shmid=%d", shmid);
        return -1;
    }

    if ((lc_ker_knob = (lc_ker_sknob_t*)shmat(shmid, (const void*)0, 0)) == (void*)-1) {
        LCPD_LOG(LOG_ERR,"shmat error:%s\n",strerror(errno));
        return -1;
    }

    memset(lc_ker_knob, 0, sizeof(lc_ker_sknob_t));


    return 0;

}

int lc_daemon(void )
{
    int fd;

    switch (fork()) {
        case -1:
            /* Fork failed! */
			LCPD_LOG(LOG_ERR,"The deamon process start is fail!\n");
            return -1;

        case 0:
            /* Daemon process */
			LCPD_LOG(LOG_INFO,"The deamon process start is success.\n");
            break;

        default:
            /* Valid PID returned, exit the parent process. */
			LCPD_LOG(LOG_INFO,"The valid PID returned,exit the parent process.\n");
            exit(EXIT_SUCCESS);
            break;
    }

    /* Change the file mode mask */
    umask(0);
	LCPD_LOG(LOG_INFO,"Change the file mode mask is ok.\n");

    /* Create a new SID for the child process */
    if (setsid() == -1) {
		LCPD_LOG(LOG_INFO,"Create a new SID for the child process is fail!\n");
        return -1;
    }

    if ((chdir("/")) < 0) {
        LCPD_LOG(LOG_ERR, "Change working directory error\n");
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

int lc_publish_msg(lc_mbuf_t *msg)
{
    int len;
    int ret = 0;

    if (!msg) {
        return -1;
    }

    LCPD_LOG(LOG_INFO, "in dir=%08x type=%d len=%d seq=%d magic=0x%08x\n",
               msg->hdr.direction, msg->hdr.type,
               msg->hdr.length, msg->hdr.seq, msg->hdr.magic);
    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.magic) {
        case MSG_MG_KER2DRV:
            ret = lc_bus_lcpd_publish_unicast((char *)msg, len,
                    LC_BUS_QUEUE_VMDRIVER);
            if (ret) {
                LCPD_LOG(LOG_ERR, "MSG_MG_KER2DRV err magic=0x%8x\n",msg->hdr.magic);
            }
            break;

        default:
            LCPD_LOG(LOG_ERR, "default maggic error.\n");
            return -1;

    }
    return ret;
}

/* Return -1 on error, 0 on success */
int parse_cmd_line(int argc, char *const argv[])
{
    lc_ker_sknob_t knob = {0};
    int option;
    const char *optstr = "t:c:l:dhDvfg?";
    struct option longopts[] = {
        {"thread", required_argument, NULL, 't'},
        {"config", required_argument, NULL, 'c'},
        {"loglevel", required_argument, NULL, 'l'},
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"debug", no_argument, NULL, 'D'},
        {"version", no_argument, NULL, 'v'},
        {"foreground", no_argument, NULL, 'f'},
        {"gstart", no_argument, NULL, 'g'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (option) {
            case 't':
                knob.lc_opt.lc_flags |= LC_FLAG_THREAD;
                knob.lc_opt.thread_num = atol(optarg);
                break;
            case 'd':
                knob.lc_opt.lc_flags |= LC_FLAG_DAEMON;
                break;
            case 'c':
                if (strlen(optarg) > MAX_FILE_NAME_LEN) {
                    LCPD_LOG(LOG_ERR,"Invalid config file name.\n");
                    exit(1);
                }
                strcpy(knob.lc_opt.config_file, optarg);
                break;
            case 'v':
                knob.lc_opt.lc_flags |= LC_FLAG_VERSION;
                break;
            case 'f':
                knob.lc_opt.lc_flags |= LC_FLAG_FOREGROUND;
                break;
            case 'g':
                knob.lc_opt.lc_flags |= LC_FLAG_GRACEFUL_RESTART;
                break;
            case 'D':
                knob.lc_opt.lc_flags |= LC_FLAG_DEBUG;
                break;
            case 'l':
                knob.lc_opt.lc_flags |= LC_FLAG_LOGLEVEL;
                knob.lc_opt.log_level = atol(optarg);
                knob.lc_opt.log_on = 1;
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
        fprintf(stderr, "LCC_LCPD_VERSION: %s\nXEN_AGENT_VERSION: %#X\n"
                "XEN_AGEXEC_VERSION: %#X\nLCG_API_VERSION: %s\n", VERSION,
                AGENT_VERSION_MAGIC, AGEXEC_VERSION_MAGIC, LG_API_VERSION);
        return -1;
    }

    /* help */
    if (knob.lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
        return -1;
    }

    if (lc_ker_sknob_init() == -1) {
        return -1;
    }
    memcpy(lc_ker_knob, &knob, sizeof(lc_ker_sknob_t));

    /* log level */
    if (lc_ker_knob->lc_opt.lc_flags & LC_FLAG_LOGLEVEL) {
        LCPD_LOG(LOG_DEBUG,"Set log level to %d\n", lc_ker_knob->lc_opt.log_level);
    } else {
        LCPD_LOG(LOG_DEBUG,"Set log level to default level (LOG_WARNING)\n");
        lc_ker_knob->lc_opt.log_level = LOG_WARNING;
        lc_ker_knob->lc_opt.log_on = 1;
    }

    /* set thread number */
    if (lc_ker_knob->lc_opt.lc_flags & LC_FLAG_THREAD) {
        if (lc_ker_knob->lc_opt.thread_num > LC_MAX_THREAD_NUM) {
            LCPD_LOG(LOG_DEBUG,"Thread# exceed max thread# (%d)\n", LC_MAX_THREAD_NUM);
            return -1;
        }
    } else {
        lc_ker_knob->lc_opt.thread_num = LC_DEFAULT_THREAD_NUM;
    }
    return 0;
}

void *lc_kernel_thread()
{

    lc_msg_t *msg;
    FILE *fp = NULL;
    char buf[10];
    char *p = NULL, *q = NULL;
    char *pbuf = NULL;

    pthread_detach(pthread_self());
    lc_db_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        LCPD_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    while (1) {

        msg = NULL;
        pthread_mutex_lock(&lc_msg_mutex);
        pthread_cond_wait(&lc_msg_cond, &lc_msg_mutex);

        fp = fopen(LOG_PATH, "r");
        if(fp){
           fread(buf, 10, 1, fp);
           fclose(fp);
           fp = NULL;
           pbuf = buf;
           p = strsep(&pbuf, "#");
           q = strsep(&p, ",");
           lc_ker_knob->lc_opt.log_level = atoi(p);
           lc_ker_knob->lc_opt.log_on = atoi(q);
        }

        if (!lc_is_msgq_empty(&lc_data_queue)) {
            LC_KER_CNT_INC(lc_thread_deq_msg1);
            msg = lc_msgq_dq(&lc_data_queue);
        }
        pthread_mutex_unlock(&lc_msg_mutex);

        /* message processing here */
        if (msg) {
            LC_KER_CNT_INC(lc_thread_free_msg);
            LCPD_LOG(LOG_INFO, "type=%d magic=%x,len=%d ",
                   msg->mbuf.hdr.type, msg->mbuf.hdr.magic, msg->mbuf.hdr.length);
            if (is_msg_to_ker(&msg->mbuf.hdr)) {
                kernel_msg_handler((struct lc_msg_head *)msg);
            }

            free(msg);
        }
        /* is there any messages still available in message queue? */
        while (1) {
            msg = NULL;
            pthread_mutex_lock(&lc_msg_mutex);
            if (!lc_is_msgq_empty(&lc_data_queue)) {
                LC_KER_CNT_INC(lc_thread_deq_msg2);
                msg = (lc_msg_t *)lc_msgq_dq(&lc_data_queue);
            } else {
                pthread_mutex_unlock(&lc_msg_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_msg_mutex);
            /* process recevied message here */

            if (msg) {
                LC_KER_CNT_INC(lc_thread_free_msg);
                LCPD_LOG(LOG_DEBUG, "type=%d magic=%x ",
                       msg->mbuf.hdr.type, msg->mbuf.hdr.magic);
                if (is_msg_to_ker(&msg->mbuf.hdr)) {
                    kernel_msg_handler((struct lc_msg_head *)msg);
                }
                free(msg);
            }
        }
    }

    lc_db_thread_term();
    pthread_exit(NULL);
}

void *lc_kernel_mon_thread()
{
    pthread_detach(pthread_self());
    lc_db_thread_init();

    run_network_device_monitor();

    lc_db_thread_term();
    pthread_exit(NULL);
}

int lc_thread_create(int num)
{
    int ret, i, success = 0;
    pthread_t *thread;
    pthread_t mon_thread;

    /* worker thread */

    thread = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (!thread) {
        LCPD_LOG(LOG_ERR, "Create thread buffer failed!");
        return -1;
    }

    memset(thread, 0, num * sizeof(thread));

    for (i = 0; i < num; i++) {
        if ((ret = pthread_create(thread + i, NULL, lc_kernel_thread, NULL)) != 0) {
            LCPD_LOG(LOG_ERR, "Create thread failed");
        } else {
            LCPD_LOG(LOG_INFO, "Create thread successfully");
            success++;
        }
    }
    free(thread);

    if (success < num) {
        LCPD_LOG(LOG_ERR, "Only %d/%d thread created!", success, num);
        return -1;
    }

    /* switch monitor thread */

    if ((ret = pthread_create(&mon_thread, NULL, lc_kernel_mon_thread, NULL)) != 0) {
        LCPD_LOG(LOG_ERR, "Create monitor thread failed");
        return -1;
    } else {
        LCPD_LOG(LOG_INFO, "Create monitor thread successfully");
    }

    return 0;
}

int main(int argc, char **argv)
{
    //int g_start = 0;
    (void)lcpd_log_level_init();

    if (argc < 2) {
        print_help();
        exit(-1);
    }

    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    if (lc_ker_parase_config()) {
        exit(-1);
    }

    if (lc_ker_knob->lc_opt.lc_flags & LC_FLAG_DAEMON) {
        LCPD_LOG(LOG_INFO, "Start LC as a daemon...");
        lc_daemon();
    }

    if (lc_ker_knob->lc_opt.lc_flags & LC_FLAG_GRACEFUL_RESTART) {
        //g_start = 1;
    }

    lc_msgq_init(&lc_data_queue);

    if (lc_db_init("localhost", "root", "security421",
                "livecloud", "0", LC_DB_VERSION) != 0 ||
        lc_db_thread_init() != 0) {
        LCPD_LOG(LOG_ERR,"lc_db_thread_init() is error\n");
        exit(-1);
    }

    if (load_running_mode_from_db()) {
        exit(-1);
    }

    curl_global_init(CURL_GLOBAL_ALL);

    if (lc_thread_create(lc_ker_knob->lc_opt.thread_num) < 0) {
        LCPD_LOG(LOG_ERR, "Create thread failure, thread num=%d\n",
               lc_ker_knob->lc_opt.thread_num);
        exit(-1);
    }

    if (lc_sock_init() != 0) {
        LCPD_LOG(LOG_ERR, "Socket init failed.\n");
        exit(-1);
    }

    if (lc_bus_init_by_kernel() != 0) {
        LCPD_LOG(LOG_ERR, "lc_bus_init_by_kernel() is failed.\n");
        exit(-1);
    }

    lc_main_process();

    return 0;
}
