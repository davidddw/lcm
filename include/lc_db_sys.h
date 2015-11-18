#ifndef _LC_DB_SYS_H
#define _LC_DB_SYS_H

int lc_sysdb_get_configuration(char *module_name, char *param_name, char *value, int size);
int lc_sysdb_get_domain(char *value, int size);
int lc_sysdb_get_lcm_ip(char *value, int size);
int lc_sysdb_get_running_mode(char *value, int size);

#endif
