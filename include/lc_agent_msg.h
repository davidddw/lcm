/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Filename: lc_agent_msg.h
 * Author Name: Jin Jie
 * Date: 2013-1-17
 *
 * Description: Header for lc_platform to agent messages
 *
 */

#ifndef _LC_AGENT_MSG_H
#define _LC_AGENT_MSG_H

#include "agent_sizes.h"
#include "agent_enum.h"

#if 0X1C240000 != AGENT_VERSION_MAGIC
#error "agent version not correct (../xcp_agent/agent_enum.h AGENT_VERSION_MAGIC)"
#endif

/* vm */

typedef struct agent_vm_id_s {
    /* logic id */
    uint32_t    vm_type;
    uint32_t    vm_id;
    uint32_t    vdc_id;
    uint32_t    host_id;

    /* physical id */
    char        vm_label[AGENT_LABEL_LEN];

    uint32_t    vm_res_type;
} agent_vm_id_t;

int lc_agent_add_vm_monitor(agent_vm_id_t *vm, int count, char *ip);
int lc_agent_del_vm_monitor(agent_vm_id_t *vm, int count, char *ip);

/* interface */

typedef struct agent_interface_id_s {
    uint32_t    vif_id;
    char        mac[AGENT_MAC_LEN];
    char        vm_uuid[AGENT_UUID_LEN];
    char        vm_name[AGENT_NAME_LEN];
    uint32_t    bandwidth;                         /* FIXME deprecated field */
           /*since all bandwidth fields in controller already upgrade to u64 */
    uint16_t    vlan_tag;
    uint16_t    ofport;

    /* for VMware */
    uint32_t    if_index;
    uint32_t    if_subnetid;

    /* assist fields */
    uint32_t    device_type;
    uint32_t    device_id;
} agent_interface_id_t;

#define LC_VM_MAX_VIF 7
typedef struct agent_vm_vif_id_s {
    /* logic id */
    uint32_t    vm_type;
    uint32_t    vm_id;
    uint32_t    vdc_id;
    uint32_t    host_id;

    /* physical id */
    char        vm_label[AGENT_LABEL_LEN];

    uint32_t    vm_res_type;

    uint32_t    vif_num;
    agent_interface_id_t vifs[LC_VM_MAX_VIF];
} agent_vm_vif_id_t;
typedef struct agent_hwdev_s {
    char           hwdev_ip[AGENT_IPV4_ADDR_LEN];
    char           community[AGENT_NAME_LEN];
    char           username[AGENT_NAME_LEN];
    char           password[AGENT_NAME_LEN];
} agent_hwdev_t;
int lc_agent_request_host_monitor(char *ip, int htype, int fd, char *buf, int len);
int lc_agent_request_vm_monitor(agent_vm_vif_id_t *vm, int count, char *ip);
int lc_vcd_request_vm_monitor(int fd, char *buf, int len);
int lc_vcd_request_vm_vif_usage(int fd, char *buf, int data_len);
int lc_agent_request_vm_vif_usage(agent_vm_vif_id_t *vm, int count, char *ip, uint32_t timestamp);
int lc_agent_request_host_usage(char *ip);
int lc_agent_request_hwdev_usage(agent_hwdev_t *hwdev, char *ip);
int lc_agent_request_vmware_host_usage(int fd, char *buf, int data_len);
#endif /* _LC_AGENT_MSG_H */
