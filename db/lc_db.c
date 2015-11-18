#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "mysql.h"
#include "lc_db.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/ipc.h>
#include <sys/shm.h>
/* database configuration */
struct db_config db_cfg;
struct db_config db_master_cfg;
uint32_t db_vm_ctrl_ip_min;
uint32_t db_vm_ctrl_ip_max;
uint32_t db_vm_ctrl_ip_mask;
int DB_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *DB_LOG_LEVEL_P = &DB_LOG_DEFAULT_LEVEL;
#define LOG_LEVEL_HEADER        "LOG_LEVEL: "
char* DBLOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};
struct thread_mysql multi_handle[MAX_THREAD];
struct thread_mysql multi_master_handle[MAX_THREAD];

static pthread_mutex_t hmutex;
static pthread_mutex_t hmutex_master;

#define MAX_WAIT_COUNT 60
int lc_db_ping_mysql(MYSQL *conn_handle)
{
#if 0
    int ret = 0;
    static int wait_count = 0;

    ret = mysql_ping(conn_handle);
    while (ret != 0 /* && (wait_count < MAX_WAIT_COUNT)*/) {
        ret = mysql_ping(conn_handle);
        wait_count ++;
    }
    LC_DB_LOG(LOG_DEBUG, "Ping mysql result[%d, %d]", ret, wait_count);
    if (ret /*&& wait_count*/) {
        wait_count = 0;
        return -1;
    }
    wait_count = 0;
#endif
    if (mysql_ping(conn_handle) != 0) {
        sleep(5);
        if (mysql_real_connect(conn_handle, db_cfg.host,
                    db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                    atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "Failed to connect to database: Error: %s",
                                  mysql_error(conn_handle));
            return -1;
        }
    }
    return 0;
}

static int get_avail_handle(pthread_t tid)
{
    int i, avail = -1;

    for (i = 0; i < MAX_THREAD; i++) {
        if (pthread_equal(multi_handle[i].tid, tid)) {
            return -1;
        }

        if (multi_handle[i].tid == 0) {
            avail = i;
        }
    }

    return avail;
}

int handle_lookup(pthread_t tid)
{
    int i;

    for (i = 0; i < MAX_THREAD; i++) {
        if (pthread_equal(multi_handle[i].tid, tid)) {
            return i;
        }
    }

    return -1;
}

int lc_db_exec_sql(char *sql_query, sql_process fp, void *traffic)
{
    int idx, ret;
    MYSQL_RES *result;
    pthread_t tid = pthread_self();

    if (sql_query == NULL) {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        return 0;
    }

    ret = fp(result, traffic);

    mysql_free_result(result);

    return ret;
}

int lc_db_table_insert(char *table_name, char *column_list,
        char *value_list)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_INSERT) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(value_list));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_INSERT, table_name, db_cfg.version,
            column_list, value_list);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_table_delete(char *table_name, char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_DELETE) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_DELETE, table_name, db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_table_update(char *table_name, char *column, char *value,
        char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_UPDATE) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column) + strlen(value) +
            strlen(condition));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_UPDATE, table_name, db_cfg.version,
            column, value, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_table_multi_update(char *table_name, char *columns_update, char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_MULTI_UPDATE) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(columns_update) +
            strlen(condition));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_MULTI_UPDATE, table_name, db_cfg.version,
            columns_update, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_table_dump(char *table_name, char *column_list)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_DUMP) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_DUMP, column_list, table_name, db_cfg.version);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        for (i = 0; i < num_field; i++) {
            printf("%s: %s\t", field[i].name, row[i] == NULL ?
                    "NULL" : row[i]);
        }
        printf("\n");
    }

    mysql_free_result(result);
    free(sql_query);

    return 0;
}

int lc_db_table_count(char *table_name, char *column, char *condition, int *count)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT_COUNT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT_COUNT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "ERROR: %s->mysql_query() failed\n", __FUNCTION__);
            free(sql_query);
            return -1;
        }

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR: %s->mysql_query() failed\n", __FUNCTION__);
            free(sql_query);
            return -1;
        }
    }
    result = mysql_store_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    while ((row = mysql_fetch_row(result))) {
        *count = atoi(row[0]);
        if (*count < 0) {
            LC_DB_LOG(LOG_WARNING,
                "WARNING: %s->entry_num %d is illegal\n",
                __FUNCTION__, *count);
            mysql_free_result(result);
            free(sql_query);
            return -1;
        }
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int lc_db_table_load(void *info, char *table_name, char *column_list,
                char *condition, db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field;
    int ret_code = LC_DB_OK;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD, column_list, table_name,
                db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info || !fp)
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i]) {
                fp(info, field[i].name, row[i]);
            }
        }
    }
    if (mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);

    return ret_code;
}

int lc_db_table_load_all(void *info, char *table_name, char *column_list,
                char *condition, int offset, int *data_cnt, db_process fp)
{
    /* MYSQL *conn; */
    MYSQL_RES *result = NULL;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field, number = 0;
    int ret_code = LC_DB_OK;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }


    if (condition == NULL || condition[0] == 0) {
        sql_query = malloc(strlen(TABLE_DUMP) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list));
    } else {
        sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    }

    if (sql_query == NULL) {
        return -1;
    }

    if (condition == NULL || condition[0] == 0) {
        sprintf(sql_query, TABLE_DUMP, column_list, table_name,
                    db_cfg.version);
    } else {
        sprintf(sql_query, TABLE_LOAD, column_list, table_name,
                    db_cfg.version, condition);
    }

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0) {
            sleep(60);
            if (mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL)
            {
                LC_DB_LOG(LOG_ERR, "Failed to connect to database: Error: %s",
                                  mysql_error(multi_handle[idx].conn_handle));
                free(sql_query);
                return LC_DB_COMMON_ERROR;
            }
        }
#endif

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info)
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i])
                fp(info, field[i].name, row[i]);
        }
        info += offset;
        number++;
    }
    *data_cnt = number;
    if(mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return ret_code;
}

int lc_db_table_loadn_all(void *info, char *table_name, char *column_list,
                char *condition, int offset, int *data_cnt, int max_cnt,
                db_process fp)
{
    /* MYSQL *conn; */
    MYSQL_RES *result = NULL;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field, number = 0;
    int ret_code = LC_DB_OK;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }


    if (condition == NULL || condition[0] == 0) {
        sql_query = malloc(strlen(TABLE_DUMP) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list));
    } else {
        sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    }

    if (sql_query == NULL) {
        return -1;
    }

    if (condition == NULL || condition[0] == 0) {
        sprintf(sql_query, TABLE_DUMP, column_list, table_name,
                    db_cfg.version);
    } else {
        sprintf(sql_query, TABLE_LOAD, column_list, table_name,
                    db_cfg.version, condition);
    }

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0) {
            sleep(60);
            if (mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL)
            {
                LC_DB_LOG(LOG_ERR, "Failed to connect to database: Error: %s",
                                  mysql_error(multi_handle[idx].conn_handle));
                free(sql_query);
                return LC_DB_COMMON_ERROR;
            }
        }
#endif

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info)
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i])
                fp(info, field[i].name, row[i]);
        }
        info += offset;
        if (++number >= max_cnt) {
            break;
        }
    }
    *data_cnt = number;
    if(mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return ret_code;
}

int lc_db_table_loadn(void *info, int info_size, int info_len,
        char *table_name, char *column_list, char *condition, db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, j, num_field;
    int ret_code = LC_DB_OK;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB): handle_lookup failed\n");
        return LC_DB_COMMON_ERROR;
    }

    if (!info) {
        return LC_DB_COMMON_ERROR;
    }
    memset(info, 0, info_size * info_len);

    sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD, column_list, table_name,
            db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_ping(%s) failed.\n",
                __FUNCTION__, sql_query);
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed.\n",
                __FUNCTION__, sql_query);
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return LC_DB_COMMON_ERROR;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    for (i = 0; i < info_len && (row = mysql_fetch_row(result)); ++i) {
        for (j = 0; j < num_field; j++) {
            if (row[j]) {
                fp(info, field[j].name, row[j]);
            }
        }

        info += info_size;
    }
    if (mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);

    return ret_code;
}

int lc_db_table_loadn_order_by_id(void *info, int info_size, int info_len,
                                  char *table_name, char *column_list,
                                  char *condition, db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, j, num_field;
    int ret_code = LC_DB_OK;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB): handle_lookup failed\n");
        return LC_DB_COMMON_ERROR;
    }

    if (!info) {
        return LC_DB_COMMON_ERROR;
    }
    memset(info, 0, info_size * info_len);

    sql_query = malloc(strlen(TABLE_LOAD_ORDER_BY_ID) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD_ORDER_BY_ID, column_list, table_name,
            db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed.\n",
                __FUNCTION__, sql_query);
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return LC_DB_COMMON_ERROR;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    for (i = 0; i < info_len && (row = mysql_fetch_row(result)); ++i) {
        for (j = 0; j < num_field; j++) {
            if (row[j]) {
                fp(info, field[j].name, row[j]);
            }
        }

        info += info_size;
    }
    if (mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);

    return ret_code;
}

int lc_db_table_inner_join_load(void *info, char *column_list,
        char *table1, char *table2, char *join_condition,
        char *where_condition, db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field;
    int ret_code = LC_DB_OK;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_LOAD_INNER_JOIN) + strlen(column_list) +
            strlen(table1) + strlen(db_cfg.version) +
            strlen(table2) + strlen(db_cfg.version) +
            strlen(join_condition) + strlen(where_condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD_INNER_JOIN, column_list,
            table1, db_cfg.version,
            table2, db_cfg.version,
            join_condition, where_condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info || !fp)
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i]) {
                fp(info, field[i].name, row[i]);
            }
        }
    }
    if (mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);

    return ret_code;
}

int lc_db_table_load_inner(void *info, char *column_list, char *table1, char *table2,
               char *join_on, char *condition, int offset, int *data_cnt, int max, db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field, number = 0;
    int ret_code = LC_DB_OK;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_LOAD_INNER_JOIN) + strlen(column_list) +
            strlen(table1) + strlen(db_cfg.version) +
            strlen(table2) + strlen(db_cfg.version) +
            strlen(join_on) + strlen(condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD_INNER_JOIN, column_list,
            table1, db_cfg.version,
            table2, db_cfg.version,
            join_on, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info || !fp || (number >= max))
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i]) {
                fp(info, field[i].name, row[i]);
            }
        }
        info += offset;
        number++;
    }
    *data_cnt = number;
    if (mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);

    return ret_code;
}

int lc_db_table_load_by_server(void *info, char *table_name, char *column_list,
                char *condition, int offset, int *data_cnt, int max, db_process fp)
{
    /* MYSQL *conn; */
    MYSQL_RES *result = NULL;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, num_field, number = 0;
    int ret_code = LC_DB_OK;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }


    sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    sprintf(sql_query, TABLE_LOAD, column_list, table_name,
                db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (!info || (number >= max))
            break;
        for (i = 0; i < num_field; i++) {
            if (row[i])
                fp(info, field[i].name, row[i]);
        }
        info += offset;
        number++;
    }
    *data_cnt = number;
    if(mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return ret_code;
}

int atomic_lc_db_get_available_vlantag(
        uint32_t *p_vlantag, uint32_t rackid, int min, int max)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    char table_name[64];
    char column_list[64];
    char condition[128];
    char order[64];
    int num_field, id;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (p_vlantag) {
        *p_vlantag = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    min = (min > MIN_VLAN_ID) ? min : MIN_VLAN_ID;
    max = (max < MAX_VLAN_ID) ? max : MAX_VLAN_ID;
    if (max == 0) {
        max = MAX_VLAN_ID;
    }
    if (min > max) {
        return -1;
    }

    memset(table_name, 0, sizeof(table_name));
    memset(column_list, 0, sizeof(column_list));
    memset(condition, 0, sizeof(condition));
    memset(order, 0, sizeof(order));

    snprintf(table_name, sizeof(table_name), "vl2_vlan");
    snprintf(column_list, sizeof(column_list), "vlantag");
    if (rackid) {
        snprintf(condition, sizeof(condition),
                "(rackid=%u OR rackid=0) AND vlantag>=%d AND vlantag<=%d",
                rackid, min, max);
    } else {
        snprintf(condition, sizeof(condition),
                "vlantag>=%d AND vlantag<=%d",
                min, max);
    }
    snprintf(order, sizeof(order), "vlantag asc");

    sql_query = malloc(strlen(TABLE_LOAD_WHERE_ORDER) + strlen(column_list) +
            strlen(table_name) + strlen(db_cfg.version) +
            strlen(condition) + strlen(order) + 1);

    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_LOAD_WHERE_ORDER, column_list, table_name,
        db_cfg.version, condition, order);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }

    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    for (id = min; id <= max; ++id) {
        row = mysql_fetch_row(result);
        if (!row || id < atoi(row[0])) {
            *p_vlantag = id;
            break;
        }
    }

    mysql_free_result(result);
    free(sql_query);

    if (*p_vlantag == 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) %s: can not get available vlantag "
                "in [%d,%d] in rack-%u.\n", __FUNCTION__, min, max, rackid);
        return -1;
    }
    return 0;
}

int lc_db_get_available_tport()
{
    /* MYSQL *conn; */
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    char table_name[64];
    char column_list[64];
    char order[64];
    char condition[128];
    int num_field, tport, min_tport = MIN_VM_TPORT;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->handle_lookup(tid:%lu) failed\n",
                  __FUNCTION__, tid);
        return -1;
    }
    memset(table_name, 0, 64);
    memset(column_list, 0, 64);
    memset(order, 0, 64);
    memset(condition, 0, 128);

    /*
     * conn = mysql_real_connect(db_handle, db_cfg.host, db_cfg.user,
     *         db_cfg.passwd, db_cfg.dbname, atoi(db_cfg.dbport), NULL, 0)
     *                ;
     * if (conn == NULL) {
     *     return -1;
     * }
     */

    strcpy(table_name, "vm");
    strcpy(column_list, "tport");
    strcpy(condition, "tport<>0 AND tport is NOT NULL");
    sprintf(order, "%s asc", column_list);

    sql_query = malloc(strlen(TABLE_LOAD_WHERE_ORDER) + strlen(table_name) +
        strlen(db_cfg.version) + strlen(column_list) + strlen(order) +
        strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->sql_query malloc failed\n",
                  __FUNCTION__);
        return -1;
    }

    sprintf(sql_query, TABLE_LOAD_WHERE_ORDER, column_list, table_name,
        db_cfg.version, condition, order);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_ping/real_connect(%s) failed\n",
                      __FUNCTION__, sql_query);
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
                      __FUNCTION__, sql_query);
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (row[0] != NULL) {
            tport = atoi(row[0]);
            if (min_tport > MAX_VM_TPORT) {
                LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->min_tport(%u) > MAX_VM_TPORT\n",
                          __FUNCTION__, min_tport);
                min_tport = -1;
                goto finish;
            } else if (tport == min_tport) {
                min_tport++;
                continue;
            } else if (tport > min_tport) {
                LC_DB_LOG(LOG_DEBUG, "DEBUG (DB) : %s->get_tport(%u)\n",
                          __FUNCTION__, min_tport);
                goto finish;
            } else {
                LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->tport(%u) < min_tport(%u)\n",
                          __FUNCTION__, tport, min_tport);
                min_tport = -1;
                goto finish;
            }
        }
    }

finish:

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return min_tport;
}

int lc_db_get_available_vedge_tport()
{
    /* MYSQL *conn; */
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    char table_name[64];
    char column_list[64];
    char order[64];
    char condition[128];
    int num_field, tport, min_tport = MIN_VEDGE_TPORT;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->sql_query malloc failed\n",
                  __FUNCTION__);
        return -1;
    }
    memset(table_name, 0, 64);
    memset(column_list, 0, 64);
    memset(order, 0, 64);
    memset(condition, 0, 128);

    /*
     * conn = mysql_real_connect(db_handle, db_cfg.host, db_cfg.user,
     *         db_cfg.passwd, db_cfg.dbname, atoi(db_cfg.dbport), NULL, 0)
     *                ;
     * if (conn == NULL) {
     *     return -1;
     * }
     */

    strcpy(table_name, "vnet");
    strcpy(column_list, "tport");
    strcpy(condition, "tport<>0 AND tport is NOT NULL");
    sprintf(order, "%s asc", column_list);

    sql_query = malloc(strlen(TABLE_LOAD_WHERE_ORDER) + strlen(table_name) +
        strlen(db_cfg.version) + strlen(column_list) + strlen(order) +
        strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->sql_query malloc failed\n",
                  __FUNCTION__);
        return -1;
    }

    sprintf(sql_query, TABLE_LOAD_WHERE_ORDER, column_list, table_name,
        db_cfg.version, condition, order);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_ping/real_connect(%s) failed\n",
                      __FUNCTION__, sql_query);
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
                      __FUNCTION__, sql_query);
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (row[0] != NULL) {
            tport = atoi(row[0]);
            if (min_tport > MAX_VEDGE_TPORT) {
                LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->min_tport(%u) > MAX_VM_TPORT\n",
                          __FUNCTION__, min_tport);
                min_tport = -1;
                goto finish;
            } else if (tport == min_tport) {
                min_tport++;
                continue;
            } else if (tport > min_tport) {
                LC_DB_LOG(LOG_DEBUG, "DEBUG (DB) : %s->get_tport(%u)\n",
                          __FUNCTION__, min_tport);
                goto finish;
            } else {
                LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->tport(%u) < min_tport(%u)\n",
                          __FUNCTION__, tport, min_tport);
                min_tport = -1;
                goto finish;
            }
        }
    }

finish:

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return min_tport;
}

int lc_db_get_available_vmware_console_port()
{
    /* MYSQL *conn; */
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    char table_name[64];
    char column_list[64];
    char order[64];
    int num_field, port, min_port = MIN_VMWARE_CONSOLE_PORT;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }
    memset(table_name, 0, 64);
    memset(column_list, 0, 64);
    memset(order, 0, 64);

    /*
     * conn = mysql_real_connect(db_handle, db_cfg.host, db_cfg.user,
     *         db_cfg.passwd, db_cfg.dbname, atoi(db_cfg.dbport), NULL, 0)
     *                ;
     * if (conn == NULL) {
     *     return -1;
     * }
     */

    strcpy(table_name, "host_device");
    strcpy(column_list, "vmware_cport");
    sprintf(order, "%s asc", column_list);

    sql_query = malloc(strlen(TABLE_LOAD_ORDER) + strlen(table_name) +
        strlen(db_cfg.version) + strlen(column_list) + strlen(order));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_LOAD_ORDER, column_list, table_name,
        db_cfg.version, order);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return -1;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result))) {
        if (row[0] != NULL) {
            port = atoi(row[0]);
            if (port < MIN_VMWARE_CONSOLE_PORT) {
                continue;
            }
            if (min_port > MAX_VMWARE_CONSOLE_PORT) {
                min_port = -1;
                goto finish;
            } else if (port == min_port) {
                min_port += 2;
                continue;
            } else if (port > min_port) {
                goto finish;
            } else {
                min_port = -1;
                goto finish;
            }
        }
    }

finish:

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return min_port;
}

int lc_db_get_all_ips_of_if(int vdc_id, int if_index, int *isp_id, char *ips, int ips_size)
{
    /* MYSQL *conn; */
    MYSQL_RES *result = NULL;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int num_field;
    int ret_code = LC_DB_OK;
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    char tmp[LC_DB_BUF_LEN];
    char *table_name = "ip_resource";
    char *column_list = "ip,netmask,isp";

    if (!vdc_id || !ips || !isp_id) {
        return -1;
    }
    *isp_id = 0;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vdcid=%d and ifid=%d", vdc_id, if_index);

    sql_query = malloc(strlen(TABLE_LOAD) + strlen(table_name) +
            strlen(db_cfg.version) + strlen(column_list) + strlen(condition));

    if (sql_query == NULL) {
        return LC_DB_COMMON_ERROR;
    }

    memset(sql_query, 0,
           (strlen(TABLE_LOAD) + strlen(table_name) + strlen(db_cfg.version) + strlen(column_list) + strlen(condition)));
    sprintf(sql_query, TABLE_LOAD, column_list, table_name,
                db_cfg.version, condition);

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#if 0
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
#endif
        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }
    }

    result = mysql_use_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    field = mysql_fetch_fields(result);
    num_field = mysql_num_fields(result);

    memset(buffer, 0, LC_DB_BUF_LEN);
    while ((row = mysql_fetch_row(result))) {
        if (row[0] && row[1] && row[2]) {
            if (!buffer[0]) {
                *isp_id = atoi(row[2]);
                snprintf(buffer, LC_DB_BUF_LEN, "%s/%s", row[0], row[1]);
            } else {
                memset(tmp, 0, LC_DB_BUF_LEN);
                snprintf(tmp, LC_DB_BUF_LEN, ":%s/%s", row[0], row[1]);
                strncat(buffer, tmp, LC_DB_BUF_LEN - strlen(buffer) - 1);
            }
        }
    }
    strncpy(ips, buffer, ips_size - 1);
    if(mysql_num_rows(result) == 0) {
        ret_code = LC_DB_EMPTY_SET;
    }

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return ret_code;
}

int lc_db_init(char *host, char *user, char *passwd,
        char *dbname, char *dbport, char *version)
{
    if (host == NULL || user == NULL || passwd == NULL ||
            dbname == NULL || dbport == NULL) {
        return -1;
    }

    db_cfg.host = malloc(strlen(host) + 1);
    db_cfg.user = malloc(strlen(user) + 1);
    db_cfg.passwd = malloc(strlen(passwd) + 1);
    db_cfg.dbname = malloc(strlen(dbname) + 1);
    db_cfg.dbport = malloc(strlen(dbport) + 1);
    db_cfg.version = malloc(strlen(version) + 1);
    if (db_cfg.host == NULL || db_cfg.user == NULL ||
            db_cfg.passwd == NULL || db_cfg.dbname == NULL ||
            db_cfg.dbport == NULL || db_cfg.version == NULL) {
        free(db_cfg.host);
        free(db_cfg.user);
        free(db_cfg.passwd);
        free(db_cfg.dbname);
        free(db_cfg.dbport);
        free(db_cfg.version);
        return -1;
    }

    strcpy(db_cfg.host, host);
    strcpy(db_cfg.user, user);
    strcpy(db_cfg.passwd, passwd);
    strcpy(db_cfg.dbname, dbname);
    strcpy(db_cfg.dbport, dbport);
    strcpy(db_cfg.version, version);

    pthread_mutex_init(&hmutex, NULL);

    return 0;
}

int lc_db_thread_init(void)
{
    int idx;
    pthread_t tid = pthread_self();

	LC_DB_LOG(LOG_DEBUG, "(DB)%s: is enter.\n", __FUNCTION__);

    pthread_mutex_lock(&hmutex);

    idx = get_avail_handle(tid);
    if (idx == -1) {
        pthread_mutex_unlock(&hmutex);
        LC_DB_LOG(LOG_ERR, "(DB)%s: failed!get_avail_handle() return idx=%d\n", __FUNCTION__,idx);
        return -1;
    }

    multi_handle[idx].tid = tid;

    multi_handle[idx].conn_handle = mysql_init(NULL);
    if (multi_handle[idx].conn_handle == NULL) {
        pthread_mutex_unlock(&hmutex);
        LC_DB_LOG(LOG_ERR, "(DB) %s: failed!multi_handle[idx].conn_handle is NULL.\n", __FUNCTION__);
        return -1;
    }

    /* set mysql auto reconnect */
    my_bool reconnect_flag = 1;
    mysql_options(multi_handle[idx].conn_handle, MYSQL_OPT_RECONNECT, &reconnect_flag);
    mysql_real_connect(multi_handle[idx].conn_handle, db_cfg.host, db_cfg.user,
            db_cfg.passwd, db_cfg.dbname, atoi(db_cfg.dbport), NULL, 0);
    if (multi_handle[idx].conn_handle == NULL) {
        mysql_close(multi_handle[idx].conn_handle);
        pthread_mutex_unlock(&hmutex);
        LC_DB_LOG(LOG_ERR, "(DB) %s: failed!multi_handle[idx].conn_handle is NULL, close mysql.\n", __FUNCTION__);
        return -1;
    }

    pthread_mutex_unlock(&hmutex);
    LC_DB_LOG(LOG_INFO, "(DB) %s: successed\n", __FUNCTION__);

    return 0;
}

int lc_db_cfg_ver_len()
{
    return strlen(db_cfg.version);
}

char *lc_db_cfg_ver()
{
    return db_cfg.version;
}

int lc_db_thread_term(void)
{
    int idx;
    pthread_t tid = pthread_self();

    pthread_mutex_lock(&hmutex);

    idx = handle_lookup(tid);
    if (idx == -1) {
        pthread_mutex_unlock(&hmutex);
        return -1;
    }

    multi_handle[idx].tid = 0;
    mysql_close(multi_handle[idx].conn_handle);

    pthread_mutex_unlock(&hmutex);

    return 0;
}

int lc_db_term(void)
{
    pthread_mutex_destroy(&hmutex);

    free(db_cfg.host);
    free(db_cfg.user);
    free(db_cfg.passwd);
    free(db_cfg.dbname);
    free(db_cfg.dbport);
    free(db_cfg.version);

    return 0;
}

static int get_avail_master_handle(pthread_t tid)
{
    int i, avail = -1;

    for (i = 0; i < MAX_THREAD; i++) {
        if (pthread_equal(multi_master_handle[i].tid, tid)) {
            return -1;
        }

        if (multi_master_handle[i].tid == 0) {
            avail = i;
        }
    }

    return avail;
}

int handle_master_lookup(pthread_t tid)
{
    int i;

    for (i = 0; i < MAX_THREAD; i++) {
        if (pthread_equal(multi_master_handle[i].tid, tid)) {
            return i;
        }
    }

    return -1;
}

int lc_db_master_init(char *host, char *user, char *passwd,
        char *dbname, char *dbport, char *version)
{
    if (host == NULL || user == NULL || passwd == NULL ||
            dbname == NULL || dbport == NULL) {
        return -1;
    }

    db_master_cfg.host = malloc(strlen(host) + 1);
    db_master_cfg.user = malloc(strlen(user) + 1);
    db_master_cfg.passwd = malloc(strlen(passwd) + 1);
    db_master_cfg.dbname = malloc(strlen(dbname) + 1);
    db_master_cfg.dbport = malloc(strlen(dbport) + 1);
    db_master_cfg.version = malloc(strlen(version) + 1);
    if (db_master_cfg.host == NULL || db_master_cfg.user == NULL ||
            db_master_cfg.passwd == NULL || db_master_cfg.dbname == NULL ||
            db_master_cfg.dbport == NULL || db_master_cfg.version == NULL) {
        free(db_master_cfg.host);
        free(db_master_cfg.user);
        free(db_master_cfg.passwd);
        free(db_master_cfg.dbname);
        free(db_master_cfg.dbport);
        free(db_master_cfg.version);
        return -1;
    }

    strcpy(db_master_cfg.host, host);
    strcpy(db_master_cfg.user, user);
    strcpy(db_master_cfg.passwd, passwd);
    strcpy(db_master_cfg.dbname, dbname);
    strcpy(db_master_cfg.dbport, dbport);
    strcpy(db_master_cfg.version, version);

    pthread_mutex_init(&hmutex_master, NULL);

    return 0;
}

int lc_db_master_thread_init(void)
{
    int idx;
    pthread_t tid = pthread_self();

	LC_DB_LOG(LOG_DEBUG, "(DB)%s: is enter.\n", __FUNCTION__);

    pthread_mutex_lock(&hmutex_master);

    idx = get_avail_master_handle(tid);
    if (idx == -1) {
        pthread_mutex_unlock(&hmutex_master);
        LC_DB_LOG(LOG_ERR, "(DB)%s: failed!get_avail_master_handle() return idx=%d\n", __FUNCTION__,idx);
        return -1;
    }

    multi_master_handle[idx].tid = tid;

    multi_master_handle[idx].conn_handle = mysql_init(NULL);
    if (multi_master_handle[idx].conn_handle == NULL) {
        pthread_mutex_unlock(&hmutex_master);
        LC_DB_LOG(LOG_ERR, "(DB) %s: failed!multi_master_handle[idx].conn_handle is NULL.\n", __FUNCTION__);
        return -1;
    }

    mysql_real_connect(multi_master_handle[idx].conn_handle,
            db_master_cfg.host, db_master_cfg.user,
            db_master_cfg.passwd, db_master_cfg.dbname,
            atoi(db_master_cfg.dbport), NULL, 0);
    if (multi_master_handle[idx].conn_handle == NULL) {
        mysql_close(multi_master_handle[idx].conn_handle);
        pthread_mutex_unlock(&hmutex_master);
        LC_DB_LOG(LOG_ERR, "(DB) %s: failed!multi_master_handle[idx].conn_handle is NULL, close mysql.\n", __FUNCTION__);
        return -1;
    }

    pthread_mutex_unlock(&hmutex_master);
    LC_DB_LOG(LOG_INFO, "(DB) %s: successed\n", __FUNCTION__);

    return 0;
}

int lc_db_master_thread_term(void)
{
    int idx;
    pthread_t tid = pthread_self();

    pthread_mutex_lock(&hmutex_master);

    idx = handle_master_lookup(tid);
    if (idx == -1) {
        pthread_mutex_unlock(&hmutex_master);
        return -1;
    }

    multi_master_handle[idx].tid = 0;
    mysql_close(multi_master_handle[idx].conn_handle);

    pthread_mutex_unlock(&hmutex_master);

    return 0;
}

int lc_db_master_term(void)
{
    pthread_mutex_destroy(&hmutex_master);

    free(db_master_cfg.host);
    free(db_master_cfg.user);
    free(db_master_cfg.passwd);
    free(db_master_cfg.dbname);
    free(db_master_cfg.dbport);
    free(db_master_cfg.version);

    return 0;
}

int lc_db_ping_master_mysql(MYSQL *conn_handle)
{
    if (mysql_ping(conn_handle) != 0) {
        sleep(5);
        if (mysql_real_connect(conn_handle, db_master_cfg.host,
                    db_master_cfg.user, db_master_cfg.passwd,
                    db_master_cfg.dbname,
                    atoi(db_master_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "Failed to connect to database: Error: %s",
                                  mysql_error(conn_handle));
            return -1;
        }
    }
    return 0;
}

int lc_db_master_table_insert(char *table_name, char *column_list,
        char *value_list)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_master_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_INSERT) + strlen(table_name) +
            strlen(db_master_cfg.version) + strlen(column_list) + strlen(value_list));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_INSERT, table_name, db_master_cfg.version,
            column_list, value_list);

    if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_master_mysql(multi_master_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
        if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_master_table_update(char *table_name, char *column, char *value,
        char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_master_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_UPDATE) + strlen(table_name) +
            strlen(db_master_cfg.version) + strlen(column) + strlen(value) +
            strlen(condition));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_UPDATE, table_name, db_master_cfg.version,
            column, value, condition);

    if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_master_mysql(multi_master_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
        if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}

int lc_db_master_table_multi_update(char *table_name, char *columns_update, char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_master_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_MULTI_UPDATE) + strlen(table_name) +
            strlen(db_master_cfg.version) + strlen(columns_update) +
            strlen(condition));
    if (sql_query == NULL) {
        return -1;
    }

    sprintf(sql_query, TABLE_MULTI_UPDATE, table_name, db_master_cfg.version,
            columns_update, condition);

    if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_master_mysql(multi_master_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return -1;
        }
        if (mysql_query(multi_master_handle[idx].conn_handle, sql_query) != 0) {
            free(sql_query);
            return -1;
        }
    }

    free(sql_query);

    return 0;
}
