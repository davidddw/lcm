/*
 * Copyright (c) 2012-2013 YunShan Networks, Inc.
 *
 * Author Name: Xiang Yang
 * Finish Date: 2013-08-12
 *
 */
#ifndef _LCS_TRAFFIC_H
#define _LCS_TRAFFIC_H


int usage_timer_init(void);
void *usage_socket_thread(void *msg);
void *usage_handle_thread(void *msg);
int get_vm_actual_memory(uint32_t *number, char *num);
int get_vm_actual_vdi_size(uint64_t *number, char *num);
void *list_vm_server_insert(void *msg);
void *list_vm_insert(void *msg);

#endif /* _LCS_TRAFFIC_H */
