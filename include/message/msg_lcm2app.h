#ifndef _MESSAGE_MSG_LCM2APP_H
#define _MESSAGE_MSG_LCM2APP_H

#include "lc_netcomp.h"

/*
 * Msg to APP level
 */
struct msg_request_vnet_add {
    int vnet_id;
    char        vnet_name[LC_NAMESIZE];
    int vl2_num;
};

struct msg_request_vnet_del {
    u32         vnet_id;
};

struct msg_request_vl2_add {
    u32         vnet_id;
    u32         vl2_id;
    char        vl2_name[LC_NAMESIZE];
    char        upif_name[LC_NAMESIZE];
    u32         flag;
    u32         portnum;
};

struct msg_request_vl2_del {
    u32     vnet_id;
    u32     vl2_id;
};

struct msg_request_vl2_change {
    u32     vnet_id;
    u32     vl2_id;
    u32     portnum;
};
struct msg_request_vm_add {
    u16         vnet_id;
    u16         vl2_id;
    u16         vm_id;
    vm_t        vm;
};

struct msg_request_vm_opt {
    u32 vnet_id;
    u32 vl2_id;
    u32 vm_id;
};

/* VM disk operations */
struct msg_request_vm_add_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    u32       device;
    u32       size; /* In Mbytes */
};

struct msg_request_vm_del_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    char      vdi_uuid[LC_UUID_LEN];
};

struct msg_request_vm_resize_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    u32       size; /* In Mbytes */
    char      vdi_uuid[LC_UUID_LEN];

};

#endif /* _MESSAGE_MSG_LCM2APP_H */
