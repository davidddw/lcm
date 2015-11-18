/*
 * Copyright 2012 yunshan.net.cn
 * All rights reserved
 * Author: Lai Yuan laiyuan@yunshan.net.cn
 */
#ifndef _LC_RM_ALLOC_H
#define _LC_RM_ALLOC_H

#include "../lc_rm_msg.h"

void alloc_vm_auto(char *vmids, char *poolids, int algo);
void alloc_vgateway_auto(char *vdcids, int algo);
void alloc_vm_manual(char *vmids);
void alloc_vgateway_manual(char *vgwids);
void alloc_vm_modify(msg_vm_modify_t *vm_modify);
int alloc_vm_snapshot(msg_vm_snapshot_t *vm_snapshot);
#endif
