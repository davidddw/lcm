/*
 * Copyright (c) 2012-2014 YunShan Networks, Inc.
 *
 * Author Name: Song Zhen
 * Finish Date: 2014-03-04
 */
#ifndef _LC_RM_ACTION_H
#define _LC_RM_ACTION_H

#include "lc_rm_db.h"

void lc_rm_save_vm_action(rm_vm_t *vm, char *actiontype, int alloctype);
void lc_rm_save_vgw_action(rm_vgw_t *vgw, char *actiontype, int alloctype);
void lc_rm_save_vgateway_action(rm_vgw_t *vgw, char *actiontype, int alloctype);
void lc_rm_del_vm_action(rm_vm_t *vm, rm_action_t *action);
void lc_rm_del_vgw_action(rm_vgw_t *vgw, rm_action_t *action);
#endif
