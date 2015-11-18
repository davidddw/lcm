#ifndef _LC_UTILS_H
#define _LC_UTILS_H

#include <xen/api/xen_all.h>

typedef int (*lc_db_log_cb)(char *action, char *loginfo, int level);

void chomp(char *str);
void dump_buffer(void *data, int count);
int lc_call_system(char* command,char *action, char *obj, char *info, lc_db_log_cb lc_db_log);

void lc_get_mac_str_for_vm(char *mac, int vnet_id, int vl2_id,
        int vm_id, int index);
void lc_get_mac_str_for_gw(char *mac, int vnet_id, int vl2_id,
        int vm_id, int index);
void get_vmname_by_id(char *vmname, int vnet_id, int vl2_id, int vm_id);

int get_param(char *value, int size, char *get_str, ...);

void lc_print_xapi_error(char *action, char *obj, char *info, xen_session *session, lc_db_log_cb lc_db_log);
int lc_call_file_stream(char *value, char* command,char *action, char *obj, char *info, lc_db_log_cb lc_db_log);

int wrap_cmd_param_inplace(char *param, int param_buf_len);
int wrap_cmd_param(char *raw_param, char *param, int param_buf_len);

#endif
