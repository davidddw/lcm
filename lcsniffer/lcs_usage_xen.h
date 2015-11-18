/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-12
 *
 */

#ifndef _LCS_TRAFFIC_XEN_H
#define _LCS_TRAFFIC_XEN_H

/* TODO:
 * int lcs_hwdev_response_parse(hwdev_result_t *phwdev, int htype);
 */
int lcs_vif_traffic_response_parse(interface_result_t *pdata);
int lcs_vm_vif_response_parse(vm_result_t *pvm, int htype);
int lcs_vm_vif_req_usage_rt(agent_vm_vif_id_t *vm, char *ip, int op, vm_result_t *vm_result);
int lcs_vgw_vif_req_usage_rt(agent_vm_vif_id_t *vm, char *ip, vm_result_t *vm_result);


#endif /* _LCS_TRAFFIC_XEN_H */
