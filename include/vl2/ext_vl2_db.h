#ifndef __EXT_VL2_DB_H__
#define __EXT_VL2_DB_H__

int is_exist(char *table_name, char *condition, int *exist_or_not);
int is_unique(char *table_name, char *column, char *condition, int *unique_or_not);
int is_join_unique(char *table_name, char *src_ip, char *des_ip, int *unique_or_not);
int ext_vl2_db_lookup(char *table_name, char *column, char *condition, char *value);
int ext_vl2_db_lookup_v2(char *table_name, char *column, char *condition, char *value, char err_grade);
int ext_vl2_db_lookups(char *table_name, char *column, char *condition, char * const values[]);
int ext_vl2_db_lookups_v2(
        char *table_name, char *column, char *condition, char * const values[], int values_size);
int ext_vl2_db_lookups_multi_v2(
        char *table_name, char *column, char *condition, char * const values[], int values_size);
int ext_vl2_db_count(char *table_name, char *column, char *condition, int *count);
int ext_vl2_db_count_v2(char *table_name, char *column, char *condition, int *count, char err_grade);
int ext_vl2_db_assign(char *table_name, char *column, char *condition, char *value);
int ext_vl2_db_update(char *table_name, char *column, char *value, char *condition);
int ext_vl2_db_updates(char *table_name, char *operation, char *condition);
int ext_vl2_db_insert(char *table_name, char *column_list, char *value_list);
int ext_vl2_db_delete(char *table_name, char *condition);
int ext_vl2_db_lookup_ids(char *table_name, char *column, char *condition, int *values);
int ext_vl2_db_lookup_ids_v2(
        char *table_name, char *column, char *condition, int *values, int values_max_size);
int ext_vl2_db_lookup_ids_v3(
        char *table_name, char *column, char *condition, int * const values[], int values_max_size);
int atomic_ext_vl2_db_assign_ids(char *table_name, char *column, char *condition, int *values);
int ext_vl2_db_policy_load(void *info, char *condition);
int ext_vl2_db_tunnel_policy_load(void *info, char *condition);
int ext_vl2_db_vl2_tunnel_policy_load(void *info, char *condition);
int ext_vl2_get_port_map_by_vnet_ipaddr(void *portmap_query, char *vnet_ip);
int ext_vl2_get_web_secu_conf_by_id(void *webconf_query, int webconfid);
int ext_vl2_get_port_map_by_web_server_confid(void *portmap_query, int web_server_confid);
int ext_vl2_get_port_map_by_id(void *portmap_query, int portmapid);
int ext_vl2_db_rack_tunnel_loadn(void *info, int info_size, int info_len, char *condition);
int ext_vl2_db_network_device_load(void *info, char *condition);
int ext_vl2_db_network_device_load_all(void *info, int info_size, int info_len, int *data_cnt, char *condition);

#endif /* __EXT_VL2_DB_H__ */
