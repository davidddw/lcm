/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Shi Lei
 * Finish Date: 2013-07-11
 *
 */

#ifndef _LCS_TRAFFIC_VMWARE_H
#define _LCS_TRAFFIC_VMWRAE_H

#include "lc_snf.h"
#include "lc_agent_msg.h"

typedef struct vcd_vm_intf_stats_s {
    char        vm_name[MAX_VM_NAME_LEN];
    intf_t      intf;

    uint64_t    rx_bytes;
    uint64_t    rx_dropped;
    uint64_t    rx_errors;
    uint64_t    rx_packets;
    uint64_t    rx_bps;
    uint64_t    rx_pps;
    uint64_t    tx_bytes;
    uint64_t    tx_dropped;
    uint64_t    tx_errors;
    uint64_t    tx_packets;
    uint64_t    tx_bps;
    uint64_t    tx_pps;
} vcd_vm_intf_stats_t;

typedef struct vmware_host_info_s {
    char                      host_address[AGENT_IPV4_ADDR_LEN];
    char                      *sr_name;
    int                       sr_count;
    int                       host_state;
    char                      cpu_usage[AGENT_CPU_USAGE_STR_LEN];
    uint32_t                  cpu_count;
    int                       *sr_disk_total;
    char                      *sr_usage;
} vmware_host_info_t;

int lc_vcd_hosts_info_msg(char *host_address, void *buf);
int lc_vcd_host_usage_reply_msg(vmware_host_info_t *host, void *buf, int len);
int lc_vcd_vm_vif_usage_msg(agent_vm_vif_id_t *vm_intf, int count, void *buf);
int lc_vcd_vm_vif_reply_msg(msg_vm_info_response_t *vms, void *buf, int len);

#endif /* _LCS_TRAFFIC_VMWARE_H */
