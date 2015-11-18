#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "vm/vm_host.h"
#include "mysql.h"
#include "lc_db.h"
#include "lc_db_res_usg.h"

extern MYSQL *conn_handle;

int lc_db_host_res_usg_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("host_usage", column_list, value_list);
}

int lc_db_vnet_res_usg_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vedge_usage", column_list, value_list);
}

int lc_db_vm_res_usg_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("vm_usage", column_list, value_list);
}

int lc_vmdb_host_res_usg_insert(host_res_usg_t *phost_res)
{
    char db_buffer[MAX_RES_USAGE_DB_BUFFER];

    memset(db_buffer, 0, MAX_RES_USAGE_DB_BUFFER);
    sprintf(db_buffer, "\'%d\',\'%s\',\"%s\",\"%s\",\'%d\',\'%d\',\'%d\',\'%d\'",
                        phost_res->cr_id, phost_res->host_name, phost_res->cpu_info, phost_res->cpu_usage,
                        phost_res->mem_total, phost_res->mem_usage, phost_res->disk_total, phost_res->disk_usage);
    if (lc_db_host_res_usg_insert(LC_DB_HOST_USG_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_res_usg_insert(vm_res_usg_t *pvm_res)
{
    char db_buffer[MAX_RES_USAGE_DB_BUFFER];

    memset(db_buffer, 0, MAX_RES_USAGE_DB_BUFFER);
    sprintf(db_buffer, "\'%d\',\'%s\',\'%s\',\"%s\",\'%u\',\'%u\',\'%d\',\'%"PRIu64"\',\'%d\',\'%"PRIu64"\'",
                        pvm_res->vm_id, pvm_res->vm_label,pvm_res->cpu_info,
                        pvm_res->cpu_usage, pvm_res->mem_total, pvm_res->mem_usage,
                        pvm_res->system_disk_total, pvm_res->system_disk_usage,
                        pvm_res->user_disk_total, pvm_res->user_disk_usage);
    if (lc_db_vm_res_usg_insert(LC_DB_VM_USG_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}
