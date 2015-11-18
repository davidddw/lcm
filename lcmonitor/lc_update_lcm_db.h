/*
 * Copyright 2012 yunshan.net.cn
 * All rights reserved
 * Author: Lai Yuan laiyuan@yunshan.net.cn
 */
#ifndef __LC_UPDATE_LCM_DB_H_INCLUDED__
#define __LC_UPDATE_LCM_DB_H_INCLUDED__

#include <sys/types.h>
#include "lc_lcm_api.h"

int get_lcm_server();
int lc_update_lcm_vmdb_state(int vmstate, uint32_t vmid);
int lc_update_lcm_vmdb_state_by_host(char *hostip);
int lc_update_lcm_vgate_state(int vgtatestate, uint32_t vgateid);
int lc_update_lcm_sharedsrdb(int disk_used, char *sr_name, char *lcm_ip, char *domain);
#endif
