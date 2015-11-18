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
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include "lc_bus.h"
#include "lc_db.h"
#include "lc_vm.h"
#include "lc_queue.h"
#include "vm/vm_host.h"
#include "lc_xen.h"
#include "lc_agent_msg.h"
#include "lc_agexec_msg.h"

#define LOG_LEVEL_HEADER        "LOG_LEVEL: "

lc_vm_sknob_t *lc_vm_knob;
pthread_mutex_t lc_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lc_msg_cond = PTHREAD_COND_INITIALIZER;
int VMDRIVER_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *VMDRIVER_LOG_LEVEL_P = &VMDRIVER_LOG_DEFAULT_LEVEL;
char local_ctrl_ip[16];

char* LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

int lc_vm_parase_config()
{
    FILE *fp = NULL;
    char buf[MAX_SYS_COMMAND_LEN] = {0};
    char *s = NULL;
    char *key, *value;
    int flag = 0;

    fp = fopen(LC_VM_CONFIG_FILE, "r");
    if (NULL == fp) {
        VMDRIVER_LOG(LOG_ERR, "%s error open config file", __FUNCTION__);
        return 1;
    }

    while (fgets(buf, MAX_SYS_COMMAND_LEN, fp) != NULL) {
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
            if (strcmp(s, LC_VM_GLOBAL_SIGN)) {
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

        if (!strcmp(key, LC_VM_KEY_LOCAL_CTRL_IP)) {
            strncpy(local_ctrl_ip, value, sizeof(local_ctrl_ip));
        }
    }

    fclose(fp);
    return 0;
}

int vmdriver_log_level_init()
{
    int ret = 0;
    int shmid;
    key_t shmkey;
    char cmd[100];

    VMDRIVER_LOG(LOG_INFO, "%s Enter\n", __FUNCTION__);
    sprintf(cmd, "touch "LC_VM_SYS_LEVEL_SHM_ID);
    system(cmd);
    shmkey = ftok(LC_VM_SYS_LEVEL_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        VMDRIVER_LOG(LOG_DEBUG, "SHM already existing, re-create it %d", shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            VMDRIVER_LOG(LOG_ERR, "Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        VMDRIVER_LOG(LOG_DEBUG, "Create shared memory for vmdriver %d", shmid);
    } else {
        VMDRIVER_LOG(LOG_ERR, "Create shared memory for vmdriver error!");
        ret = -1;
        goto error;
    }

    if ((VMDRIVER_LOG_LEVEL_P = (int *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        VMDRIVER_LOG(LOG_ERR, "shmat error:%s\n",strerror(errno));
        ret = -1;
        goto error;
    }

    *VMDRIVER_LOG_LEVEL_P = VMDRIVER_LOG_DEFAULT_LEVEL;
error:
    VMDRIVER_LOG(LOG_DEBUG, "End\n");
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
        " -t, --thread       processing thread number\n";

    printf("%s", help);
    return;
}

int lc_vm_sknob_init()
{
    int shmid;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "touch "LC_VM_SHM_ID);
    system(cmd);
    shmkey = ftok(LC_VM_SHM_ID, 'a');
    shmid = shmget(shmkey, 0, 0);

    if (shmid != -1) {
        VMDRIVER_LOG(LOG_DEBUG,"SHM already existing, re-create it %d", shmid);
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            VMDRIVER_LOG(LOG_DEBUG,"Remove existing SHM error\n");
        }
    }

    shmid = shmget(shmkey, sizeof(lc_vm_sknob_t), 0666|IPC_CREAT|IPC_EXCL);

    if (shmid != -1) {
        VMDRIVER_LOG(LOG_DEBUG,"Create shared memory for vmdriver %d", shmid);
    } else {
        VMDRIVER_LOG(LOG_ERR,"Create shared memory for vmdriver error!");
        return -1;
    }

    if ((lc_vm_knob = (lc_vm_sknob_t *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        VMDRIVER_LOG(LOG_ERR,"shmat error:%s\n", strerror(errno));
        return -1;
    }

    memset(lc_vm_knob, 0, sizeof(lc_vm_sknob_t));

    return 0;

}

int lc_daemon(void )
{
    int fd;

    switch (fork()) {
        case -1:
            /* Fork failed! */
			VMDRIVER_LOG(LOG_ERR,"The deamon process start is fail!\n");
            return -1;

        case 0:
            /* Daemon process */
			VMDRIVER_LOG(LOG_INFO,"The deamon process start is success.\n");
            break;

        default:
            /* Valid PID returned, exit the parent process. */
			VMDRIVER_LOG(LOG_INFO,"The valid PID returned,exit the parent process.\n");
            exit(EXIT_SUCCESS);
            break;
    }

    /* Change the file mode mask */
    umask(0);
	VMDRIVER_LOG(LOG_INFO,"Change the file mode mask is ok.\n");

    /* Create a new SID for the child process */
    if (setsid() == -1) {
		VMDRIVER_LOG(LOG_INFO,"Create a new SID for the child process is fail!\n");
        return -1;
    }

    if ((chdir("/")) < 0) {
        VMDRIVER_LOG(LOG_ERR, "Change working directory error\n");
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
    lc_vm_sknob_t knob = {0};
    int option;
    const char *optstr = "hl:t:vfd?";
    struct option longopts[] = {
        {"thread", required_argument, NULL, 't'},
        {"daemon", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"foreground", no_argument, NULL, 'f'},
        {"log", required_argument, NULL, 'l'},
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
        fprintf(stderr, "LCC_VMDRIVER_VERSION: %s\nXEN_AGENT_VERSION: %#X\n"
                "XEN_AGEXEC_VERSION: %#X\n", VERSION,
                AGENT_VERSION_MAGIC, AGEXEC_VERSION_MAGIC);
	    VMDRIVER_LOG(LOG_DEBUG,"command line display version is ok.\n");

        return -1;
    }

    /* help */
    if (knob.lc_opt.lc_flags & LC_FLAG_HELP) {
        print_help();
	    VMDRIVER_LOG(LOG_DEBUG,"command line display help information is ok.\n");
        return -1;
    }

    if (lc_vm_sknob_init() == -1) {
	    VMDRIVER_LOG(LOG_DEBUG,"vmdriver create share memory is ok .\n");
        return -1;
    }
    memcpy(lc_vm_knob, &knob, sizeof(lc_vm_sknob_t));

    if (lc_vm_knob->lc_opt.lc_flags & LC_FLAG_LOGSET) {
        VMDRIVER_LOG(LOG_DEBUG,"Set log level to %d\n", lc_vm_knob->lc_opt.log_level);
    } else {
        VMDRIVER_LOG(LOG_DEBUG,"Set log level to default level (LOG_WARNING)\n");
        lc_vm_knob->lc_opt.log_level = LOG_WARNING;
        lc_vm_knob->lc_opt.log_on = 1;
    }

    /* set thread number */
    if (lc_vm_knob->lc_opt.lc_flags & LC_FLAG_THREAD) {
        if (lc_vm_knob->lc_opt.thread_num > LC_MAX_THREAD_NUM) {
            VMDRIVER_LOG(LOG_ERR,"Thread number exceed max thread# (%d)\n", LC_MAX_THREAD_NUM);
            return -1;
        }
    } else {
        lc_vm_knob->lc_opt.thread_num = LC_DEFAULT_THREAD_NUM;
        VMDRIVER_LOG(LOG_DEBUG,"Thread number is (%d)\n", LC_MAX_THREAD_NUM);

    }

    return 0;
}

void *lc_vm_thread()
{

    int i;
    lc_msg_t *msg;
    FILE *fp = NULL;
    char buf[10];
    char *p = NULL, *q = NULL;
    char *pbuf = NULL;
    VMDRIVER_LOG(LOG_DEBUG,"%s enter.\n",__FUNCTION__);
    pthread_detach(pthread_self());
    lc_db_thread_init();
    if (lc_bus_thread_init() != LC_BUS_OK) {
        VMDRIVER_LOG(LOG_ERR, "lc_bus_thread_init is failed.\n");
        exit(-1);
    }

    VMDRIVER_LOG(LOG_DEBUG,"lc_db_thread_init() is success.");
    for (i = 0; i < MAX_THREAD; i++) {
        if (thread_url[i].tid == 0) {
            thread_url[i].tid = pthread_self();
            break;
        }
    }
    while (1) {

        msg = NULL;
        pthread_mutex_lock(&lc_msg_mutex);
        /* sleep if no message in queue */
        pthread_cond_wait(&lc_msg_cond, &lc_msg_mutex);

        fp = fopen(LOG_PATH, "r");
        if(fp){
           fread(buf, 10, 1, fp);
           fclose(fp);
           fp = NULL;
           pbuf = buf;
           p = strsep(&pbuf, "#");
           q = strsep(&p, ",");
           lc_vm_knob->lc_opt.log_level = atoi(p);
           lc_vm_knob->lc_opt.log_on = atoi(q);
        }

        if (!lc_is_msgq_empty(&lc_data_queue)) {
            msg = (lc_msg_t *)lc_msgq_dq(&lc_data_queue);
        }
        pthread_mutex_unlock(&lc_msg_mutex);

        /* process recevied message here */
        if (msg) {
            vm_msg_handler((struct lc_msg_head *)msg);
            VMDRIVER_LOG(LOG_INFO, "type=%d magic=%x len=%d\n",
                       msg->mbuf.hdr.type, msg->mbuf.hdr.magic,
                       msg->mbuf.hdr.length);
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
                vm_msg_handler((struct lc_msg_head *)msg);
                VMDRIVER_LOG(LOG_DEBUG, "type=%d magic=%x\n", msg->mbuf.hdr.type,
                        msg->mbuf.hdr.magic);
                free(msg);
            }
        }
    }

    lc_db_thread_term();
    VMDRIVER_LOG(LOG_DEBUG,"lc_db_thread_term() is success.");
    pthread_exit(NULL);
}

int lc_vm_thread_create(int num)
{
    int ret, i, success = 0;
    pthread_t *thread;

    thread = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (!thread) {
        VMDRIVER_LOG(LOG_ERR, "Create thread buffer failed!");
        return -1;
    }

    memset(thread, 0, num * sizeof(thread));

    for (i = 0; i < num; i++) {
        LC_VM_CNT_INC(lc_vm_thread_create);
        if ((ret = pthread_create(thread + i, NULL, lc_vm_thread, NULL)) != 0) {
            VMDRIVER_LOG(LOG_ERR, "Create thread failed!");
            LC_VM_CNT_INC(lc_vm_thread_create_failed);
        } else {
            VMDRIVER_LOG(LOG_INFO, "Create thread successfully");
            LC_VM_CNT_INC(lc_vm_thread_create_success);
            success++;
        }
    }
    free(thread);

    if (success < num) {
        VMDRIVER_LOG(LOG_ERR, "Only %d/%d thread created!", success, num);
        return -1;
    }

    return 0;
}


void lc_timer_handler(int signal)
{
     //VMDRIVER_LOG(LOG_INFO, "LC timer \n");
}

void lc_timer_init(void)
{
    struct itimerval val;
    VMDRIVER_LOG(LOG_DEBUG,"lc_timer_init enter.\n");
    val.it_value.tv_sec=0;
    val.it_value.tv_usec=0;
    val.it_interval.tv_sec=0;
    val.it_interval.tv_usec=0;

    setitimer(ITIMER_REAL,&val,NULL);
    signal(SIGALRM, lc_timer_handler);
    VMDRIVER_LOG(LOG_DEBUG,"lc_timer_init success.\n");
    return;
}

int main(int argc, char **argv)
{
    int i;
    (void)vmdriver_log_level_init();
    VMDRIVER_LOG(LOG_DEBUG,"argc=%d\n",argc);
    if (argc < 2)
    {
        print_help();
        exit(-1);
    }

    if (parse_cmd_line(argc, argv) < 0)
    {
    	VMDRIVER_LOG(LOG_ERR,"parse_cmd_line() is fail!\n");
        exit(-1);
    }

    if (lc_vm_parase_config()) {
        exit(-1);
    }

    if (lc_vm_knob->lc_opt.lc_flags & LC_FLAG_DAEMON)
    {
        VMDRIVER_LOG(LOG_INFO, "Start LC as a daemon...\n");
        lc_daemon();
    }
    lc_msgq_init(&lc_data_queue);
    VMDRIVER_LOG(LOG_INFO, "Message queue init is ok.\n");
    DB_LOG_LEVEL_P = VMDRIVER_LOG_LEVEL_P;/*set db log level to vm log level*/
    if (lc_db_init("localhost", "root", "security421","livecloud", "0", LC_DB_VERSION) != 0)
    {
        VMDRIVER_LOG(LOG_ERR, "The db of vmdriver init is fail!\n");
    }

    for (i = 0; i < MAX_THREAD; i++)
    {
        thread_url[i].tid = 0;
        memset(thread_url[i].url, 0, LC_MAX_CURL_LEN);
    }

    lc_db_thread_init();

    if (lc_vm_thread_create(lc_vm_knob->lc_opt.thread_num) < 0)
    {
        VMDRIVER_LOG(LOG_ERR, "Create thread failure, thread num=%d\n",lc_vm_knob->lc_opt.thread_num);
        exit(-1);
     }

    xen_common_init();

    if (lc_sock_init() != 0)
   {
        VMDRIVER_LOG(LOG_ERR, "Socket init failed.\n");
        exit(-1);
    }
    LB_LOG_LEVEL_P = VMDRIVER_LOG_LEVEL_P;/* set lb sys log level to vmdriver log level*/
    if (lc_bus_init_by_vmdriver() != 0)
    {
        exit(-1);
    }

    lc_xen_test_init();
/*    lc_timer_init();
*/
    lc_main_process();

    return 0;

}

int lc_publish_msg(lc_mbuf_t *msg)

{
    int len;
    int ret = 0;
    lc_mbuf_hdr_t *hdr = (lc_mbuf_hdr_t *)msg;

    VMDRIVER_LOG(LOG_INFO, "%s msg send out: dir=%08x type=%d len=%d seq=%d \n",__FUNCTION__, hdr->direction, hdr->type, hdr->length, hdr->seq);
    if (!msg)
    {
        return -1;
    }

    len = sizeof(lc_mbuf_hdr_t) + msg->hdr.length;
    switch (msg->hdr.magic)
    {
        case MSG_MG_DRV2KER:
            ret = lc_bus_vmdriver_publish_unicast((char *)msg, len,LC_BUS_QUEUE_LCPD);
            if (ret)
           {
                VMDRIVER_LOG(LOG_ERR, "error magic=0x%8x\n", msg->hdr.magic);
           }
            break;

        default:
            VMDRIVER_LOG(LOG_ERR, "%s maggic error.\n", __FUNCTION__);
            return -1;
        }
    return ret;

}
