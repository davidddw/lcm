#include <string.h>
#include <stdlib.h>

#include "mysql.h"
#include "lc_db.h"

static int sysconf_process(void *str, char *field, char *value)
{
    char *p = (char *)str;

    if (strcmp(field, "value") == 0) {
        strncpy(p, value, LC_DB_BUF_LEN - 1);
    }

    return 0;
}

int lc_sysdb_get_configuration(char *module_name, char *param_name, char *value, int size)
{
    char buf[LC_DB_BUF_LEN];
    char cond[LC_DB_BUF_LEN];
    int result;

    memset(cond, 0, sizeof(cond));
    snprintf(cond, LC_DB_BUF_LEN, "module_name='%s' and param_name='%s'",
            module_name, param_name);
    memset(buf, 0, sizeof(buf));
    result = lc_db_table_load(buf, "sys_configuration",
            "value", cond, sysconf_process);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, cond);
        return result;
    }
    strncpy(value, buf, size - 1);

    return 0;
}

int lc_sysdb_get_domain(char *value, int size)
{
    return lc_sysdb_get_configuration("SYSTEM", "domain", value, size);
}

int lc_sysdb_get_lcm_ip(char *value, int size)
{
    return lc_sysdb_get_configuration("SYSTEM", "lcm_ip", value, size);
}

int lc_sysdb_get_running_mode(char *value, int size)
{
    return lc_sysdb_get_configuration("SYSTEM", "running_mode", value, size);
}
