#ifndef _LC_MON_VMWARE_H
#define _LC_MON_VMWRAE_H

#include "vm/vm_host.h"
#include "lc_agent_msg.h"

#define SR_NAME_LEN 32
#define SR_NUMBER   3
typedef struct vmware_host_info_s {
    char                      host_address[AGENT_IPV4_ADDR_LEN];
    char                      *sr_name;
    int                       sr_count;
    int                       host_state;
    char                      cpu_usage[AGENT_CPU_USAGE_STR_LEN];
    uint32_t                  cpu_count;
    uint32_t                  role;
    int                       *sr_disk_total;
    char                      *sr_usage;
} vmware_host_info_t;

typedef struct vcd_vm_stats_s {
    char        vm_name[MAX_VM_NAME_LEN];

    uint32_t    vm_state;
    char        cpu_usage[MAX_VM_CPU_USAGE_LEN];
    char        host_address[AGENT_IPV4_ADDR_LEN];
} vcd_vm_stats_t;

int lc_vcd_hosts_info_msg(char *host_address, void *buf);
int lc_vcd_host_usage_reply_msg(vmware_host_info_t *host, void *buf, int len);
int lc_vcd_vm_stats_msg(agent_vm_vif_id_t *vm, int count, void *buf);
int lc_vcd_vm_stats_reply_msg(vcd_vm_stats_t *vm, int *count, void *buf, int len, int max);

#endif
