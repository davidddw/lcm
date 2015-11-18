#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "mysql.h"
#include "lc_db.h"
#include "vl2/ext_vl2_db.h"
#include "vl2/ext_vl2_framework.h"
#include "vl2/ext_vl2_handler.h"

#define TABLE_INSERT          "insert into %s_v%s(%s) values(%s);"
#define TABLE_DELETE          "delete from %s_v%s where %s;"
#define TABLE_UPDATE_PARA     "update %s_v%s set %s=%s where %s;"
#define TABLE_UPDATE_OPT      "update %s_v%s set %s where %s;"
#define TABLE_DUMP            "select %s from %s_v%s;"
#define TABLE_SELECT          "select %s from %s_v%s where %s;"
#define TABLE_SELECT_EXIST    "select * from %s_v%s where %s;"
#define TABLE_SELECT_DISTINCT "select distinct %s from %s_v%s where %s order by %s;"
#define TABLE_UNIQUE_TUNNELID \
    "select distinct first.tunnel_id from %s_v%s first, %s_v%s second " \
    "where first.server_ip=\"%s\" and second.server_ip=\"%s\" " \
    "and first.tunnel_id=second.tunnel_id order by first.tunnel_id;"

typedef int (*ext_db_process)(void *db_info, char *field, char *value);

//extern MYSQL *conn_handle;
extern struct db_config db_cfg;
extern struct thread_mysql multi_handle[MAX_THREAD];

int is_exist(char *table_name,
    char *condition,
    int  *exist_or_not)
{
    MYSQL_RES *result;
    char *sql_query;
    int entry_num;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT_EXIST)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT_EXIST,
        table_name, db_cfg.version, condition);
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    } else if (entry_num == 0) {
        *exist_or_not = 0;
    } else {
        *exist_or_not = 1;
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int is_unique(char *table_name,
    char *column,
    char *condition,
    int  *unique_or_not)
{
    MYSQL_RES *result;
    char *sql_query;
    int entry_num;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    } else if (entry_num == 1) {
        *unique_or_not = 1;
    } else {
        *unique_or_not = 0;
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int is_join_unique(char *table_name,
    char *src_ip,
    char *des_ip,
    int  *unique_or_not)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i, base;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_UNIQUE_TUNNELID) + strlen(table_name)*2
        + 1 + strlen(db_cfg.version)*2 + strlen(src_ip) + strlen(des_ip));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_UNIQUE_TUNNELID,
        table_name, db_cfg.version, table_name, db_cfg.version, src_ip, des_ip);
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    base = 0;
    for (i=0; i<entry_num; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            base++;
            continue;
        } else {
            if (strcmp(row[0], "0") == 0) {
                base++;
                continue;
            }
        }
    }
    if ((entry_num-base) == 1) {
        *unique_or_not = 1;
    } else if ((entry_num-base) <= 0) {
        *unique_or_not = -1;
    } else {
        *unique_or_not = 0;
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookup_v2(char *table_name,
    char *column,
    char *condition,
    char *value,
    char err_grade)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (value) {
        *value = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG((int)err_grade, "%s: %s->mysql_query() failed\n", err_grade==LOG_ERR?"ERROR":"WARNING", __FUNCTION__);
            free(sql_query);
            return -1;
        }

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG((int)err_grade, "%s: %s->mysql_query() failed\n", err_grade==LOG_ERR?"ERROR":"WARNING", __FUNCTION__);
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        LC_DB_LOG(LOG_WARNING,
            "WARNING: %s->entry_num(%s) %d is illegal\n",
            __FUNCTION__, sql_query, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num >= 1) {
        row = mysql_fetch_row(result);
        if (row[0]) {
            strcpy(value, row[0]);
        }
        while (--entry_num > 0) {
            row = mysql_fetch_row(result);
            if (row[0] == NULL || strcmp(row[0], value) != 0) {
                LC_DB_LOG(LOG_ERR, "ERROR (DB) %s: query [%s] get multiple value: [%s][%s].\n",
                    __FUNCTION__, sql_query, value, row[0] == NULL ? "" : row[0]);
                mysql_free_result(result);
                free(sql_query);
                return -1;
            }
        }
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookup(char *table_name,
    char *column,
    char *condition,
    char *value)
{
    return ext_vl2_db_lookup_v2(table_name, column, condition, value, LOG_ERR);
}

int ext_vl2_db_lookups(char *table_name,
    char *column,
    char *condition,
    char * const values[])
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values && values[0]) {
        values[0][0] = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        //LC_DB_LOG(LOG_WARNING,
        //    "WARNING: %s->entry_num %d is illegal\n",
        //    __FUNCTION__, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i=0; i<entry_num; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        strcpy(values[i], row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookups_v2(char *table_name,
    char *column,
    char *condition,
    char * const values[],
    int values_size)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values && values[0]) {
        values[0][0] = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        //LC_DB_LOG(LOG_WARNING,
        //    "WARNING: %s->entry_num %d is illegal\n",
        //    __FUNCTION__, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i = 0; i < entry_num && i < values_size; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        strcpy(values[i], row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookups_multi_v2(char *table_name,
    char *column,
    char *condition,
    char * const values[],
    int values_size)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values && values[0]) {
        values[0][0] = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i = 0; i < entry_num && i < values_size; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        strcpy(values[i], row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_count_v2(char *table_name,
    char *column,
    char *condition,
    int  *count,
    char err_grade)
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
            LC_DB_LOG((int)err_grade, "%s: %s->mysql_query() failed\n", err_grade==LOG_ERR?"ERROR":"WARNING", __FUNCTION__);
            free(sql_query);
            return -1;
        }

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG((int)err_grade, "%s: %s->mysql_query() failed\n", err_grade==LOG_ERR?"ERROR":"WARNING", __FUNCTION__);
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

int ext_vl2_db_count(char *table_name,
    char *column,
    char *condition,
    int  *count)
{
    return ext_vl2_db_count_v2(table_name, column, condition, count, LOG_ERR);
}

int ext_vl2_db_assign(char *table_name,
    char *column,
    char *condition,
    char *value)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, id_read, id_count;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (strcmp(column, "port_status") == 0 ||
        strcmp(column, "server_ip") == 0 ||
        strcmp(column, "external_id") == 0 ) {
        LC_DB_LOG(LOG_WARNING,
            "WARNING: %s->column %s has no min_id\n",
            __FUNCTION__, column);
        return -1;
    }
    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
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
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    id_count = 1;
    while ((row = mysql_fetch_row(result))) {
        if (row[0] == NULL) {
            continue;
        }
        id_read = atoi(row[0]);
        if (id_read == 0) {
            continue;
        }
        if (id_count < id_read) {
            break;
        } else {
            id_count++;
        }
    }
    sprintf(value, "%d", id_count);
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_update(char *table_name,
    char *column,
    char *value,
    char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_UPDATE_PARA) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition)
        + strlen(value));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_UPDATE_PARA,
        table_name, db_cfg.version, column, value, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (mysql_ping(multi_handle[idx].conn_handle) != 0 &&
                mysql_real_connect(multi_handle[idx].conn_handle,
                db_cfg.host, db_cfg.user, db_cfg.passwd, db_cfg.dbname,
                atoi(db_cfg.dbport), NULL, 0) == NULL) {
            LC_DB_LOG(LOG_ERR, "ERROR: %s->mysql_ping() failed\n", __FUNCTION__);
            free(sql_query);
            return -1;
        }

        if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
            LC_DB_LOG(LOG_ERR, "ERROR: %s->mysql_query() failed: %s\n",
                    __FUNCTION__, sql_query);
            free(sql_query);
            return -1;
        }
    }
    free(sql_query);
    return 0;
}

int ext_vl2_db_updates(char *table_name,
    char *operations,
    char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->handle_lookup(tid) failed\n",
            __FUNCTION__);
        return -1;
    }

    sql_query = malloc(strlen(TABLE_UPDATE_OPT) + strlen(operations)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_UPDATE_OPT,
        table_name, db_cfg.version, operations, condition);
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
    free(sql_query);
    return 0;
}

int ext_vl2_db_insert(char *table_name,
    char *column_list,
    char *value_list)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_INSERT) + strlen(column_list)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(value_list));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_INSERT,
        table_name, db_cfg.version, column_list, value_list);
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
    free(sql_query);
    return 0;
}

int ext_vl2_db_delete(char *table_name,
    char *condition)
{
    char *sql_query;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_DELETE)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_DELETE,
        table_name, db_cfg.version, condition);
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
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookup_ids(char *table_name,
    char *column,
    char *condition,
    int  *values)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values) {
        *values = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    result = mysql_store_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        //LC_DB_LOG(LOG_WARNING,
        //    "WARNING: %s->entry_num %d is illegal\n",
        //    __FUNCTION__, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i=0; i<entry_num; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        *(values+i) = atoi(row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookup_ids_v2(char *table_name,
    char *column,
    char *condition,
    int  *values,
    int  values_max_size)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values) {
        *values = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    result = mysql_store_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        //LC_DB_LOG(LOG_WARNING,
        //    "WARNING: %s->entry_num %d is illegal\n",
        //    __FUNCTION__, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i=0; i < entry_num && i < values_max_size; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        *(values+i) = atoi(row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_lookup_ids_v3(char *table_name,
    char *column,
    char *condition,
    int  * const values[],
    int  values_max_size)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, i;
    pthread_t tid = pthread_self();
    int idx = -1;

    if (values && values[0]) {
        values[0][0] = 0;
    } else {
        return -1;
    }

    idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    result = mysql_store_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    if (entry_num < 1) {
        //LC_DB_LOG(LOG_WARNING,
        //    "WARNING: %s->entry_num %d is illegal\n",
        //    __FUNCTION__, entry_num);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    for (i=0; i < entry_num && i < values_max_size; i++) {
        row = mysql_fetch_row(result);
        if (row[0] == NULL) {
            continue;
        }
        *values[i] = atoi(row[0]);
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int atomic_ext_vl2_db_assign_ids(char *table_name,
    char *column,
    char *condition,
    int  *values)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *sql_query;
    int entry_num, id_read, id_count;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    result = mysql_store_result(multi_handle[idx].conn_handle);
    if (result == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_store_result(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
    }
    entry_num = mysql_num_rows(result);
    if (entry_num < 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_num_rows(%s) failed\n",
            __FUNCTION__, sql_query);
        mysql_free_result(result);
        free(sql_query);
        return -1;
    }
    id_count = 1;
    while ((row = mysql_fetch_row(result))) {
        if (row[0] == NULL) {
            continue;
        }
        id_read = atoi(row[0]);
        if (id_read == 0) {
            continue;
        }
        if (id_count < id_read) {
            break;
        } else {
            id_count++;
        }
    }
    *values = id_count;
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

static int network_device_process(void *nr_info,
    char *field,
    char *value)
{
    torswitch_t *nr = (torswitch_t *)nr_info;
    if (strcmp(field, VL2_NETDEC_ID_ENTRY) == 0) {
        nr->id = atoi(value);
    } else if (strcmp(field, VL2_NETDEC_RI_ENTRY) == 0) {
        nr->rackid = atoi(value);
    } else if (strcmp(field, VL2_INTERF_NE_ENTRY) == 0) {
        strcpy(nr->name, value);
    } else if (strcmp(field, VL2_NETDEC_IP_ENTRY) == 0) {
        strcpy(nr->ip, value);
    } else if (strcmp(field, VL2_NETDEC_BR_ENTRY) == 0) {
        strcpy(nr->brand, value);
    } else if (strcmp(field, VL2_NETDEC_TI_ENTRY) == 0) {
        strcpy(nr->tunnel_ip, value);
    } else if (strcmp(field, VL2_NETDEC_TN_ENTRY) == 0) {
        strcpy(nr->tunnel_netmask, value);
    } else if (strcmp(field, VL2_NETDEC_TG_ENTRY) == 0) {
        strcpy(nr->tunnel_gateway, value);
    } else if (strcmp(field, VL2_NETDEC_UN_ENTRY) == 0) {
        strcpy(nr->username, value);
    } else if (strcmp(field, VL2_NETDEC_PW_ENTRY) == 0) {
        strcpy(nr->passwd, value);
    } else if (strcmp(field, VL2_NETDEC_EN_ENTRY) == 0) {
        strcpy(nr->enable, value);
    } else if (strcmp(field, VL2_NETDEC_BT_ENTRY) == 0) {
        sscanf(value, "%"SCNu64, &nr->boot_time);
    } else if (strcmp(field, VL2_NETDEC_UUID_ENTRY) == 0) {
        strcpy(nr->lcuuid, value);
    } else if (strcmp(field, VL2_NETDEC_DOMAIN_ENTRY) == 0) {
        strcpy(nr->domain, value);
    }
    return 0;
}

static int rack_tunnel_process(void *nr_info,
    char *field,
    char *value)
{
    rack_tunnel_t *nr = (rack_tunnel_t *)nr_info;
    if (strcmp(field, VL2_RACTUN_LU_ENTRY) == 0) {
        strcpy(nr->local_uplink_ip, value);
    } else if (strcmp(field, VL2_RACTUN_RU_ENTRY) == 0) {
        strcpy(nr->remote_uplink_ip, value);
    }
    return 0;
}

static int policy_process(void *nr_info,
    char *field,
    char *value)
{
    policy_t *nr = (policy_t *)nr_info;
    if (strcmp(field, VL2_POLICY_ID_ENTRY) == 0) {
        nr->id = atoi(value);
    } else if (strcmp(field, VL2_POLICY_TE_ENTRY) == 0) {
        nr->type = atoi(value);
    } else if (strcmp(field, VL2_POLICY_GN_ENTRY) == 0) {
        nr->grain = atoi(value);
    } else if (strcmp(field, VL2_POLICY_OT_ENTRY) == 0) {
        strcpy(nr->object, value);
    } else if (strcmp(field, VL2_POLICY_CT_ENTRY) == 0) {
        strcpy(nr->component, value);
    } else if (strcmp(field, VL2_POLICY_AP_ENTRY) == 0) {
        nr->app = atoi(value);
    } else if (strcmp(field, VL2_POLICY_GP_ENTRY) == 0) {
        nr->group = atoi(value);
    } else if (strcmp(field, VL2_POLICY_IX_ENTRY) == 0) {
        nr->index = atoi(value);
    } else if (strcmp(field, VL2_POLICY_ST_ENTRY) == 0) {
        strcpy(nr->socket, value);
    } else if (strcmp(field, VL2_POLICY_AE_ENTRY) == 0) {
        strcpy(nr->attribute, value);
    } else if (strcmp(field, VL2_POLICY_CN_ENTRY) == 0) {
        strcpy(nr->content, value);
    } else if (strcmp(field, VL2_POLICY_SE_ENTRY) == 0) {
        nr->state = atoi(value);
    } else if (strcmp(field, VL2_POLICY_DN_ENTRY) == 0) {
        strcpy(nr->description, value);
    }
    return 0;
}

static int tunnel_policy_process(void *nr_info,
    char *field,
    char *value)
{
    tunnel_policy_t *nr = (tunnel_policy_t *)nr_info;
    if (strcmp(field, VL2_TUNPOL_ID_ENTRY) == 0) {
        nr->id = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_FI_ENTRY) == 0) {
        nr->vifid = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_UP_ENTRY) == 0) {
        strcpy(nr->uplink_ip, value);
    } else if (strcmp(field, VL2_TUNPOL_DT_ENTRY) == 0) {
        nr->vdevicetype = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_DI_ENTRY) == 0) {
        nr->vdeviceid = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_VI_ENTRY) == 0) {
        nr->vl2id = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_VT_ENTRY) == 0) {
        nr->vlantag = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_FC_ENTRY) == 0) {
        strcpy(nr->vifmac, value);
    }
    return 0;
}

static int vl2_tunnel_policy_process(void *nr_info,
    char *field,
    char *value)
{
    vl2_tunnel_policy_t *nr = (vl2_tunnel_policy_t *)nr_info;
    if (strcmp(field, VL2_TUNPOL_ID_ENTRY) == 0) {
        nr->id = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_UP_ENTRY) == 0) {
        strcpy(nr->uplink_ip, value);
    } else if (strcmp(field, VL2_TUNPOL_VI_ENTRY) == 0) {
        nr->vl2id = atoi(value);
    } else if (strcmp(field, VL2_TUNPOL_VT_ENTRY) == 0) {
        nr->vlantag = atoi(value);
    }
    return 0;
}

int ext_vl2_db_table_load_all(void *info,
        int info_size,
        int info_len,
        char *table_name,
        char *column_list,
        char *condition,
        int *data_cnt,
        ext_db_process fp)
{
    /* MYSQL *conn; */
    MYSQL_RES *result = NULL;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, j, num_field, number = 0;

    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    if (condition == NULL || condition[0] == '\0') {
        sql_query = malloc(strlen(TABLE_DUMP) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list));
    } else {
        sql_query = malloc(strlen(TABLE_SELECT) + strlen(table_name) +
                strlen(db_cfg.version) + strlen(column_list) + strlen(condition));
    }

    if (sql_query == NULL) { return -1;
    }

    if (condition == NULL || condition[0] == 0) {
        sprintf(sql_query, TABLE_DUMP, column_list, table_name,
                    db_cfg.version);
    } else {
        sprintf(sql_query, TABLE_SELECT, column_list, table_name,
                    db_cfg.version, condition);
    }

    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        if (lc_db_ping_mysql(multi_handle[idx].conn_handle)) {
            LC_DB_LOG(LOG_ERR, "Failed to ping the database");
            free(sql_query);
            return LC_DB_COMMON_ERROR;
        }

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
    for (i = 0; i < info_len && (row = mysql_fetch_row(result)); ++i) {
        for (j = 0; j < num_field; j++) {
            if (row[j])
                fp(info, field[j].name, row[j]);
        }
        info += info_size;
        number++;
    }
    *data_cnt = number;

    mysql_free_result(result);
    free(sql_query);
    /* mysql_close(conn); */

    return 0;
}

int ext_vl2_db_table_loadn(void *info,
    int info_size,
    int info_len,
    char *table_name,
    char *column_list,
    char *condition,
    ext_db_process fp)
{
    MYSQL_RES *result;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    char *sql_query;
    int i, j, num_field;
    pthread_t tid = pthread_self();

    int idx = handle_lookup(tid);
    if (idx == -1) {
        return -1;
    }

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column_list)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column_list, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
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
    for (i = 0; i < info_len && (row = mysql_fetch_row(result)); ++i) {
        for (j = 0; j < num_field; j++) {
            if (row[j]) {
                fp(info, field[j].name, row[j]);
            }
        }
        info += info_size;
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_table_load(void *info,
    char *table_name,
    char *column_list,
    char *condition,
    ext_db_process fp)
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

    sql_query = malloc(strlen(TABLE_SELECT) + strlen(column_list)
        + 1 + strlen(table_name) + strlen(db_cfg.version) + strlen(condition));
    if (sql_query == NULL) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->malloc(%s) failed\n",
            __FUNCTION__, table_name);
        return -1;
    }
    sprintf(sql_query, TABLE_SELECT,
        column_list, table_name, db_cfg.version, condition);
    if (mysql_query(multi_handle[idx].conn_handle, sql_query) != 0) {
        LC_DB_LOG(LOG_ERR, "ERROR (DB) : %s->mysql_query(%s) failed\n",
            __FUNCTION__, sql_query);
        free(sql_query);
        return -1;
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
    }
    mysql_free_result(result);
    free(sql_query);
    return 0;
}

int ext_vl2_db_policy_load(void *info,
    char *condition)
{
    return ext_vl2_db_table_load((policy_t *)info, VL2_POLICY_TABLE,
        "*", condition, policy_process);
}

int ext_vl2_db_tunnel_policy_load(void *info, char *condition)
{
    return ext_vl2_db_table_load(info, TABLE_TUNNEL_POLICY, "*", condition,
        tunnel_policy_process);
}

int ext_vl2_db_vl2_tunnel_policy_load(void *info, char *condition)
{
    return ext_vl2_db_table_load(info, TABLE_VL2_TUNNEL_POLICY, "*", condition,
        vl2_tunnel_policy_process);
}

int ext_vl2_db_rack_tunnel_loadn(void *info, int info_size, int info_len, char *condition)
{
    return ext_vl2_db_table_loadn(info, info_size, info_len, TABLE_RACK_TUNNEL, "*", condition, rack_tunnel_process);
}

int ext_vl2_db_network_device_load(void *info, char *condition)
{
    return ext_vl2_db_table_load(info, TABLE_NETWORK_DEVICE, "*", condition, network_device_process);
}

int ext_vl2_db_network_device_load_all(void *info, int info_size, int info_len, int *data_cnt, char *condition)
{
    return ext_vl2_db_table_load_all(info, info_size, info_len, TABLE_NETWORK_DEVICE, "*", condition, data_cnt, network_device_process);
}

static int port_map_process(void *info, char *field, char *value)
{
    vmwaf_port_map_info_t *p = (vmwaf_port_map_info_t *)info;

    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        p->state = atoi(value);
    } else if (strcmp(field, "pre_state") == 0) {
        p->pre_state = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        p->flag = atoi(value);
    } else if (strcmp(field, "web_server_confid") == 0) {
        p->web_server_confid = atoi(value);
    } else if (strcmp(field, "vnetid") == 0) {
        p->vnetid = atoi(value);
    } else if (strcmp(field, "wan_ipaddr") == 0) {
        strncpy(p->wan_ipaddr, value, MAX_HOST_ADDRESS_LEN);
    } else if (strcmp(field, "wan_port") == 0) {
        p->wan_port = atoi(value);
    } else if (strcmp(field, "wan_mac") == 0) {
        strncpy(p->wan_mac, value, MAX_VIF_MAC_LEN);
    } else if (strcmp(field, "vnet_ipaddr") == 0) {
        strncpy(p->vnet_ipaddr, value, MAX_HOST_ADDRESS_LEN);
    } else if (strcmp(field, "vnet_port") == 0) {
        p->vnet_port = atoi(value);
    } else if (strcmp(field, "vm_ipaddr") == 0) {
        strncpy(p->vm_ipaddr, value, MAX_HOST_ADDRESS_LEN);
    } else if (strcmp(field, "vm_port") == 0) {
        p->vm_port = atoi(value);
    } else if (strcmp(field, "domain") == 0) {
        strncpy(p->domain, value, LC_DOMAIN_LEN);
    }

    return 0;
}

int ext_vl2_port_map_load(vmwaf_port_map_info_t* portmap_info,
                                char *column_list, char *condition)
{
    return ext_vl2_db_table_load(portmap_info, "vmwaf_port_map", column_list, condition, port_map_process);
}

int ext_vl2_get_port_map_by_vnet_ipaddr(void *portmap_query, char *vnet_ip)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "vnet_ipaddr='%s'", vnet_ip);

    result = ext_vl2_port_map_load((vmwaf_port_map_info_t*)portmap_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_PORT_MAP_NOT_FOUND;
    } else {
        return LC_DB_PORT_MAP_FOUND;
    }
}


int ext_vl2_get_port_map_by_web_server_confid(void *portmap_query, int web_server_confid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "web_server_confid=%d", web_server_confid);

    result = ext_vl2_port_map_load((vmwaf_port_map_info_t*)portmap_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_PORT_MAP_NOT_FOUND;
    } else {
        return LC_DB_PORT_MAP_FOUND;
    }
}

int ext_vl2_get_port_map_by_id(void *portmap_query, int portmapid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", portmapid);

    result = ext_vl2_port_map_load((vmwaf_port_map_info_t*)portmap_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_PORT_MAP_NOT_FOUND;
    } else {
        return LC_DB_PORT_MAP_FOUND;
    }
}

static int web_secu_conf_process(void *info, char *field, char *value)
{
    vmwaf_web_conf_info_t *p = (vmwaf_web_conf_info_t *)info;

    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        p->state = atoi(value);
    } else if (strcmp(field, "pre_state") == 0) {
        p->pre_state = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        p->flag = atoi(value);
    } else if (strcmp(field, "description") == 0) {
        strncpy(p->description, value, LC_NAMEDES);
    } else if (strcmp(field, "vnetid") == 0) {
        p->vnetid = atoi(value);
    } else if (strcmp(field, "server_name") == 0) {
        strncpy(p->server_name, value, MAX_VM_UUID_LEN);
    } else if (strcmp(field, "service_type") == 0) {
        p->service_type = atoi(value);
    } else if (strcmp(field, "pub_ipaddr") == 0) {
        strncpy(p->pub_ipaddr, value, MAX_HOST_ADDRESS_LEN);
    } else if (strcmp(field, "pub_port") == 0) {
        p->pub_port = atoi(value);
    } else if (strcmp(field, "pub_mac") == 0) {
        strncpy(p->pub_mac, value, MAX_VIF_MAC_LEN);
    } else if (strcmp(field, "domain_name") == 0) {
        strncpy(p->domain_name, value, LC_NAMEDES);
    } else if (strcmp(field, "weblog_state") == 0) {
        p->weblog_state = atoi(value);
    } else if (strcmp(field, "vmwafid") == 0) {
        p->vmwafid = atoi(value);
    } else if (strcmp(field, "domain") == 0) {
        strncpy(p->domain, value, LC_DOMAIN_LEN);
    }

    return 0;
}

int ext_vl2_web_secu_conf_load(vmwaf_web_conf_info_t* webconf_info,
                                char *column_list, char *condition)
{
    return ext_vl2_db_table_load(webconf_info, "vmwaf_web_server_conf", column_list, condition, web_secu_conf_process);
}

int ext_vl2_get_web_secu_conf_by_id(void *webconf_query, int webconfid)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d'", webconfid);

    result = ext_vl2_web_secu_conf_load((vmwaf_web_conf_info_t*)webconf_query, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_WEB_CONF_NOT_FOUND;
    } else {
        return LC_DB_WEB_CONF_FOUND;
    }
}

