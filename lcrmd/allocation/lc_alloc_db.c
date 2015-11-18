#include <string.h>
#include "mysql.h"
#include "lc_rm.h"
#include "lc_db.h"
#include "lc_alloc_db.h"

extern struct thread_mysql multi_handle[MAX_THREAD];

#define  TABLE_ALLOC_POOL_SQL     "select %s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_CR_SQL       "select %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_XEN_SR_CON_SQL     "select %s from %s_v%s where %s;"

#define  TABLE_XEN_HASR_CON_SQL   "select %s from %s_v%s where %s;"

#define  TABLE_XEN_SR_SQL         "select %s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_XEN_HASR_SQL       "select %s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_IR_SQL       "select %s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_IP_RES_SQL   "select %s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_VDC_SQL      "select %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_VNET_SQL     "select %s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_VDISK_SQL    "select %s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_VM_SQL       "select %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s from %s_v%s where %s;"

#define  TABLE_ALLOC_VIF_SQL      "select %s,%s from %s_v%s where %s;"

#define  TABLE_SYS_CONF_SQL       "select %s from %s_v%s where %s;"

#define  ALLOC_FIELDS_LEN         512

int updatedb(char* table, char* set, char* where)
{
    pthread_t tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, "update %s_v%s set %s where %s",
             table, db_cfg.version, set, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            return -1;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return -1;
        }
    }
    return 0;
}

int db2allocpool(void* object, int flag, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    struct alloc_poolh *poolh = NULL;
    alloc_pool_t       *pool  = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_POOL_SQL,
             TABLE_ALLOC_POOL_ID, TABLE_ALLOC_POOL_CTYPE, TABLE_ALLOC_POOL_IPTYPE,
             TABLE_ALLOC_POOL_DOMIAN,
             TABLE_ALLOC_POOL, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    if (flag & ALLOC_DB_FLAG_ELEMENT) {
        pool = (alloc_pool_t*)object;
        memset(pool, 0, sizeof(alloc_pool_t));
        while((row = mysql_fetch_row(result))) {

            if (NULL != row[0]) {
                pool->poolid    = atoi(row[0]);
            }
            if (NULL != row[1]) {
                pool->poolctype = atoi(row[1]);
            }
            if (NULL != row[2]) {
                pool->pooliptype = atoi(row[2]);
            }
            if (NULL != row[3]) {
                strncpy(pool->domain, row[3], LC_DOMAIN_LEN - 1);
            }
        }
    }
    if (flag & ALLOC_DB_FLAG_SLIST) {
        poolh = (struct alloc_poolh*)object;
        while((row = mysql_fetch_row(result))) {

            pool = malloc(sizeof(alloc_pool_t));
            if (NULL == pool) {
                RM_LOG(LOG_ERR, "malloc fails\n");
                continue;
            }
            memset(pool, 0, sizeof(alloc_pool_t));

            if (NULL != row[0]) {
                pool->poolid    = atoi(row[0]);
            }
            if (NULL != row[1]) {
                pool->poolctype = atoi(row[1]);
            }
            if (NULL != row[2]) {
                pool->pooliptype = atoi(row[2]);
            }
            if (NULL != row[3]) {
                strncpy(pool->domain, row[3], LC_DOMAIN_LEN - 1);
            }
            SLIST_INSERT_HEAD(poolh, pool, chain);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2alloccr(void* object, int flag, char* where, int htype, int iptype)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_head_t *alloch     = NULL;
    alloc_node_t *alloc_node = NULL;
    alloc_cr_t   *alloc_cr   = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_CR_SQL, TABLE_ALLOC_CR_ID,
        TABLE_ALLOC_CR_CPU_TOTAL, TABLE_ALLOC_CR_CPU_USED, TABLE_ALLOC_CR_CPU_ALLOC,
        TABLE_ALLOC_CR_MEM_TOTAL, TABLE_ALLOC_CR_MEM_USED, TABLE_ALLOC_CR_MEM_ALLOC,
        TABLE_ALLOC_CR_IP, TABLE_ALLOC_CR_POOLID, TABLE_ALLOC_CR_RACKID, TABLE_ALLOC_CR_DOMAIN,
        TABLE_ALLOC_CR, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    if (flag & ALLOC_DB_FLAG_ELEMENT) {
        alloc_cr = (alloc_cr_t *)object;
        memset(alloc_cr, 0, sizeof(alloc_cr_t));

        while((row = mysql_fetch_row(result))) {
            if (NULL != row[0]) {
                alloc_cr->id        = atoi(row[0]);
            }
            if (NULL != row[1]) {
                alloc_cr->cpu_total = atoi(row[1]);
            }
            if (NULL != row[2]) {
                alloc_cr->cpu_used  = atoi(row[2]);
            }
            if (NULL != row[3]) {
                alloc_cr->cpu_used += atoi(row[3]);
            }
            if (NULL != row[4]) {
                alloc_cr->mem_total = atoi(row[4]);
            }
            if (NULL != row[5]) {
                alloc_cr->mem_used  = atoi(row[5]);
            }
            if (NULL != row[6]) {
                alloc_cr->mem_used += atoi(row[6]);
            }
            if (NULL != row[7]) {
                strncpy(alloc_cr->address, row[7], LC_IPV4_LEN - 1);
            }
            if (NULL != row[8]) {
                alloc_cr->pool_id   = atoi(row[8]);
            }
            if (NULL != row[9]) {
                alloc_cr->rack_id   = atoi(row[9]);
            }
            if (NULL != row[10]) {
                strncpy(alloc_cr->domain, row[10], LC_DOMAIN_LEN - 1);
            }
            alloc_cr->pool_htype   = htype;
            if (TABLE_ALLOC_POOL_IPTYPE_GLOBAL == iptype) {
                alloc_cr->pool_ip_type = LC_POOL_IP_TYPE_GLOBAL;
            } else if (TABLE_ALLOC_POOL_IPTYPE_EXCLUSIVE == iptype) {
                alloc_cr->pool_ip_type = LC_POOL_IP_TYPE_EXCLUSIVE;
            }
        }
    }
    if (flag & ALLOC_DB_FLAG_SLIST) {
        alloch = (alloc_head_t *)object;

        while((row = mysql_fetch_row(result))) {

            alloc_node = malloc(sizeof(alloc_node_t));
            if (NULL == alloc_node) {
                RM_LOG(LOG_ERR, "malloc fails\n");
                continue;
            }
            memset(alloc_node, 0, sizeof(alloc_node_t));

            alloc_cr = malloc(sizeof(alloc_cr_t));
            if (NULL == alloc_cr) {
                RM_LOG(LOG_ERR, "malloc fails\n");
                free(alloc_node);
                continue;
            }
            memset(alloc_cr, 0, sizeof(alloc_cr_t));
            alloc_node->element = (void *)alloc_cr;

            if (NULL != row[0]) {
                alloc_cr->id        = atoi(row[0]);
            }
            if (NULL != row[1]) {
                alloc_cr->cpu_total = atoi(row[1]);
            }
            if (NULL != row[2]) {
                alloc_cr->cpu_used  = atoi(row[2]);
            }
            if (NULL != row[3]) {
                alloc_cr->cpu_used += atoi(row[3]);
            }
            if (NULL != row[4]) {
                alloc_cr->mem_total = atoi(row[4]);
            }
            if (NULL != row[5]) {
                alloc_cr->mem_used  = atoi(row[5]);
            }
            if (NULL != row[6]) {
                alloc_cr->mem_used += atoi(row[6]);
            }
            if (NULL != row[7]) {
                strncpy(alloc_cr->address, row[7], LC_IPV4_LEN - 1);
            }
            if (NULL != row[8]) {
                alloc_cr->pool_id   = atoi(row[8]);
            }
            if (NULL != row[9]) {
                alloc_cr->rack_id   = atoi(row[9]);
            }
            if (NULL != row[10]) {
                strncpy(alloc_cr->domain, row[10], LC_DOMAIN_LEN - 1);
            }
            alloc_cr->pool_htype   = htype;
            if (TABLE_ALLOC_POOL_IPTYPE_GLOBAL == iptype) {
                alloc_cr->pool_ip_type = LC_POOL_IP_TYPE_GLOBAL;
            } else if (TABLE_ALLOC_POOL_IPTYPE_EXCLUSIVE == iptype) {
                alloc_cr->pool_ip_type = LC_POOL_IP_TYPE_EXCLUSIVE;
            }

            SLIST_INSERT_HEAD(alloch, alloc_node, chain);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocsrcon(void* object, int flag, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_sr_con_t *sr_con = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_XEN_SR_CON_SQL,
             TABLE_ALLOC_CON_SR_SRID, TABLE_ALLOC_CON_SR, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    if (flag & ALLOC_DB_FLAG_ELEMENT) {
        sr_con = (alloc_sr_con_t *)object;
        memset(sr_con, 0, sizeof(alloc_sr_con_t));
        while((row = mysql_fetch_row(result))) {
            if (NULL != row[0]) {
                sr_con->srid = atoi(row[0]);
            }
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allochasrcon(void* object, int flag, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_sr_con_t *sr_con = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_XEN_HASR_CON_SQL,
             TABLE_ALLOC_CON_HASR_SRID, TABLE_ALLOC_CON_HASR, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    if (flag & ALLOC_DB_FLAG_ELEMENT) {
        sr_con = (alloc_sr_con_t *)object;
        memset(sr_con, 0, sizeof(alloc_sr_con_t));
        while((row = mysql_fetch_row(result))) {
            if (NULL != row[0]) {
                sr_con->srid = atoi(row[0]);
            }
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocsr(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_node_t *alloc_node = NULL;
    alloc_sr_t   *alloc_sr   = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_XEN_SR_SQL,
             TABLE_ALLOC_SR_DISK_TOTAL,TABLE_ALLOC_SR_DISK_USED,
             TABLE_ALLOC_SR_DISK_ALLOC, TABLE_ALLOC_SR_NAME,
             TABLE_ALLOC_SR, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    alloc_head_t *alloch = (alloc_head_t *)object;
    while((row = mysql_fetch_row(result))) {

        alloc_node = malloc(sizeof(alloc_node_t));
        if (NULL == alloc_node) {
            RM_LOG(LOG_ERR, "malloc fails\n");
            continue;
        }
        memset(alloc_node, 0, sizeof(alloc_node_t));

        alloc_sr = malloc(sizeof(alloc_sr_t));
        if (NULL == alloc_sr) {
            RM_LOG(LOG_ERR, "malloc fails\n");
            free(alloc_node);
            continue;
        }
        memset(alloc_sr, 0, sizeof(alloc_sr_t));
        alloc_node->element = (void *)alloc_sr;

        if (NULL != row[0]) {
            alloc_sr->disk_total = atoi(row[0]);
        }
        if (NULL != row[1]) {
            alloc_sr->disk_used  = atoi(row[1]);
        }
        if (NULL != row[2]) {
            alloc_sr->disk_used += atoi(row[2]);
        }
        if (NULL != row[3]) {
            strncpy(alloc_sr->name_label, row[3], LC_NAMESIZE - 1);
        }
        if (NULL != row[4]) {
            alloc_sr->storage_type = atoi(row[4]);
            if (0 == alloc_sr->storage_type) {
                alloc_sr->storage_type = LC_STORAGE_TYPE_LOCAL;
            } else {
                alloc_sr->storage_type = LC_STORAGE_TYPE_SHARED;
            }
        }
        SLIST_INSERT_HEAD(alloch, alloc_node, chain);
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allochasr(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_sr_t *sr = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_XEN_HASR_SQL,
             TABLE_ALLOC_HASR_DISK_TOTAL,TABLE_ALLOC_HASR_DISK_USED,
             TABLE_ALLOC_HASR_DISK_ALLOC, TABLE_ALLOC_HASR_NAME,
             TABLE_ALLOC_HASR, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    sr = (alloc_sr_t *)object;
    memset(sr, 0, sizeof(alloc_sr_t));
    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            sr->disk_total = atoi(row[0]);
        }
        if (NULL != row[1]) {
            sr->disk_used  = atoi(row[1]);
        }
        if (NULL != row[2]) {
            sr->disk_used += atoi(row[2]);
        }
        if (NULL != row[3]) {
            strncpy(sr->name_label, row[3], LC_NAMESIZE - 1);
        }
        if (NULL != row[4]) {
            sr->storage_type = atoi(row[4]);
            if (0 == sr->storage_type) {
                sr->storage_type = LC_STORAGE_TYPE_LOCAL;
            } else {
                sr->storage_type = LC_STORAGE_TYPE_SHARED;
            }
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocvdc(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_vdc_t* alloc_vdc = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_VDC_SQL,
             TABLE_ALLOC_VDC_ID, TABLE_ALLOC_VDC_SYS_SR_NAME,
             TABLE_ALLOC_VDC_PUBIF1_IPNUM, TABLE_ALLOC_VDC_PUBIF2_IPNUM,
             TABLE_ALLOC_VDC_PUBIF3_IPNUM, TABLE_ALLOC_VDC_PUBIF1_ISP,
             TABLE_ALLOC_VDC_PUBIF2_ISP, TABLE_ALLOC_VDC_PUBIF3_ISP,
             TABLE_ALLOC_VDC_EX_SERVER, TABLE_ALLOC_VDC_SERVER,
             TABLE_ALLOC_VDC_POOLID, TABLE_ALLOC_VDC_DOMAIN,
             TABLE_ALLOC_VDC, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    alloc_vdc = (alloc_vdc_t *)object;
    memset(alloc_vdc, 0, sizeof(alloc_vdc_t));
    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            alloc_vdc->id        = atoi(row[0]);
        }
        if (NULL != row[1]) {
            strncpy(alloc_vdc->sys_sr_name, row[1], LC_NAMESIZE - 1);
        }
        if (NULL != row[2]) {
            alloc_vdc->ip_num[0] = atoi(row[2]);
        }
        if (NULL != row[3]) {
            alloc_vdc->ip_num[1] = atoi(row[3]);
        }
        if (NULL != row[4]) {
            alloc_vdc->ip_num[2] = atoi(row[4]);
        }
        if (NULL != row[5]) {
            alloc_vdc->ip_isp[0] = atoi(row[5]);
        }
        if (NULL != row[6]) {
            alloc_vdc->ip_isp[1] = atoi(row[6]);
        }
        if (NULL != row[7]) {
            alloc_vdc->ip_isp[2] = atoi(row[7]);
        }
        if (NULL != row[8]) {
            strncpy(alloc_vdc->excluded_server, row[8], LC_DB_EX_HOST_LEN - 1);
        }
        if (NULL != row[9]) {
            strncpy(alloc_vdc->server, row[9], LC_IPV4_LEN - 1);
        }
        if (NULL != row[10]) {
            alloc_vdc->pool_id   = atoi(row[10]);
        }
        if (NULL != row[11]) {
            strncpy(alloc_vdc->domain, row[11], LC_DOMAIN_LEN - 1);
        }
        alloc_vdc->cpu_num      = TABLE_ALLOC_VDC_CPU_NUM;
        alloc_vdc->mem_size     = TABLE_ALLOC_VDC_MEM_SIZE;
        alloc_vdc->sys_vdi_size = TABLE_ALLOC_VDC_DISK_SIZE;
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocvm(void* object, int flag, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_node_t *alloc_node = NULL;
    alloc_vm_t   *alloc_vm   = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_VM_SQL,
             TABLE_ALLOC_VM_ID, TABLE_ALLOC_VM_CPU_NUM, TABLE_ALLOC_VM_MEM_SIZE,
             TABLE_ALLOC_VM_SYS_VDI_SIZE, TABLE_ALLOC_VM_USR_VDI_SIZE,
             TABLE_ALLOC_VM_SYS_SR_NAME, TABLE_ALLOC_VM_USR_SR_NAME,
             TABLE_ALLOC_VM_EX_SERVER, TABLE_ALLOC_VM_SERVER,
             TABLE_ALLOC_VM_POOLID, TABLE_ALLOC_VM_DOMAIN,
             TABLE_ALLOC_VM, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

     if (flag & ALLOC_DB_FLAG_ELEMENT) {
        alloc_vm = (alloc_vm_t *)object;
        memset(alloc_vm, 0, sizeof(alloc_vm_t));

        while((row = mysql_fetch_row(result))) {

            if (NULL != row[0]) {
                alloc_vm->id           = atoi(row[0]);
            }
            if (NULL != row[1]) {
                alloc_vm->cpu_num      = atoi(row[1]);
            }
            if (NULL != row[2]) {
                alloc_vm->mem_size     = atoi(row[2]);
            }
            if (NULL != row[3]) {
                alloc_vm->sys_vdi_size = atoi(row[3]);
            }
            if (NULL != row[4]) {
                alloc_vm->usr_vdi_size = atoi(row[4]);
            }
            if (NULL != row[5]) {
                strncpy(alloc_vm->sys_sr_name, row[5], LC_NAMESIZE - 1);
            }
            if (NULL != row[6]) {
                strncpy(alloc_vm->usr_sr_name, row[6], LC_NAMESIZE - 1);
            }
            if (NULL != row[7]) {
                strncpy(alloc_vm->excluded_server, row[7], LC_EXCLUDED_SERVER_LIST_LEN - 1);
            }
            if (NULL != row[8]) {
                strncpy(alloc_vm->server, row[8], LC_IPV4_LEN - 1);
            }
            if (NULL != row[9]) {
                alloc_vm->pool_id = atoi(row[9]);
            }
            if (NULL != row[10]) {
                strncpy(alloc_vm->domain, row[10], LC_DOMAIN_LEN - 1);
            }
        }
    }

    if (flag & ALLOC_DB_FLAG_SLIST) {
        alloc_head_t *alloch = (alloc_head_t *)object;
        while((row = mysql_fetch_row(result))) {

            alloc_node = malloc(sizeof(alloc_node_t));
            if (NULL == alloc_node) {
                RM_LOG(LOG_ERR, "malloc fails\n");
                continue;
            }
            memset(alloc_node, 0, sizeof(alloc_node_t));
            alloc_vm = malloc(sizeof(alloc_vm_t));
            if (NULL == alloc_vm) {
                RM_LOG(LOG_ERR, "malloc fails\n");
                free(alloc_node);
                continue;
            }
            memset(alloc_vm, 0, sizeof(alloc_vm_t));
            alloc_node->element = (void *)alloc_vm;

            if (NULL != row[0]) {
                alloc_vm->id           = atoi(row[0]);
            }
            if (NULL != row[1]) {
                alloc_vm->cpu_num      = atoi(row[1]);
            }
            if (NULL != row[2]) {
                alloc_vm->mem_size     = atoi(row[2]);
            }
            if (NULL != row[3]) {
                alloc_vm->sys_vdi_size = atoi(row[3]);
            }
            if (NULL != row[4]) {
                alloc_vm->usr_vdi_size = atoi(row[4]);
            }
            if (NULL != row[5]) {
                strncpy(alloc_vm->sys_sr_name, row[5], LC_NAMESIZE - 1);
            }
            if (NULL != row[6]) {
                strncpy(alloc_vm->usr_sr_name, row[6], LC_NAMESIZE - 1);
            }
            if (NULL != row[7]) {
                strncpy(alloc_vm->excluded_server, row[7], LC_EXCLUDED_SERVER_LIST_LEN - 1);
            }
            if (NULL != row[8]) {
                strncpy(alloc_vm->server, row[8], LC_IPV4_LEN - 1);
            }
            if (NULL != row[9]) {
                alloc_vm->pool_id = atoi(row[9]);
            }
            if (NULL != row[10]) {
                strncpy(alloc_vm->domain, row[10], LC_DOMAIN_LEN - 1);
            }

            SLIST_INSERT_HEAD(alloch, alloc_node, chain);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocvif(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_vif_t *vif = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_VIF_SQL,
             TABLE_ALLOC_VIF_ID, TABLE_ALLOC_VIF_IFINDEX,
             TABLE_ALLOC_VIF, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    vif = (alloc_vif_t*)object;
    memset(vif, 0, sizeof(alloc_vif_t));
    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            vif->vifid      = atoi(row[0]);
        }
        if (NULL != row[1]) {
            vif->ifindex = atoi(row[1]);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocvnet(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_vnet_t *alloc_vnet = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_VNET_SQL,
             TABLE_ALLOC_VNET_ID, TABLE_ALLOC_VNET_VDCID,
             TABLE_ALLOC_VNET_HA_FLAG, TABLE_ALLOC_VNET_SERVER,
             TABLE_ALLOC_VNET, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    alloc_vnet = (alloc_vnet_t*)object;
    memset(alloc_vnet, 0, sizeof(alloc_vnet_t));
    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            alloc_vnet->vnetid = atoi(row[0]);
        }
        if (NULL != row[1]) {
            alloc_vnet->vdcid  = atoi(row[1]);
        }
        if (NULL != row[2]) {
            alloc_vnet->haflag = atoi(row[2]);
        }
        if (NULL != row[3]) {
            strncpy(alloc_vnet->server, row[3], LC_IPV4_LEN - 1);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2allocvdisk(void* object, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];
    alloc_vdisk_t *alloc_vdisk = NULL;

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!object) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_ALLOC_VDISK_SQL,
             TABLE_ALLOC_VDISK_SIZE, TABLE_ALLOC_VDISK_SR_NAME,
             TABLE_ALLOC_VDISK, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    alloc_vdisk = (alloc_vdisk_t*)object;
    memset(alloc_vdisk, 0, sizeof(alloc_vdisk_t));
    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            alloc_vdisk->vdisize = atoi(row[0]);
        }
        if (NULL != row[1]) {
            strncpy(alloc_vdisk->srname, row[1], LC_NAMESIZE- 1);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}

int db2sysconf(char* value, char* where)
{
    MYSQL_RES *result;
    MYSQL_ROW  row;
    int        ret = ALLOC_DB_OK;
    pthread_t  tid = pthread_self();
    char sql_query[ALLOC_FIELDS_LEN];

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (!value) {
        RM_LOG(LOG_ERR, "Input error\n");
        return -1;
    }

    snprintf(sql_query, ALLOC_FIELDS_LEN, TABLE_SYS_CONF_SQL,
             TABLE_ALLOC_SYS_CONF_VALUE, TABLE_ALLOC_SYS_CONF, db_cfg.version, where);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            RM_LOG(LOG_ERR, "Failed to ping the database");
            return ALLOC_DB_COMMON_ERROR;
        }
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return ALLOC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        RM_LOG(LOG_ERR, "ERROR (DB) :mysql_store_result(%s) failed\n", sql_query);
        return -1;
    }

    while((row = mysql_fetch_row(result))) {

        if (NULL != row[0]) {
            strncpy(value, row[0], TABLE_ALLOC_SYS_CONF_VALUE_LEN - 1);
        }
    }

    if (mysql_num_rows(result) == 0) {
        ret = ALLOC_DB_EMPTY_SET;
    }
    mysql_free_result(result);
    return ret;
}
