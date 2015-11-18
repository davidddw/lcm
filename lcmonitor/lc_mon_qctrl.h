/*
 * Copyright 2012 yunshan network
 * All rights reserved
 * Author:
 * Date:
 */
#ifndef LC_MONITOR_QUEUE_H
#define LC_MONITOR_QUEUE_H

lc_data_host_t *lc_host_dpq_dq(lc_dpq_host_queue_t *dpq);
lc_data_vm_t *lc_vm_dpq_dq(lc_dpq_vm_queue_t *dpq);
lc_data_nfs_chk_t *lc_nfs_chk_dpq_dq(lc_dpq_nfs_chk_queue_t *dpq);
lc_data_vgw_ha_t *lc_vgw_ha_dpq_dq(lc_dpq_vgw_ha_queue_t *dpq);
lc_data_bk_vm_health_t *lc_bk_vm_health_dpq_dq(lc_dpq_bk_vm_health_queue_t *dpq);
void lc_host_dpq_eq(lc_dpq_host_queue_t *dpq, lc_data_host_t *pdata);
void lc_vm_dpq_eq(lc_dpq_vm_queue_t *dpq, lc_data_vm_t *pdata);
void lc_vgw_ha_dpq_eq(lc_dpq_vgw_ha_queue_t *dpq, lc_data_vgw_ha_t *pdata);
void lc_bk_vm_health_dpq_eq(lc_dpq_bk_vm_health_queue_t *dpq, lc_data_bk_vm_health_t *pdata);
void lc_nfs_chk_dpq_eq(lc_dpq_nfs_chk_queue_t *dpq, lc_data_nfs_chk_t *pdata);
void lc_host_dpq_init(lc_dpq_host_queue_t *dpq);
void lc_vm_dpq_init(lc_dpq_vm_queue_t *dpq);
void lc_vgw_ha_dpq_init(lc_dpq_vgw_ha_queue_t *dpq);
void lc_bk_vm_health_dpq_init(lc_dpq_bk_vm_health_queue_t *dpq);
void lc_nfs_chk_dpq_init(lc_dpq_nfs_chk_queue_t *dpq);
int lc_is_host_dpq_empty(lc_dpq_host_queue_t *dpq);
int lc_is_vm_dpq_empty(lc_dpq_vm_queue_t *dpq);
int lc_is_nfs_chk_dpq_empty(lc_dpq_nfs_chk_queue_t *dpq);
int lc_is_vgw_ha_dpq_empty(lc_dpq_vgw_ha_queue_t *dpq);
int lc_is_bk_vm_health_dpq_empty(lc_dpq_bk_vm_health_queue_t *dpq);
int lc_host_dpq_len(lc_dpq_host_queue_t *dpq);
int lc_vm_dpq_len(lc_dpq_vm_queue_t *dpq);
int lc_vgw_ha_dpq_len(lc_dpq_vgw_ha_queue_t *dpq);
int lc_nfs_chk_dpq_len(lc_dpq_nfs_chk_queue_t *dpq);
int lc_bk_vm_health_dpq_len(lc_dpq_bk_vm_health_queue_t *dpq);

#endif /*LC_MONITOR_QUEUE_H*/

