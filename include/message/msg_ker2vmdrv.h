#ifndef _MESSAGE_MSG_KER2VMDRV_H
#define _MESSAGE_MSG_KER2VMDRV_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <lc_netcomp.h>

struct msg_req_drv_vm_add {
    u16         vnet_id;
    u16         vl2_id;
    u16         vm_id;
    vm_t        vm;
    struct      physical_port_id pid;
};

struct msg_req_drv_vm_del {
    u32         vnet_id;
    u32         vl2_id;
    u32         vm_id;
    struct      physical_port_id pid;
};

struct msg_req_drv_vm_opt {
    u32         vnet_id;
    u32         vl2_id;
    u32         vm_id;
    struct      physical_port_id pid;
};

/* VM disk operations */
struct msg_req_drv_vm_add_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    u32       device;
    u32       size; /* In Mbytes */
};

struct msg_req_drv_vm_remove_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    u32       device;
};

struct msg_req_drv_vm_resize_disk {
    u32       vnet_id;
    u32       vl2_id;
    u32       vm_id;
    u32       size; /* In Mbytes */
    char      vdi_uuid[LC_UUID_LEN];
};

#endif /* _MESSAGE_MSG_KER2VMDRV_H */
