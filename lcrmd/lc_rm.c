#include <sys/ipc.h>
#include <sys/shm.h>
#include "lc_bus.h"
#include "lc_db.h"
#include "lc_rm.h"
#include "lc_queue.h"

lc_rm_sknob_t *lc_rm_knob = NULL;
pthread_mutex_t lc_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_msg_cond = PTHREAD_COND_INITIALIZER;
extern lc_pb_msg_t *lc_pb_msgq_dq(lc_pb_msg_queue_t *msgq);
int RM_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *RM_LOG_LEVEL_P = &RM_LOG_DEFAULT_LEVEL;

char* RM_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

void print_help(void)
{
    static const char *help =
        "Usage: lcpd [OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -v, --version      display the version and exit\n"
        " -d, --daemon       running as a daemon (default)\n"
        " -f, --foreground   running foreground\n"
        " -l, --log          log level\n"
        " -t, --thread       processing thread number\n";

    printf("%s", help);
    return;
}

/* Return -1 on error, 0 on success */
int parse_cmd_line(int argc, char *const argv[])
{
    lc_rm_sknob_t knob = {0};
    int option;
    const char *optstr = "t:l:dhvf?";
    struct option longopts[] = {
        {"thread", required_argument, NULL, 't'},
        {"loglevel", required_argument, NULL, 'l'},
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"foreground", no_argument, NULL, 'f'},
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
            case 'v':
                knob.lc_opt.lc_flags |= LC_FLAG_VERSION;
                break;
            case 'f':
                knob.lc_opt.lc_flags |= LC_FLAG_FOREGROUND;
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
        return -1;
    }

    /* help */
    if (knob.lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
        return -1;
    }

    if (lc_rm_sknob_init() == -1) {
        return -1;
    }
    memcpy(lc_rm_knob, &knob, sizeof(lc_rm_sknob_t));

    /* log level */
    if (lc_rm_knob->lc_opt.lc_flags & LC_FLAG_LOGLEVEL) {
        RM_LOG(LOG_DEBUG,"Set log level to %d\n", lc_rm_knob->lc_opt.log_level);
    } else {
        RM_LOG(LOG_DEBUG,"Set log level to default level (LOG_WARNING)\n");
        lc_rm_knob->lc_opt.log_level = LOG_WARNING;
        lc_rm_knob->lc_opt.log_on = 1;
    }

    /* set thread number */
    if (lc_rm_knob->lc_opt.lc_flags & LC_FLAG_THREAD) {
        if (lc_rm_knob->lc_opt.thread_num > LC_MAX_THREAD_NUM) {
            RM_LOG(LOG_DEBUG,"Thread# exceed max thread# (%d)\n", LC_MAX_THREAD_NUM);
            return -1;
        }
    } else {
        lc_rm_knob->lc_opt.thread_num = LC_DEFAULT_THREAD_NUM;
    }
    return 0;
}

int rm_log_level_init()
{
    int ret = 0;
    int shmid;
    key_t shmkey;

    RM_LOG(LOG_INFO, "Enter\n");

    shmkey = ftok(LC_RM_SYS_LEVEL_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        RM_LOG(LOG_DEBUG, "SHM already existing, re-create it %d", shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            RM_LOG(LOG_ERR, "Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        RM_LOG(LOG_DEBUG, "Create shared memory for lcmd %d", shmid);
    } else {
        RM_LOG(LOG_ERR, "Create shared memory for lcmd error!");
        ret = -1;
        goto error;
    }

    if ((RM_LOG_LEVEL_P = (int *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        RM_LOG(LOG_ERR, "shmat error:%s\n",strerror(errno));
        ret = -1;
        goto error;
    }

    *RM_LOG_LEVEL_P = RM_LOG_DEFAULT_LEVEL;
error:
    RM_LOG(LOG_DEBUG, "End\n");
    return ret;
}

int lc_rm_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "touch "LC_RM_SHM_ID);
    system(cmd);

    shmkey = ftok(LC_RM_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        RM_LOG(LOG_DEBUG,"SHM already existing, re-create it %d",shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            RM_LOG(LOG_DEBUG,"Remove existing SHM error\n");
        }

    }

    shmid = shmget(shmkey, sizeof(lc_rm_sknob_t), 0666|IPC_CREAT|IPC_EXCL);
    if (shmid != -1) {
        RM_LOG(LOG_INFO,"Create a shared memory segment %d",shmid);
    } else {
        RM_LOG(LOG_ERR, "Create a shared memory error! shmid=%d", shmid);
        return -1;
    }

    if ((lc_rm_knob = (lc_rm_sknob_t*)shmat(shmid, (const void*)0, 0)) == (void*)-1) {
        RM_LOG(LOG_ERR,"shmat error:%s\n",strerror(errno));
        return -1;
    }

    memset(lc_rm_knob, 0, sizeof(lc_rm_sknob_t));
    return 0;
}

int lc_daemon(void )
{
    int fd;

    switch (fork()) {
        case -1:
            /* Fork failed! */
            RM_LOG(LOG_ERR,"The deamon process start is fail!\n");
            return -1;

        case 0:
            /* Daemon process */
            RM_LOG(LOG_INFO,"The deamon process start is success.\n");
            break;

        default:
            /* Valid PID returned, exit the parent process. */
            RM_LOG(LOG_INFO,"The valid PID returned,exit the parent process.\n");
            exit(EXIT_SUCCESS);
            break;
    }

    /* Change the file mode mask */
    umask(0);
    RM_LOG(LOG_INFO,"Change the file mode mask is ok.\n");

    /* Create a new SID for the child process */
    if (setsid() == -1) {
        RM_LOG(LOG_INFO,"Create a new SID for the child process is fail!\n");
        return -1;
    }

    if ((chdir("/")) < 0) {
        RM_LOG(LOG_ERR, "Change working directory error\n");
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

    RM_LOG(LOG_INFO, "in dir=%08x type=%d len=%d seq=%d magic=0x%08x\n",
           msg->hdr.direction, msg->hdr.type,
           msg->hdr.length, msg->hdr.seq, msg->hdr.magic);
    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.magic) {
        case MSG_MG_UI2DRV:
            ret = lc_bus_rm_publish_unicast((char *)msg, len,
                                            LC_BUS_QUEUE_VMDRIVER);
            if (ret) {
                RM_LOG(LOG_ERR, "MSG_MG_UI2DRV err magic=0x%8x\n",msg->hdr.magic);
            }
            break;
        case MSG_MG_UI2MON:
            ret = lc_bus_rm_publish_unicast((char *)msg, len,
                                            LC_BUS_QUEUE_LCMOND);
            if (ret) {
                RM_LOG(LOG_ERR, "MSG_MG_UI2MON err magic=0x%8x\n",msg->hdr.magic);
            }
            break;
        case MSG_MG_UI2KER:
			ret = lc_bus_rm_publish_unicast((char *)msg, len,
					                        LC_BUS_QUEUE_LCPD);
            if (ret) {
                RM_LOG(LOG_ERR, "MSG_MG_UI2KER err magic=0x%8x\n",msg->hdr.magic);
            }
			break;
        default:
            RM_LOG(LOG_ERR, "default maggic error.\n");
            return -1;
    }
    return ret;
}

void *lc_rm_thread()
{
    lc_pb_msg_t *msg;
    FILE *fp = NULL;
    char buf[10];
    char *p = NULL, *q = NULL;
    char *pbuf = NULL;

    pthread_detach(pthread_self());
    lc_db_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        RM_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
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
           lc_rm_knob->lc_opt.log_level = atoi(p);
           lc_rm_knob->lc_opt.log_on = atoi(q);
        }

        if (!lc_is_pb_msgq_empty(&lc_data_queue)) {
            LC_RM_CNT_INC(lc_thread_deq_msg1);
            msg = lc_pb_msgq_dq(&lc_data_queue);
        }
        pthread_mutex_unlock(&lc_msg_mutex);

        /* message processing here */
        if (msg) {
            LC_RM_CNT_INC(lc_thread_free_msg);
            RM_LOG(LOG_INFO, "sender=%d type=%d len=%d",
                   msg->mbuf.hdr.sender, msg->mbuf.hdr.type, msg->mbuf.hdr.length);
            rm_msg_handler((header_t *)msg);
            free(msg);
        }
        /* is there any messages still available in message queue? */
        while (1) {
            msg = NULL;
            pthread_mutex_lock(&lc_msg_mutex);
            if (!lc_is_pb_msgq_empty(&lc_data_queue)) {
                LC_RM_CNT_INC(lc_thread_deq_msg2);
                msg = (lc_pb_msg_t *)lc_pb_msgq_dq(&lc_data_queue);
            } else {
                pthread_mutex_unlock(&lc_msg_mutex);
                break;
            }
            pthread_mutex_unlock(&lc_msg_mutex);
            /* process recevied message here */
            if (msg) {
                LC_RM_CNT_INC(lc_thread_free_msg);
                RM_LOG(LOG_INFO, "sender=%d type=%d len=%d",
                       msg->mbuf.hdr.sender, msg->mbuf.hdr.type, msg->mbuf.hdr.length);
                rm_msg_handler((header_t *)msg);
                free(msg);
            }
        }
    }

    lc_db_thread_term();
    pthread_exit(NULL);
}

int lc_thread_create(int num)
{
    int ret, i, success = 0;
    pthread_t *thread;

    thread = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (!thread) {
        RM_LOG(LOG_ERR, "Create thread buffer failed!");
        return -1;
    }

    memset(thread, 0, num * sizeof(thread));

    for (i = 0; i < num; i++) {
        if ((ret = pthread_create(thread + i, NULL, lc_rm_thread, NULL)) != 0) {
            RM_LOG(LOG_ERR, "Create thread failed");
        } else {
            RM_LOG(LOG_INFO, "Create thread successfully");
            success++;
        }
    }
    free(thread);

    if (success < num) {
        RM_LOG(LOG_ERR, "Only %d/%d thread created!", success, num);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        exit(-1);
    }

    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    if (lc_rm_knob->lc_opt.lc_flags & LC_FLAG_DAEMON) {
        RM_LOG(LOG_INFO, "Start LC as a daemon...");
        lc_daemon();
    }

    rm_log_level_init();
    lc_pb_msgq_init(&lc_data_queue);

    if (lc_db_init("localhost", "root", "security421",
                "livecloud", "0", LC_DB_VERSION) != 0 ||
        lc_db_thread_init() != 0) {
        RM_LOG(LOG_ERR,"lc_db_thread_init() is error\n");
        exit(-1);
    }

    if (lc_thread_create(lc_rm_knob->lc_opt.thread_num) < 0) {
        RM_LOG(LOG_ERR, "Create thread failure, thread num=%d\n",
               lc_rm_knob->lc_opt.thread_num);
        exit(-1);
    }

    if (lc_bus_init_by_rm() != 0) {
        RM_LOG(LOG_ERR, "lc_bus_init_by_rm() is failed.\n");
        exit(-1);
    }

    lc_main_process();

    return 0;
}
