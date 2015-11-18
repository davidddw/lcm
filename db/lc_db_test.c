#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lc_db.h"
#include "vm/vm_host.h"

/* Used for test DB APIs seperately */

#define LC_DB_BUF_LEN 512

int lc_vmdb_server_update_state (int state, char *ip)
{
    char condition[LC_DB_BUF_LEN];
    char pwd[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(pwd, 0, LC_DB_BUF_LEN);
    sprintf(condition, "ip='%s'", ip);
    sprintf(pwd, "%d", state);

    if (lc_db_server_update("state", pwd, condition) != 0) {
        LC_DB_LOG(LOG_ERR, "%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int lc_vmdb_vm_update_ip (char *vm_ip, uint32_t vnet_id, uint32_t vl2_id, uint32_t vm_id)
{
    char condition[LC_DB_BUF_LEN];
    char buffer[LC_DB_BUF_LEN];

    memset(condition, 0, LC_DB_BUF_LEN);
    memset(buffer, 0, LC_DB_BUF_LEN);
    sprintf(condition, "id='%d' and vl2='%d' and vnet='%d'", vm_id, vl2_id, vnet_id);
    sprintf(buffer, "'%s'", vm_ip);

    if (lc_db_vm_update("ip", buffer, condition) != 0) {
        printf("%s: error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

void dumppool (rpool_t *pool)
{
    printf("%d %s %s %d %s\n", pool->rpool_index, pool->rpool_name,
            pool->rpool_desc, pool->rpool_flag, pool->master_address);
}

void dumphost (host_t *host)
{
    printf("%s %s %s %s %d %d %s %s %s %s %d %d %d\n",
            host->host_name,host->host_description, host->host_address,
            host->host_uuid, host->host_flag, host->host_state,
            host->username, host->password, host->sr_uuid, host->sr_name,
            host->max_vm, host->curr_vm, host->pool_index);
}

void dumpvm (vm_info_t *vm)
{
    printf("%d %s %s %d %d %d %s %s %d %d %d %d %llu mac: %s %s %s\n",
            vm->vm_flags, vm->vm_template, vm->vm_import, vm->vnet_id,
            vm->vl2_id, vm->vm_id, vm->vm_name, vm->vm_uuid, vm->vm_state,
            vm->of_port, vm->vlan_id, vm->bandwidth, vm->vdi_size,
            vm->mac_addr, vm->vif_name, vm->host_name);
}


int main(int argc, char *argv[])
{
    rpool_t pool;
    host_t host;
    vm_info_t vm;
    int len = 0;
    uint32_t mask = (len == 0) ? 0 : (0xFFFFFFFF ^ ((1 << (32 - len)) - 1));
    char netmask[MAX_VM_ADDRESS_LEN];
    memset(netmask, 0, MAX_VM_ADDRESS_LEN);
    sprintf(netmask, "%d.%d.%d.%d", (mask >> 24) & 0xFF, (mask >> 16) & 0xFF,
                           (mask >> 8)  & 0xFF, mask         & 0xFF);
    printf("%s\n", netmask);

    memset(&pool, 0, sizeof(rpool_t));
    memset(&host, 0, sizeof(host_t));
    memset(&vm, 0, sizeof(vm_info_t));

    if (lc_db_init("localhost", "root", "security421",
                "livecloud", "0", LC_DB_VERSION) != 0) {
        printf("lc_db_init error\n");
        return -1;
    }

#if 0
    if (lc_db_pool_insert("id, name, description",
                "\"1\", \"pool_1\", \"This is the 1st pool\"") != 0) {
        printf("lc_db_pool_insert error\n");
        return -1;
    }
    if (lc_db_pool_dump("*") != 0) {
        printf("lc_db_pool_dump error\n");
        return -1;
    }

    if (lc_db_pool_update("description", "\"This is a test update\"", "name=\"pool_v1\"") != 0) {
        printf("lc_db_pool_update error\n");
        return -1;
    }

    if (lc_db_pool_dump("id, name") != 0) {
        printf("lc_db_pool_dump error\n");
        return -1;
    }

    if (lc_db_pool_delete("id=1") != 0) {
        printf("lc_db_pool_delete error\n");
        return -1;
    }

    if (lc_db_pool_dump("id, name, description") != 0) {
        printf("lc_db_pool_dump error\n");
        return -1;
    }

    lc_db_term();
#endif
    return 0;
}
