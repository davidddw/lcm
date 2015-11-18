#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lc_netcomp.h"
#include "lc_db.h"
#include "lc_db_log.h"
#include "lc_rm.h"
#include "../lc_rm_msg.h"
#include "lc_alloc_db.h"

void alloc_get_vm(alloc_head_t *vmhead, char *vmids)
{
    char          where[LC_NAMEDES];
    char          data[LC_VM_IDS_LEN];
    int           vmid       = 0;
    char         *token      = NULL;
    char         *saveptr    = NULL;
    alloc_node_t *alloc_node = NULL;
    alloc_vm_t   *alloc_vm   = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (0 == vmids[0]) {
        RM_LOG(LOG_DEBUG, "vmids is NULL");
        goto error;
    }
    memset(data, 0, LC_VM_IDS_LEN);
    memcpy(data, vmids, LC_VM_IDS_LEN);

    for (token = strtok_r(data, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        vmid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vm:%d", vmid);
        if (0 == vmid) {
            continue;
        }

        alloc_vm = (alloc_vm_t *)malloc(sizeof(alloc_vm_t));
        if (NULL == alloc_vm) {
            RM_LOG(LOG_ERR, "malloc fails");
            continue;
        }
        memset(alloc_vm, 0, sizeof(alloc_vm_t));

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vmid);
        db2allocvm((void*)alloc_vm, ALLOC_DB_FLAG_ELEMENT, where);
        if (0 == alloc_vm->id) {
            RM_LOG(LOG_ERR, "Load vm:%d from DB failed", vmid);
            free(alloc_vm);
            continue;
        }

        alloc_node = (alloc_node_t *)malloc(sizeof(alloc_node_t));
        if (NULL == alloc_node) {
            RM_LOG(LOG_ERR, "malloc fails");
            free(alloc_vm);
            continue;
        }
        memset(alloc_node, 0, sizeof(alloc_node_t));
        alloc_node->element = (void *)alloc_vm;
        SLIST_INSERT_HEAD(vmhead, alloc_node, chain);
    }

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_vdc(alloc_head_t *vdchead, char *vdcids)
{
    char          where[LC_NAMEDES];
    char          data[LC_VGW_IDS_LEN];
    int           vdcid      = 0;
    char         *token      = NULL;
    char         *saveptr    = NULL;
    alloc_node_t *alloc_node = NULL;
    alloc_vdc_t  *alloc_vdc  = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (0 == vdcids[0]) {
        RM_LOG(LOG_DEBUG, "vdcids is NULL");
        goto error;
    }
    memset(data, 0, LC_VGW_IDS_LEN);
    memcpy(data, vdcids, LC_VGW_IDS_LEN);

    for (token = strtok_r(data, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        vdcid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load vdc:%d", vdcid);
        if (0 == vdcid) {
            continue;
        }

        alloc_vdc = (alloc_vdc_t *)malloc(sizeof(alloc_vdc_t));
        if (NULL == alloc_vdc) {
            RM_LOG(LOG_ERR, "malloc fails");
            continue;
        }
        memset(alloc_vdc, 0, sizeof(alloc_vdc_t));

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDC_ID, vdcid);
        db2allocvdc((void*)alloc_vdc, where);
        if (0 == alloc_vdc->id) {
            RM_LOG(LOG_ERR, "Load vdc:%d from DB failed", vdcid);
            free(alloc_vdc);
            continue;
        }

        alloc_node = (alloc_node_t *)malloc(sizeof(alloc_node_t));
        if (NULL == alloc_node) {
            RM_LOG(LOG_ERR, "malloc fails");
            free(alloc_vdc);
            continue;
        }
        memset(alloc_node, 0, sizeof(alloc_node_t));
        alloc_node->element = (void *)alloc_vdc;
        SLIST_INSERT_HEAD(vdchead, alloc_node, chain);
    }

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_pool(struct alloc_poolh *poolh, char *poolids)
{
    char          where[LC_NAMEDES];
    char          data[LC_POOL_IDS_LEN];
    int           poolid  = 0;
    char         *token   = NULL;
    char         *saveptr = NULL;
    alloc_pool_t *pool    = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    if (0 == poolids[0]) {
        RM_LOG(LOG_ERR, "Poolids is NULL");
        goto error;
    }
    memset(data, 0, LC_POOL_IDS_LEN);
    memcpy(data, poolids, LC_POOL_IDS_LEN);

    for (token = strtok_r(data, "#", &saveptr);
         NULL != token;
         token = strtok_r(NULL, "#", &saveptr)) {

        poolid = atoi(token);
        RM_LOG(LOG_DEBUG, "Load pool:%d", poolid);
        if (0 == poolid) {
            continue;
        }

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_POOL_ID, poolid);
        pool = (alloc_pool_t *)malloc(sizeof(alloc_pool_t));
        memset(pool, 0, sizeof(alloc_pool_t));
        db2allocpool((void*)pool, ALLOC_DB_FLAG_ELEMENT, where);
        SLIST_INSERT_HEAD(poolh, pool, chain);
    }

error:
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_vgateway_cr(alloc_head_t *crh, struct alloc_poolh *poolh,
                           char *excluded_server)
{
    char where[LC_NAMEDES];
    alloc_pool_t *pool = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    SLIST_FOREACH(pool, poolh, chain) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES,
                 "%s=%d and %s=%d and %s<>'%s' and %s='%s'",
                 TABLE_ALLOC_CR_POOLID, pool->poolid,
                 TABLE_ALLOC_CR_STATE, TABLE_ALLOC_CR_STATE_COMPLETE,
                 TABLE_ALLOC_CR_IP, excluded_server,
                 TABLE_ALLOC_CR_DOMAIN, pool->domain);
        db2alloccr((void*)crh, ALLOC_DB_FLAG_SLIST, where,
                    pool->poolctype, pool->pooliptype);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_vgw_cr(alloc_head_t *crh, struct alloc_poolh *poolh,
                      uint32_t srvflag, char *excluded_server)
{
    char where[LC_NAMEDES];
    alloc_pool_t *pool = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    SLIST_FOREACH(pool, poolh, chain) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES,
                 "%s=%d and %s=%d and %s=%d and %s<>'%s' and %s='%s'",
                 TABLE_ALLOC_CR_POOLID, pool->poolid,
                 TABLE_ALLOC_CR_STATE, TABLE_ALLOC_CR_STATE_COMPLETE,
                 TABLE_ALLOC_CR_SRV_FLAG, srvflag,
                 TABLE_ALLOC_CR_IP, excluded_server,
                 TABLE_ALLOC_CR_DOMAIN, pool->domain);
        db2alloccr((void*)crh, ALLOC_DB_FLAG_SLIST, where,
                    pool->poolctype, pool->pooliptype);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_cr(alloc_head_t *crh, struct alloc_poolh *poolh)
{
    char where[LC_NAMEDES];
    alloc_pool_t *pool = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    SLIST_FOREACH(pool, poolh, chain) {

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s=%d and %s='%s'",
                 TABLE_ALLOC_CR_POOLID, pool->poolid,
                 TABLE_ALLOC_CR_STATE, TABLE_ALLOC_CR_STATE_COMPLETE,
                 TABLE_ALLOC_CR_DOMAIN, pool->domain);
        db2alloccr((void*)crh, ALLOC_DB_FLAG_SLIST, where,
                   pool->poolctype, pool->pooliptype);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_get_sr(alloc_head_t *crh, double cpu_over_alloc_ratio)
{
    char where[LC_NAMEDES];
    alloc_head_t    *srhead = {NULL};
    alloc_node_t    *crnode = NULL;
    alloc_cr_t      *cr     = NULL;
    alloc_sr_con_t   sr_con = {0};

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Load sr from db */
    SLIST_FOREACH(crnode, crh, chain) {

        cr = (alloc_cr_t *)(crnode->element);
        cr->cpu_total = cr->cpu_total * cpu_over_alloc_ratio;
        srhead = &(cr->storage);
        SLIST_INIT(srhead);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
                 TABLE_ALLOC_CON_SR_HOST, cr->address,
                 TABLE_ALLOC_CON_SR_ACTIVE, TABLE_ALLOC_CON_SR_ACTIVE_Y);
        memset(&sr_con, 0, sizeof(alloc_sr_con_t));
        db2allocsrcon((void*)&sr_con, ALLOC_DB_FLAG_ELEMENT, where);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d",
                 TABLE_ALLOC_SR_ID, sr_con.srid);
        db2allocsr((void*)srhead, where);

        if (SLIST_EMPTY(srhead)) {
            RM_LOG(LOG_ERR, "No sr in host: %s", cr->address);
        }
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_free_pool(struct alloc_poolh *poolh)
{
    alloc_pool_t *pool = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    while (!SLIST_EMPTY(poolh)) {
        pool = SLIST_FIRST(poolh);
        SLIST_REMOVE_HEAD(poolh, chain);
        free(pool);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_free_cr(alloc_head_t *crh)
{
    alloc_head_t    *srh    = {NULL};
    alloc_node_t    *crnode = NULL;
    alloc_node_t    *srnode = NULL;
    alloc_cr_t      *cr     = NULL;
    alloc_sr_t      *sr     = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    while (!SLIST_EMPTY(crh)) {
        crnode = SLIST_FIRST(crh);

        cr = (alloc_cr_t *)(crnode->element);
        srh = &(cr->storage);

        while (!SLIST_EMPTY(srh)) {
            srnode = SLIST_FIRST(srh);

            sr = (alloc_sr_t *)(srnode->element);
            free(sr);
            SLIST_REMOVE_HEAD(srh, chain);
            free(srnode);
        }

        free(cr);
        SLIST_REMOVE_HEAD(crh, chain);
        free(crnode);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_update_vm_errno(alloc_vm_t *vm)
{
    uint32_t alloc_errno = 0;

    if (NULL == vm) {
        RM_LOG(LOG_ERR, "vm is NULL");
        return;
    }

    if (!vm->alloc_errno) {
        RM_LOG(LOG_ERR, "vm %u with no errno", vm->id);
        vm->alloc_errno = TABLE_ALLOC_VM_ERRNO_CREATE_ERROR;
        return;
    }

    if (vm->alloc_errno & LC_ALLOC_VM_ERRNO_NO_DISK) {
        RM_LOG(LOG_ERR, "no disk for vm %u", vm->id);
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_DISK;
    }
    if (vm->alloc_errno & LC_ALLOC_VM_ERRNO_NO_MEMORY) {
        RM_LOG(LOG_ERR, "no memory for vm %u", vm->id);
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_MEMORY;
    }
    if (vm->alloc_errno & LC_ALLOC_VM_ERRNO_NO_CPU) {
        RM_LOG(LOG_ERR, "no cpu for vm %u", vm->id);
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_CPU;
    }
    if (vm->alloc_errno & LC_ALLOC_VM_ERRNO_NO_HOST) {
        RM_LOG(LOG_ERR, "no host for vm %u", vm->id);
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_DISK;
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_MEMORY;
        alloc_errno |= TABLE_ALLOC_VM_ERRNO_NO_CPU;
    }
    vm->alloc_errno = alloc_errno;

    return;
}

void alloc_update_vgw_errno(alloc_vdc_t *vdc)
{
    uint32_t alloc_errno = 0;

    if (NULL == vdc) {
        RM_LOG(LOG_ERR, "vgw is NULL");
        return;
    }

    if (!vdc->alloc_errno) {
        RM_LOG(LOG_ERR, "vgw %u with no errno", vdc->id);
        vdc->alloc_errno = TABLE_ALLOC_VDC_ERRNO_CREATE_ERROR;
        return;
    }

    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_NO_DISK) {
        RM_LOG(LOG_ERR, "no disk for vgw %u", vdc->id);
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_DISK;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_NO_MEMORY) {
        RM_LOG(LOG_ERR, "no memory for vgw %u", vdc->id);
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_MEMORY;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_NO_CPU) {
        RM_LOG(LOG_ERR, "no cpu for vgw %u", vdc->id);
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_CPU;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_NO_IP) {
        RM_LOG(LOG_ERR, "no ip for vgw %u", vdc->id);
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_IP;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_NO_HOST) {
        RM_LOG(LOG_ERR, "no host for vgw %u", vdc->id);
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_DISK;
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_MEMORY;
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_NO_CPU;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_TOO_MANY_VGWS) {
        RM_LOG(LOG_ERR, "too many vgws");
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_DB_ERROR;
    }
    if (vdc->alloc_errno & LC_ALLOC_VGW_ERRNO_VGWS_INFO_MISMATCH) {
        RM_LOG(LOG_ERR, "vgw info mismatch");
        alloc_errno |= TABLE_ALLOC_VDC_ERRNO_DB_ERROR;
    }
    vdc->alloc_errno = alloc_errno;

    return;
}

void alloc_free_ir(alloc_head_t *irh)
{
    alloc_node_t    *irnode = NULL;
    alloc_ip_t      *ir     = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    while (!SLIST_EMPTY(irh)) {
        irnode = SLIST_FIRST(irh);

        ir = (alloc_ip_t *)(irnode->element);
        free(ir);
        SLIST_REMOVE_HEAD(irh, chain);
        free(irnode);
    }
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

int alloc_check_vdisk(alloc_vm_t *vm, alloc_vdisk_t *vdisk)
{
    int  ret = 0;
    char where[LC_NAMEDES];
    alloc_sr_con_t ha_sr_con;
    alloc_sr_t     ha_sr;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s=%d",
             TABLE_ALLOC_CON_HASR_HOST, vm->server,
             TABLE_ALLOC_CON_HASR_ACTIVE, TABLE_ALLOC_CON_HASR_ACTIVE_Y);
    memset(&ha_sr_con, 0, sizeof(alloc_sr_con_t));
    db2allochasrcon((void*)&ha_sr_con, ALLOC_DB_FLAG_ELEMENT, where);

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_HASR_ID, ha_sr_con.srid);
    memset(&ha_sr, 0, sizeof(alloc_sr_t));
    db2allochasr(&ha_sr, where);

    if (vdisk->vdisize > (ha_sr.disk_total - ha_sr.disk_used)) {
        RM_LOG(LOG_ERR, "No disk %dG for vm: %d\n", vdisk->vdisize, vm->id);
        RM_LOG(LOG_ERR, "disk_total: %dG, disk_used:%dG\n",
               ha_sr.disk_total, ha_sr.disk_used);
        ret = -1;
    } else {
        if (0 == vdisk->srname[0]) {
            strncpy(vdisk->srname, ha_sr.name_label, LC_NAMESIZE - 1);
        }
    }

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return ret;
}

void alloc_vgateway_auto(char *vgwids, int algo)
{
    int ret = 0;
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    alloc_vdc_t  *vdc     = NULL;
    alloc_node_t *vdcnode = NULL;
    alloc_head_t  vdchead = {NULL};
    alloc_head_t  vdccrh  = {NULL};
    alloc_vnet_t  vnet    = {0};
    alloc_vnet_t  havnet  = {0};
    struct alloc_poolh vdcpoolh= {NULL};

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vdchead);
    SLIST_INIT(&vdccrh);
    SLIST_INIT(&vdcpoolh);

    /* Load vgateway from db */
    alloc_get_vdc((void*)&vdchead, vgwids);
    if (SLIST_EMPTY(&vdchead)) {
        return;
    }

    vdcnode = SLIST_FIRST(&vdchead);
    vdc     = (alloc_vdc_t *)(vdcnode->element);
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d",
             TABLE_ALLOC_VNET_ID, vdc->id);
    memset(&vnet, 0, sizeof(alloc_vnet_t));
    db2allocvnet(&vnet, where);
    /* Load vdc_pool from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
             TABLE_ALLOC_POOL_TYPE, TABLE_ALLOC_POOL_TYPE_NSP,
             TABLE_ALLOC_POOL_DOMIAN, vdc->domain);
    RM_LOG(LOG_DEBUG, "get pool info where is:%s", where);
    db2allocpool((void*)&vdcpoolh, ALLOC_DB_FLAG_SLIST, where);
    if (SLIST_EMPTY(&vdcpoolh)) {
        RM_LOG(LOG_ERR, "No gateway pool");
    }
    /* Load vdc resource from db */
    memset(&havnet, 0, sizeof(alloc_vnet_t));
    alloc_get_vgateway_cr(&vdccrh, &vdcpoolh,
                     havnet.server);
    if (SLIST_EMPTY(&vdccrh)) {
        RM_LOG(LOG_ERR, "No cr for gateway");
        goto error;
    }

    /* Resource assign */
    ret = lc_alloc_vgateway( &vdchead, &vdccrh);
    if (LC_ALLOC_RET_GATEWAY_FAILED & ret) {
        RM_LOG(LOG_ERR, "Alloc gateway failed");
        goto error;
    }

    /* Update vgateway db */
    while (!SLIST_EMPTY(&vdchead)) {
        vdcnode = SLIST_FIRST(&vdchead);
        vdc = (alloc_vdc_t *)(vdcnode->element);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDC_ID, vdc->id);
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%d, %s='%s'",
                 TABLE_ALLOC_VDC_POOLID, vdc->pool_id,
                 TABLE_ALLOC_VDC_SERVER, vdc->server);
        updatedb(TABLE_ALLOC_VDC, set, where);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDC_ID, vdc->id);
        memset(&vnet, 0, sizeof(alloc_vnet_t));
        db2allocvnet(&vnet, where);

        free(vdc);
        SLIST_REMOVE_HEAD(&vdchead, chain);
        free(vdcnode);
    }
    goto complete;

error:
    /* Update vdc db set no resource */
    while (!SLIST_EMPTY(&vdchead)) {
        vdcnode = SLIST_FIRST(&vdchead);
        vdc = (alloc_vdc_t *)(vdcnode->element);
        alloc_update_vgw_errno(vdc);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDC_ID, vdc->id);
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%u",
                 TABLE_ALLOC_VDC_ERRNO, vdc->alloc_errno);
        updatedb(TABLE_ALLOC_VDC, set, where);
        lc_rm_db_syslog("Create VGATEWAY", LC_SYSLOG_OBJECTTYPE_VGATEWAY, vdc->id,
                        "vgateway", "no resource", LC_SYSLOG_LEVEL_ERR);
        free(vdc);

        SLIST_REMOVE_HEAD(&vdchead, chain);
        free(vdcnode);
    }

complete:
    alloc_free_cr(&vdccrh);
    alloc_free_pool(&vdcpoolh);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return;
}

void alloc_vm_auto(char *vmids, char *poolids, int algo)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    char sys_conf_value[TABLE_ALLOC_SYS_CONF_VALUE_LEN];
    double vcpu_over_alloc_ratio;
    int  vcpu_over_alloc_flag   = 0;
    alloc_vm_t         *vm      = NULL;
    alloc_node_t       *vmnode  = NULL;
    alloc_head_t        vmhead  = {NULL};
    alloc_head_t        vmcrh   = {NULL};
    struct alloc_poolh  vmpoolh = {NULL};
    alloc_pool_t *pool = NULL;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vmhead);
    SLIST_INIT(&vmcrh);
    SLIST_INIT(&vmpoolh);

    /* Load vm from db */
    alloc_get_vm((void*)&vmhead, vmids);
    if (SLIST_EMPTY(&vmhead)) {
        return;
    }
    /* Load vm_pool from db */
    alloc_get_pool(&vmpoolh, poolids);
    if (SLIST_EMPTY(&vmpoolh)) {
        RM_LOG(LOG_ERR, "No vm pool");
    }

    SLIST_FOREACH(pool, &vmpoolh, chain) {
        RM_LOG(LOG_DEBUG, "pool_id: %d", pool->poolid);
        RM_LOG(LOG_DEBUG, "pool_htype: %d", pool->poolctype);
    }

    /* Load vm resource from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s'",
             TABLE_ALLOC_SYS_CONF_PARAM, "vcpu_over_allocation_ratio");
    memset(sys_conf_value, 0, TABLE_ALLOC_SYS_CONF_VALUE_LEN);
    db2sysconf((void*)sys_conf_value, where);
    if (strlen(sys_conf_value)) {
        vcpu_over_alloc_ratio = atof(sys_conf_value);
    } else {
        vcpu_over_alloc_ratio = 0;
    }
    if (vcpu_over_alloc_ratio < 0.1) {
        vcpu_over_alloc_flag = LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION;
    }

    alloc_get_cr(&vmcrh, &vmpoolh);
    if (SLIST_EMPTY(&vmcrh)) {
        RM_LOG(LOG_ERR, "No cr for vm");
    }
    alloc_get_sr(&vmcrh, vcpu_over_alloc_ratio);

    /* Resource assign */
    lc_alloc_vm(&vmhead, &vmcrh, NULL, NULL, NULL, vcpu_over_alloc_flag);

    /* Update vm db */
    while (!SLIST_EMPTY(&vmhead)) {
        vmnode = SLIST_FIRST(&vmhead);

        vm = (alloc_vm_t *)(vmnode->element);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm->id);
        memset(set, 0, LC_NAMEDES);

        if ((0 == vm->pool_id) || (0 == vm->server[0])) {
            alloc_update_vm_errno(vm);
            snprintf(set, LC_NAMEDES, "%s=%u",
                     TABLE_ALLOC_VM_ERRNO, vm->alloc_errno);
            lc_rm_db_syslog("Create VM", LC_SYSLOG_OBJECTTYPE_VM, vm->id,
                            "vm", "no resource", LC_SYSLOG_LEVEL_ERR);
        } else {
            snprintf(set, LC_NAMEDES, "%s=%d, %s='%s', %s='%s', %s='%s'",
                     TABLE_ALLOC_VM_POOLID, vm->pool_id,
                     TABLE_ALLOC_VM_SERVER, vm->server,
                     TABLE_ALLOC_VM_SYS_SR_NAME, vm->sys_sr_name,
                     TABLE_ALLOC_VM_USR_SR_NAME, vm->usr_sr_name);
        }
        updatedb(TABLE_ALLOC_VM, set, where);
        free(vm);

        SLIST_REMOVE_HEAD(&vmhead, chain);
        free(vmnode);
    }
    alloc_free_cr(&vmcrh);
    alloc_free_pool(&vmpoolh);

    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
    return;
}

void alloc_vm_manual(char *vmids)
{
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    char sys_conf_value[TABLE_ALLOC_SYS_CONF_VALUE_LEN];
    alloc_vm_t   *vm     = NULL;
    alloc_node_t *vmnode = NULL;
    alloc_head_t  vmhead = {NULL};
    alloc_head_t  vmcrh  = {NULL};
    alloc_pool_t  pool;
    double vcpu_over_alloc_ratio;
    int    vcpu_over_alloc_flag = 0;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vmhead);
    SLIST_INIT(&vmcrh);

    /* Load vm from db */
    alloc_get_vm((void*)&vmhead, vmids);
    if (SLIST_EMPTY(&vmhead)) {
        return;
    }
    /* Load vm resource from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s'",
             TABLE_ALLOC_SYS_CONF_PARAM, "vcpu_over_allocation_ratio");
    memset(sys_conf_value, 0, TABLE_ALLOC_SYS_CONF_VALUE_LEN);
    db2sysconf((void*)sys_conf_value, where);
    if (strlen(sys_conf_value)) {
        vcpu_over_alloc_ratio = atof(sys_conf_value);
    } else {
        vcpu_over_alloc_ratio = 0;
    }
    if (vcpu_over_alloc_ratio < 0.1) {
        vcpu_over_alloc_flag = LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION;
    }

    SLIST_FOREACH(vmnode, &vmhead, chain) {

        vm = (alloc_vm_t *)(vmnode->element);

        /* Load vm_pool from db */
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
                 TABLE_ALLOC_POOL_ID, vm->pool_id,
                 TABLE_ALLOC_POOL_DOMIAN, vm->domain);
        memset(&pool, 0, sizeof(alloc_pool_t));
        db2allocpool((void*)&pool, ALLOC_DB_FLAG_ELEMENT, where);

        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s' and %s='%s'",
                 TABLE_ALLOC_CR_IP, vm->server,
                 TABLE_ALLOC_CR_DOMAIN, vm->domain);
        db2alloccr((void*)&vmcrh, ALLOC_DB_FLAG_SLIST, where,
                   pool.poolctype, pool.pooliptype);
    }
    if (SLIST_EMPTY(&vmcrh)) {
        RM_LOG(LOG_ERR, "No cr for vm");
    } else {
        alloc_get_sr(&vmcrh, vcpu_over_alloc_ratio);
    }

    /* Resource check */
    lc_check_vm(&vmhead, &vmcrh, NULL, NULL, vcpu_over_alloc_flag);

    /* Update vm db */
    while (!SLIST_EMPTY(&vmhead)) {
        vmnode = SLIST_FIRST(&vmhead);

        vm = (alloc_vm_t *)(vmnode->element);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm->id);
        memset(set, 0, LC_NAMEDES);

        if ((0 == vm->pool_id) || (0 == vm->server[0])) {
            alloc_update_vm_errno(vm);
            snprintf(set, LC_NAMEDES, "%s=%u",
                     TABLE_ALLOC_VM_ERRNO, vm->alloc_errno);
            updatedb(TABLE_ALLOC_VM, set, where);
            lc_rm_db_syslog("Create VM", LC_SYSLOG_OBJECTTYPE_VM, vm->id,
                            "vm", "no resource", LC_SYSLOG_LEVEL_ERR);
        }
        free(vm);
        SLIST_REMOVE_HEAD(&vmhead, chain);
        free(vmnode);
    }

    alloc_free_cr(&vmcrh);
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_vgateway_manual(char *vgwids)
{
    int  ret = 0;
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    alloc_vdc_t  *vdc     = NULL;
    alloc_node_t *vdcnode = NULL;
    alloc_head_t  vdchead = {NULL};
    alloc_head_t  vdccrh  = {0};

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vdchead);
    SLIST_INIT(&vdccrh);

    /* Load vgateway from db */
    alloc_get_vdc((void*)&vdchead, vgwids);
    if (SLIST_EMPTY(&vdchead)) {
        return;
    }
    /* Load vdc resourc from db */
    SLIST_FOREACH(vdcnode, &vdchead, chain) {

        vdc = (alloc_vdc_t *)(vdcnode->element);

        /* Load vdc_resource from db */
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s='%s' and %s='%s'",
                TABLE_ALLOC_CR_IP, vdc->server,
                TABLE_ALLOC_CR_DOMAIN, vdc->domain);
        db2alloccr((void*)&vdccrh, ALLOC_DB_FLAG_SLIST, where,
        TABLE_ALLOC_POOL_CTYPE_XEN, TABLE_ALLOC_POOL_IPTYPE_GLOBAL);
    }
    if (SLIST_EMPTY(&vdccrh)) {
        RM_LOG(LOG_ERR, "No cr for gateway");
    }


    /* Update vdc db set no resource */
    if (LC_ALLOC_RET_GATEWAY_FAILED & ret) {
        RM_LOG(LOG_ERR, "Check gateway failed");

        while (!SLIST_EMPTY(&vdchead)) {
            vdcnode = SLIST_FIRST(&vdchead);
            vdc = (alloc_vdc_t *)(vdcnode->element);
            alloc_update_vgw_errno(vdc);
            memset(where, 0, LC_NAMEDES);
            snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDC_ID, vdc->id);
            memset(set, 0, LC_NAMEDES);
            snprintf(set, LC_NAMEDES, "%s=%u",
                    TABLE_ALLOC_VDC_ERRNO, vdc->alloc_errno);
            lc_rm_db_syslog("Create VGATEWAY", LC_SYSLOG_OBJECTTYPE_VGATEWAY,
                            vdc->id, "vgateway", "no resource",
                            LC_SYSLOG_LEVEL_ERR);
            updatedb(TABLE_ALLOC_VDC, set, where);

            free(vdc);
            SLIST_REMOVE_HEAD(&vdchead, chain);
            free(vdcnode);
        }
    }

    alloc_free_cr(&vdccrh);
    RM_LOG(LOG_DEBUG, "%s End", __FUNCTION__);
}

void alloc_vm_modify(msg_vm_modify_t *vm_modify)
{
    int  ret = 0;
    char set[LC_NAMEDES];
    char where[LC_NAMEDES];
    char sys_conf_value[TABLE_ALLOC_SYS_CONF_VALUE_LEN];
    double vcpu_over_alloc_ratio;
    int vcpu_over_alloc_flag= 0;
    alloc_vm_t      *vm     = NULL;
    alloc_node_t    *vmnode = NULL;
    alloc_head_t     vmhead = {NULL};
    alloc_head_t     vmcrh  = {NULL};
    alloc_pool_t     pool;
    alloc_vdisk_t    vdisk;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vmhead);
    SLIST_INIT(&vmcrh);

    /* Load vm from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm_modify->vm_id);
    db2allocvm((void*)&vmhead, ALLOC_DB_FLAG_SLIST, where);

    if (SLIST_EMPTY(&vmhead)) {
        RM_LOG(LOG_DEBUG, "%s error End", __FUNCTION__);
        lc_rm_db_syslog("Modify VM", LC_SYSLOG_OBJECTTYPE_VM, vm_modify->vm_id,
                        "vm", "invalid request msg", LC_SYSLOG_LEVEL_ERR);
        return;
    }

    /* Load vm resource from db */
    vmnode = SLIST_FIRST(&vmhead);
    vm = (alloc_vm_t *)(vmnode->element);

    /* Fill usr_sr_name */
    if (0 == vm->usr_vdi_size) {
        strcpy(vm->usr_sr_name, vm->sys_sr_name);
    }

    /* Modify vm_info to resource allocation */
    if (0 <= (int)(vm_modify->usr_disk_size - vm->usr_vdi_size)) {
        vm->usr_vdi_size = vm_modify->usr_disk_size - vm->usr_vdi_size;
    } else {
        vm->usr_vdi_size = 0;
    }
    if (0 <= (int)(vm_modify->mem_size - vm->mem_size)) {
        vm->mem_size = vm_modify->mem_size - vm->mem_size;
    } else {
        vm->mem_size = 0;
    }
    if (0 <= (int)(vm_modify->vcpu_num - vm->cpu_num)) {
        vm->cpu_num  = vm_modify->vcpu_num - vm->cpu_num;
    } else {
        vm->cpu_num  = 0;
    }
    vm->sys_vdi_size = 0;

    /* Load vm_pool from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s'",
             TABLE_ALLOC_SYS_CONF_PARAM, "vcpu_over_allocation_ratio");
    memset(sys_conf_value, 0, TABLE_ALLOC_SYS_CONF_VALUE_LEN);
    db2sysconf((void*)sys_conf_value, where);
    if (strlen(sys_conf_value)) {
        vcpu_over_alloc_ratio = atof(sys_conf_value);
    } else {
        vcpu_over_alloc_ratio = 0;
    }
    if (vcpu_over_alloc_ratio < 0.1) {
        vcpu_over_alloc_flag = LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION;
    }

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
             TABLE_ALLOC_POOL_ID, vm->pool_id,
             TABLE_ALLOC_POOL_DOMIAN, vm->domain);
    memset(&pool, 0, sizeof(alloc_pool_t));
    db2allocpool((void*)&pool, ALLOC_DB_FLAG_ELEMENT, where);

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s='%s'",
             TABLE_ALLOC_CR_IP, vm->server,
             TABLE_ALLOC_CR_DOMAIN, vm->domain);
    db2alloccr((void*)&vmcrh, ALLOC_DB_FLAG_SLIST, where, pool.poolctype, pool.pooliptype);
    if (SLIST_EMPTY(&vmcrh)) {
        RM_LOG(LOG_ERR, "No cr for vm");
    } else {
        alloc_get_sr(&vmcrh, vcpu_over_alloc_ratio);
    }

    /* Resource check */
    if (!SLIST_EMPTY(&vmhead)) {
        ret = lc_check_vm(&vmhead, &vmcrh, NULL, NULL, vcpu_over_alloc_flag);
    } else {
        RM_LOG(LOG_ERR, "check fails");
    }

    /* Update vm db */
    vmnode = SLIST_FIRST(&vmhead);
    vm = (alloc_vm_t *)(vmnode->element);

    if ((0 == vm->pool_id) || (0 == vm->server[0])) {
        alloc_update_vm_errno(vm);
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm->id);
        memset(set, 0, LC_NAMEDES);
        snprintf(set, LC_NAMEDES, "%s=%u", TABLE_ALLOC_VM_ERRNO, vm->alloc_errno);
        lc_rm_db_syslog("modify VM", LC_SYSLOG_OBJECTTYPE_VM, vm->id,
                        "vm", "no resource", LC_SYSLOG_LEVEL_ERR);
        updatedb(TABLE_ALLOC_VM, set, where);
    } else {
        /* HA disk check */
        memset(where, 0, LC_NAMEDES);
        snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDISK_VMID, vm->id);
        memset(&vdisk, 0, sizeof(alloc_vdisk_t));
        db2allocvdisk(&vdisk, where);

        if (0 < (int)(vm_modify->ha_disk_size - vdisk.vdisize)) {
            vdisk.vdisize = vm_modify->ha_disk_size - vdisk.vdisize;
            if (alloc_check_vdisk(vm, &vdisk)) {
                memset(where, 0, LC_NAMEDES);
                snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm->id);
                memset(set, 0, LC_NAMEDES);
                snprintf(set, LC_NAMEDES, "%s=%u",
                         TABLE_ALLOC_VM_ERRNO, TABLE_ALLOC_VM_ERR_NO_VDISK);
                lc_rm_db_syslog("Modify VM", LC_SYSLOG_OBJECTTYPE_VM, vm->id,
                                "vm", "no ha disk", LC_SYSLOG_LEVEL_ERR);
                updatedb(TABLE_ALLOC_VM, set, where);
            } else {
                memset(where, 0, LC_NAMEDES);
                snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VDISK_VMID, vm->id);
                memset(set, 0, LC_NAMEDES);
                snprintf(set, LC_NAMEDES, "%s='%s'",
                         TABLE_ALLOC_VDISK_SR_NAME, vdisk.srname);
                updatedb(TABLE_ALLOC_VDISK, set, where);
            }
        }
    }

    while (!SLIST_EMPTY(&vmhead)) {
        vmnode = SLIST_FIRST(&vmhead);
        vm = (alloc_vm_t *)(vmnode->element);
        free(vm);
        SLIST_REMOVE_HEAD(&vmhead, chain);
        free(vmnode);
    }

    alloc_free_cr(&vmcrh);
    RM_LOG(LOG_DEBUG, "%s End\n", __FUNCTION__);
}

int alloc_vm_snapshot(msg_vm_snapshot_t *vm_snapshot)
{
    int  ret = 0;
    char where[LC_NAMEDES];
    alloc_vm_t      *vm     = NULL;
    alloc_node_t    *vmnode = NULL;
    alloc_head_t     vmhead = {NULL};
    alloc_head_t     vmcrh  = {NULL};
    alloc_pool_t     pool;

    RM_LOG(LOG_DEBUG, "%s Enter", __FUNCTION__);

    /* Resource list init */
    SLIST_INIT(&vmhead);
    SLIST_INIT(&vmcrh);

    /* Load vm from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d", TABLE_ALLOC_VM_ID, vm_snapshot->vm_id);
    db2allocvm((void*)&vmhead, ALLOC_DB_FLAG_SLIST, where);

    if (SLIST_EMPTY(&vmhead)) {
        RM_LOG(LOG_DEBUG, "%s error End", __FUNCTION__);
        lc_rm_db_syslog("snapshot VM", LC_SYSLOG_OBJECTTYPE_VM, vm_snapshot->vm_id,
                        "vm", "invalid request msg", LC_SYSLOG_LEVEL_ERR);
        return -1;
    }

    /* Load vm resource from db */
    vmnode = SLIST_FIRST(&vmhead);
    vm = (alloc_vm_t *)(vmnode->element);

    /* Modify vm_info to resource allocation */
    vm->mem_size = 0;
    vm->cpu_num  = 0;

    /* Load vm_pool from db */
    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s=%d and %s='%s'",
             TABLE_ALLOC_POOL_ID, vm->pool_id,
             TABLE_ALLOC_POOL_DOMIAN, vm->domain);
    memset(&pool, 0, sizeof(alloc_pool_t));
    db2allocpool((void*)&pool, ALLOC_DB_FLAG_ELEMENT, where);

    memset(where, 0, LC_NAMEDES);
    snprintf(where, LC_NAMEDES, "%s='%s' and %s='%s'",
             TABLE_ALLOC_CR_IP, vm->server,
             TABLE_ALLOC_CR_DOMAIN, vm->domain);
    db2alloccr((void*)&vmcrh, ALLOC_DB_FLAG_SLIST, where, pool.poolctype, pool.pooliptype);
    if (SLIST_EMPTY(&vmcrh)) {
        RM_LOG(LOG_ERR, "No cr for vm");
    } else {
        alloc_get_sr(&vmcrh, 1);
    }

    /* Resource check */
    if (!SLIST_EMPTY(&vmhead)) {
        ret = lc_check_vm(&vmhead, &vmcrh, NULL, NULL,
                LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION);
    } else {
        RM_LOG(LOG_ERR, "check fails");
    }

    /* Update vm db */
    vmnode = SLIST_FIRST(&vmhead);
    vm = (alloc_vm_t *)(vmnode->element);

    if ((0 == vm->pool_id) || (0 == vm->server[0])) {
        lc_rm_db_syslog("snapshot VM", LC_SYSLOG_OBJECTTYPE_VM, vm->id,
                        "vm", "no resource", LC_SYSLOG_LEVEL_ERR);
        ret = -1;
    }

    while (!SLIST_EMPTY(&vmhead)) {
        vmnode = SLIST_FIRST(&vmhead);
        vm = (alloc_vm_t *)(vmnode->element);
        free(vm);
        SLIST_REMOVE_HEAD(&vmhead, chain);
        free(vmnode);
    }

    alloc_free_cr(&vmcrh);
    RM_LOG(LOG_DEBUG, "%s End\n", __FUNCTION__);
    return ret;
}
