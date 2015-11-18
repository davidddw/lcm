#include <string.h>
#include <stdlib.h>

#include "vm/vm_host.h"
#include "mysql.h"
#include "lc_db.h"
#include "lc_db_host.h"

extern MYSQL *conn_handle;

static int cluster_process(void *cluster_info, char *field, char *value)
{
    cluster_t *p = (cluster_t *) cluster_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "vc_datacenter_id") == 0) {
        p->vcdc_id = atoi(value);
    }

    return 0;
}

static int rack_process(void *rack_info, char *field, char *value)
{
    rack_t *p = (rack_t *) rack_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "clusterid") == 0) {
        p->cluster_id = atoi(value);
    }

    return 0;
}

static int vcdc_process(void *vcdc_info, char *field, char *value)
{
    vcdc_t *p = (vcdc_t *) vcdc_info;
    if (strcmp(field, "id") == 0) {
        p->id = atoi(value);
    } else if (strcmp(field, "vswitch_name") == 0) {
        strncpy(p->vswitch_name, value, LC_NAMESIZE - 1);
    }

    return 0;
}

static int host_device_process(void *host_info, char *field, char *value)
{
    host_t* phost = (host_t*) host_info;

    /* TODO: Add or remove columns */
    if (strcmp(field, "ip") == 0) {
        strcpy(phost->host_address, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(phost->host_name, value);
    } else if (strcmp(field, "state") == 0) {
        phost->host_state = atoi(value);
    } else if (strcmp(field, "htype") == 0) {
        phost->host_htype = atoi(value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(phost->host_description, value);
    } else if (strcmp(field, "user_name") == 0) {
        strcpy(phost->username, value);
    } else if (strcmp(field, "user_passwd") == 0) {
        strcpy(phost->password, value);
    } else if (strcmp(field, "pool") == 0) {
        phost->pool_index = atoi(value);
    } else if (strcmp(field, "inf0type") == 0) {
        phost->if0_type = atoi(value);
    } else if (strcmp(field, "inf1type") == 0) {
        phost->if1_type = atoi(value);
    } else if (strcmp(field, "inf2type") == 0) {
        phost->if2_type = atoi(value);
    } else if (strcmp(field, "cpu_info") == 0) {
        strcpy(phost->cpu_info, value);
    } else if (strcmp(field, "mem_total") == 0) {
        phost->mem_total = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        phost->disk_total = atoi(value);
    } else if (strcmp(field, "vcpu_num") == 0) {
        phost->vcpu_num = atoi(value);
    } else if (strcmp(field, "type") == 0) {
        phost->host_role = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        phost->rack_id = atoi(value);
    } else if (strcmp(field, "vmware_cport") == 0) {
        phost->vmware_cport = atoi(value);
    } else if (strcmp(field, "master_ip") == 0) {
        strcpy(phost->master_address, value);
    }

    return 0;
}

static int compute_resource_process(void *host_info, char *field, char *value)
{
    host_t* phost = (host_t*) host_info;

    /* TODO: Add or remove columns */
    if (strcmp(field, "id") == 0) {
        phost->cr_id = atoi(value);
    } else if (strcmp(field, "ip") == 0) {
        strcpy(phost->host_address, value);
    } else if (strcmp(field, "dbr_peer_server") == 0) {
        strcpy(phost->peer_address, value);
    } else if (strcmp(field, "cpu_info") == 0) {
        strcpy(phost->cpu_info, value);
    } else if (strcmp(field, "name") == 0) {
        strcpy(phost->host_name, value);
    } else if (strcmp(field, "flag") == 0) {
        phost->host_flag = atoi(value);
    } else if (strcmp(field, "state") == 0) {
        phost->host_state = atoi(value);
    } else if (strcmp(field, "description") == 0) {
        strcpy(phost->host_description, value);
    } else if (strcmp(field, "uuid") == 0) {
        strcpy(phost->host_uuid, value);
    } else if (strcmp(field, "user_name") == 0) {
        strcpy(phost->username, value);
    } else if (strcmp(field, "user_passwd") == 0) {
        strcpy(phost->password, value);
    } else if (strcmp(field, "max_vm") == 0) {
        phost->max_vm = atoi(value);
    } else if (strcmp(field, "poolid") == 0) {
        phost->pool_index = atoi(value);
    } else if (strcmp(field, "role") == 0) {
        phost->role = atoi(value);
    } else if (strcmp(field, "health_state") == 0) {
        phost->health_check = atoi(value);
    } else if (strcmp(field, "errno") == 0) {
        phost->error_number = atoi(value);
    } else if (strcmp(field, "cpu_start_time") == 0) {
        phost->cpu_start_time = atol(value);
    } else if (strcmp(field, "disk_start_time") == 0) {
        phost->disk_start_time = atol(value);
    } else if (strcmp(field, "traffic_start_time") == 0) {
        phost->traffic_start_time = atol(value);
    } else if (strcmp(field, "check_start_time") == 0) {
        phost->check_start_time = atol(value);
    } else if (strcmp(field, "stats_time") == 0) {
        phost->stats_time = atol(value);
    } else if (strcmp(field, "mem_total") == 0) {
        phost->mem_total = atoi(value);
    } else if (strcmp(field, "disk_total") == 0) {
        phost->disk_total = atoi(value);
    } else if (strcmp(field, "service_flag") == 0) {
        phost->host_service_flag = atoi(value);
    } else if (strcmp(field, "rackid") == 0) {
        phost->rack_id = atoi(value);
    } else if (strcmp(field, "master_ip") == 0) {
        strcpy(phost->master_address, value);
    }
    return 0;
}

static int network_resource_process(void *host_info, char *field, char *value)
{
    return 0;
}


static int get_compute_resource_state(void *state, char *field, char *value)
{
    uint32_t* host_state = (uint32_t *) state;
    if (strcmp(field, "state") == 0) {
        *host_state = atoi(value);
    }
    return 0;
}

int lc_db_host_device_load(host_t* host_info, char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "host_device", column_list, condition, host_device_process);
}

int lc_db_host_device_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("host_device", column, value, condition);
}

int lc_db_master_host_device_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("host_device", column, value, condition);
}

int lc_db_host_port_config_insert(char *column_list, char *value_list)
{
    return lc_db_table_insert("host_port_config", column_list,value_list);
}

int lc_db_host_device_multi_update(char *columns_update, char *condition)
{
    return lc_db_table_multi_update("host_device", columns_update, condition);
}

int lc_db_compute_resource_multi_update(char * columns, char * condition)
{
    return lc_db_table_multi_update("compute_resource", columns, condition);
}

int lc_db_master_compute_resource_multi_update(char * columns, char * condition)
{
    return lc_db_master_table_multi_update("compute_resource", columns, condition);
}

int lc_db_compute_resource_update(char *column, char *value, char *condition)
{
    return lc_db_table_update("compute_resource", column, value, condition);
}

int lc_db_master_compute_resource_update(char *column, char *value, char *condition)
{
    return lc_db_master_table_update("compute_resource", column, value, condition);
}

int lc_db_compute_resource_load(host_t* host_info, char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "compute_resource", column_list, condition, compute_resource_process);
}

int lc_db_network_resource_load(host_t* host_info, char *column_list, char *condition)
{
    return lc_db_table_load(host_info, "network_resource", column_list, condition, network_resource_process);
}

int lc_db_compute_resource_load_all(host_t* host_info, char *column_list, char *condition, int offset, int *host_num)
{
    return lc_db_table_load_all(host_info, "compute_resource", column_list, condition, offset, host_num, compute_resource_process);
}

int lc_db_compute_resource_load_all_v2(host_t* host_info, char *column_list, char *condition, int offset, int *host_num)
{
    return lc_db_table_loadn_all(host_info, "compute_resource", column_list,
            condition, offset, host_num, MAX_HOST_NUM, compute_resource_process);
}

int lc_db_host_device_load_all(host_t* host_info, char *column_list, char *condition, int offset, int *host_num)
{
    return lc_db_table_load_all(host_info, "host_device", column_list, condition, offset, host_num, host_device_process);
}

int lc_db_host_device_load_all_v2(host_t* host_info, char *column_list, char *condition, int offset, int *host_num)
{
    return lc_db_table_loadn_all(host_info, "host_device", column_list,
            condition, offset, host_num, MAX_HOST_NUM, host_device_process);
}

int lc_db_cluster_load(cluster_t* cluster_info, char *column_list, char *condition)
{
    return lc_db_table_load(cluster_info, "cluster", column_list, condition,
            cluster_process);
}

int lc_db_rack_load(rack_t* rack_info, char *column_list, char *condition)
{
    return lc_db_table_load(rack_info, "rack", column_list, condition,
            rack_process);
}

int lc_db_vcdc_load(vcdc_t* vcdc_info, char *column_list, char *condition)
{
    return lc_db_table_load(vcdc_info, "vc_datacenter", column_list, condition,
            vcdc_process);
}

int lc_vmdb_host_device_load (host_t *host, char *ip, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_device_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_load_by_id(host_t *host, int id, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_host_device_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_check_host_device_exist (char *ip)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_host_device_load(NULL, "*", condition);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error condition=%s \n", __FUNCTION__, condition);
        return result;
    } else if (result == LC_DB_EMPTY_SET)
        return LC_DB_HOST_NOT_FOUND;
    else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_info_update(host_t *host, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "name='%s', cpu_info='%s', mem_total='%d', disk_total='%d', vcpu_num='%d', master_ip='%s'",
        host->host_name, host->cpu_info, host->mem_total, host->disk_total, host->vcpu_num, host->master_address);

    if (lc_db_host_device_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_host_device_iftype_update(host_t *host, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "inf0type=%u, inf1type=%u, inf2type=%u",
        host->if0_type, host->if1_type, host->if2_type);

    if (lc_db_host_device_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_host_device_state_update(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", state);

    if (lc_db_host_device_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_host_device_state_update(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", state);

    if (lc_db_master_host_device_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_host_device_vmware_console_port_update(int cport, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", cport);

    if (lc_db_host_device_update("vmware_cport", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_host_device_errno_update(int err, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", err);

    if (lc_db_host_device_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}


int lc_vmdb_host_port_config_insert(char *host_ip,char *eth_name,char *eth_mac)
{
    char column_list[LC_DB_BUF_LEN];
    char value_list[LC_DB_BUF_LEN];

    memset(column_list, 0, LC_DB_BUF_LEN);
    memset(value_list, 0, LC_DB_BUF_LEN);

    sprintf(column_list, "host_ip,name,mac");
    sprintf(value_list, "'%s','%s','%s'", host_ip,eth_name,eth_mac);

    if (lc_db_host_port_config_insert(column_list, value_list) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_uuid_update(char *uuid, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "'%s'", uuid);

    if (lc_db_compute_resource_update("uuid", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_disk_update(int disk, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", disk);

    if (lc_db_compute_resource_update("disk_total", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_memory_update(int memory, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", memory);

    if (lc_db_compute_resource_update("mem_total", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_compute_resource_state_update(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", state);

    if (lc_db_compute_resource_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_compute_resource_state_update(int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", state);

    if (lc_db_master_compute_resource_update("state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_errno_update(int err, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", err);

    if (lc_db_compute_resource_update("errno", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_usage_update(char *cpu_usage, int cpu_used,
                                          int mem_usage, int disk_usage, char *ip)
{
#if 0
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "cpu_usage=\'%s\', cpu_used=\'%d\', mem_usage=\'%d\', disk_usage=\'%d\'",
            cpu_usage, cpu_used, mem_usage, disk_usage);

    if (lc_db_compute_resource_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
#endif
    return 0;
}

int lc_vmdb_master_compute_resource_time_update(time_t cpu_start_time,
                                         time_t disk_start_time,
                                         time_t check_start_time, char *ip)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "cpu_start_time = \'%ld\', disk_start_time = \'%ld\', check_start_time = \'%ld\'",
                     cpu_start_time, disk_start_time, check_start_time);

    if (lc_db_master_compute_resource_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_compute_resource_stats_time_update(time_t stats_time, char *ip)
{
    int buf_len = LC_DB_BUF_LEN*2;
    char condition[LC_DB_BUF_LEN];
    char buffer[buf_len];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, buf_len);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%ld", stats_time);

    if (lc_db_master_compute_resource_update("stats_time", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

int lc_vmdb_load_compute_resource_state(int *value, char *ip)
{
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    if (lc_db_table_load((void*)value, "compute_resource", "state", condition,
        get_compute_resource_state) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_master_compute_resource_state_multi_update(int state, int error_number, int healthcheck, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "state = \'%d\', errno=\'%d\'", state, error_number);

    if (lc_db_master_compute_resource_multi_update(buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;

}

int lc_vmdb_master_compute_resource_update_health_check(int healthcheck, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", healthcheck);

    if (lc_db_master_compute_resource_update("health_state", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;

}

int lc_vmdb_compute_resource_flag_update(int flag, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "%d", flag);

    if (lc_db_compute_resource_update("flag", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_master_ip_update(char *master_ip, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "'%s'", master_ip);

    if (lc_db_compute_resource_update("master_ip", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_compute_resource_load (host_t *host, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_compute_resource_load(host, "*", condition);
    if (result == -1) {
        LC_DB_LOG(LOG_ERR, "%s: db thread is not enough", __FUNCTION__);
        return result;
    }
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_by_col (host_t *host, char *ip, char *columns)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_compute_resource_load(host, columns, condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_network_resource_load(host_t *host, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);

    result = lc_db_network_resource_load(host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_by_id(host_t *host, int id)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", id);

    result = lc_db_compute_resource_load(host, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_all (host_t *host, int offset, int *host_num)
{
    int result;

    result = lc_db_compute_resource_load_all(host, "*", "", offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "lc_vmdb_compute_resource_load_all: error");
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_by_domain (host_t *host, int offset, int *host_num, char *domain)
{
    int result;
    char condition[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "domain='%s'", domain);
    result = lc_db_compute_resource_load_all(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "lc_vmdb_compute_resource_load_all: error");
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_all_v2 (host_t *host, int offset, int *host_num)
{
    int result;

    result = lc_db_compute_resource_load_all_v2(host, "*", "", offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "lc_vmdb_compute_resource_load_all_v2: error");
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_compute_resource_load_all_v2_by_poolid (host_t *host, int offset,
        int *host_num, int poolid)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "poolid=%d", poolid);

    result = lc_db_compute_resource_load_all_v2(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "lc_vmdb_compute_resource_load_all_v2: error");
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_load_all (host_t *host, int offset, int *host_num, int poolid)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "poolid=%d", poolid);


    result = lc_db_host_device_load_all(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_load_all_v2 (host_t *host, int offset, int *host_num, int poolid)
{
    char condition[LC_DB_BUF_LEN];
    int result;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "poolid=%d", poolid);


    result = lc_db_host_device_load_all_v2(host, "*", condition, offset, host_num);

    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        return LC_DB_HOST_NOT_FOUND;
    } else {
        return LC_DB_HOST_FOUND;
    }
}

int lc_vmdb_host_device_master_update(char *master_ip, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(buffer, "'%s'", master_ip);

    if (lc_db_host_device_update("master_ip", buffer, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vcdc_load_by_rack_id(vcdc_t *vcdc, int rack_id)
{
    char condition[LC_DB_BUF_LEN];
    int result;
    rack_t rack;
    cluster_t cluster;

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", rack_id);
    memset(&rack, 0, sizeof rack);
    result = lc_db_rack_load(&rack, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: rack (%d) not found\n", __FUNCTION__, rack_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", rack.cluster_id);
    memset(&cluster, 0, sizeof cluster);
    result = lc_db_cluster_load(&cluster, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: cluster (%d) not found\n", __FUNCTION__,
                rack.cluster_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    memset(condition, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id=%d", cluster.vcdc_id);
    result = lc_db_vcdc_load(vcdc, "*", condition);
    if (result == LC_DB_COMMON_ERROR) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return result;
    } else if (result == LC_DB_EMPTY_SET) {
        LC_DB_LOG(LOG_ERR, "%s: vcdc (%d) not found\n", __FUNCTION__,
                cluster.vcdc_id);
        return LC_DB_VCDC_NOT_FOUND;
    }

    return LC_DB_VCDC_FOUND;
}
