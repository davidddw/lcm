#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/lc_driver.h"

enum {
    TEST_TYPE1,
    TEST_TYPE2,
    TEST_TYPE3,
    TEST_TYPE4,
    TEST_TYPE5,
    TEST_TYPE6,
    TEST_MAX
};

void print_help(void)
{
    static const char *help =
        "Usage: driver[OPTIONS]\n"
        "Options are:\n"
        " -h, --help         print this help info\n"
        " -c, --config       read config from file\n";

    printf("%s", help);
    return;
}

/* Return -1 on error, 0 on success */
int parse_cmd_line(int argc, char *const argv[])
{
    int option;
    const char *optstr = "hc:?";
    struct option longopts[] = {
        {"config", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (option) {
            case 'c':
                if (strlen(optarg) > MAX_FILE_NAME_LEN) {
                    printf("invalid config file name\n");
                    exit(1);
                }
                strcpy(lc_opt.config_file, optarg);
                break;
            case '?':
                print_help();
                break;
            default:
                return 0;
        }
    }

    return 0;
}


int create_socket(struct sockaddr_un *sa_addr)
{
    int fd_server;
    int val;

    fd_server = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd_server < 0) {
        syslog(LOG_ERR, "call socket() function error!");
        return -1;
    }

    val = bind(fd_server, (struct sockaddr *)sa_addr, sizeof(struct sockaddr_un));
    if (val < 0) {
        syslog(LOG_ERR, "call socket() function error!");
        return -2;
    }

    return fd_server;
}

#define SOCK_DRV_RCV1   "/var/run/.drv_test"
int lc_sock_init()
{
    int fd;

    memset(&sa_drv_rcv, 0, sizeof(sa_drv_rcv));
    strcpy(sa_drv_rcv.sun_path, SOCK_DRV_RCV1);
    sa_drv_rcv.sun_family = AF_UNIX;
    unlink(SOCK_DRV_RCV1);

    fd = create_socket(&sa_drv_rcv);
    if (fd < 0) {
        syslog(LOG_ERR, "create socket error for SOCK_DRV_RCV");
        unlink(SOCK_DRV_RCV1);
        lc_sock.sock_rcv = -1;
    } else {
        lc_sock.sock_rcv = fd;
    }

    memset(&sa_drv_snd_ker, 0, sizeof(sa_drv_snd_ker));
    strcpy(sa_drv_snd_ker.sun_path, SOCK_KER_RCV_DRV);
    //strcpy(sa_drv_snd_ker.sun_path, "/var/run/.sock_drv_ker");
    sa_drv_snd_ker.sun_family = AF_UNIX;

    return 0;
}


void lc_main_process(void)
{
    char buffer[sizeof(lc_mbuf_t)];
    struct sockaddr_storage sa;
    socklen_t sa_len;
    int nrecv;


    while (1) {
        printf("loop in %s\n", __FUNCTION__);
        sa_len = sizeof(sa);
        memset(buffer, 0, sizeof(lc_mbuf_t));
        nrecv = recvfrom(lc_sock.sock_rcv, buffer, sizeof(lc_mbuf_t), 0,
            (struct sockaddr *)&sa, &sa_len);

        if (nrecv < 0) {
            return;
        }
        if (nrecv >= sizeof(lc_mbuf_t)) {
            printf("xxxx %d bytes message received\n", nrecv);

        }
    }

    return;

}

void lc_drv_build_test_msg_type1(lc_mbuf_t *msg)
{
    struct msg_ntf_sw_recv_pkt *pkt;

    msg->hdr.magic = MSG_MG_DRV2KER;
    msg->hdr.direction = MSG_DIR_DRV2KER;
    //msg->hdr.type = LC_MSG_NOTIFY_SWITCH_RECV_PKT;
    msg->hdr.length = sizeof(struct msg_ntf_sw_recv_pkt);

    pkt = (struct msg_ntf_sw_recv_pkt *)msg->data;

    pkt->datapath_id = 0x00006aceffcb4b70ULL;
    pkt->in_port = 10;
    pkt->in_sw_buf_id = 0x12345678;
    pkt->pkt_len = 60;
    pkt->pkt_snap.dl_src.octet[0] = 0x00;
    pkt->pkt_snap.dl_src.octet[1] = 0x1D;
    pkt->pkt_snap.dl_src.octet[2] = 0xBB;
    pkt->pkt_snap.dl_src.octet[3] = 0xAA;
    pkt->pkt_snap.dl_src.octet[4] = 0xFF;
    pkt->pkt_snap.dl_src.octet[5] = 0xCE;

    pkt->pkt_snap.dl_dst.octet[0] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[1] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[2] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[3] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[4] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[5] = 0xFF;

    pkt->pkt_snap.flag.s.is_snap_valid = 1;
    pkt->pkt_snap.dl_type = 0x0806;
    pkt->pkt_snap.nw_proto = 0x1;
    pkt->pkt_snap.nw_src.octet[0] = 172;
    pkt->pkt_snap.nw_src.octet[1] = 16;
    pkt->pkt_snap.nw_src.octet[2] = 0;
    pkt->pkt_snap.nw_src.octet[3] = 7;

    pkt->pkt_snap.nw_dst.octet[0] = 172;
    pkt->pkt_snap.nw_dst.octet[1] = 16;
    pkt->pkt_snap.nw_dst.octet[2] = 0;
    pkt->pkt_snap.nw_dst.octet[3] = 6;
}

void lc_drv_build_test_msg_type2(lc_mbuf_t *msg)
{
    struct msg_ntf_sw_recv_pkt *pkt;

    msg->hdr.magic = MSG_MG_DRV2KER;
    msg->hdr.direction = MSG_DIR_DRV2KER;
    //msg->hdr.type = LC_MSG_NOTIFY_SWITCH_RECV_PKT;
    msg->hdr.length = sizeof(struct msg_ntf_sw_recv_pkt);

    pkt = (struct msg_ntf_sw_recv_pkt *)msg->data;

    pkt->datapath_id = 0x00006aceffcb4b70ULL;
    pkt->in_port = 10;
    pkt->in_sw_buf_id = 0x12345678;
    pkt->pkt_len = 60;
    pkt->pkt_snap.dl_src.octet[0] = 0x00;
    pkt->pkt_snap.dl_src.octet[1] = 0x1D;
    pkt->pkt_snap.dl_src.octet[2] = 0xBB;
    pkt->pkt_snap.dl_src.octet[3] = 0xAA;
    pkt->pkt_snap.dl_src.octet[4] = 0xFF;
    pkt->pkt_snap.dl_src.octet[5] = 0xCE;

    pkt->pkt_snap.dl_dst.octet[0] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[1] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[2] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[3] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[4] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[5] = 0xFF;

    pkt->pkt_snap.flag.s.is_snap_valid = 1;
    pkt->pkt_snap.dl_type = 0x0806;
    pkt->pkt_snap.nw_proto = 0x1;
    pkt->pkt_snap.nw_src.octet[0] = 172;
    pkt->pkt_snap.nw_src.octet[1] = 16;
    pkt->pkt_snap.nw_src.octet[2] = 0;
    pkt->pkt_snap.nw_src.octet[3] = 7;

    pkt->pkt_snap.nw_dst.octet[0] = 172;
    pkt->pkt_snap.nw_dst.octet[1] = 16;
    pkt->pkt_snap.nw_dst.octet[2] = 0;
    pkt->pkt_snap.nw_dst.octet[3] = 2;
}

void lc_drv_build_test_msg_type3(lc_mbuf_t *msg)
{
    struct msg_ntf_sw_recv_pkt *pkt;

    msg->hdr.magic = MSG_MG_DRV2KER;
    msg->hdr.direction = MSG_DIR_DRV2KER;
    //msg->hdr.type = LC_MSG_NOTIFY_SWITCH_RECV_PKT;
    msg->hdr.length = sizeof(struct msg_ntf_sw_recv_pkt);

    pkt = (struct msg_ntf_sw_recv_pkt *)msg->data;

    pkt->datapath_id = 0x00006aceffcb4b70ULL;
    pkt->in_port = 10;
    pkt->in_sw_buf_id = 0x12345678;
    pkt->pkt_len = 60;
    pkt->pkt_snap.dl_src.octet[0] = 0x00;
    pkt->pkt_snap.dl_src.octet[1] = 0x1D;
    pkt->pkt_snap.dl_src.octet[2] = 0xBB;
    pkt->pkt_snap.dl_src.octet[3] = 0xAA;
    pkt->pkt_snap.dl_src.octet[4] = 0xFF;
    pkt->pkt_snap.dl_src.octet[5] = 0xCE;

    pkt->pkt_snap.dl_dst.octet[0] = 0x00;
    pkt->pkt_snap.dl_dst.octet[1] = 0x1D;
    pkt->pkt_snap.dl_dst.octet[2] = 0xBB;
    pkt->pkt_snap.dl_dst.octet[3] = 0xAA;
    pkt->pkt_snap.dl_dst.octet[4] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[5] = 0xCD;

    pkt->pkt_snap.dl_type = 0x0800;
    pkt->pkt_snap.nw_proto = 17;
    pkt->pkt_snap.nw_src.octet[0] = 172;
    pkt->pkt_snap.nw_src.octet[1] = 16;
    pkt->pkt_snap.nw_src.octet[2] = 0;
    pkt->pkt_snap.nw_src.octet[3] = 7;

    pkt->pkt_snap.nw_dst.octet[0] = 172;
    pkt->pkt_snap.nw_dst.octet[1] = 16;
    pkt->pkt_snap.nw_dst.octet[2] = 0;
    pkt->pkt_snap.nw_dst.octet[3] = 6;
    pkt->pkt_snap.tp_src = 43152;
    pkt->pkt_snap.tp_dst = 53;

}



void lc_drv_build_test_msg_type4(lc_mbuf_t *msg)
{
    struct msg_ntf_sw_recv_pkt *pkt;

    msg->hdr.magic = MSG_MG_DRV2KER;
    msg->hdr.direction = MSG_DIR_DRV2KER;
    //msg->hdr.type = LC_MSG_NOTIFY_SWITCH_RECV_PKT;
    msg->hdr.length = sizeof(struct msg_ntf_sw_recv_pkt);

    pkt = (struct msg_ntf_sw_recv_pkt *)msg->data;

    pkt->datapath_id = 0x00006aceffcb4b70ULL;
    pkt->in_port = 10;
    pkt->in_sw_buf_id = 0x12345678;
    pkt->pkt_len = 60;
    pkt->pkt_snap.dl_src.octet[0] = 0x00;
    pkt->pkt_snap.dl_src.octet[1] = 0x1D;
    pkt->pkt_snap.dl_src.octet[2] = 0xBB;
    pkt->pkt_snap.dl_src.octet[3] = 0xAA;
    pkt->pkt_snap.dl_src.octet[4] = 0xFF;
    pkt->pkt_snap.dl_src.octet[5] = 0xCE;


    pkt->pkt_snap.dl_dst.octet[0] = 0x00;
    pkt->pkt_snap.dl_dst.octet[1] = 0x1D;
    pkt->pkt_snap.dl_dst.octet[2] = 0xBB;
    pkt->pkt_snap.dl_dst.octet[3] = 0xAA;
    pkt->pkt_snap.dl_dst.octet[4] = 0xFF;
    pkt->pkt_snap.dl_dst.octet[5] = 0xC9;

    pkt->pkt_snap.dl_type = 0x0800;
    pkt->pkt_snap.nw_proto = 17;
    pkt->pkt_snap.nw_src.octet[0] = 172;
    pkt->pkt_snap.nw_src.octet[1] = 16;
    pkt->pkt_snap.nw_src.octet[2] = 0;
    pkt->pkt_snap.nw_src.octet[3] = 7;

    pkt->pkt_snap.nw_dst.octet[0] = 172;
    pkt->pkt_snap.nw_dst.octet[1] = 16;
    pkt->pkt_snap.nw_dst.octet[2] = 0;
    pkt->pkt_snap.nw_dst.octet[3] = 2;
    pkt->pkt_snap.tp_src = 43152;
    pkt->pkt_snap.tp_dst = 53;
}

void lc_drv_build_test_msg_type5(lc_mbuf_t *msg)
{
    struct msg_ntf_sw_recv_pkt *pkt;

    msg->hdr.magic = MSG_MG_DRV2KER;
    msg->hdr.direction = MSG_DIR_DRV2KER;
    //msg->hdr.type = LC_MSG_NOTIFY_SWITCH_RECV_PKT;
    msg->hdr.length = sizeof(struct msg_ntf_sw_recv_pkt);

    pkt = (struct msg_ntf_sw_recv_pkt *)msg->data;

    pkt->datapath_id = 0x000046262a189aabULL;
    pkt->in_port = 1;
    pkt->in_sw_buf_id = 0x12345678;
    pkt->pkt_len = 60;
    pkt->pkt_snap.dl_src.octet[0] = 0x00;
    pkt->pkt_snap.dl_src.octet[1] = 0x01;
    pkt->pkt_snap.dl_src.octet[2] = 0x04;
    pkt->pkt_snap.dl_src.octet[3] = 0xA0;
    pkt->pkt_snap.dl_src.octet[4] = 0x00;
    pkt->pkt_snap.dl_src.octet[5] = 0x31;


    pkt->pkt_snap.dl_dst.octet[0] = 0x00;
    pkt->pkt_snap.dl_dst.octet[1] = 0x01;
    pkt->pkt_snap.dl_dst.octet[2] = 0x08;
    pkt->pkt_snap.dl_dst.octet[3] = 0x20;
    pkt->pkt_snap.dl_dst.octet[4] = 0x00;
    pkt->pkt_snap.dl_dst.octet[5] = 0x31;

    pkt->pkt_snap.dl_type = 0x0800;
    pkt->pkt_snap.nw_proto = 17;
    pkt->pkt_snap.nw_src.octet[0] = 172;
    pkt->pkt_snap.nw_src.octet[1] = 16;
    pkt->pkt_snap.nw_src.octet[2] = 0;
    pkt->pkt_snap.nw_src.octet[3] = 7;

    pkt->pkt_snap.nw_dst.octet[0] = 172;
    pkt->pkt_snap.nw_dst.octet[1] = 16;
    pkt->pkt_snap.nw_dst.octet[2] = 0;
    pkt->pkt_snap.nw_dst.octet[3] = 2;
    pkt->pkt_snap.tp_src = 43152;
    pkt->pkt_snap.tp_dst = 53;
}


void lc_drv_build_message(int test_type, lc_mbuf_t *msg)
{
    switch (test_type) {
        case TEST_TYPE1:
            lc_drv_build_test_msg_type1(msg);
            break;
        case TEST_TYPE2:
            lc_drv_build_test_msg_type2(msg);
            break;
        case TEST_TYPE3:
            lc_drv_build_test_msg_type3(msg);
            break;
        case TEST_TYPE4:
            lc_drv_build_test_msg_type4(msg);
            break;
        case TEST_TYPE5:
            lc_drv_build_test_msg_type5(msg);
            break;
        default:
            break;
    }
    return;

}
void *lc_kernel_thread()
{

    lc_mbuf_t *msg;
    int nsend, i, j;

    sleep(2);

    for (j = 0; j < 1; j++) {
        /* for (i = 0; i < TEST_MAX - 1; i++) { */
    i = 0;
//        printf("xxxxx %d \n", i);
        msg = (lc_mbuf_t *)malloc(sizeof(lc_mbuf_t));
        memset(msg, 0, sizeof(lc_mbuf_t));

        if (msg) {
            lc_drv_build_message(i, msg);
            nsend = sendto(lc_sock.sock_rcv, msg,
                    sizeof(lc_mbuf_hdr_t) + msg->hdr.length, 0,
                (struct sockaddr *)&sa_drv_snd_ker,sizeof(struct sockaddr_un));

            if (nsend < 0) {
                printf("xxx driver send message error\n");
            }

            free(msg);

        }

#if 0
        memset(&msg_send, 0, sizeof(msg_send));
        msg_send.type = 2;
        msg_send.magic = 0x1234;
        strcpy(msg_send.buffer, "kernel to app");
        nsend = sendto(lc_sock.sock_rcv_app, &msg_send, sizeof(lc_mbuf_t), 0,
                (struct sockaddr *)&sa_ker_snd_app[0],sizeof(struct sockaddr_un));

        if (nsend < 0) {
            strerror(errno);
            syslog(LOG_ERR, "send error from sock=%d to %s %s",
                    lc_sock.sock_rcv_app, sa_ker_snd_app[0].sun_path, strerror(errno));
        } else {
            syslog(LOG_ERR, "send success to %s", sa_ker_snd_app[0].sun_path);
        }
        sleep(1);
#endif
    /* } */
    }

    printf("xxx driver send message finished\n");
    pthread_exit(NULL);
}


void lc_thread_create(int num)
{
    int ret, i;
    pthread_t *thread;

    thread = (pthread_t *)malloc(num * sizeof(pthread_t));

    memset(thread, 0, num * sizeof(thread));

    for (i = 0; i < num; i++) {
        if ((ret = pthread_create(thread, NULL, lc_kernel_thread, NULL)) != 0) {
            syslog(LOG_ERR, "Create thread failed");
        } else {
            syslog(LOG_INFO, "Create thread successfully");
        }
        thread++;
    }
    return;

}

int main(int argc, char **argv)
{

    if (parse_cmd_line(argc, argv) < 0) {
        exit(-1);
    }

    lc_sock_init();
    lc_thread_create(1);
    lc_main_process();

    return 0;
}
