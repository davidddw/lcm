#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lc_vm.h"
#include "lc_bus.h"
#include "message/msg.h"

void send_vgw_create_msg(int id)
{
    lc_mbuf_t msg;
    struct msg_vedge_opt *p;
    int len;

    len = sizeof(struct lc_msg_head) + sizeof(struct msg_vedge_opt);

    memset(&msg, 0, sizeof msg);
    msg.hdr.magic = MSG_MG_UI2DRV;
    msg.hdr.type = LC_MSG_VEDGE_ADD;
    msg.hdr.length = sizeof(struct msg_vedge_opt);

    p = (struct msg_vedge_opt *)msg.data;
    p->vnet_id = id;

    lc_bus_publish_unicast((void *)&msg, len, LC_BUS_QUEUE_VMDRIVER);
}

void send_vm_create_msg(int id)
{
    lc_mbuf_t msg;
    struct msg_vm_opt *p;
    int len;

    len = sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt);

    memset(&msg, 0, sizeof msg);
    msg.hdr.magic = MSG_MG_UI2DRV;
    msg.hdr.type = LC_MSG_VM_ADD;
    msg.hdr.length = sizeof(struct msg_vm_opt);

    p = (struct msg_vm_opt *)msg.data;
    p->vm_id = id;

    lc_bus_publish_unicast((void *)&msg, len, LC_BUS_QUEUE_VMDRIVER);
}

void send_vm_delete_msg(int id)
{
    lc_mbuf_t msg;
    struct msg_vm_opt *p;
    int len;

    len = sizeof(struct lc_msg_head) + sizeof(struct msg_vm_opt);

    memset(&msg, 0, sizeof msg);
    msg.hdr.magic = MSG_MG_UI2KER;
    msg.hdr.type = LC_MSG_VM_DEL;
    msg.hdr.length = sizeof(struct msg_vm_opt);

    p = (struct msg_vm_opt *)msg.data;
    p->vm_id = id;

    lc_bus_publish_unicast((void *)&msg, len, LC_BUS_QUEUE_LCPD);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("Illegal arguments\n");
        return -1;
    }
    if (lc_bus_init()) {
        printf("Error: lc_bus_init()\n");
    }
    if (strcmp(argv[1], "vgw-create") == 0) {
        if (argc != 3) {
            printf("Illegal arguments\n");
            return -1;
        }
        send_vgw_create_msg(atoi(argv[2]));
    } else if (strcmp(argv[1], "vm-create") == 0) {
        if (argc != 3) {
            printf("Illegal arguments\n");
            return -1;
        }
        send_vm_create_msg(atoi(argv[2]));
    } else if (strcmp(argv[1], "vm-delete") == 0) {
        if (argc != 3) {
            printf("Illegal arguments\n");
            return -1;
        }
        send_vm_delete_msg(atoi(argv[2]));
    }
    if (lc_bus_exit()) {
        printf("Error: lc_bus_exit()\n");
    }
    return 0;
}
