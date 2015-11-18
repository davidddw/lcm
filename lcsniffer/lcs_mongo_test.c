/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Wang Songbo
 * Finish Date: 2014-09-16
 *
 */
#include "lcs_socket.h"

#include <signal.h>
#include <sys/time.h>
#include <syslog.h>

#include "lc_snf.h"
#include "lc_mongo_db.h"
#include "lc_agent_msg.h"

#include "lc_db.h"
#include "lcs_utils.h"
#include "lcs_socket.h"
#include "lcs_usage_db.h"
#include "lcs_usage_xen.h"
#include "lcs_usage_vmware.h"
#include "lc_db_sys.h"
#include "cloudmessage.pb-c.h"
#include "agexec_inner.h"
#include "lc_bus.h"
#include "lc_header.h"
#include "nxjson.h"
#include "lc_livegate_api.h"
#include "lc_lcmg_api.h"
#include "framework/list.h"

int per;
int total;
int tail_num;
int batch_num;

#define INSERT_TYPE_BATCH 1
#define INSERT_TYPE_SINGL 2

int vm_server_batch_insert(vm_server_load_t *vm_t)
{
    int i;
    vm_server_load_t vm_server_load;
    memset(&vm_server_load, 0, sizeof(vm_server_load_t));
    vm_server_load.timestamp = (uint32_t)time(NULL);
    vm_server_load.object.htype = 1;
    strcpy(vm_server_load.object.name, "test");
    strcpy(vm_server_load.object.server_ip, "127.0.0.1");
    vm_server_load.system.dom0.mem_total = 10;
    vm_server_load.system.dom0.mem_usage = 1;
    vm_server_load.system.dom0.disk_total = 11;
    vm_server_load.system.dom0.disk_usage = 2;
    vm_server_load.system.domu.mem_total = 112;
    vm_server_load.system.domu.mem_alloc = 22;
    vm_server_load.system.domu.mem_usage = 2;
    vm_server_load.system.domu.disk_total = 122;
    vm_server_load.system.domu.disk_alloc = 2;
    vm_server_load.system.domu.disk_usage = 23;
    vm_server_load.system.domu.io_read = 2;
    vm_server_load.system.domu.io_write = 222;
    vm_server_load.system.ha_disk_usage = 88;
    for (i = 0; i < LC_N_BR_ID; i++){
        vm_server_load.traffic[i].iftype = i;
        vm_server_load.traffic[i].rx_bps = i;
        vm_server_load.traffic[i].tx_bps = i;
        vm_server_load.traffic[i].rx_pps = i;
        vm_server_load.traffic[i].tx_pps = i;
    }
    memcpy(vm_t, &vm_server_load, sizeof(vm_server_load_t));
    return 0;
}

static void *mongo_db_regular_singl_test(void *argv)
{
    int i;
    int rv;
    float timeuse;
    struct timeval tpstart,tpend;
    vm_server_load_t *vm_server_load;
    vm_server_load = (vm_server_load_t *)malloc(sizeof(vm_server_load_t));
    memset(vm_server_load, 0, sizeof(vm_server_load_t));

    vm_server_batch_insert(vm_server_load);

    if (mongo_handler_init() < 0) {
        fprintf(stderr,"%s: mongo_handler_init error %d\n", __FUNCTION__, __LINE__);
        goto err;
    }

    gettimeofday(&tpstart,NULL);
    for (i = 0; i < total; i++){
        if ((rv = mongo_db_entry_insert(
                        MONGO_APP_VM_SERVER_LOAD, (void *)vm_server_load)) < 0 ) {
            fprintf(stderr, "batch insert error\n");
        }
    }
    gettimeofday(&tpend,NULL);

    timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
    timeuse/=1000000;
    fprintf(stderr, "time use:[%f]\n", timeuse);
    mongo_handler_exit();

err:
    free(vm_server_load);
    return NULL;
}

static void *mongo_db_regular_batch_test(void *argv)
{
    int i;
    int rv;
    float timeuse;
    struct timeval tpstart,tpend;
    vm_server_load_t *vm_server_load;
    vm_server_load = (vm_server_load_t *)malloc(sizeof(vm_server_load_t) * per);
    memset(vm_server_load, 0, sizeof(vm_server_load_t)* per);

    for(i = 0; i<per; i++){
        vm_server_batch_insert(&vm_server_load[i]);
    }

    if (mongo_handler_init() < 0) {
        fprintf(stderr,"%s: mongo_handler_init error %d\n", __FUNCTION__, __LINE__);
        goto err;
    }

    gettimeofday(&tpstart,NULL);
    for (i = 0; i < batch_num; i++){
        if ((rv = mongo_db_entrys_batch_insert(
                        MONGO_APP_VM_SERVER_LOAD, (void *)vm_server_load, per)) < 0 ) {
            fprintf(stderr, "batch insert error %d\n", rv);
        }
    }
    if (tail_num) {
        if ((rv = mongo_db_entrys_batch_insert(
                        MONGO_APP_VM_SERVER_LOAD, (void *)vm_server_load, tail_num)) < 0 ) {
            fprintf(stderr, "batch insert error\n");
        }
    }
    gettimeofday(&tpend,NULL);

    timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
    timeuse/=1000000;
    fprintf(stderr, "time use:%f\n", timeuse);

    mongo_handler_exit();
err:
    free(vm_server_load);
    return NULL;
}

static void usage(void)
{
    printf( "USAGE: lc_test inserttype totalnum pernum server_ip\n"
            "\tserverip: xx.xx.xx.xx\n"
            "\tinserttype: \n"
            "\t\t1: batch insert \n"
            "\t\t2: single insert\n");
    return;
}
int main(int argc, char *argv[])
{
    int insert_type;
    char server_ip[100];
    pthread_t tid;
    if (argc < 5){
        usage();
        return 1;
    }
    fprintf(stderr, "begin....\n");

    const uint32_t mongo_apps =
         (1 << MONGO_APP_HOST_USAGE) |
         (1 << MONGO_APP_VM_USAGE) |
         (1 << MONGO_APP_VGW_USAGE) |
         (1 << MONGO_APP_TRAFFIC_USAGE) |
         (1 << MONGO_APP_VM_SERVER_LOAD) |
         (1 << MONGO_APP_HWDEV_USAGE);

    memset(server_ip, 0, 100);
    insert_type = atoi(argv[1]);
    total = atoi(argv[2]);
    per = atoi(argv[3]);
    strcpy(server_ip, argv[4]);

    batch_num = total/per;
    tail_num = total - batch_num * per;

    fprintf(stderr, "total:%d per:%d batch_num= total/per + 1 :%d\n",
            total, per, batch_num);

    if (lc_db_init("localhost", "root", "security421",
                "livecloud", "0", LC_DB_VERSION)) {
        fprintf(stderr,"%s: db init error.\n", __FUNCTION__);
        exit(-1);
    }
    if (mongo_db_init(server_ip, "20011", "root",
                "security421", "livecloud", LC_DB_VERSION, mongo_apps) < 0) {
        fprintf(stderr, "%s: mongo_db_init error\n", __FUNCTION__);
        exit(1);
    }

    if (insert_type == INSERT_TYPE_BATCH) {
        if (pthread_create(&tid, NULL, mongo_db_regular_batch_test, NULL) != 0) {
            return -ECHILD;
        }
    }else if (insert_type == INSERT_TYPE_SINGL) {
        if (pthread_create(&tid, NULL, mongo_db_regular_singl_test, NULL) != 0) {
            return -ECHILD;
        }
    }else{
        fprintf(stderr, "%s: error insert type;\n", __FUNCTION__);
        usage();
        return 1;
    }
    pthread_join(tid, NULL);

    return 0;
}

