#ifndef _LC_VNET_WORKER_H
#define _LC_VNET_WORKER_H

#include "lc_netcomp.h"

typedef struct lc_vm_s {
    vm_t vm;
    struct service_port_id sp_id;
} lc_vm_t;

typedef struct lc_vl2_s {
    vl2_t vl2;
    lc_vm_t *vm_tbl;
} lc_vl2_t;

typedef struct lc_vnet_s {
    vnet_t vnet;
    lc_vl2_t *vl2_tbl;
} lc_vnet_t;

#endif
