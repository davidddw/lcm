/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-04
 */

#ifndef _LC_RM_MSG_H
#define _LC_RM_MSG_H

#include "lc_netcomp.h"
#include "lc_rm_db.h"
#include "talker.pb-c.h"

typedef struct msg_vm_add_s {
    char vm_ids[LC_VM_IDS_LEN];
    char pool_ids[LC_POOL_IDS_LEN];
    int  alloc_type;
    char vm_passwd[MAX_VM_PASSWD_LEN];
} msg_vm_add_t;

typedef struct msg_vgw_add_s {
    char vgw_ids[LC_VGW_IDS_LEN];
    int  alloc_type;
} msg_vgw_add_t;

typedef struct msg_vm_modify_s {
    uint32_t vm_id;
    uint32_t mask;
    uint32_t vcpu_num;
    uint32_t mem_size;
    uint32_t usr_disk_size;
    uint32_t ha_disk_size;
    uint32_t vdc_id;
    uint32_t vl2_id;
    char     ip[MAX_HOST_ADDRESS_LEN];
} msg_vm_modify_t;

typedef struct msg_vm_snapshot_s {
    uint32_t vm_id;
    uint32_t snapshot_id;
} msg_vm_snapshot_t;

typedef struct msg_ops_reply_s {
    uint32_t id;
    uint32_t type;
    uint32_t ops;
    uint32_t err;
    uint32_t result;
} msg_ops_reply_t;

void lc_rm_send_vm_add_msg(rm_vm_t *vm, char *passwd, uint32_t seq);
void lc_rm_send_vgateway_add_msg(rm_vgw_t *vgw, uint32_t seq);
void lc_rm_send_vgw_add_msg(rm_vgw_t *vgw, uint32_t seq);
void lc_rm_send_vm_modify_msg(msg_vm_modify_t *modify, rm_vdisk_t *vdisk, uint32_t seq);
void lc_rm_send_vm_snapshot_msg(msg_vm_snapshot_t *snapshot, uint32_t seq);
void lc_rm_send_ops_reply(msg_ops_reply_t *ops, uint32_t seq);
void lc_rm_send_vgateway_ops_reply(msg_ops_reply_t *ops, uint32_t seq);
#endif
