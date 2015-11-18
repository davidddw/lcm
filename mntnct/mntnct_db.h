#ifndef _MNTNCT_DB_H
#define _MNTNCT_DB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE     1024
#define BIG_BUFSIZE 2048

#define USERNAME    "root"
#define PASSWORD    "security421"
#define DATABASE    "livecloud"
#define BSSDATABASE    "livecloud_bss"
#define DBVER       "2_2"

#define DBCMDPRE    "mysql --database=" DATABASE
#define BSSDBCMDPRE    "mysql --database=" BSSDATABASE
#define DBCMD       DBCMDPRE " -e "
#define DBCMD_raw   DBCMDPRE " -sNe "
#define BSSDBCMD       BSSDBCMDPRE " -e "
#define BSSDBCMD_raw   BSSDBCMDPRE " -sNe "

#define SELECT      "SELECT %s FROM %s_v" DBVER " WHERE %s"
#define BSS_SELECT  "SELECT %s FROM %s WHERE %s"
#define SELECT_cat  "SELECT IFNULL(GROUP_CONCAT(%s),'') FROM %s_v" \
                    DBVER " WHERE %s"
#define DUMP_SR     "SELECT %s FROM storage_v" DBVER " %s " \
                    "INNER JOIN storage_connection_v" DBVER " %s " \
                    "ON %s.id = %s.storage_id WHERE %s GROUP BY uuid"
#define DUMP_SR_cat "SELECT IFNULL(GROUP_CONCAT(%s),'') FROM storage_v" \
                    DBVER " %s INNER JOIN storage_connection_v" DBVER " %s " \
                    "ON %s.id = %s.storage_id WHERE %s GROUP BY uuid"
#define DUMP_HA_SR  "SELECT %s FROM ha_storage_v" DBVER " %s " \
                    "INNER JOIN ha_storage_connection_v" DBVER " %s " \
                    "ON %s.id = %s.storage_id WHERE %s GROUP BY uuid"
#define DUMP_HA_SR_cat "SELECT IFNULL(GROUP_CONCAT(%s),'') FROM ha_storage_v" \
                    DBVER " %s INNER JOIN ha_storage_connection_v" DBVER " %s " \
                    "ON %s.id = %s.storage_id WHERE %s GROUP BY uuid"
#define DUMP_POOL_PS   "SELECT %s FROM product_specification_v" DBVER " %s " \
                    "INNER JOIN pool_product_specification_v" DBVER " %s " \
                    "ON %s.lcuuid = %s.product_spec_lcuuid WHERE %s GROUP BY lcuuid"
#define DUMP_IP     "SELECT %s FROM ip_resource_v" DBVER " %s " \
                    "INNER JOIN vl2_v" DBVER " %s " \
                    "ON %s.isp = %s.isp WHERE %s"
#define DUMP_VUL    "SELECT %s FROM vul_scanner_v" DBVER " %s " \
                    "INNER JOIN vm_v" DBVER " %s " \
                    "ON %s.vm_lcuuid = %s.lcuuid WHERE %s"
#define DUMP_HWDEV_CON     "SELECT %s FROM third_party_device_v" DBVER " l " \
                           "INNER JOIN vinterface_v" DBVER " r " \
                           "ON l.id = r.deviceid AND r.devicetype = 3 " \
                           "WHERE %s"
#define DUMP_HOST_CON     "SELECT %s FROM host_device_v" DBVER " l " \
                          "INNER JOIN compute_resource_v" DBVER " r " \
                          "ON l.ip = r.ip WHERE %s"
#define UPDATE      "UPDATE %s_v" DBVER " SET %s WHERE %s"
#define DELETE      "DELETE FROM %s_v" DBVER " WHERE %s"
#define BSS_DELETE  "DELETE FROM %s WHERE %s"
#define INSERT      "INSERT INTO %s_v" DBVER " (%s) VALUES (%s)"
#define BSS_INSERT      "INSERT INTO %s (%s) VALUES (%s)"
#define INSSEL      "INSERT INTO %s_v" DBVER "(%s) " \
                    "SELECT %s FROM %s_v" DBVER " WHERE %s"

/* use single quote ONLY in parameters */
#define db_dump_cond(table, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), SELECT, columns, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump(table, columns) \
    db_dump_cond(table, columns, "true")

/* use single quote ONLY in parameters */
#define db_dump_cond_minimal(table, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD_raw, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), SELECT_cat, columns, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_minimal(table, column) \
    db_dump_cond_minimal(table, column, "true")

/* use single quote ONLY in parameters */
#define db_dump_sr_cond(alias1, alias2, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_SR, \
                columns, alias1, alias2, alias1, alias2, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_sr(alias1, alias2, columns) \
    db_dump_sr_cond(alias1, alias2, columns, "true")

#define db_dump_ha_sr_cond(alias1, alias2, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_HA_SR, \
                columns, alias1, alias2, alias1, alias2, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_ha_sr(alias1, alias2, columns) \
    db_dump_ha_sr_cond(alias1, alias2, columns, "true")

#define db_dump_pool_ps_cond(alias1, alias2, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_POOL_PS, \
                columns, alias1, alias2, alias1, alias2, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_pool_ps(alias1, alias2, columns) \
    db_dump_pool_ps_cond(alias1, alias2, columns, "true")

#define db_dump_ip_cond(alias1, alias2, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_IP, \
                columns, alias1, alias2, alias1, alias2, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_vul_cond(alias1, alias2, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_VUL, \
                columns, alias1, alias2, alias1, alias2, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_vul(alias1, alias2, columns) \
    db_dump_pool_vul_cond(alias1, alias2, columns, "true")

#define db_dump_hwdev_cond(columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_HWDEV_CON, \
                columns, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_dump_host_cond(columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DUMP_HOST_CON, \
                columns, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_select(buf, len, cnt, table, column, cond) \
    do { \
        FILE *fp; \
        char *__p; \
        int __n; \
        char __cmd[BUFSIZE]; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD_raw, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), SELECT, column, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        if ((fp = popen(__cmd, "r")) == 0) { \
            break; \
        } \
        __n = 0; \
        while (__n < cnt) { \
            __p = buf[__n]; \
            if (fgets(__p, len, fp) == 0) { \
                break; \
            } \
            while (*__p++); \
            if (*(__p - 2) == '\n') { \
                *(__p - 2) = 0; \
            } \
            ++__n; \
        } \
        if (fp) {pclose(fp);} \
    } while (0)

#define bss_db_select(buf, len, cnt, table, column, cond) \
    do { \
        FILE *fp; \
        char *__p; \
        int __n; \
        char __cmd[BUFSIZE]; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, BSSDBCMD_raw, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), BSS_SELECT, column, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        if ((fp = popen(__cmd, "r")) == 0) { \
            break; \
        } \
        __n = 0; \
        while (__n < cnt) { \
            __p = buf[__n * len]; \
            if (fgets(__p, len, fp) == 0) { \
                break; \
            } \
            while (*__p++); \
            if (*(__p - 2) == '\n') { \
                *(__p - 2) = 0; \
            } \
            ++__n; \
        } \
        if (fp) {pclose(fp);} \
    } while (0)

#define db_update(table, columns, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), UPDATE, table, columns, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_delete(table, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), DELETE, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_insert(table, columns, values) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), INSERT, table, columns, values); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define db_insert_select(table_to, columns_to, table_from, columns_from, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, DBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), INSSEL, table_to, columns_to, columns_from, table_from, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define bss_db_delete(table, cond) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, BSSDBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), BSS_DELETE, table, cond); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#define bss_db_insert(table, columns, values) \
    do { \
        char __cmd[BUFSIZE]; \
        char *__p; \
        memset(__cmd, 0, sizeof(__cmd)); \
        strncpy(__cmd, BSSDBCMD, BUFSIZE - 1); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        __p = __cmd; \
        while (*__p++); \
        --__p; \
        snprintf(__p, BUFSIZE - strlen(__cmd), BSS_INSERT, table, columns, values); \
        strncat(__cmd, "\"", BUFSIZE - strlen(__cmd) - 1); \
        system(__cmd); \
    } while (0)

#endif
