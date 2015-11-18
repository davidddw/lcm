#include <string.h>
#include <stdlib.h>

#include "mysql.h"
#include "lc_db.h"
#include "message/msg_lcm2dri.h"
#include "vm/vm_host.h"

extern MYSQL *conn_handle;

static int storage_process(void *info, char *field, char *value)
{
    storage_info_t *psr = (storage_info_t *)info;

    if (strcmp(field, "id") == 0) {
        psr->id = atoi(value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(psr->name_label, value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(psr->uuid, value);
    } else if (strcmp(field, "is_shared") == 0) {
        psr->is_shared = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        psr->disk_total = atoi(value);
    } else if (strcmp(field, "disk_used") == 0) {
        psr->disk_used = atoi(value);
    }

    return 0;
}

static int template_os_process(void *info, char *field, char *value)
{
    template_os_info_t *p = (template_os_info_t *)info;

    if (strcmp(field, "server_ip") == 0) {
        strcpy(p->server_ip, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(p->vm_template, value);
    } else if (strcmp(field, "type") == 0) {
        p->type = atoi(value);
    } else if (strcmp(field, "htype") == 0) {
        p->htype = atoi(value);
    } else if (strcmp(field, "server_state") == 0) {
        p->state = atoi(value);
    }

    return 0;
}

int lc_db_master_storage_multi_update(char * columns, char * condition)
{
    return lc_db_master_table_multi_update("storage", columns, condition);
}

static int storage_connection_process(void *info, char *field, char *value)
{
    int *p = (int *)info;

    if (strcmp(field, "storage_id") == 0) {
        *p = atoi(value);
    }

    return 0;
}

static int storage_uuid_process(void *uuid, char *field, char *value)
{
    char *p = (char *)uuid;

    if (strcmp(field, "uuid") == 0) {
        strcpy(p, value);
    }

    return 0;
}

static int storage_resource_process(void *sr_info, char *field, char *value)
{
    storage_resource_t* psr = (storage_resource_t*) sr_info;

    if (strcmp(field, "id") == 0) {
        psr->storage_id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        psr->storage_state = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        psr->storage_error = atoi(value);
    } else if (strcmp(field, "attachip") == 0) {
        strcpy(psr->host_address, value);
    } else if (strcmp(field, "type") == 0) {
        psr->storage_type = atoi(value);
    } else if (strcmp(field, "private") == 0) {
        strcpy(psr->private, value);
    } else if (strcmp(field, "sr_name") == 0) {
        strcpy(psr->sr_name, value);
    } else if (strcmp(field, "sr_uuid") == 0) {
        strcpy(psr->sr_uuid, value);
    }

    return 0;
}

static int shared_storage_resource_process(void *ssr_info, char *field, char *value)
{
    shared_storage_resource_t *pssr = (shared_storage_resource_t *)ssr_info;

    if (strcmp(field, "id") == 0) {
        pssr->storage_id = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        pssr->storage_state = atoi(value);
    } else if (strcmp(field, "flag") == 0) {
        pssr->storage_flag = atoi(value);
    } else if (strcmp(field, "htype") == 0) {
        pssr->storage_htype = atoi(value);
    } else if (strcmp(field, "sr_name") == 0) {
        strncpy(pssr->sr_name, value, MAX_SR_NAME_LEN - 1);
    } else if (strcmp(field, "disk_total") == 0) {
        pssr->disk_total = atoi(value);
    } else if (strcmp(field, "disk_used") == 0) {
        pssr->disk_used = atoi(value);
    }

    return 0;
}

int lc_db_storage_resource_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("storage_resource", column, value, condition);
}

int lc_db_storage_resource_load(storage_resource_t* sr_info, char *column_list, char *condition)
{
    return lc_db_table_load(sr_info, "storage_resource", column_list, condition, storage_resource_process);
}

int lc_db_storage_resource_delete(char *condition)
{
    return lc_db_table_delete("storage_resource", condition);
}

int lc_db_shared_storage_resource_load(shared_storage_resource_t *sr_info, char *column_list, char *condition)
{
    return lc_db_table_load(sr_info, "shared_storage_resource", column_list, condition, shared_storage_resource_process);
}

int lc_db_shared_storage_resource_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("shared_storage_resource", column_list, value_list);
}

int lc_db_snapshot_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("snapshot", column_list, value_list);
}

int lc_db_snapshot_delete(char *condition)
{
    return lc_db_table_delete("snapshot", condition);
}

int lc_db_snapshot_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("snapshot", column, value, condition);
}

int lc_db_snapshot_dump(char *column_list)
{
    return lc_db_table_dump("snapshot", column_list);
}

int lc_db_xva_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("xva", column_list, value_list);
}

int lc_db_xva_delete(char *condition)
{
    return lc_db_table_delete("xva", condition);
}

int lc_db_xva_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("xva", column, value, condition);
}

int lc_db_xva_dump(char *column_list)
{
    return lc_db_table_dump("xva", column_list);
}

int lc_db_storage_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("storage", column_list, value_list);
}

int lc_db_storage_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("storage", column, value, condition);
}

int lc_db_storage_delete(char *condition)
{
    return lc_db_table_delete("storage", condition);
}

int lc_db_storage_load(storage_info_t* info, char *column_list, char *condition)
{
    return lc_db_table_load(info, "storage", column_list, condition, storage_process);
}

int lc_db_ha_storage_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("ha_storage", column_list, value_list);
}

int lc_db_master_ha_storage_multi_update(char * columns, char * condition)
{
    return lc_db_master_table_multi_update("ha_storage", columns, condition);
}

int lc_db_ha_storage_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("ha_storage", column, value, condition);
}

int lc_db_ha_storage_delete(char *condition)
{
    return lc_db_table_delete("ha_storage", condition);
}

int lc_db_ha_storage_load(storage_info_t* info, char *column_list, char *condition)
{
    return lc_db_table_load(info, "ha_storage", column_list, condition, storage_process);
}

int lc_db_template_os_load(template_os_info_t* info, char *column_list, char *condition)
{
    return lc_db_table_load(info, "template_os", column_list, condition, template_os_process);
}

int lc_db_master_template_os_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("template_os", column, value, condition);
}

int lc_db_storage_connection_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("storage_connection", column_list, value_list);
}

int lc_db_storage_connection_delete(char *condition)
{
    return lc_db_table_delete("storage_connection", condition);
}

int lc_db_storage_connection_check(char *condition)
{
    return lc_db_table_load(0, "storage_connection", "*", condition, 0);
}

int lc_db_ha_storage_connection_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("ha_storage_connection", column_list, value_list);
}

int lc_db_ha_storage_connection_delete(char *condition)
{
    return lc_db_table_delete("ha_storage_connection", condition);
}

int lc_db_ha_storage_connection_check(char *condition)
{
    return lc_db_table_load(0, "ha_storage_connection", "*", condition, 0);
}

int lc_storagedb_snapshot_insert (struct msg_request_vm_snapshot* vm, char* sname, char* serverip)
{
    char db_buffer[MAX_STORAGE_DB_BUFFER];

    memset(db_buffer, 0, MAX_STORAGE_DB_BUFFER);
    sprintf(db_buffer, "'%s','%d','%d','%d','%s','%s'",
        sname, vm->vl2_id, vm->vnet_id, vm->vm_id, vm->description, serverip);

    if (lc_db_snapshot_insert(LC_DB_SNAPSHOT_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_storagedb_backup_insert (struct msg_request_vm_backup* vm, char* backup_name, char* serverip, char* path)
{
    char db_buffer[MAX_STORAGE_DB_BUFFER];

    memset(db_buffer, 0, MAX_STORAGE_DB_BUFFER);
    sprintf(db_buffer, "'%s','%d','%d','%d','%s','%s', '%s'",
        backup_name, vm->vl2_id, vm->vnet_id, vm->vm_id, vm->description, serverip, path);

    if (lc_db_xva_insert(LC_DB_XVA_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_storage_resource_load(storage_resource_t *sr, char *ip)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "attachip='%s'", ip);

    if (lc_db_storage_resource_load(sr, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_delete (char *sr_uuid)
{
    char db_buffer[LC_DB_BUF_LEN];

    memset(db_buffer, 0, LC_DB_BUF_LEN);
    sprintf(db_buffer, "sr_uuid='%s'", sr_uuid);

    if (lc_db_storage_resource_delete(db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_storage_resource_state_update(int state, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%d'", state);

    if (lc_db_storage_resource_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_errno_update(int err, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%d'", err);

    if (lc_db_storage_resource_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_sr_name_update(char *sr_name, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%s'", sr_name);

    if (lc_db_storage_resource_update("sr_name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_sr_uuid_update(char *sr_uuid, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%s'", sr_uuid);

    if (lc_db_storage_resource_update("sr_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_sr_backup_name_update(char *sr_name, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%s'", sr_name);

    if (lc_db_storage_resource_update("sr_backup_name", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_sr_backup_uuid_update(char *sr_uuid, int id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);
    sprintf(buffer, "'%s'", sr_uuid);

    if (lc_db_storage_resource_update("sr_backup_uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_storage_resource_load_by_id(storage_resource_t *sr, int id)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_storage_resource_load(sr, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_STORAGE_NOT_FOUND;
    } else {
        return LC_DB_STORAGE_FOUND;
    }
}

int lc_vmdb_storage_resource_load_by_ip(storage_resource_t *sr, char *ip)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "ip='%s'", ip);

    if (lc_db_storage_resource_load(sr, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

static int lc_db_shared_sr_multi_update(char * columns, char * condition)
{
    return lc_db_table_multi_update("shared_storage_resource", columns, condition);
}

int lc_vmdb_shared_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *sr_name)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "sr_name='%s'", sr_name);
    sprintf(buffer, "disk_total=\'%d\', disk_used=\'%d\'", disk_total, disk_usage);

    if (lc_db_shared_sr_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_shared_storage_resource_check(char *sr_name, int rack_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "sr_name='%s' and rackid=%d", sr_name, rack_id);

    if (lc_db_shared_storage_resource_load(0, "*", condition) != 0) {
        LC_DB_LOG(LOG_DEBUG, "%s: %s not found \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_storagedb_shared_storage_resource_load(shared_storage_resource_t *ssr, char *sr_name, int rack_id)
{
    char condition[LC_DB_BUF_LEN] = {0};

    sprintf(condition, "sr_name='%s' and rackid=%d", sr_name, rack_id);

    if (lc_db_shared_storage_resource_load(ssr, "*", condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_storagedb_shared_storage_resource_insert(shared_storage_resource_t *ssr)
{
    char db_buffer[MAX_STORAGE_DB_BUFFER];

    memset(db_buffer, 0, MAX_STORAGE_DB_BUFFER);
    sprintf(db_buffer, "'%d','%d','%d','%s','%d','%d'",
        ssr->storage_state, ssr->storage_flag, ssr->storage_htype, ssr->sr_name, ssr->disk_total, ssr->rack_id);

    if (lc_db_shared_storage_resource_insert(LC_DB_SSR_COL_STR, db_buffer) != 0) {
        LC_DB_LOG(LOG_ERR, "%s error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_storagedb_host_device_add_storage(host_t *host, storage_info_t *storage)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    storage_info_t storage_info;

    /* check storage exists first by name and rackid */
    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "name='%s' and rack_id=%d and uuid='%s'",
            storage->name_label, host->rack_id, storage->uuid);
    memset(&storage_info, 0, sizeof storage_info);
    if (lc_db_storage_load(&storage_info, "*", condition) != 0) {
        /* not exist, add storage */
        LC_DB_LOG(LOG_DEBUG, "%s: nothing match condition [%s], adding\n",
                __FUNCTION__, condition);
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, LC_DB_BUF_LEN, "'%s','%d','%s','%d','%d','%d'",
                storage->name_label, host->rack_id, storage->uuid,
                storage->is_shared, storage->disk_total, storage->disk_used);
        if (lc_db_storage_insert(LC_DB_STORAGE_COL_STR, buffer) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: insert storage error\n", __FUNCTION__);
            return -1;
        }
        /* get storage id */
        memset(&storage_info, 0, sizeof storage_info);
        if (lc_db_storage_load(&storage_info, "*", condition) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: storage not inserted\n", __FUNCTION__);
            return -1;
        }
    } else if (!storage->is_shared) {
        /* exist but not shared */
        LC_DB_LOG(LOG_ERR, "%s: storage exists\n", __FUNCTION__);
        /* return -1; */
    } else {
        LC_DB_LOG(LOG_DEBUG, "%s: add shared storage\n", __FUNCTION__);
    }

    /* check connection table */
    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "host_address='%s' and storage_id=%d",
            host->host_address, storage_info.id);
    if (lc_db_storage_connection_check(condition) != 0) {
        /* add connection */
        LC_DB_LOG(LOG_DEBUG, "%s: nothing match condition [%s], adding\n",
                __FUNCTION__, condition);
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, LC_DB_BUF_LEN, "'%s','%d'",
                host->host_address, storage_info.id);
        if (lc_db_storage_connection_insert(LC_DB_SC_COL_STR, buffer) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: insert storage connection error\n", __FUNCTION__);
            return -1;
        }
    }

    return 0;
}

int lc_storagedb_host_device_del_storage(host_t *host)
{
    int ids[LC_MAX_SR_NUM];
    char condition[LC_DB_BUF_LEN];
    int ret, i;
    storage_info_t storage_info;

    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "host_address='%s'", host->host_address);
    memset(ids, 0, sizeof ids);
    ret = lc_db_table_loadn(ids, sizeof(int), LC_MAX_SR_NUM, "storage_connection", "storage_id",
            condition, storage_connection_process);
    if (ret == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_DEBUG, "%s: no storage on host %s, skipping\n",
                __FUNCTION__, host->host_address);
        return 0;
    } else if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: db error, condition [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }
    for (i = 0; i < LC_MAX_SR_NUM && ids[i]; ++i) {
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "host_address='%s' and storage_id=%d",
                host->host_address, ids[i]);
        if (lc_db_storage_connection_delete(condition)) {
            LC_DB_LOG(LOG_ERR, "%s: storage connection delete error, condition [%s]\n",
                    __FUNCTION__, condition);
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", ids[i]);
        memset(&storage_info, 0, sizeof storage_info);
        if (lc_db_storage_load(&storage_info, "*", condition) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: no storage, condition [%s]\n",
                    __FUNCTION__, condition);
            continue;
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "storage_id=%d", ids[i]);
        if (storage_info.is_shared && lc_db_storage_connection_check(condition) == 0) {
            /* skip shared and used by other host */
            continue;
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", ids[i]);
        if (lc_db_storage_delete(condition)) {
            LC_DB_LOG(LOG_ERR, "%s: storage delete error, condition [%s]\n",
                    __FUNCTION__, condition);
        }
    }

    return 0;
}

int lc_storagedb_master_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *uuid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "uuid='%s'", uuid);
    sprintf(buffer, "disk_total = \'%d\', disk_used = \'%d\'", disk_total, disk_usage);

    if (lc_db_master_storage_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_master_sr_usage_update_by_name(int disk_total, int disk_usage, int disk_allocated, char *name, int rackid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "name='%s' and rack_id=%d", name, rackid);
    sprintf(buffer, "disk_total = \'%d\', disk_used = \'%d\'", disk_total, disk_usage);

    if (lc_db_master_storage_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_master_ha_sr_usage_update(int disk_total, int disk_usage, int disk_allocated, char *uuid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "uuid='%s'", uuid);
    sprintf(buffer, "disk_total = \'%d\', disk_used = \'%d\'", disk_total, disk_usage);

    if (lc_db_master_ha_storage_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_master_ha_sr_usage_update_by_name(int disk_total, int disk_usage, int disk_allocated, char *name, int rackid)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "name='%s' and rack_id=%d", name, rackid);
    sprintf(buffer, "disk_total = \'%d\', disk_used = \'%d\'", disk_total, disk_usage);

    if (lc_db_master_ha_storage_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_storagedb_host_device_get_storage_uuid(char *uuid, uint32_t len,
        char *address, char *sr_name)
{
    char where_condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int ret;

    memset(uuid, 0, len);
    memset(buffer, 0, sizeof buffer);
    memset(where_condition, 0, sizeof where_condition);
    snprintf(where_condition, LC_DB_BUF_LEN,
            "table1.name='%s' and table2.host_address='%s'",
            sr_name, address);
    ret = lc_db_table_inner_join_load(buffer, "table1.uuid",
            "storage", "storage_connection", "table1.id=table2.storage_id",
            where_condition, storage_uuid_process);
    if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: load error with cond [%s]\n",
                __FUNCTION__, where_condition);
        return -1;
    }

    strncpy(uuid, buffer, len - 1);

    return 0;
}

int lc_hastoragedb_host_device_get_storage_uuid(char *uuid, uint32_t len,
        char *address, char *sr_name)
{
    char where_condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int ret;

    memset(uuid, 0, len);
    memset(buffer, 0, sizeof buffer);
    memset(where_condition, 0, sizeof where_condition);
    snprintf(where_condition, LC_DB_BUF_LEN,
            "table1.name='%s' and table2.host_address='%s'",
            sr_name, address);
    ret = lc_db_table_inner_join_load(buffer, "table1.uuid",
            "ha_storage", "ha_storage_connection", "table1.id=table2.storage_id",
            where_condition, storage_uuid_process);
    if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: load error with cond [%s]\n",
                __FUNCTION__, where_condition);
        return -1;
    }

    strncpy(uuid, buffer, len - 1);

    return 0;
}

int lc_storagedb_host_device_add_ha_storage(host_t *host, storage_info_t *storage)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    storage_info_t storage_info;

    /* check storage exists first by name and rackid */
    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "name='%s' and rack_id=%d and uuid='%s'",
            storage->name_label, host->rack_id, storage->uuid);
    memset(&storage_info, 0, sizeof storage_info);
    if (lc_db_ha_storage_load(&storage_info, "*", condition) != 0) {
        /* not exist, add storage */
        LC_DB_LOG(LOG_DEBUG, "%s: nothing match condition [%s], adding\n",
                __FUNCTION__, condition);
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, LC_DB_BUF_LEN, "'%s','%d','%s','%d','%d','%d'",
                storage->name_label, host->rack_id, storage->uuid,
                storage->is_shared, storage->disk_total, storage->disk_used);
        if (lc_db_ha_storage_insert(LC_DB_STORAGE_COL_STR, buffer) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: insert ha storage error\n", __FUNCTION__);
            return -1;
        }
        /* get storage id */
        memset(&storage_info, 0, sizeof storage_info);
        if (lc_db_ha_storage_load(&storage_info, "*", condition) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: ha storage not inserted\n", __FUNCTION__);
            return -1;
        }
    } else if (!storage->is_shared) {
        /* exist but not shared */
        LC_DB_LOG(LOG_ERR, "%s: ha storage exists\n", __FUNCTION__);
        /* return -1; */
    } else {
        LC_DB_LOG(LOG_DEBUG, "%s: add ha shared storage\n", __FUNCTION__);
    }

    /* check connection table */
    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "host_address='%s' and storage_id=%d",
            host->host_address, storage_info.id);
    if (lc_db_ha_storage_connection_check(condition) != 0) {
        /* add connection */
        LC_DB_LOG(LOG_DEBUG, "%s: nothing match condition [%s], adding\n",
                __FUNCTION__, condition);
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, LC_DB_BUF_LEN, "'%s','%d'",
                host->host_address, storage_info.id);
        if (lc_db_ha_storage_connection_insert(LC_DB_SC_COL_STR, buffer) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: insert ha storage connection error\n", __FUNCTION__);
            return -1;
        }
    }

    return 0;
}

int lc_storagedb_host_device_del_ha_storage(host_t *host)
{
    int ids[MAX_VM_HA_DISK];
    char condition[LC_DB_BUF_LEN];
    int ret, i;
    storage_info_t storage_info;

    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "host_address='%s'", host->host_address);
    memset(ids, 0, sizeof ids);
    ret = lc_db_table_loadn(ids, sizeof(int), MAX_VM_HA_DISK, "ha_storage_connection", "storage_id",
            condition, storage_connection_process);
    if (ret == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_DEBUG, "%s: no ha storage on host %s, skipping\n",
                __FUNCTION__, host->host_address);
        return 0;
    } else if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: db error, condition [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }
    for (i = 0; i < MAX_VM_HA_DISK && ids[i]; ++i) {
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "host_address='%s' and storage_id=%d",
                host->host_address, ids[i]);
        if (lc_db_ha_storage_connection_delete(condition)) {
            LC_DB_LOG(LOG_ERR, "%s: ha storage connection delete error, condition [%s]\n",
                    __FUNCTION__, condition);
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", ids[i]);
        memset(&storage_info, 0, sizeof storage_info);
        if (lc_db_ha_storage_load(&storage_info, "*", condition) != 0) {
            LC_DB_LOG(LOG_ERR, "%s: no ha storage, condition [%s]\n",
                    __FUNCTION__, condition);
            continue;
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "storage_id=%d", ids[i]);
        if (storage_info.is_shared && lc_db_ha_storage_connection_check(condition) == 0) {
            /* skip shared and used by other host */
            continue;
        }
        memset(condition, 0, sizeof condition);
        snprintf(condition, LC_DB_BUF_LEN, "id=%d", ids[i]);
        if (lc_db_ha_storage_delete(condition)) {
            LC_DB_LOG(LOG_ERR, "%s: ha storage delete error, condition [%s]\n",
                    __FUNCTION__, condition);
        }
    }

    return 0;
}

int lc_get_template_info(template_os_info_t *template_info, char *template_os)
{
    char condition[LC_DB_BUF_LEN];
    int ret;

    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "name='%s'", template_os);
    ret = lc_db_template_os_load(template_info, "*", condition);
    if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: load error with cond [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_get_remote_template_info(template_os_info_t *template_info, char *template_os)
{
    char condition[LC_DB_BUF_LEN];
    int ret;

    memset(condition, 0, sizeof condition);
    snprintf(condition, LC_DB_BUF_LEN, "name='%s'&&type=%d", template_os, LC_TEMPLATE_OS_REMOTE);
    ret = lc_db_template_os_load(template_info, "*", condition);
    if (ret) {
        LC_DB_LOG(LOG_WARNING, "%s: load error with cond [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_update_master_template_os_state(int state, char *template_os)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int ret;

    memset(condition, 0, sizeof condition);
    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN, "name='%s'", template_os);
    sprintf(buffer, "'%d'", state);
    ret = lc_db_master_template_os_update("server_state", buffer, condition);
    if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: update error with cond [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }

    return 0;
}

int lc_update_master_template_os_state_by_ip(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];
    int ret;

    memset(condition, 0, sizeof condition);
    memset(buffer, 0, LC_DB_BUF_LEN);
    snprintf(condition, LC_DB_BUF_LEN, "server_ip='%s'",ip);
    sprintf(buffer, "'%d'", state);
    ret = lc_db_master_template_os_update("server_state", buffer, condition);
    if (ret) {
        LC_DB_LOG(LOG_ERR, "%s: update error with cond [%s]\n",
                __FUNCTION__, condition);
        return -1;
    }

    return 0;
}
