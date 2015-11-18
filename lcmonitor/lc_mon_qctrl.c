/*
 * Copyright (c) 2012 Yunshan Networks
 * All right reserved.
 *
 * Filename:lc_mon_qctrl.c
 * Port from lc_qctrl.c
 * Date: 2013-01-26
 *
 * Description: queue management
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include "../include/lc_global.h"
#include "lc_mon.h"

lc_data_host_t *lc_host_dpq_dq(lc_dpq_host_queue_t *dpq)
{
    lc_data_host_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

lc_data_vm_t *lc_vm_dpq_dq(lc_dpq_vm_queue_t *dpq)
{
    lc_data_vm_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

lc_data_vgw_ha_t *lc_vgw_ha_dpq_dq(lc_dpq_vgw_ha_queue_t *dpq)
{
    lc_data_vgw_ha_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

lc_data_nfs_chk_t *lc_nfs_chk_dpq_dq(lc_dpq_nfs_chk_queue_t *dpq)
{
    lc_data_nfs_chk_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

lc_data_bk_vm_health_t *lc_bk_vm_health_dpq_dq(lc_dpq_bk_vm_health_queue_t *dpq)
{
    lc_data_bk_vm_health_t *pdata = NULL;

    if (STAILQ_EMPTY(&dpq->qhead)) {
        return NULL;
    }

    pdata = STAILQ_FIRST(&dpq->qhead);
    STAILQ_REMOVE_HEAD(&dpq->qhead, entries);
    dpq->deq_cnt++;
    dpq->dp_cnt--;
    return pdata;
}

void lc_host_dpq_eq(lc_dpq_host_queue_t *dpq, lc_data_host_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_vm_dpq_eq(lc_dpq_vm_queue_t *dpq, lc_data_vm_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_nfs_chk_dpq_eq(lc_dpq_nfs_chk_queue_t *dpq, lc_data_nfs_chk_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_vgw_ha_dpq_eq(lc_dpq_vgw_ha_queue_t *dpq, lc_data_vgw_ha_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_bk_vm_health_dpq_eq(lc_dpq_bk_vm_health_queue_t *dpq, lc_data_bk_vm_health_t *pdata)
{
    STAILQ_INSERT_TAIL(&dpq->qhead, pdata, entries);
    dpq->enq_cnt++;
    dpq->dp_cnt++;
    return;
}

void lc_host_dpq_init(lc_dpq_host_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

void lc_vm_dpq_init(lc_dpq_vm_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

void lc_nfs_chk_dpq_init(lc_dpq_nfs_chk_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

void lc_vgw_ha_dpq_init(lc_dpq_vgw_ha_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

void lc_bk_vm_health_dpq_init(lc_dpq_bk_vm_health_queue_t *dpq)
{
    STAILQ_INIT(&dpq->qhead);
    dpq->enq_cnt = 0;
    dpq->deq_cnt = 0;
    dpq->dp_cnt = 0;
    return;
}

int lc_is_host_dpq_empty(lc_dpq_host_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_is_vm_dpq_empty(lc_dpq_vm_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_is_vgw_ha_dpq_empty(lc_dpq_vgw_ha_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_is_bk_vm_health_dpq_empty(lc_dpq_bk_vm_health_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_is_nfs_chk_dpq_empty(lc_dpq_nfs_chk_queue_t *dpq)
{
    if (STAILQ_EMPTY(&dpq->qhead)) {
        return 1;
    } else {
        return 0;
    }
}

int lc_host_dpq_len(lc_dpq_host_queue_t *dpq)
{
    return dpq->dp_cnt;
}

int lc_vm_dpq_len(lc_dpq_vm_queue_t *dpq)
{
    return dpq->dp_cnt;
}

int lc_nfs_chk_dpq_len(lc_dpq_nfs_chk_queue_t *dpq)
{
    return dpq->dp_cnt;
}

int lc_vgw_ha_dpq_len(lc_dpq_vgw_ha_queue_t *dpq)
{
    return dpq->dp_cnt;
}

int lc_bk_vm_health_dpq_len(lc_dpq_bk_vm_health_queue_t *dpq)
{
    return dpq->dp_cnt;
}
