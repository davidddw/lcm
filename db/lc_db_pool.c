#include <string.h>
#include <stdlib.h>

#include "vm/vm_host.h"
#include "mysql.h"
#include "lc_db.h"

extern MYSQL *conn_handle;

static int pool_process(void *pool_info, char *field, char *value)
{
    rpool_t* pool = (rpool_t*) pool_info;

    if (strcmp(field, "id") == 0) {
        pool->rpool_index = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(pool->rpool_name, value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(pool->rpool_desc, value);
    } else if (strcmp(field, "ctype") == 0) {
        pool->ctype = atoi(value);
    } else if (strcmp(field, "stype") == 0) {
        pool->stype = atoi(value);
    } else if (strcmp(field, "master") == 0) {
        strcpy(pool->master_address, value);
    }

    return 0;
}

static int pool_ctype_process(void *p, char *field, char *value)
{
    int *type = (int *)p;

    if (strcmp(field, "ctype") == 0) {
        *type = atoi(value);
    }

    return 0;
}

int lc_db_pool_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("pool", column_list, value_list);
}

int lc_db_pool_delete(char *condition)
{
    return lc_db_table_delete("pool", condition);
}

int lc_db_pool_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("pool", column, value, condition);
}

int lc_db_pool_dump(char *column_list)
{
    return lc_db_table_dump("pool", column_list);
}

int lc_db_pool_load(rpool_t* pool, char *column_list, char *condition)
{
    return lc_db_table_load(pool, "pool", column_list, condition, pool_process);
}

int lc_db_pool_load_all(rpool_t* pool, char *column_list, char *condition,
        int offset, int *pool_num)
{
    return lc_db_table_load_all(pool, "pool", column_list, condition, offset,
            pool_num, pool_process);
}

int lc_db_pool_load_all_v2(rpool_t* pool, char *column_list, char *condition,
        int offset, int *pool_num)
{
    return lc_db_table_loadn_all(pool, "pool", column_list, condition, offset,
            pool_num, MAX_RPOOL_NUM, pool_process);
}

int lc_vmdb_rpool_insert (rpool_t *pool)
{
    char db_buffer[MAX_POOL_DB_BUFFER];

    memset(db_buffer, 0, MAX_POOL_DB_BUFFER);
    sprintf(db_buffer, "'%d','%s','%s','%d'",
        pool->rpool_index, pool->rpool_name, pool->rpool_desc, pool->rpool_flag);

    if (lc_db_pool_insert(LC_DB_POOL_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}


int lc_vmdb_rpool_delete (uint32_t pool_index)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    sprintf(db_buffer, "id=%d", pool_index);

    if (lc_db_pool_delete(db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%serror\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_pool_update_name (char *pool_name, uint32_t pool_index)
{
    char condition[LC_DB_BUF_LEN];
    char name[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(name, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", pool_index);
    sprintf(name, "'%s'", pool_name);

    if (lc_db_pool_update("name", name, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_pool_update_desc (char *pool_desc, uint32_t pool_index)
{
    char condition[LC_DB_BUF_LEN];
    char desc[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(desc, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", pool_index);
    sprintf(desc, "'%s'", pool_desc);

    if (lc_db_pool_update("description", desc, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_pool_update_master_ip (char *master_ip, uint32_t pool_index)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", pool_index);
    sprintf(buffer, "'%s'", master_ip);

    if (lc_db_pool_update("master_ip", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_pool_load (rpool_t *pool, uint32_t pool_index)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", pool_index);

    result = lc_db_pool_load(pool, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_POOL_NOT_FOUND;
    } else {
        return LC_DB_POOL_FOUND;
    }
}

int lc_vmdb_pool_load_all_v2_by_type (rpool_t *pool, int offset, int *pool_num,
        int type)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "type=%d", type);

    result = lc_db_pool_load_all_v2(pool, "*", condition, offset, pool_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_POOL_NOT_FOUND;
    } else {
        return LC_DB_POOL_FOUND;
    }
}

int lc_vmdb_pool_load_all_v2_by_type_domain (rpool_t *pool, int offset, int *pool_num,
                                             int type, char *domain)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "type=%d and domain='%s'", type, domain);

    result = lc_db_pool_load_all_v2(pool, "*", condition, offset, pool_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_POOL_NOT_FOUND;
    } else {
        return LC_DB_POOL_FOUND;
    }
}

int lc_vmdb_check_pool_exist (uint32_t pool_index)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", pool_index);

    result = lc_db_pool_load(NULL, "id", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_POOL_NOT_FOUND;
    } else {
        return LC_DB_POOL_FOUND;
    }
}

int lc_vmdb_get_pool_htype(uint32_t poolid)
{
    int type = 0;
    char cond[LC_DB_BUF_LEN];

    memset(cond, 0, LC_DB_BUF_LEN);
    sprintf(cond, "id=%d", poolid);

    lc_db_table_load(&type, "pool", "ctype", cond, pool_ctype_process);

    return type;
}
