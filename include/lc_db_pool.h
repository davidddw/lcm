#ifndef _LC_DB_POOL_H
#define _LC_DB_POOL_H

#include "vm/vm_host.h"

int lc_vmdb_rpool_insert (rpool_t *pool);
int lc_vmdb_rpool_delete (uint32_t pool_index);
int lc_vmdb_pool_update_name (char *pool_name, uint32_t pool_index);
int lc_vmdb_pool_update_desc (char *pool_desc, uint32_t pool_index);
int lc_vmdb_pool_update_master_ip (char *master_ip, uint32_t pool_index);
int lc_vmdb_pool_load (rpool_t *pool, uint32_t pool_index);
int lc_vmdb_pool_load_all_v2_by_type (rpool_t *pool, int offset, int *pool_num,
        int type);
int lc_vmdb_check_pool_exist (uint32_t pool_index);
int lc_vmdb_get_pool_htype(uint32_t poolid);
int lc_vmdb_pool_load_all_v2_by_type_domain (rpool_t *pool, int offset, int *pool_num,
                                             int type, char *domain);

#endif
