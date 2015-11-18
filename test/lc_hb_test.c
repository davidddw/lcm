#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/lc_vm.h"

struct sockaddr_un sa_snd_chk;
struct sockaddr_un sa_chk_rcv;

int sock_fd = -1;

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

int lc_sock_init()
{
    int fd;

    memset(&sa_snd_chk, 0, sizeof(sa_snd_chk));
    sprintf(sa_snd_chk.sun_path, "%s", SOCK_KER_RCV_APP);
    sa_snd_chk.sun_family = AF_UNIX;
    unlink(sa_snd_chk.sun_path);

    fd = create_socket(&sa_snd_chk);
    if (fd < 0) {
        printf("create socket error");
        unlink(sa_snd_chk.sun_path);
        sock_fd = -1;
    } else {
        sock_fd = fd;
    }

    memset(&sa_chk_rcv, 0, sizeof(sa_chk_rcv));
    strcpy(sa_chk_rcv.sun_path, SOCK_CHK_RCV);
    sa_chk_rcv.sun_family = AF_UNIX;
         
    return 0;
}

#if 1
void lc_build_hb_message(lc_mbuf_t *msg)
{
//    struct msg_req_drv_vm_opt *vms;

    msg->hdr.magic = MSG_MG_KER2CHK;
    msg->hdr.direction = MSG_DIR_KER2CHK;
    msg->hdr.type = LC_MSG_KER_REPLY_CHK_HEARTBEAT;
    msg->hdr.length = 0;//sizeof(struct msg_req_drv_vm_opt);

//    vms = (struct msg_req_drv_vm_opt *)msg->data;
//
//    vms->vnet_id = 1;
//    vms->vl2_id = 1;
//    vms->vm_id = 1;
//    vms->server_ip = inet_addr("10.0.0.21");
}

int lc_send_hb_message()
{
    lc_mbuf_t msg;
    int nsend;

    memset(&msg, 0, sizeof(msg));
    lc_build_hb_message(&msg);
    nsend = sendto(sock_fd, &msg, 
           sizeof(lc_mbuf_hdr_t) + msg.hdr.length, 0,
           (struct sockaddr *)&sa_chk_rcv, sizeof(struct sockaddr_un));

    if (nsend < 0) {
        printf("xxx driver send message error\n");
    }

    printf("xxx driver send message finished\n");
    return 0;
}
#endif

#if 1
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
        nrecv = recvfrom(sock_fd, buffer, sizeof(lc_mbuf_t), 0, 
            (struct sockaddr *)&sa, &sa_len);
        
        if (nrecv < 0) {
            return;
        }
        if (nrecv >= sizeof(lc_mbuf_t)) {
            printf("xxxx %d bytes message received\n", nrecv);
            lc_send_hb_message();
        }
    }

    return;

}
#endif
 
int main(int argc, char **argv)
{
    lc_sock_init();
    //lc_send_hb_message();
    lc_main_process();

    return 0;
} 

