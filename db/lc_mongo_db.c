/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Kai WANG
 * Finish Date: 2013-04-07
 *
 */

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "mongoc.h"
#include "bson.h"
#include "lc_mongo_db.h"
#include "agent_sizes.h"

/* the number of mongo connections debug */
#undef MONGO_DEBUG

#define BSON_SYSLOG(level, entry) \
    if (level <= SYSLOG_LEVEL) { \
        bson_print(entry); \
    }

static const char * const mongo_db_tables[] = {
    "",
#define HOST_USAGE_CONVERT(x)    (*((host_usage_t *)x))
    ".host_usage",
#define VM_USAGE_CONVERT(x)      (*((vm_usage_t *)x))
    ".vm_usage",
    ".vgw_usage",
#define TRAFFIC_USAGE_CONVERT(x) (*((traffic_usage_t *)x))
    ".traffic_usage",
    ".table_index",
    ".vgw_server_load",
    ".vm_server_load",
    ".vgw_load",
    ".vm_load",
    ".flow_load",
#define HWDEV_USAGE_CONVERT(x) (*((hwdev_usage_t *)x))
    ".hwdev_usage",
    ".lb_listener_load",
    ".switch_load",
};
#define OLD_MONGO_TABLE_NUM 5

static const char * const mongo_db_tables_branch[] = {
    "_v", "_5mins_v", "_1hour_v", "_1day_v", "_1week_v", "_1month_v", "_1year_v"
};

#define TIMEVAL 60
static const time_t const time_gaps[] = {
    6,
    TIMEVAL*5,
    TIMEVAL*60,
    TIMEVAL*60*24,
    TIMEVAL*60*24*7,
    TIMEVAL*60*24*30,
    TIMEVAL*60*24*30*14 // Used to return 0
};

typedef struct pthread_mongo {
    pthread_t tid;
    mongo conn;
#if MONGO_DEBUG
    int ins_num;
    int access_raw_num;
    int rename_num;
#endif
} pthread_mongo_t;

#define MAX_VM_NUM 32768
#define MAX_THREAD ( 16 + 1 )
static pthread_mutex_t mutex;
static pthread_mongo_t mongo_handler[MAX_THREAD];
static mongo_db_conf_t mongo_db_conf;

/*
static int bson_append_object(bson *b, const char *name, const char *array,
                              const char *array_size_key)
{
    double value = 0;
    char key[32] = {0};
    int ikey, array_size = 0, n, rv = BSON_OK;
    rv += bson_append_start_object(b, name);
    while (strlen(array) > 0) {
        n = sscanf(array, "%d,%lf#", &ikey, &value);
        if (n == 2) {
            sprintf(key, "%d", ikey);
            rv += bson_append_double(b, key, value);
        }
        array_size++;
        array = (const char *)strchr(array, '#');
        if (!array) {
            break;
        }
        array++;
    }
    rv += bson_append_finish_object(b);
    rv += bson_append_int(b, array_size_key, array_size);
    return rv;
}
*/

static int bson_append_array0(bson_t *b, const char *name, const char *array,
                             const char *array_size_key)
{
    double value = 0;
    char key[32] = {0};
    int ikey, array_size = 0, n, rv = BSON_OK;
    rv += bson_append_start_array(b, name);
    while (strlen(array) > 0) {
        n = sscanf(array, "%d,%lf#", &ikey, &value);
        if (n == 2 && ikey >= 0) {
            sprintf(key, "%d", ikey);
            rv += bson_append_double(b, key, value);
        }
        array_size++;
        array = (const char *)strchr(array, '#');
        if (!array) {
            break;
        }
        array++;
    }
    rv += bson_append_finish_array(b);
    rv += bson_append_int(b, array_size_key, array_size);
    return rv;
}

static int bson_append_datum(bson_t *b, const char *name, const double *datum,
                             int size)
{
    char key[32] = {0};
    int n = 0, rv = BSON_OK;
    rv += bson_append_start_array(b, name);
    while (n < size) {
        sprintf(key, "%d", n++);
        rv += bson_append_double(b, key, *datum);
        datum++;
    }
    rv += bson_append_finish_array(b);
    return rv;
}

static int bson_package_host_usage(int idx, bson_t *b, const host_usage_t *entry)
{
    int rv = BSON_OK;
    if (!entry->host_name || !entry->cpu_info || !entry->cpu_usage) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_time_t(b, "timestamp",         entry->timestamp);
    rv += bson_append_int(   b, "cr_id",             entry->cr_id);
    rv += bson_append_string(b, "host_name",         entry->host_name);
    rv += bson_append_string(b, "cpu_info",          entry->cpu_info);
    if (idx != 0) {
        rv += bson_append_array0(b, "cpu_usage",      entry->cpu_usage,
                                                     "cpu_usage_size");
    } else {
        rv += bson_append_datum(b, "cpu_usage",      (const double *)entry->cpu_usage,
                                                     entry->cpu_usage_size);
        rv += bson_append_int(  b, "cpu_usage_size", entry->cpu_usage_size);
    }
    rv += bson_append_long(  b, "cpu_total",         entry->cpu_total);
    rv += bson_append_long(  b, "cpu_allocated",     entry->cpu_allocated);
    rv += bson_append_long(  b, "mem_total",         entry->mem_total);
    rv += bson_append_long(  b, "mem_usage",         entry->mem_usage);
    rv += bson_append_long(  b, "disk_total",        entry->disk_total);
    rv += bson_append_long(  b, "disk_usage",        entry->disk_usage);
    rv += bson_finish(b);
    return rv;
}
static int bson_package_hwdev_usage(bson_t *b, const hwdev_usage_t *entry)
{
    int rv = BSON_OK;
    if (!entry->hwdev_name || !entry->hwdev_ip || !entry->cpu_data) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_int(   b, "id",                entry->id);
    rv += bson_append_int(   b, "userid",            entry->userid);
    rv += bson_append_int(   b, "cpu_num",           entry->cpu_num);
    rv += bson_append_int(   b, "dsk_num",           entry->dsk_num);
    rv += bson_append_int(   b, "mem_size",          entry->mem_size);
    rv += bson_append_int(   b, "mem_used",          entry->mem_used);
    rv += bson_append_time_t(b, "curr_time",         entry->curr_time);

    rv += bson_append_string(b, "hwdev_ip",          entry->hwdev_ip);
    rv += bson_append_string(b, "hwdev_mac",         entry->hwdev_mac);
    rv += bson_append_string(b, "sys_uptime",        entry->sys_uptime);
    rv += bson_append_string(b, "sys_os",            entry->sys_os);
    rv += bson_append_string(b, "cpu_type",          entry->cpu_type);
    rv += bson_append_string(b, "cpu_data",          entry->cpu_data);
    rv += bson_append_string(b, "mem_usage",         entry->mem_usage);
    rv += bson_append_string(b, "dsk_info",          entry->dsk_info);
    rv += bson_append_string(b, "hwdev_name",        entry->hwdev_name);
    rv += bson_append_string(b, "community",         entry->community);
    rv += bson_append_string(b, "data_ip",           entry->data_ip);
    rv += bson_append_string(b, "launch_server",     entry->launch_server);
    rv += bson_append_string(b, "user_name",         entry->user_name);
    rv += bson_append_string(b, "user_passwd",       entry->user_passwd);
    rv += bson_append_string(b, "rack_name",         entry->rack_name);
    rv += bson_finish(b);
    return rv;
}
static int bson_package_flow_load(bson_t *b, const flow_load_t *entry)
{
    int rv = BSON_OK;
    rv += bson_init(b);
    rv += bson_append_time_t(b, "time",              entry->timestamp);
    rv += bson_append_long(  b, "timestamp",         entry->timestamp);
    rv += bson_append_long(  b, "packet_count",      entry->packet_count);
    rv += bson_append_long(  b, "byte_count",        entry->byte_count);
    rv += bson_append_long(  b, "duration",          entry->duration);
    rv += bson_append_long(  b, "used",              entry->used);
    rv += bson_append_long(  b, "datapath",          entry->datapath);
    rv += bson_append_int(   b, "src_port",          entry->src_port);
    rv += bson_append_int(   b, "dst_port",          entry->dst_port);
    rv += bson_append_int(   b, "vlantag",           entry->vlantag);
    rv += bson_append_int(   b, "eth_type",          entry->eth_type);
    rv += bson_append_int(   b, "protocol",          entry->protocol);
    rv += bson_append_int(   b, "direction",         entry->direction);
    rv += bson_append_int(   b, "tcp_flags",         entry->tcp_flags);
    rv += bson_append_string(b, "src_ip",            entry->src_ip);
    rv += bson_append_string(b, "dst_ip",            entry->dst_ip);
    rv += bson_append_string(b, "src_mac",           entry->src_mac);
    rv += bson_append_string(b, "dst_mac",           entry->dst_mac);
    rv += bson_finish(b);
    return rv;
}
static int bson_package_vm_usage(int idx, bson *b, const vm_usage_t *entry)
{
    int rv = BSON_OK;
    if (!entry->vm_name || !entry->cpu_info || !entry->cpu_usage) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_time_t(b, "timestamp",         entry->timestamp);
    rv += bson_append_int(   b, "vm_id",             entry->vm_id);
    rv += bson_append_string(b, "vm_name",           entry->vm_name);
    rv += bson_append_string(b, "cpu_info",          entry->cpu_info);
    if (idx != 0) {
        rv += bson_append_array0(b, "cpu_usage",      entry->cpu_usage,
                                                     "cpu_usage_size");
    } else {
        rv += bson_append_datum(b, "cpu_usage",      (const double *)entry->cpu_usage,
                                                     entry->cpu_usage_size);
        rv += bson_append_int(  b, "cpu_usage_size", entry->cpu_usage_size);
    }
    rv += bson_append_long(  b, "mem_total",         entry->mem_total);
    rv += bson_append_long(  b, "mem_usage",         entry->mem_usage);
    rv += bson_append_long(  b, "system_disk_total", entry->system_disk_total);
    rv += bson_append_long(  b, "system_disk_usage", entry->system_disk_usage);
    rv += bson_append_long(  b, "user_disk_total",   entry->user_disk_total);
    rv += bson_append_long(  b, "user_disk_usage",   entry->user_disk_usage);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_traffic_usage(bson_t *b, const traffic_usage_t *entry)
{
    int rv = BSON_OK;
    if (!entry->vif_name) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_time_t(b, "timestamp",  entry->timestamp);
    rv += bson_append_int(   b, "vif_id",     entry->vif_id);
    rv += bson_append_int(   b, "unused",     entry->unused);
    rv += bson_append_string(b, "vif_name",   entry->vif_name);
    rv += bson_append_long(  b, "rx_bytes",   entry->rx_bytes);
    rv += bson_append_long(  b, "rx_dropped", entry->rx_dropped);
    rv += bson_append_long(  b, "rx_errors",  entry->rx_errors);
    rv += bson_append_long(  b, "rx_packets", entry->rx_packets);
    rv += bson_append_long(  b, "rx_bps",     entry->rx_bps);
    rv += bson_append_long(  b, "rx_pps",     entry->rx_pps);
    rv += bson_append_long(  b, "tx_bytes",   entry->tx_bytes);
    rv += bson_append_long(  b, "tx_dropped", entry->tx_dropped);
    rv += bson_append_long(  b, "tx_errors",  entry->tx_errors);
    rv += bson_append_long(  b, "tx_packets", entry->tx_packets);
    rv += bson_append_long(  b, "tx_bps",     entry->tx_bps);
    rv += bson_append_long(  b, "tx_pps",     entry->tx_pps);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_table_index(bson_t *b, const table_index_t *entry)
{
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_string(b, "table_name", entry->table);
    rv += bson_append_int(   b, "table_type", entry->type);
    rv += bson_append_time_t(b, "start_time", entry->start_time);
    rv += bson_append_time_t(b, "end_time",   entry->end_time);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_vgw_server_load(bson_t *b, const vgw_server_load_t *entry)
{
    int i;
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_string(b, "name",          entry->object.name);
    rv += bson_append_string(b, "server_ip",     entry->object.server_ip);
    rv += bson_append_int(   b, "htype",         entry->object.htype);
    rv += bson_append_long(  b, "timestamp",     entry->timestamp);
    rv += bson_append_time_t(b, "time",          entry->timestamp);

    rv += bson_append_start_object(b, "system");
    rv += bson_append_array0( b, "cpu_usage",     entry->system.cpu_usage,
                                                 "cpu_usage_size");
    rv += bson_append_double(b, "mem_total",     entry->system.mem_total);
    rv += bson_append_double(b, "mem_usage",     entry->system.mem_usage);
    rv += bson_append_double(b, "disk_total",    entry->system.disk_total);
    rv += bson_append_double(b, "disk_usage",    entry->system.disk_usage);
    rv += bson_append_double(b, "io_read",    entry->system.io_read);
    rv += bson_append_double(b, "io_write",    entry->system.io_write);
    rv += bson_append_double(b, "ha_disk_usage",    entry->system.ha_disk_usage);
    rv += bson_append_finish_object(b);

    rv += bson_append_start_array(b, "interfaces");
    for (i=0; i<LC_N_BR_ID; i++) {
        if (i == LC_ULNK_BR_ID) {
            continue;
        }
        rv += bson_append_start_object(b, "");
        rv += bson_append_int(   b, "iftype", entry->traffic[i].iftype);
        rv += bson_append_long(  b, "rx_bps", entry->traffic[i].rx_bps);
        rv += bson_append_long(  b, "tx_bps", entry->traffic[i].tx_bps);
        rv += bson_append_long(  b, "rx_pps", entry->traffic[i].rx_pps);
        rv += bson_append_long(  b, "tx_pps", entry->traffic[i].tx_pps);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_vm_server_load(bson_t *b, const vm_server_load_t *entry)
{
    int i;
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_string(b, "name",          entry->object.name);
    rv += bson_append_string(b, "server_ip",     entry->object.server_ip);
    rv += bson_append_int(   b, "htype",         entry->object.htype);
    rv += bson_append_long(  b, "timestamp",     entry->timestamp);
    rv += bson_append_time_t(b, "time",          entry->timestamp);

    rv += bson_append_start_object(b, "system");
    rv += bson_append_array0( b, "cpu_usage",     entry->system.cpu_usage,
                                                 "cpu_usage_size");
    rv += bson_append_start_object(b, "dom0");
    rv += bson_append_double(b, "mem_total",     entry->system.dom0.mem_total);
    rv += bson_append_double(b, "mem_usage",     entry->system.dom0.mem_usage);
    rv += bson_append_double(b, "disk_total",    entry->system.dom0.disk_total);
    rv += bson_append_double(b, "disk_usage",    entry->system.dom0.disk_usage);
    rv += bson_append_finish_object(b);

    rv += bson_append_start_object(b, "domu");
    rv += bson_append_double(b, "mem_total",     entry->system.domu.mem_total);
    rv += bson_append_double(b, "mem_usage",     entry->system.domu.mem_usage);
    rv += bson_append_double(b, "mem_alloc",     entry->system.domu.mem_alloc);
    rv += bson_append_double(b, "disk_total",    entry->system.domu.disk_total);
    rv += bson_append_double(b, "disk_usage",    entry->system.domu.disk_usage);
    rv += bson_append_double(b, "disk_alloc",    entry->system.domu.disk_alloc);
    rv += bson_append_double(b, "io_read",    entry->system.domu.io_read);
    rv += bson_append_double(b, "io_write",    entry->system.domu.io_write);
    rv += bson_append_finish_object(b);

    rv += bson_append_double(b, "ha_disk_usage",    entry->system.ha_disk_usage);
    rv += bson_append_finish_object(b);

    rv += bson_append_start_array(b, "interfaces");
    for (i=0; i<LC_N_BR_ID; i++) {
        if (i == LC_ULNK_BR_ID) {
            continue;
        }
        rv += bson_append_start_object(b, "");
        rv += bson_append_long(  b, "iftype", entry->traffic[i].iftype);
        rv += bson_append_long(  b, "rx_bps", entry->traffic[i].rx_bps);
        rv += bson_append_long(  b, "tx_bps", entry->traffic[i].tx_bps);
        rv += bson_append_long(  b, "rx_pps", entry->traffic[i].rx_pps);
        rv += bson_append_long(  b, "tx_pps", entry->traffic[i].tx_pps);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_vgw_load(bson_t *b, const vgw_load_t *entry)
{
    int i;
    //char key[] = "vif_xxx";
    int rv = BSON_OK;
    int64_t rx_bps = 0, tx_bps = 0, rx_pps = 0, tx_pps = 0;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_long(  b, "timestamp",      entry->timestamp);
    rv += bson_append_time_t(b, "time",           entry->timestamp);
    rv += bson_append_int(   b, "vgw_id",         entry->object.vgw_id);
    rv += bson_append_start_array(b, "interfaces");
    for (i=0; i<MONGO_VGW_VIF_NUM; i++) {
        //sprintf(key, "vif_%d", i+1);
        rv += bson_append_start_object(b, "");
        rv += bson_append_int( b, "vif_id",       entry->vif_info[i].vif_id);
        rv += bson_append_int( b, "vif_index",    entry->vif_info[i].ifindex);
        rv += bson_append_long(b, "rx_bps",       entry->vif_info[i].traffic.rx_bps);
        rx_bps += entry->vif_info[i].traffic.rx_bps;
        rv += bson_append_long(b, "tx_bps",       entry->vif_info[i].traffic.tx_bps);
        tx_bps += entry->vif_info[i].traffic.tx_bps;
        rv += bson_append_long(b, "rx_pps",       entry->vif_info[i].traffic.rx_pps);
        rx_pps += entry->vif_info[i].traffic.rx_pps;
        rv += bson_append_long(b, "tx_pps",       entry->vif_info[i].traffic.tx_pps);
        tx_pps += entry->vif_info[i].traffic.tx_pps;
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
#if 0
    rv += bson_init(b);
    rv += bson_append_string(b, "name",           entry->object.name);
    rv += bson_append_string(b, "server_ip",      entry->object.server_ip);
    rv += bson_append_long(  b, "timestamp",      entry->timestamp);
    rv += bson_append_time_t(b, "time",           entry->timestamp);
    rv += bson_append_int(   b, "vdc_id",         entry->object.vdc_id);
    rv += bson_append_int(   b, "vgw_id",         entry->object.vgw_id);
    rv += bson_append_int(   b, "user_id",        entry->object.user_id);
    rv += bson_append_int(   b, "htype",          entry->object.htype);
    rv += bson_append_array( b, "cpu_usage",      entry->system.cpu_usage,
                                                  "cpu_usage_size");
    rv += bson_append_double(b, "mem_usage",      entry->system.mem_usage);
    rv += bson_append_start_array(b, "interfaces");
    for (i=0; i<MONGO_VGW_VIF_NUM; i++) {
        //sprintf(key, "vif_%d", i+1);
        rv += bson_append_start_object(b, "");
        rv += bson_append_int( b, "vif_id",       entry->vif_info[i].vif_id);
        rv += bson_append_int( b, "vif_index",    i);
        rv += bson_append_long(b, "rx_bps",       entry->vif_info[i].traffic.rx_bps);
        rx_bps += entry->vif_info[i].traffic.rx_bps;
        rv += bson_append_long(b, "tx_bps",       entry->vif_info[i].traffic.tx_bps);
        tx_bps += entry->vif_info[i].traffic.tx_bps;
        rv += bson_append_long(b, "rx_pps",       entry->vif_info[i].traffic.rx_pps);
        rx_pps += entry->vif_info[i].traffic.rx_pps;
        rv += bson_append_long(b, "tx_pps",       entry->vif_info[i].traffic.tx_pps);
        tx_bps += entry->vif_info[i].traffic.tx_pps;
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    sprintf(key, "vif_sum");
    rv += bson_append_start_object(b, key);
    rv += bson_append_int( b, "vif_id",       0);
    rv += bson_append_long(b, "rx_bps",       rx_bps);
    rv += bson_append_long(b, "tx_bps",       tx_bps);
    rv += bson_append_long(b, "rx_pps",       rx_pps);
    rv += bson_append_long(b, "tx_pps",       tx_pps);
    rv += bson_append_finish_object(b);
    rv += bson_finish(b);
#endif
    return rv;
}

static int bson_package_vm_load(bson *b, const vm_load_t *entry)
{
    int i;
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_long(  b, "timestamp",    entry->timestamp);
    rv += bson_append_time_t(b, "time",         entry->timestamp);
    rv += bson_append_int(   b, "vm_id",        entry->object.vm_id);
    rv += bson_append_array0( b, "cpu_usage",    entry->system.cpu_usage,
                                                "cpu_usage_size");
    rv += bson_append_double(b, "mem_usage",    entry->system.mem_usage);

    rv += bson_append_start_object(b, "disk");
    rv += bson_append_long(b, "sys_disk_total",    entry->system.sys_disk_total);
    rv += bson_append_long(b, "sys_disk_usage",    entry->system.sys_disk_usage);
    rv += bson_append_long(b, "user_disk_total",    entry->system.user_disk_total);
    rv += bson_append_long(b, "user_disk_usage",    entry->system.user_disk_usage);
    rv += bson_append_double(b, "sys_disk_read_rate",    entry->system.sys_disk_read_rate);
    rv += bson_append_double(b, "sys_disk_write_rate",    entry->system.sys_disk_write_rate);
    rv += bson_append_double(b, "user_disk_read_rate",    entry->system.user_disk_read_rate);
    rv += bson_append_double(b, "user_disk_write_rate",    entry->system.user_disk_write_rate);
    rv += bson_append_finish_object(b);

    rv += bson_append_int(b, "disk_num", entry->disk_num);
    rv += bson_append_start_array(b, "disks");
    for (i = 0; i < entry->disk_num; ++i) {
        rv += bson_append_start_object(b, "");
        rv += bson_append_string(b, "device", entry->disk_info[i].device);
        rv += bson_append_double(b, "read_rate", entry->disk_info[i].io.read_rate);
        rv += bson_append_double(b, "write_rate", entry->disk_info[i].io.write_rate);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);

    rv += bson_append_start_array(b, "interfaces");
    for (i=0; i < AGENT_VM_MAX_VIF; i++) {
        rv += bson_append_start_object(b, "");
        rv += bson_append_int( b, "vif_id",       entry->vif_info[i].vif_id);
        rv += bson_append_int( b, "vif_index",    i);
        rv += bson_append_long(b, "rx_bps",       entry->vif_info[i].traffic.rx_bps);
        rv += bson_append_long(b, "tx_bps",       entry->vif_info[i].traffic.tx_bps);
        rv += bson_append_long(b, "rx_pps",       entry->vif_info[i].traffic.rx_pps);
        rv += bson_append_long(b, "tx_pps",       entry->vif_info[i].traffic.tx_pps);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_listener_load(bson *b, const listener_load_t *entry)
{
    int i;
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_int(   b, "conn_num",     entry->conn_num);
    rv += bson_append_long(  b, "timestamp",    entry->timestamp);
    rv += bson_append_string(b, "lcuuid",       entry->lcuuid);
    rv += bson_append_int(   b, "lb_id",        entry->lb_id);
    rv += bson_append_start_array(b, "bk_vms");
    for (i=0; i < MONGO_BK_VM_NUM && i < entry->bk_vm_num; i++) {
        rv += bson_append_start_object(b, "");
        rv += bson_append_string(b, "name",     entry->bk_vm[i].name);
        rv += bson_append_int(   b, "conn_num", entry->bk_vm[i].conn_num);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
    return rv;
}

static int bson_package_switch_load(bson *b, const switch_load_t *entry)
{
    int i;
    int rv = BSON_OK;
    if (!b || !entry) {
        return -EFAULT;
    }
    rv += bson_init(b);
    rv += bson_append_int(   b, "switch_id",      entry->switch_id);
    rv += bson_append_string(b, "switch_name",    entry->switch_name);
    rv += bson_append_string(b, "switch_mip",     entry->switch_mip);
    rv += bson_append_long(  b, "timestamp",      entry->timestamp);
    rv += bson_append_int(   b, "port_num",       entry->port_num);
    rv += bson_append_start_array(b, "ports");
    for (i=0; i<entry->port_num; i++) {
        rv += bson_append_start_object(b, "");
        rv += bson_append_int(   b, "port_index", entry->port[i].port_index);
        rv += bson_append_string(b, "port_name",  entry->port[i].port_name);
        rv += bson_append_long(  b, "rx_bps",     entry->port[i].rx_bps);
        rv += bson_append_long(  b, "rx_pps",     entry->port[i].rx_pps);
        rv += bson_append_long(  b, "tx_bps",     entry->port[i].tx_bps);
        rv += bson_append_long(  b, "tx_pps",     entry->port[i].tx_pps);
        rv += bson_append_finish_object(b);
    }
    rv += bson_append_finish_array(b);
    rv += bson_finish(b);
    return rv;
}

static char *mongo_table_init(mongo_app_t app, time_grain_t grain)
{
    size_t table_size = strlen(mongo_db_conf.db) + strlen(mongo_db_tables[app])
        + strlen(mongo_db_tables_branch[grain]) + strlen(mongo_db_conf.version)
        + 1;
    char *table = (char *)malloc(table_size);
    if (!table) {
        return NULL;
    }
    memset(table, 0, table_size);
    strcat(table, mongo_db_conf.db);
    strcat(table, mongo_db_tables[app]);
    strcat(table, mongo_db_tables_branch[grain]);
    strcat(table, mongo_db_conf.version);
    if (strlen(table) <= 0) {
        free(table);
        return NULL;
    }
    return table;
}

static void mongo_table_free(char *table)
{
    free(table);
}

static int mongo_handler_find_idle(pthread_t tid)
{
    int idx, idle = -ESRCH;
    for (idx=1; idx<MAX_THREAD; idx++) {
        if (pthread_equal(mongo_handler[idx].tid, tid)) {
            return -ESRCH;
        }
        if (mongo_handler[idx].tid == 0) {
            idle = idx;
        }
    }
    return idle;
}

static int mongo_handler_find_conn(pthread_t tid)
{
    int idx;
    for (idx=1; idx<MAX_THREAD; idx++) {
        if (pthread_equal(mongo_handler[idx].tid, tid)) {
            return idx;
        }
    }
    return -ESRCH;
}
#if MONGO_DEBUG
int mongo_get_conn_num(void)
{
    int idx;
    for (idx=1; idx<MAX_THREAD; idx++) {
        if (mongo_handler[idx].tid == 0) {
            continue;
        }else {
            MONGO_SYSLOG(LOG_DEBUG, "%s MONGO_GET_CONNECTIONS thread:[%lu], "
                    "mongo ins_num [%d]  "
                    "mongo access raw num [%d] "
                    "mongo rename name [%d] ",
                    __FUNCTION__, mongo_handler[idx].tid,
                    mongo_handler[idx].ins_num,
                    mongo_handler[idx].access_raw_num,
                    mongo_handler[idx].rename_num);
        }
    }
    return 0;
}
#endif
int mongo_handler_init()
{
    int idx;
    int rv;
    pthread_t tid = pthread_self();
    pthread_mutex_lock(&mutex);
    idx = mongo_handler_find_idle(tid);
    if (idx < 0) {
        pthread_mutex_unlock(&mutex);
        goto err;
    }
    mongo_handler[idx].tid = tid;
    pthread_mutex_unlock(&mutex);

    rv = mongo_client(&mongo_handler[idx].conn, mongo_db_conf.host, atoi(mongo_db_conf.port));
    if (rv != MONGO_OK) {
        rv = -mongo_handler[idx].conn.err;
        MONGO_SYSLOG(LOG_ERR, "%s %d mongo client error :%d\n", __FUNCTION__, __LINE__, rv);
        goto err;
    }

    return MONGO_OK;

err:
    pthread_mutex_lock(&mutex);
    mongo_handler[idx].tid = 0;
    pthread_mutex_unlock(&mutex);
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, idx);
    return idx;
}

void mongo_handler_exit()
{
    int idx;
    pthread_t tid = pthread_self();
    pthread_mutex_lock(&mutex);
    idx = mongo_handler_find_conn(tid);
    if (idx < 0) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    mongo_handler[idx].tid = 0;
    pthread_mutex_unlock(&mutex);

    mongo_disconnect(&mongo_handler[idx].conn);
    mongo_destroy(&mongo_handler[idx].conn);
}

static void *mongo_db_get_idx_entry(mongo_app_t app, void *entry, int i)
{
    void *next;
    switch (app) {
        case MONGO_APP_HOST_USAGE:
            next = &(((host_usage_t *)entry)[i]);
            break;
        case MONGO_APP_VM_USAGE:
        case MONGO_APP_VGW_USAGE:
            next = &(((vm_usage_t *)entry)[i]);
            break;
        case MONGO_APP_TRAFFIC_USAGE:
            next = &(((traffic_usage_t *)entry)[i]);
            break;
        case MONGO_APP_TABLE_INDEX:
            next = &(((table_index_t *)entry)[i]);
            break;
        case MONGO_APP_VGW_SERVER_LOAD:
            next = &(((vgw_server_load_t *)entry)[i]);
            break;
        case MONGO_APP_VM_SERVER_LOAD:
            next = &(((vm_server_load_t *)entry)[i]);
            break;
        case MONGO_APP_VGW_LOAD:
            next = &(((vgw_load_t *)entry)[i]);
            break;
        case MONGO_APP_VM_LOAD:
            next = &(((vm_load_t *)entry)[i]);
            break;
        case MONGO_APP_HWDEV_USAGE:
            next = &(((hwdev_usage_t *)entry)[i]);
            break;
        case MONGO_APP_FLOW_LOAD:
            next = &(((flow_load_t *)entry)[i]);
            break;
        case MONGO_APP_LISTENER_CONN:
            next = &(((listener_load_t *)entry)[i]);
            break;
        case MONGO_APP_SWITCH_LOAD:
            next = &(((switch_load_t *)entry)[i]);
            break;

        default:
            next = NULL;
            break;
    }
    return next;
}

static int mongo_db_bson_package_get(int idx, mongo_app_t app, bson *b, void *entry)
{
    int rv = BSON_OK;
    switch (app) {
        case MONGO_APP_HOST_USAGE:
            rv = bson_package_host_usage(idx, b, entry);
            break;
        case MONGO_APP_VM_USAGE:
        case MONGO_APP_VGW_USAGE:
            rv = bson_package_vm_usage(idx, b, entry);
            break;
        case MONGO_APP_TRAFFIC_USAGE:
            rv = bson_package_traffic_usage(b, entry);
            break;
        case MONGO_APP_TABLE_INDEX:
            rv = bson_package_table_index(b, entry);
            break;
        case MONGO_APP_VGW_SERVER_LOAD:
            rv = bson_package_vgw_server_load(b, entry);
            break;
        case MONGO_APP_VM_SERVER_LOAD:
            rv = bson_package_vm_server_load(b, entry);
            break;
        case MONGO_APP_VGW_LOAD:
            rv = bson_package_vgw_load(b, entry);
            break;
        case MONGO_APP_VM_LOAD:
            rv = bson_package_vm_load(b, entry);
            break;
        case MONGO_APP_HWDEV_USAGE:
            rv = bson_package_hwdev_usage(b, entry);
            break;
        case MONGO_APP_FLOW_LOAD:
            rv = bson_package_flow_load(b, entry);
            break;
        case MONGO_APP_LISTENER_CONN:
            rv = bson_package_listener_load(b, entry);
            break;
        case MONGO_APP_SWITCH_LOAD:
            rv = bson_package_switch_load(b, entry);
            break;

        default:
            rv = -ENOSYS;
            break;
    }
    return rv;
}
struct appinfo{
    int app;
#define APP_NAME_LEN 32
    char name[APP_NAME_LEN];
}app_debug[MONGO_APP_NUM] = {
    {MONGO_APP_NONE            ,""},
    {MONGO_APP_HOST_USAGE      ,"MONGO_APP_HOST_USAGE"},
    {MONGO_APP_VM_USAGE        ,"MONGO_APP_VM_USAGE"},
    {MONGO_APP_VGW_USAGE       ,"MONGO_APP_VGW_USAGE"},
    {MONGO_APP_TRAFFIC_USAGE   ,"MONGO_APP_TRAFFIC_USAGE"},
    {MONGO_APP_TABLE_INDEX     ,"MONGO_APP_TABLE_INDEX"},
    {MONGO_APP_VGW_SERVER_LOAD ,"MONGO_APP_VGW_SERVER_LOAD"},
    {MONGO_APP_VM_SERVER_LOAD  ,"MONGO_APP_VM_SERVER_LOAD"},
    {MONGO_APP_VGW_LOAD        ,"MONGO_APP_VGW_LOAD"},
    {MONGO_APP_VM_LOAD         ,"MONGO_APP_VM_LOAD"},
    {MONGO_APP_FLOW_LOAD       ,"MONGO_APP_FLOW_LOAD"},
    {MONGO_APP_HWDEV_USAGE     ,"MONGO_APP_HWDEV_USAGE"},
    {MONGO_APP_LISTENER_CONN   ,"MONGO_APP_LISTENER_CONN"},
    {MONGO_APP_SWITCH_LOAD     ,"MONGO_APP_SWITCH_LOAD"}
};
static int mongo_db_batch_insert(int idx, mongo_app_t app, int num, time_grain_t grain, void *entry)
{
    int i = 0;
    void *next;
    char *table;
    int rv = MONGO_OK;
    bson *p, **ps = NULL;

    ps = (bson **)malloc(sizeof(bson *) * num);
    if (!ps) {
        MONGO_SYSLOG(LOG_ERR, "%s %d, num:%d , malloc failed exit\n", __FUNCTION__, __LINE__, num);
        exit(1);
    }

    memset(ps, 0 , sizeof(bson *) * num);
    for ( i = 0; i < num; i++ ) {

        next = mongo_db_get_idx_entry(app, entry, i);
        if (next == NULL) {
            MONGO_SYSLOG(LOG_ERR, "%s err app %d\n", __FUNCTION__, app);
            rv = -EINVAL;
            goto err_bson;
        }

        p = (bson *)malloc(sizeof(bson));
        if (!p) {
            MONGO_SYSLOG(LOG_ERR, "%s %d, malloc error i :%d \n", __FUNCTION__, __LINE__, i);
            goto err_bson;
        }
        memset(p, 0 , sizeof(bson));
        if (mongo_db_bson_package_get(idx, app, p, next) != BSON_OK) {
            MONGO_SYSLOG(LOG_ERR, "%s mongo_db_bson_package_get error:%d\n", __FUNCTION__, __LINE__);
            rv = -EINVAL;
            goto err_bson;
        }
        //BSON_SYSLOG(9, p);
        ps[i] = p;
    }

    table = mongo_table_init(app, grain);
    if (!table) {
        rv = -ENOMEM;
        MONGO_SYSLOG(LOG_ERR, "%s mongo_table_init error%d\n", __FUNCTION__, __LINE__);
        goto err_bson;
    }

    mongo_insert_batch(&mongo_handler[idx].conn, table, (const bson **)ps, num, 0, 0 );

    mongo_table_free(table);

err_bson:
    for ( i = 0; i < num; i++ ) {
        if (ps[i]) {
            bson_destroy( ps[i] );
            free(ps[i]);
        }
    }

    if (ps) {
        free(ps);
    }
    MONGO_SYSLOG(LOG_DEBUG, "%s() %d (errno = %d) i: %d num:%d table:%d, %s\n", __FUNCTION__, __LINE__, rv, i, num, app, app_debug[app].name);
    return rv;
}
static int mongo_db_entry_insert_raw(int idx, mongo_app_t app, time_grain_t grain,
                                     void *entry)
{
    char *table;
    bson b;
    int rv;

    if (mongo_db_bson_package_get(idx, app, &b, entry) != BSON_OK) {
        rv = -EINVAL;
        goto err;
    }

    BSON_SYSLOG(9, &b);
    table = mongo_table_init(app, grain);
    if (!table) {
        rv = -ENOMEM;
        MONGO_SYSLOG(LOG_ERR, "%s mongo_table_init error%d\n", __FUNCTION__, __LINE__);
        goto err_bson;
    }
#if MONGO_DEBUG
    mongo_handler[idx].ins_num++;
#endif
    rv = mongo_insert(&mongo_handler[idx].conn, table, &b, NULL);
    if (rv != MONGO_OK) {
        rv = -mongo_handler[idx].conn.err;
        MONGO_SYSLOG(LOG_ERR, "%s mongo insert error %d\n", __FUNCTION__, __LINE__);
        goto err_table;
    }

    mongo_table_free(table);
    bson_destroy(&b);
    return MONGO_OK;

err_table:
    mongo_table_free(table);
err_bson:
    bson_destroy(&b);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, rv);
    return rv;
}

int mongo_db_entrys_batch_insert(mongo_app_t app, void *entry, int num)
{
    int idx = mongo_handler_find_conn(pthread_self());
    if (idx < 0) {
        goto err;
    }
    if (!entry) {
        idx = -EINVAL;
        goto err;
    }
    //return mongo_db_entry_insert_raw(idx, app, 0, entry);
    return mongo_db_batch_insert(idx, app, num, 0, entry);

err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, idx);
    return idx;
}

int mongo_db_entry_insert(mongo_app_t app, void *entry)
{
    int idx = mongo_handler_find_conn(pthread_self());
    if (idx < 0) {
        goto err;
    }
    if (!entry) {
        idx = -EINVAL;
        goto err;
    }
    return mongo_db_entry_insert_raw(idx, app, 0, entry);

err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, idx);
    return idx;
}

int mongo_db_table_rename(const char *old_name, const char *new_name)
{
    bson cmd;
    char *old, *new;
    int idx = mongo_handler_find_conn(pthread_self());
    int rv = 0;
    if (!old_name || !new_name) {
        rv = -EINVAL;
        goto err;
    }
    old = (char *)malloc(strlen(mongo_db_conf.db) + strlen(".") + strlen(old_name)
            + strlen(mongo_db_tables_branch[0]) + strlen(mongo_db_conf.version) + 1);
    new = (char *)malloc(strlen(mongo_db_conf.db) + strlen(".") + strlen(new_name)
            + strlen(mongo_db_tables_branch[0]) + strlen(mongo_db_conf.version) + 1);
    if (!old || !new) {
        rv = -ENOMEM;
        goto err_name;
    }
    memset(old, 0, strlen(mongo_db_conf.db) + strlen(old_name) + 1);
    strcat(old, mongo_db_conf.db);
    strcat(old, ".");
    strcat(old, old_name);
    strcat(old, mongo_db_tables_branch[0]);
    strcat(old, mongo_db_conf.version);
    memset(new, 0, strlen(mongo_db_conf.db) + strlen(new_name) + 1);
    strcat(new, mongo_db_conf.db);
    strcat(new, ".");
    strcat(new, new_name);
    strcat(new, mongo_db_tables_branch[0]);
    strcat(new, mongo_db_conf.version);
    rv += bson_init(&cmd);
    rv += bson_append_string(&cmd, "renameCollection", old);
    rv += bson_append_string(&cmd, "to", new);
    rv += bson_finish(&cmd);
    if (rv != BSON_OK) {
        rv = -EINVAL;
        goto err_bson;
    }
#if MONGO_DEBUG
    mongo_handler[idx].rename_num++;
#endif

    rv = mongo_run_command(&mongo_handler[idx].conn, "admin", &cmd, NULL);
    if (rv != MONGO_OK) {
        rv = -mongo_handler[idx].conn.err;
        goto err_bson;
    }

    bson_destroy(&cmd);
    free(new);
    free(old);
    return 0;

err_bson:
    bson_destroy(&cmd);
err_name:
    free(new);
    free(old);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, rv);
    return rv;
}

static time_t bson_find_timestamp(const bson *b)
{
    bson_iterator it;
    if (bson_find(&it, b, "timestamp") == BSON_DATE) {
        return bson_iterator_time_t(&it);
    }
    return -1;
}

static int bson_find_object_id(const bson *b, const char *name)
{
    bson_iterator it;
    if (bson_find(&it, b, name) == BSON_INT) {
        return bson_iterator_int(&it);
    }
    return -1;
}

static const char *bson_find_object_name(const bson *b, const char *name)
{
    bson_iterator it;
    if (bson_find(&it, b, name) == BSON_STRING) {
        return bson_iterator_string(&it);
    }
    return NULL;
}

static int bson_find_array_size(const bson *b, const char *name)
{
#ifndef __NO_UNION__
    bson_iterator it;
    if (bson_find(&it, b, name) == BSON_INT) {
        return bson_iterator_int(&it);
    }
    return 0;
#else
    int size = 0;
    bson_iterator it;
    if (bson_find(&it, b, name) == BSON_ARRAY) {
        const char *data = bson_iterator_value(&it);
        bson_iterator_from_buffer(&it, data);
        while (bson_iterator_next(&it)) {
            switch (bson_iterator_type(&it)) {
                case BSON_DOUBLE:
                    size++;
                    break;
                default:
                    break;
            }
        }
        size++;
    }
    return size;
#endif
}

static void *mongo_host_usage_entry_init(int object_id, const bson *b)
{
    host_usage_t *entry;
    const char *object_name;
    int size;
    entry = (host_usage_t *)malloc(sizeof(host_usage_t));
    if (!entry) {
        goto err;
    }
    memset(entry, 0, sizeof(host_usage_t));
    /* cr_id */
    entry->cr_id = object_id;
    /* host_name_label */
    object_name = bson_find_object_name(b, "host_name");
    if (!object_name) {
        goto err_entry;
    }
    entry->host_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->host_name) {
        goto err_entry;
    }
    strcpy(entry->host_name, object_name);
    /* cpu_info */
    object_name = bson_find_object_name(b, "cpu_info");
    if (!object_name) {
        goto err_host_name;
    }
    entry->cpu_info = (char *)malloc(strlen(object_name) + 1);
    if (!entry->cpu_info) {
        goto err_host_name;
    }
    strcpy(entry->cpu_info, object_name);
    /* cpu_usage */
    size = bson_find_array_size(b, "cpu_usage_size");
    if (!size) {
        goto err_cpu_info;
    }
    entry->cpu_usage = (char *)malloc(size * sizeof(double));
    if (!entry->cpu_usage) {
        goto err_cpu_info;
    }
    memset(entry->cpu_usage, 0, size * sizeof(double));
    entry->cpu_usage_size = size;
    return (void *)entry;

err_cpu_info:
    free(entry->cpu_info);
err_host_name:
    free(entry->host_name);
err_entry:
    free(entry);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, -ENOMEM);
    return NULL;
}
static void *mongo_hwdev_usage_entry_init(int object_id, const bson *b)
{
    hwdev_usage_t *entry;
    const char *object_name;
    entry = (hwdev_usage_t *)malloc(sizeof(hwdev_usage_t));
    if (!entry) {
        goto err;
    }
    memset(entry, 0, sizeof(hwdev_usage_t));
    /* cr_id */
    entry->id = object_id;
    /* hwdev_name */
    entry->userid = bson_find_object_id(b, "userid");
    entry->cpu_num = bson_find_object_id(b, "cpu_num");
    entry->dsk_num = bson_find_object_id(b, "dsk_num");
    entry->mem_size = bson_find_object_id(b, "mem_size");
    entry->mem_used = bson_find_object_id(b, "mem_used");
    /* hwdev_ip */
    object_name = bson_find_object_name(b, "hwdev_ip");
    if (!object_name) {
        goto err_entry;
    }
    entry->hwdev_ip = (char *)malloc(strlen(object_name) + 1);
    if (!entry->hwdev_ip) {
        goto err_entry;
    }
    strcpy(entry->hwdev_ip, object_name);
    /* hwdev_mac */
    object_name = bson_find_object_name(b, "hwdev_mac");
    if (!object_name) {
        goto err_hwdev_ip;
    }
    entry->hwdev_mac = (char *)malloc(strlen(object_name) + 1);
    if (!entry->hwdev_mac) {
        goto err_hwdev_ip;
    }
    strcpy(entry->hwdev_mac, object_name);
    /* sys_uptime */
    object_name = bson_find_object_name(b, "sys_uptime");
    if (!object_name) {
        goto err_hwdev_mac;
    }
    entry->sys_uptime = (char *)malloc(strlen(object_name) + 1);
    if (!entry->sys_uptime) {
        goto err_hwdev_mac;
    }
    strcpy(entry->sys_uptime, object_name);
    /* sys_os */
    object_name = bson_find_object_name(b, "sys_os");
    if (!object_name) {
        goto err_sys_uptime;
    }
    entry->sys_os = (char *)malloc(strlen(object_name) + 1);
    if (!entry->sys_os) {
        goto err_sys_uptime;
    }
    strcpy(entry->sys_os, object_name);
    /* cpu_type */
    object_name = bson_find_object_name(b, "cpu_type");
    if (!object_name) {
        goto err_sys_os;
    }
    entry->cpu_type = (char *)malloc(strlen(object_name) + 1);
    if (!entry->cpu_type) {
        goto err_sys_os;
    }
    strcpy(entry->cpu_type, object_name);
    /* cpu_data */
    object_name = bson_find_object_name(b, "cpu_data");
    if (!object_name) {
        goto err_cpu_type;
    }
    entry->cpu_data = (char *)malloc(strlen(object_name) + 1);
    if (!entry->cpu_data) {
        goto err_cpu_type;
    }
    strcpy(entry->cpu_data, object_name);
    /* mem_usage */
    object_name = bson_find_object_name(b, "mem_usage");
    if (!object_name) {
        goto err_cpu_data;
    }
    entry->mem_usage = (char *)malloc(strlen(object_name) + 1);
    if (!entry->mem_usage) {
        goto err_cpu_data;
    }
    strcpy(entry->mem_usage, object_name);

    /* dsk_info */
    object_name = bson_find_object_name(b, "dsk_info");
    if (!object_name) {
        goto err_mem_usage;
    }
    entry->dsk_info = (char *)malloc(strlen(object_name) + 1);
    if (!entry->dsk_info) {
        goto err_mem_usage;
    }
    strcpy(entry->dsk_info, object_name);
    /* hwdev_name */
    object_name = bson_find_object_name(b, "hwdev_name");
    if (!object_name) {
        goto err_dsk_info;
    }
    entry->hwdev_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->hwdev_name) {
        goto err_dsk_info;
    }
    strcpy(entry->hwdev_name, object_name);
    /* community */
    object_name = bson_find_object_name(b, "community");
    if (!object_name) {
        goto err_hwdev_name;
    }
    entry->community = (char *)malloc(strlen(object_name) + 1);
    if (!entry->community) {
        goto err_hwdev_name;
    }
    strcpy(entry->community, object_name);
    /* data_ip */
    object_name = bson_find_object_name(b, "data_ip");
    if (!object_name) {
        goto err_community;
    }
    entry->data_ip = (char *)malloc(strlen(object_name) + 1);
    if (!entry->data_ip) {
        goto err_community;
    }
    strcpy(entry->data_ip, object_name);
    /* launch_server */
    object_name = bson_find_object_name(b, "launch_server");
    if (!object_name) {
        goto err_data_ip;
    }
    entry->launch_server = (char *)malloc(strlen(object_name) + 1);
    if (!entry->launch_server) {
        goto err_data_ip;
    }
    strcpy(entry->launch_server, object_name);
    /* user_name */
    object_name = bson_find_object_name(b, "user_name");
    if (!object_name) {
        goto err_launch_server;
    }
    entry->user_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->user_name) {
        goto err_launch_server;
    }
    strcpy(entry->user_name, object_name);
    /* user_passwd */
    object_name = bson_find_object_name(b, "user_passwd");
    if (!object_name) {
        goto err_user_name;
    }
    entry->user_passwd = (char *)malloc(strlen(object_name) + 1);
    if (!entry->user_passwd) {
        goto err_user_name;
    }
    strcpy(entry->user_passwd, object_name);
    /* rack_name */
    object_name = bson_find_object_name(b, "rack_name");
    if (!object_name) {
        goto err_user_passwd;
    }
    entry->rack_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->rack_name) {
        goto err_user_passwd;
    }
    strcpy(entry->rack_name, object_name);
    return (void *)entry;
err_user_passwd:
    free(entry->user_passwd);
err_user_name:
    free(entry->user_name);

err_launch_server:
    free(entry->launch_server);
err_data_ip:
    free(entry->data_ip);
err_community:
    free(entry->community);
err_hwdev_name:
    free(entry->hwdev_name);
err_dsk_info:
    free(entry->dsk_info);
err_mem_usage:
    free(entry->mem_usage);
err_cpu_data:
    free(entry->cpu_data);
err_cpu_type:
    free(entry->cpu_type);
err_sys_os:
    free(entry->sys_os);
err_sys_uptime:
    free(entry->sys_uptime);
err_hwdev_mac:
    free(entry->hwdev_mac);
err_hwdev_ip:
    free(entry->hwdev_ip);
err_entry:
    free(entry);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, -ENOMEM);
    return NULL;
}
static void *mongo_vm_usage_entry_init(int object_id, const bson *b)
{
    vm_usage_t *entry;
    const char *object_name;
    int size;
    entry = (vm_usage_t *)malloc(sizeof(vm_usage_t));
    if (!entry) {
        goto err;
    }
    memset(entry, 0, sizeof(vm_usage_t));
    /* vm_id */
    entry->vm_id = object_id;
    /* vm_name_label */
    object_name = bson_find_object_name(b, "vm_name");
    if (!object_name) {
        goto err_entry;
    }
    entry->vm_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->vm_name) {
        goto err_entry;
    }
    strcpy(entry->vm_name, object_name);
    /* cpu_info */
    object_name = bson_find_object_name(b, "cpu_info");
    if (!object_name) {
        goto err_vm_name;
    }
    entry->cpu_info = (char *)malloc(strlen(object_name) + 1);
    if (!entry->cpu_info) {
        goto err_vm_name;
    }
    strcpy(entry->cpu_info, object_name);
    /* cpu_usage */
    size = bson_find_array_size(b, "cpu_usage_size");
    if (!size) {
        goto err_cpu_info;
    }
    entry->cpu_usage = (char *)malloc(size * sizeof(double));
    if (!entry->cpu_usage) {
        goto err_cpu_info;
    }
    memset(entry->cpu_usage, 0, size * sizeof(double));
    entry->cpu_usage_size = size;
    return (void *)entry;

err_cpu_info:
    free(entry->cpu_info);
err_vm_name:
    free(entry->vm_name);
err_entry:
    free(entry);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, -ENOMEM);
    return NULL;
}

static void *mongo_traffic_usage_entry_init(int object_id, const bson *b)
{
    traffic_usage_t *entry;
    const char *object_name;
    entry = (traffic_usage_t *)malloc(sizeof(traffic_usage_t));
    if (!entry) {
        goto err;
    }
    memset(entry, 0, sizeof(traffic_usage_t));
    /* vif_id */
    entry->vif_id = object_id;
    /* vif_name_label */
    object_name = bson_find_object_name(b, "vif_name");
    if (!object_name) {
        goto err_entry;
    }
    entry->vif_name = (char *)malloc(strlen(object_name) + 1);
    if (!entry->vif_name) {
        goto err_entry;
    }
    strcpy(entry->vif_name, object_name);
    return (void *)entry;

err_entry:
    free(entry);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, -ENOMEM);
    return NULL;
}

static void *mongo_entry_init(mongo_app_t app, int object_id, const bson *b)
{
    switch (app) {
        case MONGO_APP_HOST_USAGE:
            return mongo_host_usage_entry_init(object_id, b);
        case MONGO_APP_VM_USAGE:
        case MONGO_APP_VGW_USAGE:
            return mongo_vm_usage_entry_init(object_id, b);
        case MONGO_APP_TRAFFIC_USAGE:
            return mongo_traffic_usage_entry_init(object_id, b);
        case MONGO_APP_HWDEV_USAGE:
            return mongo_hwdev_usage_entry_init(object_id, b);

        default:
            break;
    }
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, -EFAULT);
    return NULL;
}

static void mongo_entry_free(mongo_app_t app, void *entry)
{
    if (!entry) {
        return;
    }
    switch (app) {
        case MONGO_APP_HOST_USAGE:
            free(HOST_USAGE_CONVERT(entry).host_name);
            free(HOST_USAGE_CONVERT(entry).cpu_info);
            free(HOST_USAGE_CONVERT(entry).cpu_usage);
            break;
        case MONGO_APP_VM_USAGE:
        case MONGO_APP_VGW_USAGE:
            free(VM_USAGE_CONVERT(entry).vm_name);
            free(VM_USAGE_CONVERT(entry).cpu_info);
            free(VM_USAGE_CONVERT(entry).cpu_usage);
            break;
        case MONGO_APP_TRAFFIC_USAGE:
            free(TRAFFIC_USAGE_CONVERT(entry).vif_name);
            break;
        case MONGO_APP_HWDEV_USAGE:
            free(HWDEV_USAGE_CONVERT(entry).hwdev_ip);
            free(HWDEV_USAGE_CONVERT(entry).hwdev_mac);
            free(HWDEV_USAGE_CONVERT(entry).sys_uptime);
            free(HWDEV_USAGE_CONVERT(entry).sys_os);
            free(HWDEV_USAGE_CONVERT(entry).cpu_type);
            free(HWDEV_USAGE_CONVERT(entry).cpu_data);
            free(HWDEV_USAGE_CONVERT(entry).mem_usage);
            free(HWDEV_USAGE_CONVERT(entry).dsk_info);
            free(HWDEV_USAGE_CONVERT(entry).hwdev_name);
            free(HWDEV_USAGE_CONVERT(entry).community);
            free(HWDEV_USAGE_CONVERT(entry).data_ip);
            free(HWDEV_USAGE_CONVERT(entry).launch_server);
            free(HWDEV_USAGE_CONVERT(entry).user_name);
            free(HWDEV_USAGE_CONVERT(entry).user_passwd);
            free(HWDEV_USAGE_CONVERT(entry).rack_name);
            break;
        default:
            break;
    }
    free(entry);
}

static void mongo_averaging_universal_usage(mongo_app_t app, double weight,
                                            void *entry, const char *data, int round)
{
    void *p = NULL, *q = NULL;
    bson_iterator it;
    bson_iterator_from_buffer(&it, data);
    if (!entry) {
        return;
    }
    if (round == 0) {
        switch (app) {
           case MONGO_APP_HOST_USAGE:
               p = &(HOST_USAGE_CONVERT(entry).timestamp);
               q = HOST_USAGE_CONVERT(entry).cpu_usage;
               break;
           case MONGO_APP_VM_USAGE:
           case MONGO_APP_VGW_USAGE:
               p = &(VM_USAGE_CONVERT(entry).timestamp);
               q = VM_USAGE_CONVERT(entry).cpu_usage;
               break;
           case MONGO_APP_TRAFFIC_USAGE:
               p = &(TRAFFIC_USAGE_CONVERT(entry).timestamp);
               break;
           default:
               return;
        }
    } else {
        p = entry;
    }
    while (bson_iterator_next(&it)) {
        switch (bson_iterator_type(&it)) {
            case BSON_LONG:
                if (app == MONGO_APP_TRAFFIC_USAGE
                        && strcmp(bson_iterator_key(&it) + 4, "ps") != 0) {
                    *((int64_t *)p) = bson_iterator_long(&it);
                    p += sizeof(int64_t);
                    break;
                }
                *((int64_t *)p) = ((*((int64_t *)p))*weight
                    + bson_iterator_long(&it)) / (weight + 1);
                p += sizeof(int64_t);
                break;
            case BSON_DOUBLE:
                *((double *)p) = ((*((double *)p))*weight
                    + bson_iterator_double(&it)) / (weight + 1);
                p += sizeof(double);
                break;
            case BSON_STRING:
                p += sizeof(char *);
                break;
            case BSON_INT:
                p += sizeof(int);
                break;
            case BSON_DATE:
                *((time_t *)p) = ((*((time_t *)p))*weight
                    + bson_iterator_time_t(&it)) / (weight + 1);
                p += sizeof(time_t);
                break;
            case BSON_ARRAY:
                mongo_averaging_universal_usage(app, weight, (double *)q,
                        bson_iterator_value(&it), 1);
                p += sizeof(char *);
                break;
            default:
                break;
        }
    }
}

static int mongo_entry_value_calculate(mongo_app_t app, double *weight,
                                       void *(*entries)[], bson *b, int *item)
{
    int object_id, size, i, rv;
    switch (app) {
        case MONGO_APP_HOST_USAGE:
            object_id = bson_find_object_id(b, "cr_id");
            for (i=0; i<*item; i++) {
                if (object_id == HOST_USAGE_CONVERT((*entries)[i]).cr_id) {
                    size = bson_find_array_size(b, "cpu_usage_size");
                    if (size != HOST_USAGE_CONVERT((*entries)[i]).cpu_usage_size) {
                        rv = -ERANGE;
                        goto err;
                    }
                    break;
                }
            }
            break;
        case MONGO_APP_VM_USAGE:
        case MONGO_APP_VGW_USAGE:
            object_id = bson_find_object_id(b, "vm_id");
            for (i=0; i<*item; i++) {
                if (object_id == VM_USAGE_CONVERT((*entries)[i]).vm_id) {
                    size = bson_find_array_size(b, "cpu_usage_size");
                    if (size != VM_USAGE_CONVERT((*entries)[i]).cpu_usage_size) {
                        rv = -ERANGE;
                        goto err;
                    }
                    break;
                }
            }
            break;
        case MONGO_APP_TRAFFIC_USAGE:
            object_id = bson_find_object_id(b, "vif_id");
            for (i=0; i<*item; i++) {
                if (object_id == TRAFFIC_USAGE_CONVERT((*entries)[i]).vif_id) {
                    break;
                }
            }
            break;
        case MONGO_APP_HWDEV_USAGE:
            object_id = bson_find_object_id(b, "id");
            for (i=0; i<*item; i++) {
                if (object_id == HWDEV_USAGE_CONVERT((*entries)[i]).id) {
                    break;
                }
            }
            break;

        default:
            rv = -ENOSYS;
            goto err;
    }
    if (i == *item) {
        (*entries)[i] = mongo_entry_init(app, object_id, b);
        if (!(*entries)[i]) {
            rv = -ENOMEM;
            goto err;
        }
        (*item)++;
    }
    mongo_averaging_universal_usage(app, weight[i]++, (*entries)[i], b->data, 0);
    return MONGO_OK;

err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, rv);
    return rv;
}

static int mongo_db_entry_access_raw(int idx, mongo_app_t app, time_grain_t grain,
                                     time_t begin)
{
    mongo_cursor cursor[1];
    char *table;
    void *entries[MAX_VM_NUM] = {NULL};
    time_t now, time_gap = (grain == 0) ? TIMEVAL : time_gaps[grain];
    double weight[MAX_VM_NUM] = {0};
    int item = 0, i, rv;
    table = mongo_table_init(app, grain);
    if (!table) {
        rv = -ENOMEM;
        goto err;
    }
#if MONGO_DEBUG
    mongo_handler[idx].access_raw_num++;
#endif
    mongo_cursor_init(cursor, &mongo_handler[idx].conn, table);
    for (;;) {
        /* TODO: query in sequence */
        rv = mongo_cursor_next(cursor);
        if (rv != MONGO_OK) {
            if (cursor->err == MONGO_CURSOR_EXHAUSTED) {
                break;
            }
            rv = -cursor->err;
            goto err_cursor;
        }
        now = bson_find_timestamp(&cursor->current);
#if 0
        if (now > begin + time_gap) {
            break;
        }
        if (now > begin)
#endif
        {
            rv = mongo_entry_value_calculate(app, weight, &entries,
                    &cursor->current, &item);
            if (rv != MONGO_OK && rv != -ENOMEM) {
                goto err_cursor;
            }
            MONGO_SYSLOG(LOG_INFO,
                    "(TIMER) query \"%s\" @time %ld in time range [%ld %ld]\n",
                    table, now, begin, begin + time_gap);
            BSON_SYSLOG(9, &cursor->current);
        }
        rv = mongo_remove(&mongo_handler[idx].conn, table, &cursor->current, NULL);
        if (rv != MONGO_OK) {
            rv = -mongo_handler[idx].conn.err;
            goto err_cursor;
        }
    }
    mongo_cursor_destroy(cursor);
    mongo_table_free(table);
    grain++;
    for (i=0; i<item; i++) {
        mongo_db_entry_insert_raw(idx, app, grain, entries[i]);
    }
    for (i=0; i<item; i++) {
        mongo_entry_free(app, entries[i]);
    }
    return MONGO_OK;

err_cursor:
    mongo_cursor_destroy(cursor);

    mongo_table_free(table);
    for (i=0; i<item; i++) {
        mongo_entry_free(app, entries[i]);
    }
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, rv);
    return rv;
}

static int mongo_db_entry_access(uint32_t apps, time_grain_t grain, time_t begin)
{
    int i;
    int rv = MONGO_OK;

    for (i = 0; i <= OLD_MONGO_TABLE_NUM; ++i) {
        if (apps & (1 << i)) {
            rv += mongo_db_entry_access_raw(0, i, grain, begin);
        }
    }
    if (apps & (1<<10))
    {
        rv += mongo_db_entry_access_raw(0, 10, grain, begin);
    }
    return rv;
}

static void *mongo_db_regular_access(void *argv)
{
    time_grain_t grain;
    struct timeval now;
    time_t begin[time_gaps[0]], time_gap = 0;
    intptr_t apps = (intptr_t) argv;

    if (mongo_handler_init() < 0) {
        goto err;
    }

    gettimeofday(&now, NULL);
    for (grain=0; grain<time_gaps[0]; grain++) {
        begin[grain] = now.tv_sec;
    }

    for (;;) {
        sleep(TIMEVAL);
        mongo_db_entry_access(apps, 0, begin[0]);
        begin[0] += TIMEVAL;
        time_gap += TIMEVAL;
        for (grain=1; grain<time_gaps[0]; grain++) {
            if ((time_gap % time_gaps[grain]) == 0) {
                mongo_db_entry_access(apps, grain, begin[grain]);
                begin[grain] += time_gaps[grain];
            }
        }
        if (time_gap == time_gaps[grain]) {
            time_gap = 0;
        }
    }
    mongo_handler_exit();
    return NULL;

err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed\n", __FUNCTION__);
    return NULL;
}

static int mongo_db_timer_init(intptr_t apps)
{
    if (pthread_create(&mongo_handler[0].tid, NULL,
                mongo_db_regular_access, (void*)apps) != 0) {
        return -ECHILD;
    }
    return MONGO_OK;
}

static void mongo_db_timer_exit(void)
{
    pthread_join(mongo_handler[0].tid, NULL);
}

int mongo_db_init(const char *host, const char *port,
                  const char *user, const char *passwd,
                  const char *db, const char *version,
                  uint32_t apps)
{
    int rv;
    if (!host || !port || !user || !passwd || !db || !version) {
        rv = -EINVAL;
        goto err;
    }
    mongo_db_conf.host = malloc(strlen(host) + 1);
    mongo_db_conf.port = malloc(strlen(port) + 1);
    mongo_db_conf.user = malloc(strlen(user) + 1);
    mongo_db_conf.passwd = malloc(strlen(passwd) + 1);
    mongo_db_conf.db = malloc(strlen(db) + 1);
    mongo_db_conf.version = malloc(strlen(version) + 1);
    if (!mongo_db_conf.host || !mongo_db_conf.port ||
        !mongo_db_conf.user || !mongo_db_conf.passwd ||
        !mongo_db_conf.db || !mongo_db_conf.version) {
        rv = -ENOMEM;
        goto err_conf;
    }
    strcpy(mongo_db_conf.host, host);
    strcpy(mongo_db_conf.port, port);
    strcpy(mongo_db_conf.user, user);
    strcpy(mongo_db_conf.passwd, passwd);
    strcpy(mongo_db_conf.db, db);
    strcpy(mongo_db_conf.version, version);
    if ((rv = mongo_db_timer_init(apps)) < 0) {
        goto err_conf;
    }
    return MONGO_OK;

err_conf:
    free(mongo_db_conf.host);
    free(mongo_db_conf.port);
    free(mongo_db_conf.user);
    free(mongo_db_conf.passwd);
    free(mongo_db_conf.db);
    free(mongo_db_conf.version);
err:
    MONGO_SYSLOG(LOG_ERR, "%s() failed (errno = %d)\n", __FUNCTION__, rv);
    return rv;
}

void mongo_db_exit(void)
{
    mongo_db_timer_exit();
    free(mongo_db_conf.host);
    free(mongo_db_conf.port);
    free(mongo_db_conf.user);
    free(mongo_db_conf.passwd);
    free(mongo_db_conf.db);
    free(mongo_db_conf.version);
}
