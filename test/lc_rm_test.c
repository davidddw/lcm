#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lc_bus.h"
#include "lc_header.h"
#include "talker.pb-c.h"
#include "message/msg.h"
#include "lc_rm.h"
#include "../lcrmd/lc_rm_msg.h"

void send_vgw_create_msg(int id)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VgwAddReq vgw_add = TALKER__VGW_ADD_REQ__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    char vgwids[LC_VGW_IDS_LEN];

    memset(buf, 0, MAX_MSG_BUFFER);
    memset(vgwids, 0, LC_VGW_IDS_LEN);
    snprintf(vgwids, LC_VGW_IDS_LEN, "%d#", id);

    vgw_add.alloc_type = TALKER__ALLOC_TYPE__MANUAL;
    vgw_add.vgw_ids = vgwids;
    msg.vgw_add_req = &vgw_add;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    lc_bus_publish_unicast(buf, (hdr_len + msg_len),
                           LC_BUS_QUEUE_LCRMD);
}

void send_vm_create_msg(int id)
{
    header_t hdr;
    Talker__Message msg = TALKER__MESSAGE__INIT;
    Talker__VmAddReq vm_add = TALKER__VM_ADD_REQ__INIT;
    int hdr_len = get_message_header_pb_len();
    int msg_len;
    char buf[MAX_MSG_BUFFER];
    char vmids[LC_VM_IDS_LEN];
    char poolids[LC_POOL_IDS_LEN];
    char passwd[MAX_VM_PASSWD_LEN];

    memset(buf, 0, MAX_MSG_BUFFER);
    memset(vmids, 0, LC_VM_IDS_LEN);
    snprintf(vmids, LC_VM_IDS_LEN, "%d#", id);
    memset(poolids, 0, LC_POOL_IDS_LEN);
    memset(passwd, 0, MAX_VM_PASSWD_LEN);
    snprintf(passwd, MAX_VM_PASSWD_LEN, "%s", "12345");

    vm_add.alloc_type = TALKER__ALLOC_TYPE__MANUAL;
    vm_add.vm_ids = vmids;
    vm_add.pool_ids = poolids;
    vm_add.vm_passwd = passwd;
    msg.vm_add_req = &vm_add;
    talker__message__pack(&msg, (void *)buf + hdr_len);
    msg_len = talker__message__get_packed_size(&msg);

    hdr.sender = HEADER__MODULE__VMDRIVER;
    hdr.type = HEADER__TYPE__UNICAST;
    hdr.length = msg_len;
    hdr.seq = 0;
    fill_message_header(&hdr, buf);

    lc_bus_publish_unicast(buf, (hdr_len + msg_len),
                           LC_BUS_QUEUE_LCRMD);

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
    }
    if (lc_bus_exit()) {
        printf("Error: lc_bus_exit()\n");
    }
    return 0;
}
