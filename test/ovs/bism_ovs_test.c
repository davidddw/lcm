#include <stdio.h>
#include <stdlib.h>

#include "bism_ovs.h"
#include "bism_ovs_c.h"
#include "bismarck.h"

#define RES_MAX 4096

static void bism_ovs_db_ntfy_cb(char *o, char *n)
{
    printf("@%s: old: %s new: %s\n", __FUNCTION__, o, n);
}

int bism_framework_callback(char *socket,
    char *table,
    char *action,
    char *old,
    char *new)
{
    return 0;
}

int main(int argc, char *argv[])
{
    char res[RES_MAX];
    const char *cols[] = {"name", "statistics"};
    const char *conds[] = {"insert", "modify"};

    printf("bism_init\n");
    printf("%d\n", bism_init());
    getchar();

    printf("bism_ovs_register\n");
    printf("%d\n", bism_ovs_register("tcp:127.0.0.1:6632"));
    getchar();

    printf("bism_ovs_db_reg_ntfy\n");
    printf("%d\n", bism_ovs_db_reg_ntfy("tcp:127.0.0.1:6632", "Interface", cols, sizeof(cols) / sizeof(cols[0]),
                conds, sizeof(conds) / sizeof(conds[0]), bism_ovs_db_ntfy_cb));
    getchar();

    printf("bism_ovs_br_add\n");
    printf("%d\n", bism_ovs_br_add("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_db_unreg_ntfy\n");
    printf("%d\n", bism_ovs_db_unreg_ntfy("tcp:127.0.0.1:6632", "Interface", cols, sizeof(cols) / sizeof(cols[0]),
                conds, sizeof(conds) / sizeof(conds[0])));
    getchar();

    printf("bism_ovs_br_exist\n");
    printf("%d\n", bism_ovs_br_exist("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_br_list_name\n");
    printf("%d\n", bism_ovs_br_list_name(res, RES_MAX, "tcp:127.0.0.1:6632"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_br_set_controller\n");
    printf("%d\n", bism_ovs_br_set_controller("tcp:127.0.0.1:6632", "br0", "ptcp:6633"));
    getchar();

    printf("bism_ovs_br_get_controller\n");
    printf("%d\n", bism_ovs_br_get_controller(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_br_del_controller\n");
    printf("%d\n", bism_ovs_br_del_controller("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_br_get_controller\n");
    printf("%d\n", bism_ovs_br_get_controller(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_br_set_fail_mode\n");
    printf("%d\n", bism_ovs_br_set_fail_mode("tcp:127.0.0.1:6632", "br0", "secure"));
    getchar();

    printf("bism_ovs_br_get_fail_mode\n");
    printf("%d\n", bism_ovs_br_get_fail_mode(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_br_del_fail_mode\n");
    printf("%d\n", bism_ovs_br_del_fail_mode("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_br_get_fail_mode\n");
    printf("%d\n", bism_ovs_br_get_fail_mode(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_port_add\n");
    printf("%d\n", bism_ovs_port_add("tcp:127.0.0.1:6632", "br0", "eth0"));
    getchar();

    printf("bism_ovs_port_add_patch\n");
    printf("%d\n", bism_ovs_port_add_patch("tcp:127.0.0.1:6632", "br0", "patch-tun", "patch-int"));
    getchar();

    printf("bism_ovs_port_add_gre\n");
    printf("%d\n", bism_ovs_port_add_gre("tcp:127.0.0.1:6632", "br0", "gre", "10.0.0.1", "flow"));
    getchar();

    printf("bism_ovs_port_list_name\n");
    printf("%d\n", bism_ovs_port_list_name(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_port_to_br\n");
    printf("%d\n", bism_ovs_port_to_br(res, RES_MAX, "tcp:127.0.0.1:6632", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_iface_list_name\n");
    printf("%d\n", bism_ovs_iface_list_name(res, RES_MAX, "tcp:127.0.0.1:6632", "br0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_iface_to_br\n");
    printf("%d\n", bism_ovs_iface_to_br(res, RES_MAX, "tcp:127.0.0.1:6632", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_iface_admin_state_c\n");
    printf("%d\n", bism_ovs_iface_admin_state_c(res, RES_MAX, "tcp:127.0.0.1:6632", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_iface_statistics_c\n");
    printf("%d\n", bism_ovs_iface_statistics_c(res, RES_MAX, "tcp:127.0.0.1:6632", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_set\n");
    printf("%d\n", bism_ovs_db_set("tcp:127.0.0.1:6632", "port", "eth0", "tag=2"));
    getchar();

    printf("bism_ovs_db_add\n");
    printf("%d\n", bism_ovs_db_add("tcp:127.0.0.1:6632", "port", "eth0", "external_ids", "test=jonas"));
    getchar();

    printf("bism_ovs_db_get\n");
    printf("%d\n", bism_ovs_db_get(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "external_ids:test", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_del\n");
    printf("%d\n", bism_ovs_db_del("tcp:127.0.0.1:6632", "port", "eth0", "external_ids", "test"));
    getchar();

    printf("bism_ovs_db_get\n");
    printf("%d\n", bism_ovs_db_get(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "external_ids:test", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_find\n");
    printf("%d\n", bism_ovs_db_find(res, RES_MAX, "tcp:127.0.0.1:6632", "port", NULL, "tag=2"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_find\n");
    printf("%d\n", bism_ovs_db_find(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "name", "tag=2"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_get\n");
    printf("%d\n", bism_ovs_db_get(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "tag", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_clear\n");
    printf("%d\n", bism_ovs_db_clear("tcp:127.0.0.1:6632", "port", "eth0", "tag"));
    getchar();

    printf("bism_ovs_db_get\n");
    printf("%d\n", bism_ovs_db_get(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "tag", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_list (need huge buffer) \n");
    printf("%d\n", bism_ovs_db_list(res, RES_MAX, "tcp:127.0.0.1:6632", "port", NULL, NULL));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_list\n");
    printf("%d\n", bism_ovs_db_list(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "name", NULL));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_list (need big buffer) \n");
    printf("%d\n", bism_ovs_db_list(res, RES_MAX, "tcp:127.0.0.1:6632", "port", NULL, "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_db_list\n");
    printf("%d\n", bism_ovs_db_list(res, RES_MAX, "tcp:127.0.0.1:6632", "port", "name", "eth0"));
    printf("%s", res);
    getchar();

    printf("bism_ovs_port_del\n");
    printf("%d\n", bism_ovs_port_del("tcp:127.0.0.1:6632", "br0", "eth0"));
    getchar();

    printf("bism_ovs_br_del\n");
    printf("%d\n", bism_ovs_br_del("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_br_exist\n");
    printf("%d\n", bism_ovs_br_exist("tcp:127.0.0.1:6632", "br0"));
    getchar();

    printf("bism_ovs_unregister\n");
    printf("%d\n", bism_ovs_unregister("tcp:127.0.0.1:6632"));
    getchar();

    printf("bism_ovs_register\n");
    printf("%d\n", bism_ovs_register("tcp:127.0.0.1:6632"));
    getchar();

    printf("bism_ovs_unregister\n");
    printf("%d\n", bism_ovs_unregister("tcp:127.0.0.1:6632"));
    getchar();

    printf("bism_term\n");
    printf("%d\n", bism_term());
    getchar();

    return 0;
}

