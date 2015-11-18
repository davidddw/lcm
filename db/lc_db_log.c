#include "lc_db.h"
#include "lc_db_log.h"

#define MOD_RM_STR                  "RM"
#define MOD_VMDRIVER_STR            "VMDRIVER"
#define MOD_LCPD_STR                "LCPD"
#define MOD_LCMOND_STR              "MONITOR"

int lc_db_log_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("log", column_list, value_list);
}

int lc_db_log(char *module, char *action, char *loginfo, int level)
{
    char buffer[LC_DB_BUF_LEN];

    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(buffer, LC_DB_BUF_LEN, "'%s','%s','%s',%d", module, action, loginfo, level);

    LC_DB_LOG(LOG_DEBUG, "%s: buf=%s", __FUNCTION__, buffer);
    if (lc_db_log_insert(LC_DB_LOG_COL_STR, buffer)) {
        LC_DB_LOG(LOG_ERR, "%s: error", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdriver_db_log(char *action, char *loginfo, int level)
{
    return lc_db_log(MOD_VMDRIVER_STR, action, loginfo, level);
}

int lc_monitor_db_log(char *action, char *loginfo, int level)
{
    return lc_db_log(MOD_LCMOND_STR, action, loginfo, level);
}

int lc_db_syslog_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("syslog", column_list, value_list);
}

int lc_db_syslog(char *module, char *action, char* otype, int oid, char* oname,
                 char *loginfo, int level)
{
    char info[LC_NAMEDES];
    char buffer[LC_DB_BUF_LEN];

    memset(info, 0, LC_NAMEDES);
    snprintf(info, LC_NAMEDES, "%s %s: %s", action, oname, loginfo);
    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(buffer, LC_DB_BUF_LEN, "'%s','%s','%s',%d,'%s',\"%s\",%d",
             module, action, otype, oid, oname,info, level);

    LC_DB_LOG(LOG_DEBUG, "%s: db buf=%s\n", __FUNCTION__, buffer);
    if (lc_db_syslog_insert(LC_DB_SYSLOG_COL_STR, buffer)) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_rm_db_syslog(char *action, char* otype, int oid, char* oname,
                    char *loginfo, int level)
{
    return lc_db_syslog(MOD_RM_STR, action, otype, oid, oname, loginfo, level);
}

int lc_vmdriver_db_syslog(char *action, char* otype, int oid, char* oname,
                          char *loginfo, int level)
{
    return lc_db_syslog(MOD_VMDRIVER_STR, action, otype, oid, oname, loginfo, level);
}

int lc_monitor_db_syslog(char *action, char* otype, int oid, char* oname,
                         char *loginfo, int level)
{
    return lc_db_syslog(MOD_LCMOND_STR, action, otype, oid, oname, loginfo, level);
}

int lc_lcpd_db_syslog(char *action, char* otype, int oid, char* oname,
                      char *loginfo, int level)
{
    return lc_db_syslog(MOD_LCPD_STR, action, otype, oid, oname, loginfo, level);
}
