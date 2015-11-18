#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#include "mntnct_db.h"
#include "mntnct_mongo.h"
#include "mntnct_socket.h"
#include "mntnct_commands.h"
#include "mntnct_utils.h"

#include "lc_bus.h"
#include "lc_netcomp.h"
#include "lc_aes.h"
#include "lc_agexec_msg.h"
#include "message/msg.h"
#include "message/msg_vm.h"
#include "header.pb-c.h"
#include "talker.pb-c.h"

#define NAME_LENGTH 60
#define UUID_LENGTH 64

#define XEN_VM_STATE_STR "xe -s %s -p 443 -u %s -pw %s vm-list params=power-state name-label=%s --minimal"
#define XEN_VM_UUID_STR "xe -s %s -p 443 -u %s -pw %s vm-list params=uuid name-label=%s --minimal"
#define OVS_GET_BR_NAME                                                        \
    "sshpass -p %s ssh %s@%s "                                                 \
    "\"br=\\`grep -E \\\"^%d=\\\" /etc/network_uuid_mapping | "                \
    " awk -F\\\"=\\\" '{print \\$2}'\\` "                                      \
    " && xe network-list uuid=\\$br params=bridge "                            \
    " | awk -F\\\":\\\" '{printf \\\"%%s\\\", \\$2}'"                          \
    "\""
#define KVM_VM_MIGRATE_STR                                                     \
    "sshpass -p %s ssh %s@%s "                                                 \
    "virsh migrate --live --persistent --undefinesource %s "                   \
    "qemu+tcp://%s/system"

#define LC_SET_OS_TEMPLATE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O A -s %s -t %s -g %s -v %s -d %s"
#define LC_SET_LOCAL_OS_TEMPLATE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O A -t %s -g %s -v %s -d %s -u %d"
#define LC_UNSET_OS_TEMPLATE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O D -t %s"
#define LC_UNSET_OS_TEMPLATE_BY_TTYPE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O D -t %s -g %s"
#define LC_MOD_OS_TEMPLATE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O M -s %s -t %s"
#define LC_MOD_OS_TEMPLATE_BY_TTYPE_STR "/usr/local/livecloud/script/" \
                               "lc_template_mangement.sh -O M -s %s -t %s -g %s"
#define LC_CHECK_IPMI_CONFIG_STR                                               \
    "ipmitool -I lanplus -H %s -U %s -P %s power status"
#define DATA_PLANE             "DATA"
#define CTRL_PLANE             "CTRL"
#define DATA_PUBLIC           "DATA_PUBLIC"
#define MAX_SR_NUM        10
#define MAX_HOST_ETH_COUNT 6
#define HOST_TYPE_VM      1
#define HOST_TYPE_GW      2
#define HOST_TYPE_NSP     3
#define HOST_HTYPE_XEN    1
#define HOST_HTYPE_VMWARE 2
#define HOST_HTYPE_KVM    3
#define HOST_NTYPE_GIB    1
#define HOST_NTYPE_10GIB  2
#define POOL_TYPE_VM      1
#define POOL_TYPE_GW      2
#define POOL_TYPE_NSP     3
#define POOL_TYPE_TPD     4
#define POOL_CTYPE_XEN    1
#define POOL_CTYPE_VMWARE 2
#define POOL_CTYPE_KVM    3
#define POOL_STYPE_LOCAL  1
#define POOL_STYPE_SHARED 2
#define POOL_IP_GLOBAL    0
#define POOL_IP_EXCLUSIVE 1
#define CR_SRV_FLAG_NULL  0
#define CR_SRV_FLAG_PEER  1
#define CR_SRV_FLAG_HA    2

#define CR_STATE_NORMAL        4
#define CR_STATE_ABNORMAL      5
#define CR_STATE_MAINTENANCE   8

#define HOST_STATE_CREATING    1
#define HOST_STATE_COMPLETE    2
#define HOST_STATE_MODIFYING   3
#define HOST_STATE_ABNORNAL    4
#define HOST_STATE_MAINTENANCE 5

#define LC_VM_TYPE_VM    1
#define LC_VM_TYPE_GW    2

#define LC_CTRL_BR_ID    0
#define LC_DATA_BR_ID    1
#define LC_ULNK_BR_ID    2
#define LC_BR_ID_NUM     8

#define LC_VM_STATE_RUNNING      4
#define LC_VM_STATE_ERROR        11

#define LICENSE_UER_NAME_LEN     60
#define LICENSE_SERVER_NUM_MAX   999
#define LICENSE_LEASE_MAX        999
#define LICENSE_TIME_LEN         10
#define LICENSE_MON_NUM          12
#define LICENSE_FIRST_DAY_OF_MON 1
#define ADMINISTRATOR_NAME       "admin"

#define SERVICE_VUL_SCANNER      3
#define SERVICE_INSTANCE_TYPE    1

#define DB_IPMI_STATE_RUNNING 1
#define DB_IPMI_STATE_STOPPED 2
#define DB_IPMI_ACTION_DEPLOYING 1

#define SWITCH_TYPE_ETHERNET 0
#define SWITCH_TYPE_OPENFLOW 1
#define SWITCH_TYPE_TUNNEL 2

#define SERVICE_TYPE_VUL_SCANNER    1
#define SERVICE_TYPE_NAS_STORAGE    2
#define SERVICE_TYPE_CLOUD_DISK     3
#define SERVICE_TYPE_BACKUP         4

#define VIF_SUBTYPE_SERVICE_VENDOR  0x2

#define CONTROL_PORT_VUL_SCANNER    443
#define CONTROL_PORT_BACKUP         9800

typedef int (*fp)(int opts, argument *args);

typedef struct {
    char     *name;
    char     *req_params;
    char     *opt_params;
    char     *description;
    char     *example;
    fp       func;
    uint32_t flag;
} operation;

#define OP_FLAG_HIDDEN              0x1

static int cluster_list(int opts, argument *args);
static int cluster_add(int opts, argument *args);
static int cluster_attach_to_vcdatacenter(int opts, argument *args);
static int cluster_remove(int opts, argument *args);
static int rack_list(int opts, argument *args);
static int rack_add(int opts, argument *args);
static int rack_remove(int opts, argument *args);
static int rack_update_license(int opts, argument *args);
static int beacon_list(int opts, argument *args);
static int host_list(int opts, argument *args);
static int host_goto(int opts, argument *args);
static int host_add(int opts, argument *args);
static int host_rescan(int opts, argument *args);
static int host_remove(int opts, argument *args);
/*hwdev added begin*/
static int hwdev_list(int opts, argument *args);
static int hwdev_add(int opts, argument *args);
static int hwdev_connect(int opts, argument *args);
static int hwdev_connect_list(int opts, argument *args);
static int hwdev_attach(int opts, argument *args);
static int hwdev_detach(int opts, argument *args);
static int hwdev_disconnect(int opts, argument *args);
static int hwdev_remove(int opts, argument *args);
static int live_x_add(int opts, argument *args);
static int live_x_remove(int opts, argument *args);
static int live_x_list(int opts, argument *args);
/*hwdev added end*/
static int torswitch_list(int opts, argument *args);
static int torswitch_add(int opts, argument *args);
static int torswitch_remove(int opts, argument *args);
static int host_port_torswitch_list(int opts, argument *args);
static int host_port_torswitch_config(int opts, argument *args);
static int host_port_torswitch_remove(int opts, argument *args);
static int host_join(int opts, argument *args);
static int host_peer_join(int opts, argument *args);
static int host_eject(int opts, argument *args);
static int host_peer_replace(int opts, argument *args);
static int storage_list(int opts, argument *args);
static int storage_activate(int opts, argument *args);
static int ha_storage_list(int opts, argument *args);
static int ha_storage_activate(int opts, argument *args);
static int pool_list(int opts, argument *args);
static int pool_add(int opts, argument *args);
static int pool_remove(int opts, argument *args);
static int ip_list(int opts, argument *args);
static int ip_add(int opts, argument *args);
static int ip_remove(int opts, argument *args);
static int isp_config(int opts, argument *args);
static int isp_list(int opts, argument *args);
static int vgateway_list(int opts, argument *args);
static int vm_list(int opts, argument *args);
static int vm_start(int opts, argument *args);
static int vm_stop(int opts, argument *args);
static int vm_migrate(int opts, argument *args);
static int vm_import(int opts, argument *args);
static int lcm_config(int opts, argument *args);
static int template_os_config(int opts, argument *args);
static int template_os_modify(int opts, argument *args);
static int template_os_delete(int opts, argument *args);
static int template_os_list(int opts, argument *args);
static int vcenter_list(int opts, argument *args);
static int vcenter_add(int opts, argument *args);
static int vcenter_remove(int opts, argument *args);
static int vcdatacenter_list(int opts, argument *args);
static int vcdatacenter_add(int opts, argument *args);
static int vcdatacenter_remove(int opts, argument *args);
static int log_level_list(int opts, argument *args);
static int log_level_set(int opts, argument *args);
static int nsp_vgateway_upper_limit_config(int opts, argument *args);
static int user_list(int opts, argument *args);
static int license_generate(int opts, argument *args);
static int tunnel_protocol_config(int opts, argument *args);
static int tunnel_protocol_list(int opts, argument *args);
static int product_specification_list(int opts, argument *args);
static int product_specification_add(int opts, argument *args);
static int product_specification_remove(int opts, argument *args);
static int product_specification_update(int opts, argument *args);
static int service_charge_add(int opts, argument *args);
static int service_charge_del(int opts, argument *args);
static int pool_add_product_spec(int opts, argument *args);
static int pool_remove_product_spec(int opts, argument *args);
static int pool_list_product_spec(int opts, argument *args);
static int vlantag_ranges_config(int opts, argument *args);
static int vlan_modify(int opts, argument *args);
static int ip_ranges_config(int opts, argument *args);
static int ipmi_list(int opts, argument *args);
static int ipmi_config(int opts, argument *args);
static int ipmi_remove(int opts, argument *args);
static int vul_scanner_config(int opts, argument *args);
static int vul_scanner_list(int opts, argument *args);
static int service_vendor_add(int opts, argument *args);
static int service_vendor_remove(int opts, argument *args);
static int service_vendor_list(int opts, argument *args);
static int ctrl_plane_bandwidth_config(int opts, argument *args);
static int ctrl_plane_bandwidth_list(int opts, argument *args);
static int service_plane_bandwidth_config(int opts, argument *args);
static int service_plane_bandwidth_list(int opts, argument *args);
static int ctrl_plane_vlan_config(int opts, argument *args);
static int ctrl_plane_vlan_list(int opts, argument *args);
static int service_plane_vlan_config(int opts, argument *args);
static int service_plane_vlan_list(int opts, argument *args);
static int domain_add(int opts, argument *args);
static int domain_remove(int opts, argument *args);
static int domain_list(int opts, argument *args);
static int domain_update(int opts, argument *args);
static int vif_list(int opts, argument *args);
/*web sys setting*/
static int invitation_list(int opts, argument *args);
static int invitation_add(int opts, argument *args);

static operation operations[] =
{
    {
        "cluster.list",
        "",
        "params, name, domain",
        "Show cluster(s) in LiveCloud",
        "mt cluster.list",
        cluster_list
    },

    {
        "cluster.add",
        "name, domain",
        "location, admin, description, support, vcdatacenter_name",
        "Add cluster to LiveCloud",
        "mt cluster.add name=clustername domain=BeiJing",
        cluster_add
    },

    {
        "cluster.attach-to-vcdatacenter",
        "vcdatacenter_name",
        "id, name",
        "Add connection from cluster to vcdatacenter",
        "mt cluster.attach-to-vcdatacenter vcdatacenter_name=dcname",
        cluster_attach_to_vcdatacenter
    },

    {
        "cluster.remove",
        "",
        "id, name",
        "Remove cluster from LiveCloud",
        "mt cluster.remove id=ID name=clustername",
        cluster_remove
    },

    {
        "rack.list",
        "",
        "params, name, cluster_name, domain",
        "Show rack(s) in LiveCloud",
        "mt rack.list name=rackname cluster_name=clustername",
        rack_list
    },

    {
        "rack.add",
        "name, cluster_name, license,switch_type",
        "location",
        "Add rack to LiveCloud",
        "mt rack.add name=rackname cluster_name=clustername license=xxxxx switch_type=Ethernet/Tunnel",
        rack_add
    },

    {
        "rack.remove",
        "",
        "id, name",
        "Remove rack from LiveCloud",
        "mt rack.remove id=ID name=rackname",
        rack_remove
    },

    {
        "rack.update-license",
        "id, license",
        "",
        "Update rack license in LiveCloud",
        "mt rack.update-license id=ID license=xxxxx",
        rack_update_license
    },

    {
        "beacon.list",
        "",
        "params",
        "Show pacemaker beacon (a.k.a., standby node) in LiveCloud",
        "mt beacon.list",
        beacon_list
    },

    {
        "host.list",
        "",
        "params, domain",
        "Show host device(s) in LiveCloud",
        "mt host.list",
        host_list
    },

    {
        "host.goto",
        "",
        "ip, name",
        "Ssh to host in LiveCloud",
        "mt host.goto ip=x.x.x.x",
        host_goto,
        OP_FLAG_HIDDEN
    },

    {
        "host.add",
        "ip, user_name, user_passwd, type, rack_name, htype, nic_type",
        "uplink_ip, uplink_netmask, uplink_gateway, misc, brand, model",
        "Add host device to LiveCloud",
        "mt host.add ip=x.x.x.x user_name=USER user_passwd=PASSWD "\
        "type=VM/Gateway/NSP rack_name=rackname htype=Xen/VMWare/KVM "\
        "nic_type=Gigabit/10Gigabit [ uplink_ip=x.x.x.x "\
        "uplink_netmask=x.x.x.x uplink_gateway=x.x.x.x ]",
        host_add
    },

    {
        "host.rescan",
        "type, htype",
        "id, ip",
        "Rescan host in LiveCloud",
        "mt host.rescan ip=x.x.x.x type=VM/Gateway/NSP htype=Xen/VMWare/KVM",
        host_rescan
    },

    {
        "host.remove",
        "",
        "id, ip",
        "Remove host from LiveCloud",
        "mt host.remove ip=x.x.x.x",
        host_remove
    },

    {
        "host.join",
        "ip, name, pool_name",
        "",
        "Add host device to LiveCloud pool",
        "mt host.join ip=x.x.x.x name=host pool_name=poolname",
        host_join
    },

    {
        "host.peer-join",
        "ip1, ip2, name, pool_name",
        "",
        "Add two host devices to LiveCloud Peer HA pool",
        "mt host.peer-join ip1=x.x.x.x ip2=x.x.x.x name=PeerHA pool_name=poolname",
        host_peer_join
    },

    {
        "host.eject",
        "ip",
        "",
        "Remove host from LiveCloud pool",
        "mt host.eject ip=x.x.x.x",
        host_eject
    },

    {
        "host.peer-replace",
        "old-host, new-host",
        "",
        "Replace a failed host (peer mode) with a new one",
        "mt host.peer-replace old-host=x.x.x.x new-host=x.x.x.x",
        host_peer_replace
    },

    {
        "host-port-torswitch.config",
        "host_ip, port_name, torswitch_port, torswitch_id",
        "port_type",
        "Configured to connect to the torswitch the host port on a switch",
        "mt host-port-torswitch.config host_ip=1.2.3.4 port_name=eth1" \
        "torswitch_port=eth-0-21 torswitch_id=1 # For Centec switch\n" \
        "mt host-port-torswitch.config host_ip=1.2.3.4 port_name=eth1" \
        "torswitch_port=21 torswitch_id=1 # For Arista switch",
        host_port_torswitch_config
    },

    {
        "host-port-torswitch.remove",
        "host_ip, port_name",
        "",
        "Remove configured to connect to the torswitch the host port on a switch",
        "mt host-port-torswitch.remove host_ip=1.2.3.4 port_name=eth2",
        host_port_torswitch_remove
    },

    {
        "host-port-torswitch.list",
        "",
        "params",
        "List configured to connect to the torswitch the host port on a switch",
        "mt host-port-torswitch.list",
        host_port_torswitch_list
     },

    {
        "storage.list",
        "",
        "params, host",
        "Show storage in LiveCloud",
        "mt storage.list host=x.x.x.x",
        storage_list
    },

    {
        "storage.activate",
        "id, host",
        "",
        "Activate storage in LiveCloud",
        "mt storage.activate id=ID host=x.x.x.x",
        storage_activate
    },

    {
        "ha-disk-storage.list",
        "",
        "params, host",
        "Show HA storage in LiveCloud",
        "mt ha-disk-storage.list host=x.x.x.x",
        ha_storage_list
    },

    {
        "ha-disk-storage.activate",
        "id, host",
        "",
        "Activate HA storage in LiveCloud",
        "mt ha-disk-storage.activate id=ID host=x.x.x.x",
        ha_storage_activate
    },

    {
        "pool.list",
        "",
        "params, name, domain",
        "Show pool(s) in LiveCloud",
        "mt pool.list",
        pool_list
    },

    {
        "pool.add",
        "name, type, cluster_name, ctype, stype",
        "description",
        "Add pool to LiveCloud",
        "mt pool.add name=poolname type=VM/Gateway/NSP cluster_name=clustername "
        "ctype=Xen/VMWare/KVM stype=local/shared",
        pool_add
    },

    {
        "pool.remove",
        "",
        "id, name",
        "Remove pool from LiveCloud",
        "mt pool.remove id=ID",
        pool_remove
    },

    {
        "pool.add-product-specification",
        "name, product_specification",
        "",
        "Add product-specification to pool",
        "mt pool.add-product-specification name=poolname "\
        "product_specification=product_spec_plan_name",
        pool_add_product_spec
    },

    {
        "pool.remove-product-specification",
        "name, product_specification",
        "",
        "Remove product-specification from pool",
        "mt pool.remove-product-specification name=poolname "\
        "product_specification=product_spec_plan_name",
        pool_remove_product_spec
    },

    {
        "pool.list-product-specification",
        "name",
        "",
        "List product-specification in pool",
        "mt pool.list-product-specification name=poolname",
        pool_list_product_spec
    },

    {
        "ip.list",
        "",
        "params, ISP, pool_name, vlantag, domain",
        "Show ip(s) in LiveCloud",
        "mt ip.list",
        ip_list
    },

    {
        "ip.add",
        "ip, netmask, gateway, ISP, product_specification, domain",
        "pool_name, vlantag",
        "Add ip to LiveCloud gateway pool",
        "mt ip.add ip=192.168.233.213 netmask=255.255.255.0 gateway=192.168.233.1 "
        "domain=BeiJing pool_name=poolname ISP=1 product_specification=product_spec_plan_name",
        ip_add
    },

    {
        "ip.remove",
        "",
        "id, ip",
        "Remove ip from LiveCloud",
        "mt ip.remove ip=192.168.233.213",
        ip_remove
    },

    {
        "isp.config",
        "ISP, name, bandwidth, domain",
        "",
        "Config name of system ISP (1-8), set name='' will disable the corresponding ISP",
        "mt isp.config ISP=1 bandwidth(Mbps)=1 name=ChinaMobile domain=BeiJing",
        isp_config
    },

    {
        "isp.list",
        "",
        "",
        "Show system ISP (1-8)",
        "mt isp.list",
        isp_list
    },

    {
        "vgateway.list",
        "",
        "params",
        "Show VGateway(s) in LiveCloud",
        "mt vgateway.list",
        vgateway_list
    },

    {
        "vm.list",
        "",
        "params",
        "Show VM(s) in LiveCloud",
        "mt vm.list",
        vm_list
    },

    {
        "vm.start",
        "name-label",
        "",
        "Start VM on host",
        "mt vm.start name-label=vm_YunShan",
        vm_start
    },

    {
        "vm.stop",
        "name-label",
        "",
        "Stop VM on host",
        "mt vm.stop name-label=vm_YunShan",
        vm_stop
    },

    {
        "vm.migrate",
        "name-label, remote-host, remote-sr",
        "",
        "Please use script/upgrade/vm_migrate.sh to migrate VM",
        "mt vm.migrate name-label=vm_YunShan remote-host=172.16.1.102 "\
        "remote-sr=SR_Local",
        vm_migrate,
        OP_FLAG_HIDDEN
    },

    {
        "vm.import",
        "launch-server, label, os, user-email, product-specification, domain",
        "",
        "Import VM to database",
        "mt vm.import launch-server=172.16.1.120 uuid=ssssssss "
        "os=_01_CentOS_02_6.5_04_64Bit_05_En user-email=x@yunshan.net.cn "
        "product-specification=Basic domain=BeiJing",
        vm_import
    },

    {
        "lcm-server.config",
        "lcm_server",
        "",
        "Config LCM",
        "mt lcm-server.config lcm_server=10.33.2.1",
        lcm_config
    },

    {
        "template.config",
        "name, ttype, vendor, domain",
        "server_ip, userid",
        "Config os template",
        "mt template.config name=templatename ttype=VM/Gateway "
        "vendor=YUNSHAN domain=BeiJing",
        template_os_config
    },

    {
        "template.modify",
        "name, domain",
        "ttype, server_ip, state, new_name",
        "Modify os template info",
        "mt template.modify name=templatename domain=BeiJing state=offline",
        template_os_modify
    },

    {
        "template.delete",
        "name",
        "ttype",
        "Delete os template",
        "mt template.delete name=templatename",
        template_os_delete
    },

    {
        "template.list",
        "",
        "params, name, domain",
        "Show templates in LiveCloud",
        "mt template.list",
        template_os_list
    },

    {
        "vcenter.list",
        "",
        "params",
        "Show vCenters in LiveCloud",
        "mt vcenter.list",
        vcenter_list
    },

    {
        "vcenter.add",
        "ip, username, passwd, in_mode, rack_name, domain",
        "description",
        "Add vCenter to LiveCloud",
        "mt vcenter.add ip=x.x.x.x username=USER passwd=PASSWD " \
        "in_mode=gateway/access rack_name=RACKNAME domain=DOMAIN",
        vcenter_add
    },

    {
        "vcenter.remove",
        "",
        "id, ip",
        "Remove vCenter from LiveCloud",
        "mt vcenter.remove ip=x.x.x.x",
        vcenter_remove
    },

    {
        "vcdatacenter.list",
        "",
        "params, server_ip, datacenter_name",
        "Show vDatacenters in LiveCloud",
        "mt vcdatacenter.list",
        vcdatacenter_list
    },

    {
        "vcdatacenter.add",
        "server_ip, datacenter_name, vswitch_name",
        "vm_folder, vm_templatefolder, description",
        "Add vDatacenters to LiveCloud",
        "mt vcdatacenter.add server_ip=x.x.x.x datacenter_name=dcname "\
        "vswitch_name=vsname",
        vcdatacenter_add
    },

    {
        "vcdatacenter.remove",
        "",
        "id, datacenter_name",
        "Remove vDatacenters from LiveCloud",
        "mt vcdatacenter.remove datacenter_name=dcname",
        vcdatacenter_remove
    },

    {
        "log-level.list",
        "",
        "component",
        "Dump log level",
        "mt log-level.list",
        log_level_list
    },

    {
        "log-level.set",
        "component, log-level",
        "",
        "Set log level",
        "mt log-level.set component=lcmd log-level=DEBUG",
        log_level_set
    },

    {
        "nsp-vgateway-upper-limit.config",
        "upper-limit",
        "",
        "Set the upper limit of vgateway count on one nsp",
        "mt nsp-vgateway-upper-limit.config upper-limit=20",
        nsp_vgateway_upper_limit_config
    },

    {
        "license.generate",
        "user_name, rack_serial_num, server_num, lease",
        "activation_time",
        "Generate license",
        "mt license.generate user_name=Yunshan rack_serial_num=1 server_num=10 "\
        "lease=12 activation_time=2014-01-10",
        license_generate,
        OP_FLAG_HIDDEN
    },

    {
        "hwdev.list",
        "",
        "params",
        "Show thrid party device(s) in LiveCloud",
        "mt hwdev.list",
        hwdev_list
    },

    {
        "hwdev.add",
        "name, EPC_user_email, order_id, pool_name, rack_name, role, "\
        "product_specification, brand, mgmt_ip, user_name, user_passwd",
        "",
        "Add third party device to LiveCloud, set all-caps brand name when "
        "device is managed by LiveCloud, otherwise set it to `non-managed'",
        "mt hwdev.add name=hwdev_name EPC_user_email=user_email order_id=id "\
        "rack_name=rackname role=STORAGE "\
        "product_specification=hwdev_product_spec_plan_name "\
        "brand=DELL mgmt_ip=192.168.21.21 "\
        "user_name=xyz user_passwd=abc\n"\
        "mt hwdev.add name=hwdev_name pool_name=pool_name "\
        "rack_name=rackname role=GENERAL_PURPOSE "\
        "product_specification=hwdev_product_spec_plan_name "\
        "brand=DELL mgmt_ip=192.168.21.21 "\
        "user_name=xyz user_passwd=abc",
        hwdev_add
    },

    {
        "hwdev.connect",
        "hwdev_id, if_index, if_mac, switch_id, switch_port, speed",
        "",
        "Connect an interface of a third-party device to a switch port",
        "mt hwdev.connect hwdev_id=2 if_index=1 if_mac=01:32:34:23:23:10 "\
        "switch_id=5 switch_port=20 speed=1000Mbps",
        hwdev_connect
    },

    {
        "hwdev-connect.list",
        "",
        "params, hwdev_name",
        "Show hwdev in LiveCloud",
        "mt hwdev-connect.list hwdev_name=xx",
        hwdev_connect_list
    },

    {
        "hwdev.attach",
        "hwdev_id, if_index, if_type",
        "",
        "Attach an interface of a third-party device to the service network",
        "mt hwdev.attach hwdev_id=2 if_index=1 if_type=service",
        hwdev_attach
    },

    {
        "hwdev.detach",
        "hwdev_id, if_index",
        "",
        "Attach an interface of a third-party device to the service network",
        "mt hwdev.detach hwdev_id=2 if_index=1",
        hwdev_detach
    },

    {
        "hwdev.disconnect",
        "hwdev_id, if_index",
        "",
        "Disconnect an interface of a third-party device from a switch port",
        "mt hwdev.disconnect hwdev_id=2 if_index=1",
        hwdev_disconnect
    },

    {
        "hwdev.remove",
        "id",
        "ctrl_ip",
        "Remove thrid party device from LiveCloud",
        "mt hwdev.remove id=X",
        hwdev_remove
    },
    {
        "live-x.add",
        "name, bandwidth",
        "",
        "Add a live connector for third-party device to connect livecloud",
        "mt live-x.add name=live_x_10M bandwidth=10485760",
        live_x_add
    },
    {
        "live-x.remove",
        "name",
        "",
        "Remove live conector from DB",
        "mt live-x.remove name=live_x_10M",
        live_x_remove
    },
    {
        "live-x.list",
        "",
        "params",
        "Show live connector(s) in LiveCloud",
        "mt live-x.list",
        live_x_list
    },

    {
        "torswitch.list",
        "",
        "params",
        "Show network device(s) in LiveCloud",
        "mt torswitch.list",
        torswitch_list
    },

    {
        "torswitch.add",
        "name, rackid, mip, switch_brand, username, password, enable",
        "tunnel_ip",
        "Add network device to LiveCloud",
        "\nToR: mt torswitch.add name=rack1_up_switch_huawei rackid=rackid " \
        "mip=manage_ip switch_brand=Arista tunnel_ip=1.2.3.4 " \
        "username=admin password=arista enable=arista\n" \
        "Spine: mt torswitch.add name=spine_A rackid=0 " \
        "mip=manage_ip switch_brand=Huawei " \
        "username=admin password=huawei enable=huawei",
        torswitch_add
    },

    {
        "torswitch.remove",
        "",
        "id, mip",
        "Remove torswitch from LiveCloud",
        "mt torswitch.remove id=ID mip=manage_ip",
        torswitch_remove
    },

    {
        "user.list",
        "",
        "params",
        "Show user(s) in LiveCloud",
        "mt user.list",
        user_list
    },

    {
        "tunnel-protocol.config",
        "type, domain",
        "",
        "Config tunnel protocol to LiveCloud",
        "mt tunnel-protocol.config type=GRE/VXLAN domain=BeiJing",
        tunnel_protocol_config
    },

    {
        "tunnel-protocol.list",
        "",
        "domain",
        "List LiveCloud tunnel protocol",
        "mt tunnel-protocol.list domain=BeiJing",
        tunnel_protocol_list
    },

    {
        "product-specification.list",
        "",
        "params, domain",
        "Show product-specification(s) in LiveCloud",
        "mt product-specification.list",
        product_specification_list
    },

    {
        "product-specification.add",
        "product-type",
        "",
        "Add product specification",
        "mt product-specification.add product-type=VM",
        product_specification_add
    },

    {
        "product-specification.remove",
        "",
        "id, name, lcuuid",
        "Remove product specification",
        "mt product-specification.remove id=1",
        product_specification_remove
    },

    {
        "product-specification.update",
        "",
        "id, name, lcuuid, price, state",
        "Update product specification",
        "mt product-specification.update id=1 price=1.00",
        product_specification_update
    },

    {
        "service-charge.add",
        "service_type, instance_type, instance_name, product_specification, EPC_user",
        "start_time, end_time",
        "Add service charge",
        "mt service-charge.add service_type=Vul_Scanner instance_type=VM "\
        "instance_name=Yunshan product_specification=Vul_Scanner EPC_user=Yunshan "\
        "start_time/end_time=yyyy-mm-dd-hh",
        service_charge_add
    },

    {
        "service-charge.del",
        "service_type, instance_type, instance_name, product_specification, EPC_user",
        "start_time",
        "Delete service charge",
        "mt service-charge.add service_type=Vul_Scanner instance_type=VM "\
        "instance_name=Yunshan product_specification=Vul_Scanner EPC_user=Yunshan",
        service_charge_del
    },

    {
        "vlantag-ranges.config",
        "rack_name, ranges",
        "",
        "Configure vlantag range on the rack",
        "mt vlantag-ranges.config rack_name=rack1 ranges=10-20",
        vlantag_ranges_config
    },

    {
        "vlan.modify",
        "vl2id, vlantag",
        "rack_name",
        "Modify vl2 vlantag in the rack",
        "mt vlan.modify vl2id=1 rack_name=rack1 vlantag=10",
        vlan_modify
    },

    {
        "ip-ranges.config",
        "type, ip_min, ip_max, netmask, domain",
        "",
        "Config ip ranges for controller/vm/server/vmsevice/sevice, "
        "[ip_min, ip_max] must be a CIDR prefix. http://en.wikipedia.org/wiki/CIDR",
        "mt ip-ranges.config type=CONTROLLER ip_min=192.168.22.0 ip_max=192.168.22.255 netmask=255.255.0.0 domain=BeiJing",
        ip_ranges_config
    },

    {
        "vul-scanner.list",
        "task_type",
        "params",
        "Show vul_scanner item(s) in LiveCloud",
        "mt vul-scanner.list task_type=system",
        vul_scanner_list
    },

    {
        "vul-scanner.config",
        "id, task_id, service_vendor_ip",
        "",
        "Config taskid and service_vendor_ip for vul_scanner item",
        "mt vul-scanner.config id=1 task_id=1 service_vendor_ip=x.x.x.x",
        vul_scanner_config
    },

    {
        "ipmi.list",
        "",
        "params",
        "Show IPMI device(s) in LiveCloud",
        "mt ipmi.list",
        ipmi_list
    },

    {
        "ipmi.config",
        "host, ipmi_ip, username, password",
        "",
        "Config IPMI device",
        "mt ipmi.config host=172.16.1.131 "
        "ipmi_ip=172.17.1.131 username=admin password=passwd",
        ipmi_config
    },

    {
        "ipmi.remove",
        "host",
        "",
        "Remove IPMI device",
        "mt ipmi.remove host=172.16.1.131",
        ipmi_remove
    },

#if 0
    {
        "ipmi.start",
        "ipmi_ip",
        "",
        "Start IPMI device",
        "mt ipmi.start ipmi_ip=192.168.22.0",
        ipmi_start
    },

    {
        "ipmi.stop",
        "ipmi_ip",
        "",
        "Stop IPMI device",
        "mt ipmi.stop ipmi_ip=192.168.22.0",
        ipmi_stop
    },

    {
        "ipmi.deploy",
        "os_type, ipmi_ip, ipmi_mac, hostname, os_password, "
        "os_ip, os_netmask, os_gateway, os_dns, os_device_name",
        "",
        "Deploy IPMI device",
        "mt ipmi.deploy os_type=CentOS_6_5 ipmi_ip=192.168.22.0 "
        "ipmi_mac=02:04:06:08:0A:0C os_password=passwd "
        "os_ip=111.222.111.222 os_netmask=255.255.0.0 os_gateway=111.222.1.1 "
        "os_dns=8.8.8.8 os_device_name=eth0",
        ipmi_deploy
    },

    {
        "ipmi.console",
        "ipmi_ip",
        "",
        "Open IPMI device console",
        "mt ipmi.console ipmi_ip=192.168.22.0",
        ipmi_console
    },
#endif

    {
        "service-vendor.add",
        "service_vendor, service_type, service_ip, "\
        "service_user_name, service_user_passwd, control_ip",
        "",
        "Add service vendor",
        "mt service-vendor.add service_vendor=NSFOCUS service_type=Vul_Scanner "
        "service_ip=x.x.x.x, control_ip=x.x.x.x",
        service_vendor_add
    },

    {
        "service-vendor.remove",
        "service_vendor, service_type, service_ip",
        "",
        "Remove service vendor",
        "mt service-vendor.remove service_vendor=NSFOCUS service_type=Vul_Scanner "
        "service_ip=x.x.x.x",
        service_vendor_remove
    },

    {
        "service-vendor.list",
        "",
        "params",
        "Show service vendor(s) in LiveCloud",
        "mt service-vendor.list",
        service_vendor_list
    },
    {
        "ctrl-plane-bandwidth.config",
        "bandwidth, domain",
        "",
        "Config control plane bandwidth of VM",
        "mt ctrl-plane-bandwidth.config bandwidth=123456(Mbps) domain=BeiJing",
        ctrl_plane_bandwidth_config
    },

    {
        "ctrl-plane-bandwidth.list",
        "",
        "params, domain",
        "Show control plane bandwidth of VM",
        "mt ctrl-plane-bandwidth.list domain=BeiJing",
        ctrl_plane_bandwidth_list
    },
    {
        "service-plane-bandwidth.config",
        "bandwidth, domain",
        "",
        "Config service plane bandwidth of VM",
        "service-plane-bandwidth.config bandwidth=123456(Mbps) domain=BeiJing",
        service_plane_bandwidth_config
    },

    {
        "service-plane-bandwidth.list",
        "",
        "params, domain",
        "Show service plane bandwidth of VM",
        "mt service-plane-bandwidth.list domain=BeiJing",
        service_plane_bandwidth_list
    },

    {
        "ctrl-plane-vlan.config",
        "vlan, domain",
        "",
        "Config control plane bandwidth of VM",
        "mt ctrl-plane-vlan.config vlan=2 domain=BeiJing",
        ctrl_plane_vlan_config
    },

    {
        "ctrl-plane-vlan.list",
        "",
        "params, domain",
        "Show control plane bandwidth of VM",
        "mt ctrl-plane-vlan.list domain=BeiJing",
        ctrl_plane_vlan_list
    },

    {
        "service-plane-vlan.config",
        "vlan, domain",
        "",
        "Config service plane bandwidth of VM",
        "service-plane-vlan.config vlan=2 domain=BeiJing",
        service_plane_vlan_config
    },

    {
        "service-plane-vlan.list",
        "",
        "params, domain",
        "Show service plane bandwidth of VM",
        "mt service-plane-vlan.list domain=BeiJing",
        service_plane_vlan_list
    },

    {
        "invitation.add",
        "code, expdays",
        "",
        "Add a LCWEB invitation code, expdays from 1 to 365, default 30",
        "mt invitation.add code=a3fxk2, expdays=20",
        invitation_add
    },

    {
        "invitation.list",
        "",
        "",
        "show LCWEB available invitation codes",
        "mt invitation.list",
        invitation_list
    },

    {
        "domain.list",
        "",
        "params",
        "Show domain(s) in LiveCloud",
        "mt domain.list",
        domain_list
    },

    {
        "domain.add",
        "name, ip, role, public_ip",
        "",
        "Add domain",
        "mt domain.add name=BeiJing ip=10.33.6.8 role=BSS/OSS, public_ip=222.92.240.38",
        domain_add
    },

    {
        "domain.remove",
        "role",
        "id, name, lcuuid",
        "Remove domain",
        "mt domain.remove name=BeiJing",
        domain_remove
    },

    {
        "domain.update",
        "",
        "id, lcuuid, name, ip, role, public_ip",
        "Update domain",
        "mt domain.update id=1 name=BeiJing role=BSS/OSS",
        domain_update
    },

    {
        "vif.list",
        "",
        "params, ip, mac, subnetid, vlantag",
        "Show vif(s) in LiveCloud",
        "mt vif.list ip=10.10.10.1",
        vif_list
    },

    { 0, 0, 0, 0, 0 }
};

inline char* strlwr( char* str )
{
    char* orig = str;
    for ( ; *str != '\0'; str++ ) {
         *str = tolower(*str);
    }
    return orig;
}

static int check_netmask(char *netmask, int *net)
{
    uint32_t i, j;

    if (!strchr(netmask, '.')) {
        return -1;
    }

    i = inet_network(netmask);
    i = ~i;
    j = (uint32_t)log2((double)(++i));
    if ((0x1 << j) != i) {
        return -1;
    }
    if (net) {
        *net = 32 - j;
    }
    return 0;
}

static int check_masklen(int masklen, char **netmask)
{
    uint32_t i;

    if (masklen < 0 || masklen > 32) {
        return -1;
    }

    if (netmask) {
        i = htonl((~0) << (32 - masklen));
        *netmask = inet_ntoa(*(struct in_addr *)(&i));
    }

    return 0;
}

static int check_domain_consistence(char *host_ip, char *pool_name)
{
    char cond[BUFSIZE];
    char host_domain[BUFSIZE];
    char pool_domain[BUFSIZE];

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", host_ip);
    memset(host_domain, 0, sizeof host_domain);
    db_select(&host_domain, BUFSIZE, 1, "host_device", "domain", cond);
    if (!host_domain[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", host_ip);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_domain, 0, sizeof pool_domain);
    db_select(&pool_domain, BUFSIZE, 1, "pool", "domain", cond);
    if (!pool_domain[0]) {
        fprintf(stderr, "Error: Pool with name '%s' not exist\n", pool_name);
        return -1;
    }
    if (strcmp(host_domain, pool_domain)) {
        fprintf(stderr, "Error: Host and pool domain is inconsistent\n");
        return -1;
    }

    return 0;
}

bool has_arg(argument *args, char *key)
{
    while (strlen(args->key) > 0) {
        if (strcmp(args->key, key) == 0) {
            return true;
        }
        ++args;
    }
    return false;
}

static char *get_arg(argument *args, char *key)
{
    while (strlen(args->key) > 0) {
        if (strcmp(args->key, key) == 0) {
            if (strlen(args->value) > 0) {
                return args->value;
            } else {
                return 0;
            }
        }
        ++args;
    }
    return 0;
}

int help_func_(char *func, int opts)
{
    operation *op;
    int n, len;
    struct winsize size;

    if (!func) {
        if (opts & MNTNCT_OPTION_MINIMAL) {
            op = operations;
            while (op->name) {
                if (op->flag & OP_FLAG_HIDDEN) {
                    ++op;
                    continue;
                }
                printf("%s", op->name);
                ++op;
                if (op->name) {
                    putchar(',');
                }
            }
            putchar('\n');
        } else {
            ioctl(STDIN_FILENO, TIOCGWINSZ, &size);
            printf("Usage: mt <command> [command specific arguments]\n\n");
            printf("To get help on a specific command: mt help <command>\n\n");
            printf("Common command list\n-------------------\n");
            n = 0;
            op = operations;
            while (op->name) {
                if (op->flag & OP_FLAG_HIDDEN) {
                    ++op;
                    continue;
                }
                len = strlen(op->name);
                if (n > 0) {
                    if (n + len > size.ws_col) {
                        putchar('\n');
                        n = 0;
                    } else {
                        printf(", ");
                        n += 2;
                    }
                }
                if (n == 0) {
                    printf("    ");
                    n += 4;
                }
                printf("%s", op->name);
                n += len;
                ++op;
            }
            putchar('\n');
        }
    } else {
        op = operations;
        while (op->name) {
            if (strcmp(func, op->name) == 0) {
                break;
            }
            ++op;
        }

        if (op->name) {
            printf("command name            : %s\n", op->name);
            printf("        reqd params     : %s\n", op->req_params);
            printf("        optional params : %s\n", op->opt_params);
            printf("        description     : %s\n", op->description);
            printf("        example         : %s\n", op->example);
            printf("\n\n");
        } else {
            fprintf(stderr, "Unknown command: %s\n", func);
            fprintf(stderr, "For usage run: 'mntnct help'\n");
            return -1;
        }
    }

    return 0;
}

int call_func_(char *func, int opts, argument *args)
{
    operation *op = 0;
    argument *arg = 0;
    char *p = 0;
    char *all_params = 0;
    int len;
    int arg_len;
    int ok;

    op = operations;
    while (op->name) {
        if (strcmp(func, op->name) == 0) {
            break;
        }
        ++op;
    }

    if (!op->name) {
        fprintf(stderr, "Unknown command: %s\n", func);
        fprintf(stderr, "For usage run: 'mntnct help'\n");
        return -1;
    }

    /* 3 for ', ' and the last \0 */
    len = strlen(op->req_params) + strlen(op->opt_params) + 3;
    all_params = malloc(len * sizeof(char));
    if (!all_params) {
        fprintf(stderr, "Error: malloc() failed\n");
        return -1;
    }
    memset(all_params, 0, len * sizeof(char));
    snprintf(all_params, len, "%s, %s", op->req_params, op->opt_params);

    arg = args;
    for (arg = args; strlen(arg->key) > 0; ++arg) {
        ok = 0;
        p = strstr(all_params, arg->key);
        do {
            if (p) {
                arg_len = strlen(arg->key);
                if ((p == all_params || !isalnum(*(p - 1))) &&
                         strncmp(arg->key, p, arg_len) == 0 &&
                                   !isalnum(*(p + arg_len))) {

                    ok = 1;
                    break;
                }
                p = strstr(p + arg_len, arg->key);
            }
        } while (p);
        if (ok) {
            continue;
        }
        fprintf(stderr, "Error: Unknown field '%s'\n", arg->key);
        if (all_params) {
            free(all_params);
        }
        return -1;
    }
    if (all_params) {
        free(all_params);
    }

    return op->func(opts, args);
}

/* cluster */
static int cluster_list(int opts, argument *args)
{
    char *default_params = "id,name,location,deploy_time,admin,support,"
                           "vc_datacenter_id,lcuuid,domain";
    char *params = get_arg(args, "params");
    char *name = get_arg(args, "name");
    char *domain_name = get_arg(args, "domain");
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (name) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and name='%s'", name);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (domain[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and domain='%s'", domain);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            if (cond[0]) {
                db_dump_cond_minimal("cluster", "id", cond);
            } else {
                db_dump_minimal("cluster", "id");
            }
        } else {
            if (cond[0]) {
                db_dump_cond_minimal("cluster", params, cond);
            } else {
                db_dump_minimal("cluster", params);
            }
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("cluster", params, cond);
        } else {
            db_dump("cluster", params);
        }
    }

    return 0;
}

static int cluster_add(int opts, argument *args)
{
    char *params = "name,location,admin,deploy_time,platform,support,"
                   "vc_datacenter_id,lcuuid,domain";
    char *domain_name = 0;
    char *name = 0;
    char *location = 0;
    char *admin = 0;
    char *description = 0;
    char *support = 0;
    char *vcdatacenter_name = 0;
    char vcdatacenterid[BUFSIZE] = {0};
    char values[BUFSIZE];
    char deploy_time[MNTNCT_CMD_RESULT_LEN];
    char dummy[BUFSIZE];
    int err;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char domain[BUFSIZE];
    char lcuuid[UUID_LENGTH];

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    } else if (strlen(name) > NAME_LENGTH) {
        fprintf(stderr, "Error: Name label too long\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "cluster", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Cluster '%s' already exists\n", name);
        return -1;
    }
    if (!(location = get_arg(args, "location"))) {
        location = "";
    }
    if (!(admin = get_arg(args, "admin"))) {
        admin = "";
    }
    if (!(description = get_arg(args, "description"))) {
        description = "";
    }
    if (!(support = get_arg(args, "support"))) {
        support = "";
    }
    memset(deploy_time, 0, sizeof deploy_time);
    err = call_system_output("date +'%F %T'",
            deploy_time, MNTNCT_CMD_RESULT_LEN);
    chomp(deploy_time);
    if (err || !deploy_time[0]) {
        fprintf(stderr, "Error: get date failed\n");
    }
    if (!(vcdatacenter_name = get_arg(args, "vcdatacenter_name"))) {
        snprintf(vcdatacenterid, BUFSIZE, "0");
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "datacenter_name='%s'", vcdatacenter_name);
        memset(vcdatacenterid, 0, sizeof vcdatacenterid);
        db_select(&vcdatacenterid, BUFSIZE, 1, "vc_datacenter", "id", cond);
        if (!vcdatacenterid[0]) {
            fprintf(stderr, "Error: vDatacenter with name '%s' not exist\n",
                    vcdatacenter_name);
            return -1;
        }
    }
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s','%s','%s','%s','%s','%s','%s','%s'",
            name, location, admin, deploy_time, description, support,
            vcdatacenterid, lcuuid, domain);
    db_insert("cluster", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "cluster", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int cluster_attach_to_vcdatacenter(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char *vcdatacenter_name = 0;
    char vcdatacenterid[BUFSIZE] = {0};
    char values[BUFSIZE];
    char dummy[BUFSIZE];
    char cond[BUFSIZE];
    char id_s[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    if (!id && !name) {
        fprintf(stderr, "Error: No id or name label specified\n");
        return -1;
    }
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "cluster", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Cluster '%s' not exist\n", name);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Cluster id and name mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "cluster", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Cluster with id '%s' not exist\n", id);
            return -1;
        }
    }
    if (!(vcdatacenter_name = get_arg(args, "vcdatacenter_name"))) {
        fprintf(stderr, "Error: No vDatacenter name specified\n");
        return -1;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "datacenter_name='%s'", vcdatacenter_name);
        memset(vcdatacenterid, 0, sizeof vcdatacenterid);
        db_select(&vcdatacenterid, BUFSIZE, 1, "vc_datacenter", "id", cond);
        if (!vcdatacenterid[0]) {
            fprintf(stderr, "Error: vDatacenter with name '%s' not exist\n",
                    vcdatacenter_name);
            return -1;
        }
        if (strcmp(vcdatacenterid, "0")) {
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "id=%s", vcdatacenterid);
            memset(dummy, 0, sizeof dummy);
            db_select(&dummy, BUFSIZE, 1, "vc_datacenter", "id", cond);
            if (!dummy[0]) {
                fprintf(stderr, "Error: vDatacenter with id '%s' not exist\n",
                        vcdatacenterid);
                return -1;
            }
        }
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "vc_datacenter_id=%s", vcdatacenterid);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id='%s'", id_s);
    db_update("cluster", values, cond);

    return 0;
}

static int cluster_remove(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    if (!id && !name) {
        fprintf(stderr, "Error: No id or name label specified\n");
        return -1;
    }
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "cluster", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Cluster '%s' not exist\n", name);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Cluster id and name mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "cluster", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Cluster with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "clusterid=%s", id);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "rack", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Cluster with id '%s' not empty\n", id);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("cluster", cond);

    return 0;
}

/* rack */
static int rack_list(int opts, argument *args)
{
    char *default_params = "id,name,location,clusterid,switch_type,lcuuid";
    char *params = get_arg(args, "params");
    char *name = get_arg(args, "name");
    char *cluster_name = get_arg(args, "cluster_name");
    char *domain_name = get_arg(args, "domain");
    char cluster_id[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};

    if (cluster_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", cluster_name);
        memset(cluster_id, 0, sizeof cluster_id);
        db_select(&cluster_id, BUFSIZE, 1, "cluster", "id", cond);
        if (!cluster_id[0]) {
            fprintf(stderr, "Error: Cluster with name '%s' not exist\n",
                    cluster_name);
            return -1;
        }
    }
    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (name) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and name='%s'", name);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (cluster_id[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and clusterid=%s", cluster_id);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (domain[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and domain='%s'", domain);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            if (cond[0]) {
                db_dump_cond_minimal("rack", "id", cond);
            } else {
                db_dump_minimal("rack", "id");
            }
        } else {
            if (cond[0]) {
                db_dump_cond_minimal("rack", params, cond);
            } else {
                db_dump_minimal("rack", params);
            }
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("rack", params, cond);
        } else {
            db_dump("rack", params);
        }
    }

    return 0;
}

static int rack_add(int opts, argument *args)
{
    char *params = "name,location,clusterid,server_num,license,lease,"
                   "activation_time,expiry_time,switch_type,lcuuid,domain";
    char *name = 0;
    char *location = 0;
    char *cluster_name = 0;
    char *switch_type = 0;
    char *license = 0;
    char cluster_id[BUFSIZE] = {0};
    char user_name[BUFSIZE] = {0};
    char rack_serial_num[BUFSIZE] = {0};
    char server_num[BUFSIZE] = {0};
    char lease[BUFSIZE] = {0};
    char activation_time[BUFSIZE] = {0};
    char expiry_time[BUFSIZE] = {0};
    char local_time[BUFSIZE] = {0};
    char dummy[BUFSIZE];
    char cond[BUFSIZE] = {0};
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char domain[BUFSIZE];
    int  switch_type_int = 0;
    time_t lt;
    struct tm* ptr = NULL;
    const char* pformat = "%Y-%m-%d";
    char lcuuid[UUID_LENGTH];

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    } else if (strlen(name) > NAME_LENGTH) {
        fprintf(stderr, "Error: Name label too long\n");
        return -1;
    }
    if (!(cluster_name = get_arg(args, "cluster_name"))) {
        fprintf(stderr, "Error: No cluster name specified\n");
        return -1;
    }
    if (!(switch_type = get_arg(args, "switch_type"))) {
        fprintf(stderr, "Error: No switch_type specified\n");
        return -1;
    }
    if ( !strcmp (switch_type, "OpenFlow")) {
        switch_type_int = SWITCH_TYPE_OPENFLOW;
    } else if ( !strcmp (switch_type, "Ethernet")) {
        switch_type_int = SWITCH_TYPE_ETHERNET;
    } else if ( !strcmp (switch_type, "Tunnel")) {
        switch_type_int = SWITCH_TYPE_TUNNEL;
    }

    if (!(license = get_arg(args, "license"))) {
        fprintf(stderr, "Error: No license specified\n");
        return -1;
    } else {
        if (BASE64_ENCODE_MAX_LEN < strlen(license)) {
            fprintf(stderr, "Error: Illegal license\n");
            return -1;
        }
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "license='%s'", license);
        memset(dummy, 0, sizeof dummy);
        db_select(&dummy, BUFSIZE, 1, "rack", "id", cond);
        if (dummy[0]) {
            fprintf(stderr, "Error: License '%s' already in use\n", license);
            return -1;
        }
    }
    snprintf(cond, BUFSIZE, "name='%s'", cluster_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, BUFSIZE, 1, "cluster", "domain", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Cluster with name '%s' not exist\n", cluster_name);
        return -1;
    }
    snprintf(cond, BUFSIZE, "name='%s'", cluster_name);
    memset(cluster_id, 0, sizeof cluster_id);
    db_select(&cluster_id, BUFSIZE, 1, "cluster", "id", cond);
    if (!cluster_id[0]) {
        fprintf(stderr, "Error: Cluster with name '%s' not exist\n", cluster_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "rack", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Rack with name '%s' already exists\n", name);
        return -1;
    }
    if (!(location = get_arg(args, "location"))) {
        location = "";
    }

    /* license decrypt */
    str_replace(license, '\n', '-');
    memset(values, 0, sizeof values);
    if (aes_cbc_128_decrypt(values, license)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    memset(server_num, 0, sizeof server_num);
    memset(lease, 0, sizeof lease);
    memset(activation_time, 0, sizeof activation_time);
    memset(expiry_time, 0, sizeof expiry_time);
    sscanf(values, "%[^#]#%[^#]#%[^#]#%[^#]#%[^#]#%[^#]#",
           user_name, rack_serial_num, server_num,
           activation_time, lease, expiry_time);
    if (LICENSE_UER_NAME_LEN < strlen(user_name)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_SERVER_NUM_MAX < atoi(server_num)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_LEASE_MAX < atoi(lease)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_TIME_LEN < strlen(activation_time)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_TIME_LEN < strlen(expiry_time)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }

    /* chk activation_time & expiry_time */
    lt = time(NULL);
    ptr = localtime(&lt);
    memset(local_time, 0, sizeof local_time);
    strftime(local_time, sizeof local_time, pformat, ptr);
    if (0 > strcmp(local_time, activation_time)) {
        fprintf(stderr, "Error: Activation time has not yet arrived\n");
        return -1;
    }
    if (0 < strcmp(local_time, expiry_time)) {
        fprintf(stderr, "Error: License has expired\n");
        return -1;
    }

    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE,
             "'%s','%s','%s','%s','%s','%s','%s','%s',%d,'%s','%s'",
             name, location, cluster_id, server_num, license,
             lease, activation_time, expiry_time, switch_type_int,
             lcuuid, domain);
    db_insert("rack", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "rack", "id", cond);
    printf("%s\n", id_s);
    printf("Please use the command torswitch.add to add the info "
           "of ToR switch.\r\n");
    return 0;
}

static int rack_remove(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    if (!id && !name) {
        fprintf(stderr, "Error: No id or name label specified\n");
        return -1;
    }
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "rack", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Rack '%s' not exist\n", name);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Rack id and name mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "rack", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Rack with id '%s' not exist\n", id);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "rackid=%s", id);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "host_device", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Rack with id '%s' not empty, host_device exists.\n", id);
        return -1;
    }
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "pacemaker_beacon", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Rack with id '%s' not empty, pacemaker_beacon exists.\n", id);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("rack", cond);

    return 0;
}

static int rack_update_license(int opts, argument *args)
{
    char *id;
    char *license = 0;
    char user_name[BUFSIZE] = {0};
    char rack_serial_num[BUFSIZE] = {0};
    char server_num[BUFSIZE] = {0};
    char lease[BUFSIZE] = {0};
    char activation_time[BUFSIZE] = {0};
    char expiry_time[BUFSIZE] = {0};
    char local_time[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char values[BUFSIZE] = {0};
    char ori_server_num[BUFSIZE] = {0};
    char ip_s[BUFSIZE] = {0};
    char id_s[BUFSIZE] = {0};
    int  loop = 0;
    time_t lt;
    struct tm* ptr = NULL;
    const char* pformat = "%Y-%m-%d";

    if (!(id = get_arg(args, "id"))) {
        fprintf(stderr, "Error: No rack id specified\n");
        return -1;
    }
    if (!(license = get_arg(args, "license"))) {
        fprintf(stderr, "Error: No license specified\n");
        return -1;
    } else {
        if (BASE64_ENCODE_MAX_LEN < strlen(license)) {
            fprintf(stderr, "Error: Illegal license\n");
            return -1;
        }
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "license='%s' and id<>%s", license, id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "rack", "id", cond);
        if (id_s[0]) {
            fprintf(stderr, "Error: License '%s' already in use\n", license);
            return -1;
        }
   }

    /* license decrypt */
    str_replace(license, '\n', '-');
    memset(values, 0, sizeof values);
    if (aes_cbc_128_decrypt(values, license)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    memset(server_num, 0, sizeof server_num);
    memset(lease, 0, sizeof lease);
    memset(activation_time, 0, sizeof activation_time);
    memset(expiry_time, 0, sizeof expiry_time);
    sscanf(values, "%[^#]#%[^#]#%[^#]#%[^#]#%[^#]#%[^#]#",
           user_name, rack_serial_num, server_num,
           activation_time, lease, expiry_time);
    if (LICENSE_UER_NAME_LEN < strlen(user_name)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_SERVER_NUM_MAX < atoi(server_num)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_LEASE_MAX < atoi(lease)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_TIME_LEN < strlen(activation_time)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }
    if (LICENSE_TIME_LEN < strlen(expiry_time)) {
        fprintf(stderr, "Error: Illegal license\n");
        return -1;
    }

    /* chk activation_time & expiry_time */
    lt = time(NULL);
    ptr = localtime(&lt);
    memset(local_time, 0, sizeof local_time);
    strftime(local_time, sizeof local_time, pformat, ptr);
    if (0 > strcmp(local_time, activation_time)) {
        fprintf(stderr, "Error: Activation time has not yet arrived\n");
        return -1;
    }
    if (0 < strcmp(local_time, expiry_time)) {
        fprintf(stderr, "Error: License has expired\n");
        return -1;
    }

    /* active rack & user */
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_update("rack", "is_expired=0", cond);

    memset(ori_server_num, 0, sizeof ori_server_num);
    db_select(&ori_server_num, BUFSIZE, 1, "rack", "server_num", cond);
    if (!ori_server_num[0]) {
        fprintf(stderr, "Error: Get server num in rack failed\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "rackid=%s", id);
    memset(ip_s, 0, sizeof ip_s);
    db_select(&ip_s, MAX_HOST_ADDRESS_LEN, atoi(ori_server_num),
              "host_device", "ip", cond);
    for (loop = 0; loop < atoi(ori_server_num); loop++) {

        if (!ip_s[MAX_HOST_ADDRESS_LEN * loop]) {
            break;
        }
        printf("%s\r\n", ip_s + MAX_HOST_ADDRESS_LEN * loop);
        while (1) {
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "launch_server='%s' and is_expired=1",
                     ip_s + (MAX_HOST_ADDRESS_LEN * loop));
            memset(id_s, 0, sizeof id_s);
            db_select(&id_s, BUFSIZE, 1, "vm", "distinct userid", cond);
            printf("%s\r\n", id_s);
            if (!id_s[0]) {
                break;
            }
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE,
                     "launch_server='%s' and is_expired=1 and userid=%s",
                     ip_s + (MAX_HOST_ADDRESS_LEN * loop), id_s);
            db_update("vm", "is_expired=0", cond);
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "id=%s", id_s);
            db_update("user", "is_expired=0", cond);
        }
    }

    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "rack", "id", "is_expired=1");
    if (!id_s[0]) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "username='%s'", ADMINISTRATOR_NAME);
        db_update("user", "is_expired=0", cond);
    }

    /* update rack info */
    str_replace(license, '-', '\n');
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "license='%s',server_num=%s,lease=%s,"\
             "activation_time='%s',expiry_time='%s'",
             license, server_num, lease, activation_time, expiry_time);
    db_update("rack", values, cond);

    return 0;
}

/* beacon */
static int beacon_list(int opts, argument *args)
{
    char *default_params = "id,name,ip,user_name,user_passwd,rackid,ring0_bindnet,ring0_mcastaddr,ring0_mcastport,ring1_bindnet,ring1_mcastaddr,ring1_mcastport";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("pacemaker_beacon", "ip");
        } else {
            db_dump_minimal("pacemaker_beacon", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("pacemaker_beacon", params);
    }

    return 0;
}

/* host */
static int host_list(int opts, argument *args)
{
    char *default_params = "l.id,l.type,l.state,l.name,l.ip,l.htype,l.poolid,l.rackid,l.uplink_ip,r.mem_total,r.mem_usage,r.cpu_used";
    char *params = get_arg(args, "params");
    char *domain_name = get_arg(args, "domain");
    char cond[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    char tmp_params[BUFSIZE] = {0};
    int flag = 0;
    char *l = "l.";
    char *tok = NULL;

    memset(cond, 0, sizeof cond);
    if (domain_name) {
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        memset(cond, BUFSIZE, sizeof cond);
        snprintf(cond, BUFSIZE, "domain='%s'", domain);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("host_device", "ip");
        } else if (strcmp(params, "user_name") &&
                strcmp(params, "user_passwd")) {
            db_dump_minimal("host_device", params);
        }
    } else {
        if (!params) {
            strcpy(tmp_params, default_params);
        } else {
            tok = strtok(params, ",");
            while (tok) {
                if (strcmp(tok, "user_name") == 0 ||
                        strcmp(tok, "user_passwd") == 0) {
                    tok = strtok(NULL, ",");
                    continue;
                }
                if (flag) {
                    strcat(tmp_params, ",");
                }
                strcat(tmp_params, l);
                strcat(tmp_params, tok);
                tok = strtok(NULL, ",");
                flag++;
            }
        }
        if (cond[0] == '\0') {
            snprintf(cond, BUFSIZE, "true");
        }
        db_dump_host_cond(tmp_params, cond);
        printf("type:\n\t1. VM host 2. Gateway host 3. NSP host\n");
        printf("htype:\n\t1. Xen host 2. VMware host 3. KVM host\n");
        printf("nic_type:\n\t1. Gigabit-NIC 2. 10Gigabit-NIC\n");
    }

    return 0;
}

static int host_goto(int opts, argument *args)
{
    char *ip = 0;
    char *name = 0;
    char cond[BUFSIZE];
    char ip_s[BUFSIZE];
    char cmd[BUFSIZE];

    return 0;

    ip = get_arg(args, "ip");
    name = get_arg(args, "name");
    if (!ip && !name) {
        fprintf(stderr, "Error: No ip or name label specified\n");
        return -1;
    }
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(ip_s, 0, sizeof ip_s);
        db_select(&ip_s, BUFSIZE, 1, "host_device", "ip", cond);
        if (!ip_s[0]) {
            fprintf(stderr, "Error: Host '%s' not exist\n", name);
            return -1;
        }
        if (ip && strcmp(ip, ip_s)) {
            fprintf(stderr, "Error: Host ip and name mismatch\n");
            return -1;
        }
        ip = ip_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(ip_s, 0, sizeof ip_s);
        db_select(&ip_s, BUFSIZE, 1, "host_device", "ip", cond);
        if (!ip_s[0]) {
            fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(cmd, 0, sizeof cmd);
    db_select(&cmd, BUFSIZE, 1, "host_device",
            "group_concat('sshpass -p ',user_passwd,' ssh ',user_name,'@',ip)",
            cond);
    system(cmd);

    return 0;
}

int port_determine_scope(char *portlist,char *port)
{
    char *portlisttemp = portlist;
    char *p;
    char *q;
    char star_cond[BUFSIZE] = {0};
    char  end_cond[BUFSIZE] = {0};
    int port_init = atoi(port);
    int find_flag = 0;
    int i = 0;
    sprintf(star_cond,strtok(portlisttemp,","));
    p = strpbrk(star_cond,"-");
    if (p == NULL){
        if (port_init == atoi(star_cond)){
            find_flag = 1;
        }
    } else {
           strcpy(end_cond, p+1);
           for (i = 0; i < BUFSIZE; i++) {
                if (star_cond[i] ==  '-') {
                    star_cond[i]= 0;
                    if ((port_init >=atoi(star_cond)) &&
                            (port_init <=atoi(end_cond))){
                        find_flag = 1;
                    }
                    break;
                }

           }
    }
    if (!find_flag) {
        while ((p = strtok(NULL, ","))) {
            q = strpbrk(p,"-");
            if (q == NULL) {
                if (port_init == atoi(p)) {
                    find_flag = 1;
                    break;
                }
            } else {
                   strcpy(end_cond, q + 1);
                   for (i = 0; i < BUFSIZE; i++) {
                        if (p[i] ==  '-') {
                            p[i]= 0;
                            if ((port_init >=atoi(p)) &&
                                    (port_init<=atoi(end_cond))){
                                find_flag = 1;
                            }
                            break;
                        }

                   }
            }
        }
    }
    return find_flag;
}

static int check_host_ip(char *ip, char *domain)
{
    char server_ip_min[BUFSIZE] = {0};
    char server_ip_max[BUFSIZE] = {0};
    char prefix[BUFSIZE] = {0};
    char cond[BUFSIZE];
    uint32_t min= 0;
    uint32_t max= 0;
    uint32_t ip_prefix= 0;
    uint32_t netmask, i;
    uint32_t u_ip = 0;

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "param_name='server_ctrl_ip_min' and domain='%s'", domain);
    db_select(&server_ip_min, BUFSIZE, 1, "domain_configuration", "value", cond);
    if (!server_ip_min[0]) {
        fprintf(stderr, "Error: Please config ip-ranges for SERVER_CTRL\n");
        return false;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "param_name='server_ctrl_ip_max' and domain='%s'", domain);
    db_select(&server_ip_max, BUFSIZE, 1, "domain_configuration", "value", cond);
    if (!server_ip_max[0]) {
        fprintf(stderr, "Error: Please config ip-ranges for SERVER_CTRL\n");
        return false;
    }

    min = inet_network(server_ip_min);
    max = inet_network(server_ip_max);
    u_ip = inet_network(ip);
    netmask = min ^ max;
    i = (uint32_t)log2((double)(++netmask));
    netmask = htonl(0xffffffff << i);
    ip_prefix = htonl(min) & netmask;
    sprintf(prefix, "%u.%u.%u.%u", ip_prefix & 0xff, (ip_prefix >> 8) & 0xff,
            (ip_prefix >> 16) & 0xff, (ip_prefix >> 24) & 0xff);

    if((htonl(u_ip) & netmask) != ip_prefix) {
        fprintf(stderr, "Error: invalid IP! prefix must be %s\n", prefix);
        return false;
    }
    return true;
}

static int host_add(int opts, argument *args)
{
    char *params = "type,state,ip,uplink_ip,uplink_netmask,uplink_gateway,"
                   "htype,nic_type,user_name,user_passwd,misc,brand,model,"
                   "rackid,clusterid,lcuuid,domain";
    char *type = 0;
    int type_int = 0;
    char *ip = 0;
    char *uplink_ip = 0;
    char *uplink_netmask = 0;
    char *uplink_gateway = 0;
    int masklen;
    char *htype = 0;
    int htype_int = 0;
    char *nic_type = 0;
    int nic_type_int = 0;
    char *user_name = 0;
    char *user_passwd = 0;
    char *misc = 0;
    char *brand = 0;
    char *model = 0;
    char *rack_name = 0;
    char rack_id[BUFSIZE] = { 0 };
    char rack_flag[BUFSIZE] = {0};
    int rack_flag_init = 0;
    char cluster_id[BUFSIZE] = { 0 };
    char is_expired[BUFSIZE] = { 0 };
    char max_server_num[BUFSIZE] = { 0 };
    char cur_server_num[BUFSIZE] = { 0 };
    char id[BUFSIZE] = { 0 };
    char dummy[BUFSIZE];
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char state[BUFSIZE];
    char domain[BUFSIZE];
    lc_mbuf_t msg;
    struct msg_host_device_opt *stg;
    char lcuuid[UUID_LENGTH];

    if (!(type = get_arg(args, "type"))) {
        fprintf(stderr, "Error: No host type specified\n");
        return -1;
    }
    if (strcasecmp(type, "vm") == 0 ||
        strcasecmp(type, "server") == 0 ||
        strcasecmp(type, "1") == 0) {

        type_int = HOST_TYPE_VM;

    } else if (strcasecmp(type, "gateway") == 0 ||
               strcasecmp(type, "gw") == 0 ||
               strcasecmp(type, "2") == 0) {

        type_int = HOST_TYPE_GW;

    } else if (strcasecmp(type, "nspgateway") == 0 ||
               strcasecmp(type, "nsp") == 0 ||
               strcasecmp(type, "3") == 0) {

        type_int = HOST_TYPE_NSP;

    } else {

        fprintf(stderr, "Error: Unknown host type %s\n", type);
        return -1;

    }
    if (type_int == HOST_TYPE_VM) {
        if (!(htype = get_arg(args, "htype"))) {
            fprintf(stderr, "Error: No host hypervisor type specified\n");
            return -1;
        }
        if (strcasecmp(htype, "xen") == 0 ||
            strcasecmp(htype, "xenserver") == 0 ||
            strcasecmp(htype, "xcp") == 0 ||
            strcasecmp(htype, "1") == 0) {

            htype_int = HOST_HTYPE_XEN;

        } else if (strcasecmp(htype, "vmware") == 0 ||
                   strcasecmp(htype, "esxi") == 0 ||
                   strcasecmp(htype, "2") == 0) {

            htype_int = HOST_HTYPE_VMWARE;
        } else {

            fprintf(stderr, "Error: Unknown hypervisor type %s\n", htype);
            return -1;

        }
    } else if (type_int == HOST_TYPE_NSP) {
        htype = get_arg(args, "htype");
        if (!htype) {

            htype_int = HOST_HTYPE_KVM;

        } else if (strcasecmp(htype, "kvm") == 0 ||
                   strcasecmp(htype, "3") == 0) {

            htype_int = HOST_HTYPE_KVM;

        } else {

            fprintf(stderr, "Error: Unknown hypervisor type %s\n", htype);
            return -1;

        }
    } else {
        htype_int = HOST_HTYPE_XEN;
    }
    if (!(nic_type = get_arg(args, "nic_type"))) {
        fprintf(stderr, "Error: No host nic type specified\n");
        return -1;
    }
    if (strcasecmp(nic_type, "Gigabit") == 0) {
        nic_type_int = HOST_NTYPE_GIB;
    } else if (strcasecmp(nic_type, "10Gigabit") == 0) {
        nic_type_int = HOST_NTYPE_10GIB;
    } else {
        fprintf(stderr, "Error: Unknown nic type %s\n", nic_type);
        return -1;
    }
    if (!(rack_name = get_arg(args, "rack_name"))) {
        fprintf(stderr, "Error: No rack name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", rack_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, BUFSIZE, 1, "rack", "domain", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    }
    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No ip specified\n");
        return -1;
    }
    if (!check_host_ip(ip, domain)) {
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "host_device", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Host with ip '%s' already exists\n", ip);
        return -1;
    }

    if (!(uplink_ip = get_arg(args, "uplink_ip"))) {
        uplink_ip = "";
    }
    if (!(uplink_netmask = get_arg(args, "uplink_netmask"))) {
        uplink_netmask = "";
    } else {
        if (strlen(uplink_netmask) <= 2) {
            masklen = atoi(uplink_netmask);
            if (check_masklen(masklen, &uplink_netmask)) {
                fprintf(stderr, "Error: uplink_netmask '%s' invalid\n", uplink_netmask);
                return -1;
            }
        } else if (check_netmask(uplink_netmask, NULL)) {
            fprintf(stderr, "Error: uplink_netmask '%s' invalid\n", uplink_netmask);
            return -1;
        }
    }
    if (!(uplink_gateway = get_arg(args, "uplink_gateway"))) {
        uplink_gateway = "";
    }

    if (!(user_name = get_arg(args, "user_name"))) {
        fprintf(stderr, "Error: No username specified\n");
        return -1;
    }
    if (!(user_passwd = get_arg(args, "user_passwd"))) {
        fprintf(stderr, "Error: No password specified\n");
        return -1;
    }
    if (!(misc = get_arg(args, "misc"))) {
        misc = "";
    }
    if (!(brand = get_arg(args, "brand"))) {
        brand = "";
    }
    if (!(model = get_arg(args, "model"))) {
        model = "";
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", rack_name);
    memset(rack_id, 0, sizeof rack_id);
    db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
    if (!rack_id[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    }
    db_select(&rack_flag, BUFSIZE, 1, "rack", "switch_type", cond);
    rack_flag_init = atoi(rack_flag);
    if (rack_flag_init == SWITCH_TYPE_ETHERNET && type_int != HOST_TYPE_VM) {
        if(!uplink_ip[0] || !uplink_netmask[0] || !uplink_gateway[0]) {
            fprintf(stderr, "Error: No uplink_ip/uplink_netmask/uplink_gateway "
                    "specified for Ethernet rack\n");
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", rack_id);
    memset(cluster_id, 0, sizeof cluster_id);
    db_select(&cluster_id, BUFSIZE, 1, "rack", "clusterid", cond);
    if (!cluster_id[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", rack_id);
    memset(is_expired, 0, sizeof is_expired);
    db_select(&is_expired, BUFSIZE, 1, "rack", "is_expired", cond);
    if (!is_expired[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    } else {
        if (0 != atoi(is_expired)) {
            fprintf(stderr, "Error: Rack with name '%s' has expired\n", rack_name);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", rack_id);
    memset(max_server_num, 0, sizeof max_server_num);
    db_select(&max_server_num, BUFSIZE, 1, "rack", "server_num", cond);
    if (!max_server_num[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "type=1 and rackid=%s", rack_id);
    memset(cur_server_num, 0, sizeof cur_server_num);
    db_select(&cur_server_num, BUFSIZE, 1, "host_device", "count(*)", cond);
    if (!cur_server_num[0]) {
        fprintf(stderr, "Error: Host tables not exist\n");
        return -1;
    }
    if (atoi(cur_server_num) >= atoi(max_server_num)) {
        fprintf(stderr, "Error: Host num exceeds the limit in rack with name '%s'\n", rack_name);
        return -1;
    }
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE,
            "%d,%d,'%s','%s','%s','%s',%d,%d,'%s','%s','%s','%s','%s',%s,%s,'%s','%s'",
            type_int, 1, ip, uplink_ip, uplink_netmask, uplink_gateway,
            htype_int, nic_type_int, user_name, user_passwd,
            misc, brand, model, rack_id, cluster_id, lcuuid, domain);
    db_insert("host_device", params, values);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(id, 0, sizeof id);
    db_select(&id, BUFSIZE, 1, "host_device", "id", cond);

    memset(&msg, 0, sizeof msg);
    msg.hdr.type = LC_MSG_HOST_ADD;
    msg.hdr.magic = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_host_device_opt);

    stg = (struct msg_host_device_opt *)msg.data;
    stg->host_id = atoi(id);
    if (rack_flag_init){
        stg->host_is_ofs =1;
    }
    send_message(&msg);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    while (1) {
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "host_device", "state", cond);
        if (atoi(state) == 2) {
            break;
        }
        if (atoi(state) == 4) {
            fprintf(stderr, "Error: host with ip %s failed\n", ip);
            return -1;
        }
        printf("Waiting for host device to complete ...\n");
        sleep(10);
    }
    if (type_int == HOST_TYPE_NSP) {
        printf("No need to activate storage of this host before use\n");
    } else {
        printf("Please activate storage of this host before use\n");
    }

    return 0;
}

static int host_rescan(int opts, argument *args)
{
    char *id = 0;
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char state[BUFSIZE];
    lc_mbuf_t msg;
    struct msg_host_device_opt *stg;

    id = get_arg(args, "id");
    ip = get_arg(args, "ip");
    if (!id && !ip) {
        fprintf(stderr, "Error: No id or ip specified\n");
        return -1;
    }
    if (ip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "host_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Host '%s' not exist\n", ip);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Host id and ip mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "host_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Host with id '%s' not exist\n", id);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_update("host_device", "state=1", cond);

    memset(&msg, 0, sizeof msg);
    msg.hdr.type = LC_MSG_HOST_ADD;
    msg.hdr.magic = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_host_device_opt);

    stg = (struct msg_host_device_opt *)msg.data;
    stg->host_id = atoi(id);

    send_message(&msg);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    while (1) {
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "host_device", "state", cond);
        if (atoi(state) == 2) {
            break;
        }
        if (atoi(state) == 4) {
            fprintf(stderr, "Error: host with id %s failed\n", id);
            return -1;
        }
        printf("Waiting for host device to complete ...\n");
        sleep(10);
    }

    return 0;
}

static int host_remove(int opts, argument *args)
{
    char *id = 0;
    char *ip = 0;
    char cond[BUFSIZE];
    char dummy[BUFSIZE];
    char id_s[BUFSIZE];
    char pool_id[BUFSIZE];
    char ip_s[BUFSIZE];
    char storage_ids[MAX_SR_NUM][BUFSIZE];
    int i;

    id = get_arg(args, "id");
    ip = get_arg(args, "ip");
    if (!id && !ip) {
        fprintf(stderr, "Error: No id or ip specified\n");
        return -1;
    }
    if (ip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "host_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Host '%s' not exist\n", ip);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Host id and ip mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "host_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Host with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    memset(pool_id, 0, sizeof pool_id);
    db_select(&pool_id, BUFSIZE, 1, "host_device", "poolid", cond);
    if (atoi(pool_id) != -1) {
        fprintf(stderr, "Error: Host with id '%s' in use\n", id);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    memset(ip_s, 0, sizeof ip_s);
    db_select(&ip_s, BUFSIZE, 1, "host_device", "ip", cond);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_address='%s'", ip_s);
    memset(storage_ids, 0, sizeof(char) * BUFSIZE * MAX_SR_NUM);
    db_select(storage_ids, BUFSIZE, MAX_SR_NUM,
            "storage_connection", "storage_id", cond);
    for (i = 0; i < MAX_SR_NUM; ++i) {
        if (storage_ids[i][0]) {
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "host_address='%s' and storage_id=%s",
                    ip_s, storage_ids[i]);
            db_delete("storage_connection", cond);
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "storage_id=%s", storage_ids[i]);
            memset(dummy, 0, sizeof dummy);
            db_select(&dummy, BUFSIZE, 1, "storage_connection", "id", cond);
            if (dummy[0]) {
                continue;
            }
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "id=%s", storage_ids[i]);
            db_delete("storage", cond);
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_ip='%s'", ip_s);
    db_delete("host_port_config", cond);


    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("host_device", cond);

    return 0;
}

static int host_join(int opts, argument *args)
{
    char *cr_params = "name,type,service_flag,state,ip,uplink_ip,"
                      "uplink_netmask,uplink_gateway,"
                      "user_name,user_passwd,cpu_info,"
                      "mem_total,disk_total,poolid,rackid,lcuuid,domain";
    char *nr_params = "state,poolid,ip,type,lcuuid";
    char *sr_params = "state,type,ip,poolid,capacity,attachip,lcuuid";
    char *ip;
    char *name;
    char *pool_name;
    char pool_id[BUFSIZE] = {0};
    char host_type[BUFSIZE];
    char pool_type[BUFSIZE];
    char host_desc[BUFSIZE];
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id[BUFSIZE];
    char state[BUFSIZE];
    char pool_id_now[BUFSIZE];
    lc_mbuf_t msg;
    struct msg_compute_resource_opt *cr;
    struct msg_storage_resource_opt *sr;
    int count;

    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No host ip specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(pool_id_now, 0, sizeof pool_id_now);
    db_select(&pool_id_now, BUFSIZE, 1, "host_device", "poolid", cond);
    if (!pool_id_now[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip);
        return -1;
    }
    if (atoi(pool_id_now) != -1) {
        fprintf(stderr, "Error: Host with ip '%s' already in pool with id '%s'\n",
                ip, pool_id_now);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(state, 0, sizeof state);
    db_select(&state, BUFSIZE, 1, "host_device", "state", cond);
    if (!state[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip);
        return -1;
    }
    if (atoi(state) == 5) {
        fprintf(stderr, "Error: Host with ip '%s' in maintain state\n", ip);
        return -1;
    }
    memset(host_type, 0, sizeof host_type);
    db_select(&host_type, BUFSIZE, 1, "host_device", "type", cond);
    if (!(pool_name = get_arg(args, "pool_name"))) {
        fprintf(stderr, "Error: No pool name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_id, 0, sizeof pool_id);
    db_select(&pool_id, BUFSIZE, 1, "pool", "id", cond);
    if (!pool_id[0]) {
        fprintf(stderr, "Error: Pool with name '%s' not exist\n", pool_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", pool_id);
    memset(pool_type, 0, sizeof pool_type);
    db_select(&pool_type, BUFSIZE, 1, "pool", "type", cond);
    if (!pool_type[0]) {
        fprintf(stderr, "Error: Pool with id '%s' not exist\n", pool_id);
        return -1;
    }
    if (check_domain_consistence(ip, pool_name)) {
        return -1;
    }

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name specified\n");
        return -1;
    }
    if (!((atoi(host_type) == HOST_TYPE_VM &&
           atoi(pool_type) == POOL_TYPE_VM) ||
          (atoi(host_type) == HOST_TYPE_GW &&
           atoi(pool_type) == POOL_TYPE_GW) ||
          (atoi(host_type) == HOST_TYPE_NSP &&
           atoi(pool_type) == POOL_TYPE_NSP))) {

        fprintf(stderr, "Error: Host type and pool type mismatch\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE,
            "'%s',%s,0,1,ip,uplink_ip,uplink_netmask,uplink_gateway,"
            "user_name,user_passwd,cpu_info,"
            "mem_total,disk_total,%s,rackid,lcuuid,domain",
            name, host_type, pool_id);
    db_insert_select("compute_resource", cr_params,
            "host_device", values, cond);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "1,%s,ip,0,lcuuid", pool_id);
    db_insert_select("network_resource", nr_params,
            "host_device", values, cond);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "1,0,ip,%s,disk_total,ip,lcuuid", pool_id);
    db_insert_select("storage_resource", sr_params,
            "host_device", values, cond);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "poolid=%s", pool_id);
    db_update("host_device", values, cond);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_HOST_JOIN;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_compute_resource_opt);

    cr = (struct msg_compute_resource_opt *)&msg.data;
    memset(cond, 0, sizeof(cond));
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(id, 0, sizeof(id));
    db_select(&id, BUFSIZE, 1, "compute_resource", "id", cond);
    cr->compute_id = atoi(id);
    memset(id, 0, sizeof(id));
    db_select(&id, BUFSIZE, 1, "network_resource", "id", cond);
    cr->network_id = atoi(id);

    send_message(&msg);

    if (atoi(host_type) != HOST_TYPE_NSP) {
        memset(&msg, 0, sizeof(msg));
        msg.hdr.type   = LC_MSG_SR_JOIN;
        msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
        msg.hdr.length = sizeof(struct msg_storage_resource_opt);

        sr = (struct msg_storage_resource_opt *)&msg.data;
        memset(id, 0, sizeof(id));
        db_select(&id, BUFSIZE, 1, "storage_resource", "id", cond);
        sr->storage_id = atoi(id);
    }

    while (1) {
        count = 0;
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "compute_resource", "state", cond);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: compute resource of %s failed\n", ip);
            goto error;
        case 4:
            memset(cond, 0, sizeof cond);
            snprintf(cond, BUFSIZE, "ip='%s'", ip);
            memset(host_desc, 0, sizeof host_desc);
            db_select(&host_desc, BUFSIZE, 1, "host_device", "description", cond);
            //set openstack server in maintenace state
            if (state[0] && !strcmp(host_desc, "OPENSTACK")) {
                memset(values, 0, sizeof values);
                snprintf(values, BUFSIZE, "state=%d", HOST_STATE_MAINTENANCE);
                db_update("host_device", values, cond);

                memset(values, 0, sizeof values);
                snprintf(values, BUFSIZE, "state=%d", CR_STATE_MAINTENANCE);
                db_update("compute_resource", values, cond);
            }
            ++count;
            break;
        default:
            break;
        }
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "network_resource", "state", cond);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: network resource of %s failed\n", ip);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        if (count == 2) {
            break;
        }
        printf("Waiting for compute and network resource to complete ...\n");
        sleep(10);
    }

    if (atoi(host_type) != HOST_TYPE_NSP) {
        send_message(&msg);

        while (1) {
            count = 0;
            memset(state, 0, sizeof(state));
            db_select(&state, BUFSIZE, 1, "storage_resource", "state", cond);
            switch (atoi(state)) {
            case 5:
                fprintf(stderr, "Error: storage resource of %s failed\n", ip);
                goto error;
            case 4:
                ++count;
                break;
            default:
                break;
            }
            if (count == 1) {
                break;
            }
            printf("Waiting for storage resource to complete ...\n");
            sleep(10);
        }
    }

    return 0;

error:

    return -1;
}

static int host_peer_join(int opts, argument *args)
{
    char *cr_params = "name,type,service_flag,state,ip,uplink_ip,"
                      "uplink_netmask,uplink_gateway,"
                      "user_name,user_passwd,cpu_info,mem_total,disk_total,"
                      "poolid,rackid,dbr_peer_server,lcuuid,domain";
    char *nr_params = "state,poolid,ip,type,lcuuid";
    char *sr_params = "state,type,ip,poolid,capacity,attachip,lcuuid";
    char *ip1, *ip2;
    char *name;
    char *pool_name;
    int  srvflag;
    char pool_id[BUFSIZE] = {0};
    char host_type[BUFSIZE];
    char pool_type[BUFSIZE];
    char cond[BUFSIZE];
    char cond2[BUFSIZE];
    char values[BUFSIZE];
    char id[BUFSIZE];
    char state[BUFSIZE];
    char pool_id_now[BUFSIZE];
    lc_mbuf_t msg;
    struct msg_compute_resource_opt *cr;
    struct msg_storage_resource_opt *sr;
    int count;

    if (!(ip1 = get_arg(args, "ip1"))) {
        fprintf(stderr, "Error: No host1 ip specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip1);
    memset(pool_id_now, 0, sizeof pool_id_now);
    db_select(&pool_id_now, BUFSIZE, 1, "host_device", "poolid", cond);
    if (!pool_id_now[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip1);
        return -1;
    }
    if (atoi(pool_id_now) != -1) {
        fprintf(stderr, "Error: Host with ip '%s' already in pool with id '%s'\n",
                ip1, pool_id_now);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip1);
    memset(state, 0, sizeof state);
    db_select(&state, BUFSIZE, 1, "host_device", "state", cond);
    if (!state[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip1);
        return -1;
    }
    if (atoi(state) == 5) {
        fprintf(stderr, "Error: Host with ip '%s' in maintain state\n", ip1);
        return -1;
    }
    memset(host_type, 0, sizeof host_type);
    db_select(&host_type, BUFSIZE, 1, "host_device", "type", cond);
    if (!(pool_name = get_arg(args, "pool_name"))) {
        fprintf(stderr, "Error: No pool name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_id, 0, sizeof pool_id);
    db_select(&pool_id, BUFSIZE, 1, "pool", "id", cond);
    if (!pool_id[0]) {
        fprintf(stderr, "Error: Pool with name '%s' not exist\n", pool_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", pool_id);
    memset(pool_type, 0, sizeof pool_type);
    db_select(&pool_type, BUFSIZE, 1, "pool", "type", cond);
    if (!pool_type[0]) {
        fprintf(stderr, "Error: Pool with id '%s' not exist\n", pool_id);
        return -1;
    }
    if (check_domain_consistence(ip1, pool_name)) {
        return -1;
    }

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name specified\n");
        return -1;
    }
    if (!((atoi(host_type) == HOST_TYPE_VM &&
           atoi(pool_type) == POOL_TYPE_VM) ||
          (atoi(host_type) == HOST_TYPE_GW &&
           atoi(pool_type) == POOL_TYPE_GW))) {

        if (atoi(host_type) == HOST_TYPE_NSP) {

            fprintf(stderr, "Error: Hosts of this type cannot join as peer\n");
            return -1;
        }

        fprintf(stderr, "Error: Host type and pool type mismatch\n");
        return -1;
    }
    if (!(ip2 = get_arg(args, "ip2"))) {
        fprintf(stderr, "Error: No host1 ip specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip2);
    memset(pool_id_now, 0, sizeof pool_id_now);
    db_select(&pool_id_now, BUFSIZE, 1, "host_device", "poolid", cond);
    if (!pool_id_now[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip2);
        return -1;
    }
    if (atoi(pool_id_now) != -1) {
        fprintf(stderr, "Error: Host with ip '%s' already in pool with id '%s'\n",
                ip2, pool_id_now);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip2);
    memset(state, 0, sizeof state);
    db_select(&state, BUFSIZE, 1, "host_device", "state", cond);
    if (!state[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip2);
        return -1;
    }
    if (atoi(state) == 5) {
        fprintf(stderr, "Error: Host with ip '%s' in maintain state\n", ip2);
        return -1;
    }
    if (check_domain_consistence(ip2, pool_name)) {
        return -1;
    }

    memset(host_type, 0, sizeof host_type);
    db_select(&host_type, BUFSIZE, 1, "host_device", "type", cond);
    if (!((atoi(host_type) == HOST_TYPE_VM &&
           atoi(pool_type) == POOL_TYPE_VM) ||
          (atoi(host_type) == HOST_TYPE_GW &&
           atoi(pool_type) == POOL_TYPE_GW))) {

        if (atoi(host_type) == HOST_TYPE_NSP) {

            fprintf(stderr, "Error: Hosts of this type cannot join as peer\n");
            return -1;
        }

        fprintf(stderr, "Error: Host type and pool type mismatch\n");
        return -1;
    }
    if (atoi(host_type) == HOST_TYPE_VM) {
        srvflag = CR_SRV_FLAG_PEER;
    } else {
        srvflag = CR_SRV_FLAG_HA;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip1);
    memset(cond2, 0, sizeof cond2);
    snprintf(cond2, BUFSIZE, "ip='%s'", ip2);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE,
            "'%s',%s,%d,1,ip,uplink_ip,uplink_netmask,uplink_gateway,"
            "user_name,user_passwd,cpu_info,"
            "mem_total,disk_total,%s,rackid,'%s',lcuuid,domain",
            name, host_type, srvflag, pool_id, ip2);
    db_insert_select("compute_resource", cr_params,
            "host_device", values, cond);
    snprintf(values, BUFSIZE,
            "'%s',%s,%d,1,ip,uplink_ip,uplink_netmask,uplink_gateway,"
            "user_name,user_passwd,cpu_info,"
            "mem_total,disk_total,%s,rackid,'%s',lcuuid, domain",
            name, host_type, srvflag, pool_id, ip1);
    db_insert_select("compute_resource", cr_params,
            "host_device", values, cond2);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "1,%s,ip,0,lcuuid", pool_id);
    db_insert_select("network_resource", nr_params,
            "host_device", values, cond);
    db_insert_select("network_resource", nr_params,
            "host_device", values, cond2);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "1,0,ip,%s,disk_total,ip,lcuuid", pool_id);
    db_insert_select("storage_resource", sr_params,
            "host_device", values, cond);
    db_insert_select("storage_resource", sr_params,
            "host_device", values, cond2);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "poolid=%s", pool_id);
    db_update("host_device", values, cond);
    db_update("host_device", values, cond2);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_HOST_JOIN;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_compute_resource_opt);

    cr = (struct msg_compute_resource_opt *)&msg.data;
    memset(cond, 0, sizeof(cond));
    snprintf(cond, BUFSIZE, "ip='%s'", ip1);
    memset(cond2, 0, sizeof cond2);
    snprintf(cond2, BUFSIZE, "ip='%s'", ip2);
    memset(id, 0, sizeof(id));
    db_select(&id, BUFSIZE, 1, "compute_resource", "id", cond);
    cr->compute_id = atoi(id);
    memset(id, 0, sizeof(id));
    db_select(&id, BUFSIZE, 1, "network_resource", "id", cond);
    cr->network_id = atoi(id);

    send_message(&msg);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_SR_JOIN;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_storage_resource_opt);

    sr = (struct msg_storage_resource_opt *)&msg.data;
    memset(id, 0, sizeof(id));
    db_select(&id, BUFSIZE, 1, "storage_resource", "id", cond);
    sr->storage_id = atoi(id);

    while (1) {
        count = 0;
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "compute_resource", "state", cond);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: compute resource of %s failed\n", ip1);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "network_resource", "state", cond);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: network resource of %s failed\n", ip1);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "compute_resource", "state", cond2);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: compute resource of %s failed\n", ip2);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "network_resource", "state", cond2);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: network resource of %s failed\n", ip2);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        if (count == 4) {
            break;
        }
        printf("Waiting for compute and network resource to complete ...\n");
        sleep(10);
    }

    send_message(&msg);

    while (1) {
        count = 0;
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "storage_resource", "state", cond);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: storage resource of %s failed\n", ip1);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "storage_resource", "state", cond2);
        switch (atoi(state)) {
        case 5:
            fprintf(stderr, "Error: storage resource of %s failed\n", ip2);
            goto error;
        case 4:
            ++count;
            break;
        default:
            break;
        }
        if (count == 2) {
            break;
        }
        printf("Waiting for storage resource to complete ...\n");
        sleep(10);
    }

    return 0;

error:

    return -1;
}

static int host_eject(int opts, argument *args)
{
    char *ip;
    char dummy[BUFSIZE];
    char type[BUFSIZE];
    char cond[BUFSIZE];
    char table[BUFSIZE];

    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No host ip specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(type, 0, sizeof type);
    db_select(&type, BUFSIZE, 1, "host_device", "type", cond);
    if (!type[0]) {
        fprintf(stderr, "Error: Host with ip '%s' not exist\n", ip);
        return -1;
    }
    memset(table, 0, sizeof table);
    memset(cond, 0, sizeof cond);
    if (atoi(type) == HOST_TYPE_VM) {
        strncpy(table, "vm", BUFSIZE - 1);
        snprintf(cond, BUFSIZE, "launch_server='%s'", ip);
    } else if (atoi(type) == HOST_TYPE_GW || atoi(type) == HOST_TYPE_NSP) {
        strncpy(table, "vnet", BUFSIZE - 1);
        snprintf(cond, BUFSIZE, "gw_launch_server='%s'", ip);
    } else {
        fprintf(stderr, "Error: Host with ip '%s' type error\n", ip);
        return -1;
    }
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, table, "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Host with ip '%s' in use\n", ip);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    db_delete("compute_resource", cond);
    db_delete("network_resource", cond);
    db_delete("storage_resource", cond);
    db_update("host_device", "poolid=-1", cond);

    return 0;
}

static int host_peer_replace(int opts, argument *args)
{
    char *old_host, *new_host;
    char columns[BUFSIZE];
    char cond[BUFSIZE];
    char id[LC_NAMESIZE];
    char type[LC_NAMESIZE];
    char state[LC_NAMESIZE];
    char poolid[LC_NAMESIZE];
    char sflag[LC_NAMESIZE];
    char srvflag[LC_NAMESIZE];
    int flag;
    char peerip[MAX_HOST_ADDRESS_LEN];
    char *crc = "type,state,ip,user_name,user_passwd,cpu_info,mem_total,"
                "disk_total,poolid,flag,service_flag,dbr_peer_server,lcuuid,domain";
    char *src = "state,ip,attachip,type,capacity,poolid,lcuuid";
    char *nrc = "state,type,ip,poolid,lcuuid";
    lc_mbuf_t msg;
    struct msg_compute_resource_opt *cr;
    struct msg_storage_resource_opt *sr;

    if (!(old_host = get_arg(args, "old-host"))) {
        fprintf(stderr, "Error: No old host specified\n");
        return -1;
    }
    if (!(new_host = get_arg(args, "new-host"))) {
        fprintf(stderr, "Error: No new host specified\n");
        return -1;
    }

    if (strcmp(old_host, new_host) != 0) {
        /* check old */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", old_host);
        memset(poolid, 0, sizeof(poolid));
        db_select(&poolid, LC_NAMESIZE, 1, "compute_resource", "poolid", cond);
        if (!strlen(poolid)) {
            fprintf(stderr, "Error: cannot find %s\n", old_host);
            return -1;
        }

        /* check new */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", new_host);
        memset(poolid, 0, sizeof(poolid));
        db_select(&poolid, LC_NAMESIZE, 1, "host_device", "poolid", cond);
        if (strcmp(poolid, "-1")) {
            fprintf(stderr, "Error: host %s not available\n", new_host);
            return -1;
        }

        /* get params */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", old_host);
        memset(type, 0, sizeof(type));
        db_select(&type, LC_NAMESIZE, 1, "compute_resource", "type", cond);
        memset(poolid, 0, sizeof(poolid));
        db_select(&poolid, LC_NAMESIZE, 1, "compute_resource", "poolid", cond);
        memset(sflag, 0, sizeof(sflag));
        db_select(&sflag, LC_NAMESIZE, 1, "compute_resource", "flag", cond);
        flag = atoi(sflag) & 0x10;
        memset(srvflag, 0, sizeof(srvflag));
        db_select(&srvflag, LC_NAMESIZE, 1, "compute_resource", "service_flag", cond);
        memset(peerip, 0, sizeof(peerip));
        db_select(&peerip, MAX_HOST_ADDRESS_LEN, 1,
                "compute_resource", "dbr_peer_server", cond);

        /* delete old */
        db_delete("storage_resource", cond);
        db_delete("network_resource", cond);
        db_delete("compute_resource", cond);

        /* add new */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", new_host);
        memset(columns, 0, sizeof(columns));
        snprintf(columns, BUFSIZE, "%s,1,ip,user_name,user_passwd,cpu_info,"
                                   "mem_total,disk_total,%s,%d,%s,'%s',lcuuid,domain",
                                   type, poolid, flag, srvflag, peerip);
        db_insert_select("compute_resource", crc, "host_device", columns, cond);
        memset(columns, 0, sizeof(columns));
        snprintf(columns, BUFSIZE, "1,ip,ip,0,disk_total,%s,lcuuid", poolid);
        db_insert_select("storage_resource", src, "host_device", columns, cond);
        memset(columns, 0, sizeof(columns));
        snprintf(columns, BUFSIZE, "1,0,ip,%s,lcuuid", poolid);
        db_insert_select("network_resource", nrc, "host_device", columns, cond);

        /* update state */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", peerip);
        memset(columns, 0, sizeof(columns));
        snprintf(columns, BUFSIZE, "state=1,dbr_peer_server='%s'", new_host);
        db_update("compute_resource", columns, cond);
        db_update("network_resource", "state=1", cond);
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", new_host);
        memset(columns, 0, sizeof(columns));
        snprintf(columns, BUFSIZE, "poolid=%s", poolid);
        db_update("host_device", columns, cond);
        snprintf(cond, BUFSIZE, "ip='%s'", old_host);
        memset(columns, 0, sizeof(columns));
        db_update("host_device", "poolid='-1'", cond);
    } else {
        /* get params */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", old_host);
        memset(peerip, 0, sizeof(peerip));
        db_select(&peerip, MAX_HOST_ADDRESS_LEN, 1,
                "compute_resource", "dbr_peer_server", cond);

        /* update state */
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", peerip);
        db_update("compute_resource", "state=1", cond);
        db_update("network_resource", "state=1", cond);
        db_update("storage_resource", "state=1", cond);
        memset(cond, 0, sizeof(cond));
        snprintf(cond, BUFSIZE, "ip='%s'", new_host);
        db_update("compute_resource", "state=1", cond);
        db_update("network_resource", "state=1", cond);
        db_update("storage_resource", "state=1", cond);
    }

    /* send message */
    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_HOST_JOIN;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_compute_resource_opt);

    cr = (struct msg_compute_resource_opt *)&msg.data;
    memset(cond, 0, sizeof(cond));
    snprintf(cond, BUFSIZE, "ip='%s'", new_host);
    memset(id, 0, sizeof(id));
    db_select(&id, LC_NAMESIZE, 1, "compute_resource", "id", cond);
    cr->compute_id = atoi(id);
    memset(id, 0, sizeof(id));
    db_select(&id, LC_NAMESIZE, 1, "network_resource", "id", cond);
    cr->network_id = atoi(id);

    send_message(&msg);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_SR_JOIN;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_storage_resource_opt);

    sr = (struct msg_storage_resource_opt *)&msg.data;
    memset(id, 0, sizeof(id));
    db_select(&id, LC_NAMESIZE, 1, "storage_resource", "id", cond);
    sr->storage_id = atoi(id);

    while (1) {
        memset(state, 0, sizeof(state));
        db_select(&state, LC_NAMESIZE, 1, "compute_resource", "state", cond);
        if (atoi(state) != 1) {
            break;
        }
        printf("Waiting for compute resource to complete ...\n");
        sleep(10);
    }

    send_message(&msg);

    return 0;
}

/* storage */
static int storage_list(int opts, argument *args)
{
    char *default_params = "l.id,l.name,l.rack_id,l.is_shared,l.disk_total,"
                           "l.disk_used,group_concat(r.host_address) host,"
                           "group_concat(r.active) active";
    char *params = get_arg(args, "params");
    char *host = get_arg(args, "host");
    char cond[BUFSIZE];
    char buffer[BUFSIZE];

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (host) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and r.host_address='%s'", host);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (opts & MNTNCT_OPTION_MINIMAL) {
        db_dump_minimal("storage", "name");
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_sr_cond("l", "r", params, cond);
    }

    return 0;
}

static int storage_activate(int opts, argument *args)
{
    char *id;
    char *host;
    char cond[BUFSIZE];
    char dummy[BUFSIZE];

    if (!(id = get_arg(args, "id"))) {
        fprintf(stderr, "Error: No storage id specified\n");
        return -1;
    }
    if (!(host = get_arg(args, "host"))) {
        fprintf(stderr, "Error: No host specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_address='%s'", host);
    db_update("storage_connection", "active=0", cond);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "storage_id=%s and host_address='%s'", id, host);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "storage_connection", "id", cond);
    if (!dummy[0]) {
        fprintf(stderr, "Error: Failed to activate the nonexistent storage\n");
        return -1;
    }
    db_update("storage_connection", "active=1", cond);

    return 0;
}

/* HA storage */
static int ha_storage_list(int opts, argument *args)
{
    char *default_params = "l.id,l.name,l.rack_id,l.is_shared,l.disk_total,"
                           "l.disk_used,group_concat(r.host_address) host,"
                           "group_concat(r.active) active";
    char *params = get_arg(args, "params");
    char *host = get_arg(args, "host");
    char cond[BUFSIZE];
    char buffer[BUFSIZE];

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (host) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and r.host_address='%s'", host);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (!params) {
        params = default_params;
    }
    db_dump_ha_sr_cond("l", "r", params, cond);

    return 0;
}

static int ha_storage_activate(int opts, argument *args)
{
    char *id;
    char *host;
    char cond[BUFSIZE];
    char dummy[BUFSIZE];

    if (!(id = get_arg(args, "id"))) {
        fprintf(stderr, "Error: No storage id specified\n");
        return -1;
    }
    if (!(host = get_arg(args, "host"))) {
        fprintf(stderr, "Error: No host specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_address='%s'", host);
    db_update("ha_storage_connection", "active=0", cond);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "storage_id=%s and host_address='%s'", id, host);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "ha_storage_connection", "id", cond);
    if (!dummy[0]) {
        fprintf(stderr, "Error: Failed to activate the nonexistent storage\n");
        return -1;
    }
    db_update("ha_storage_connection", "active=1", cond);

    return 0;
}

/* pool */
static int pool_list(int opts, argument *args)
{
    char *default_params = "id,name,type,ctype,stype,lcuuid,domain";
    char *params = get_arg(args, "params");
    char *name = get_arg(args, "name");
    char *domain_name = get_arg(args, "domain");
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (name) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and name='%s'", name);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (domain[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and domain='%s'", domain);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            if (cond[0]) {
                db_dump_cond_minimal("pool", "id", cond);
            } else {
                db_dump_minimal("pool", "id");
            }
        } else {
            if (cond[0]) {
                db_dump_cond_minimal("pool", params, cond);
            } else {
                db_dump_minimal("pool", params);
            }
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("pool", params, cond);
        } else {
            db_dump("pool", params);
        }
        printf("type:\n\t1. VM pool 2. Gateway pool 3. NSP pool\n");
        printf("ctype:\n\t1. Xen pool 2. VMware pool 3. KVM pool\n");
    }

    return 0;
}

static int pool_add(int opts, argument *args)
{
    char *params = "state,type,name,description,ctype,stype,exclusive_ip,"
                   "clusterid,userid,lcuuid,domain";
    char *type = 0;
    int type_int = 0;
    char *name = 0;
    char *description = 0;
    char *ctype = 0;
    int ctype_int = 0;
    char *stype = 0;
    int stype_int = 0;
    int iptype_int = 0;
    char *cluster_name = 0;
    int  user_id = 1;
    char cluster_id[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    char dummy[BUFSIZE];
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char lcuuid[UUID_LENGTH];

    if (!(type = get_arg(args, "type"))) {
        fprintf(stderr, "Error: No pool type specified\n");
        return -1;
    }
    if (strcasecmp(type, "vm") == 0 ||
        strcasecmp(type, "server") == 0 ||
        strcasecmp(type, "1") == 0) {

        type_int = POOL_TYPE_VM;

    } else if (strcasecmp(type, "gateway") == 0 ||
               strcasecmp(type, "gw") == 0 ||
               strcasecmp(type, "2") == 0) {

        type_int = POOL_TYPE_GW;

    } else if (strcasecmp(type, "nspgateway") == 0 ||
               strcasecmp(type, "nsp") == 0 ||
               strcasecmp(type, "3") == 0) {

        type_int = POOL_TYPE_NSP;

    } else if (strcasecmp(type, "tpdserver") == 0 ||
               strcasecmp(type, "tpd") == 0 ||
               strcasecmp(type, "4") == 0) {

        type_int = POOL_TYPE_TPD;

    } else {

        fprintf(stderr, "Error: Unknown pool type %s\n", type);
        return -1;

    }
    if (type_int == POOL_TYPE_VM) {
        if (!(ctype = get_arg(args, "ctype"))) {
            fprintf(stderr, "Error: No pool hypervisor type (ctype) specified\n");
            return -1;
        }
        if (strcasecmp(ctype, "xen") == 0 ||
            strcasecmp(ctype, "xenserver") == 0 ||
            strcasecmp(ctype, "xcp") == 0 ||
            strcasecmp(ctype, "1") == 0) {

            ctype_int = POOL_CTYPE_XEN;

        } else if (strcasecmp(ctype, "vmware") == 0 ||
                   strcasecmp(ctype, "esxi") == 0 ||
                   strcasecmp(ctype, "2") == 0) {

            ctype_int = POOL_CTYPE_VMWARE;

        } else if (strcasecmp(ctype, "kvm") == 0 ||
                   strcasecmp(ctype, "3") == 0) {

            ctype_int = POOL_CTYPE_KVM;

        } else {

            fprintf(stderr, "Error: Unknown hypervisor type %s\n", ctype);
            return -1;

        }

        if (!(stype = get_arg(args, "stype"))) {
            fprintf(stderr, "Error: No pool sharing type (stype) specified\n");
            return -1;
        }
        if (strcasecmp(stype, "local") == 0 ||
            strcasecmp(stype, "1") == 0) {

            stype_int = POOL_STYPE_LOCAL;

        } else if (strcasecmp(stype, "shared") == 0 ||
                   strcasecmp(stype, "2") == 0) {

            stype_int = POOL_STYPE_SHARED;

        } else {

            fprintf(stderr, "Error: Unknown sharing type %s\n", stype);
            return -1;

        }
        iptype_int = POOL_IP_GLOBAL;

    } else {
        if (type_int == POOL_TYPE_NSP || type_int == POOL_TYPE_TPD) {
            ctype_int = POOL_CTYPE_KVM;
        } else {
            ctype_int = POOL_CTYPE_XEN;
        }
        stype_int = POOL_STYPE_LOCAL;

#if 0
        if (!(iptype = get_arg(args, "iptype"))) {
            fprintf(stderr, "Error: No pool ip type (iptype) specified\n");
            return -1;
        }
        if (strcasecmp(iptype, "global") == 0 ||
            strcasecmp(iptype, "0") == 0) {

            iptype_int = POOL_IP_GLOBAL;

        } else if (strcasecmp(iptype, "exclusive") == 0 ||
                   strcasecmp(iptype, "1") == 0) {

            iptype_int = POOL_IP_EXCLUSIVE;

        } else {

            fprintf(stderr, "Error: Unknown ip type %s\n", iptype);
            return -1;
        }
#endif
        iptype_int = POOL_IP_GLOBAL;
    }
    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    } else if (strlen(name) > NAME_LENGTH) {
        fprintf(stderr, "Error: Name label too long\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "pool", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Pool '%s' already exists\n", name);
        return -1;
    }
    if (!(description = get_arg(args, "description"))) {
        description = "";
    }
    if (!(cluster_name = get_arg(args, "cluster_name"))) {
        fprintf(stderr, "Error: No cluster name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", cluster_name);
    db_select(&domain, BUFSIZE, 1, "cluster", "domain", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Cluster with name '%s' not found\n", cluster_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", cluster_name);
    db_select(&cluster_id, BUFSIZE, 1, "cluster", "id", cond);
    if (!cluster_id[0]) {
        fprintf(stderr, "Error: Cluster with name '%s' not found\n", cluster_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%d", user_id);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "user", "id", cond);
    if (!dummy[0]) {
        fprintf(stderr, "Error: User with id '%d' not found\n", user_id);
        return -1;
    }
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "%d,%d,'%s','%s',%d,%d,%d,%s,%d,'%s','%s'",
             4, type_int, name, description, ctype_int, stype_int, iptype_int,
             cluster_id, user_id, lcuuid, domain);
    db_insert("pool", params, values);

#if 0
    if ((type_int == POOL_TYPE_GW || type_int == POOL_TYPE_NSP)
            && iptype_int == POOL_IP_EXCLUSIVE) {
        printf("Please add ip resource for this gateway pool\n");
    }
#endif

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "pool", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int pool_remove(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    if (!id && !name) {
        fprintf(stderr, "Error: No id or name label specified\n");
        return -1;
    }
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "pool", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: pool '%s' not exist\n", name);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: pool id and name mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "pool", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: pool with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "poolid=%s", id);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "host_device", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: pool with id '%s' not empty\n", id);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("pool", cond);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "poolid=%s", id);
    db_delete("ip_resource", cond);
    return 0;
}

static int pool_add_product_spec(int opts, argument *args)
{
    char *params = "pool_lcuuid, product_spec_lcuuid";
    char *pool_name = 0;
    char *product_spec = 0;
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char pool_lcuuid[BUFSIZE];
    char product_spec_lcuuid[BUFSIZE];
    char pool_domain[BUFSIZE];
    char product_spec_domain[BUFSIZE];

    if (!(pool_name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No pool name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_lcuuid, 0, sizeof pool_lcuuid);
    db_select(&pool_lcuuid, BUFSIZE, 1, "pool", "lcuuid", cond);
    if (!pool_lcuuid[0]) {
        fprintf(stderr, "Error: pool '%s' not exist\n", pool_name);
        return -1;
    }
    memset(pool_domain, 0, sizeof pool_lcuuid);
    db_select(&pool_domain, BUFSIZE, 1, "pool", "domain", cond);

    if (!(product_spec = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s' and domain='%s'", product_spec, pool_domain);
    memset(product_spec_lcuuid, 0, sizeof product_spec_lcuuid);
    db_select(&product_spec_lcuuid, BUFSIZE, 1,
              "product_specification", "lcuuid", cond);
    if (!product_spec_lcuuid[0]) {
        fprintf(stderr, "Error: product_specification '%s' not exist\n",
                product_spec);
        return -1;
    }

    memset(product_spec_domain, 0, sizeof product_spec_domain);
    db_select(&product_spec_domain, BUFSIZE, 1, "product_specification", "domain", cond);
    if (strcmp(pool_domain, product_spec_domain)) {
        fprintf(stderr, "Error: pool '%s' and product_specification '%s' not in same domain\n",
                pool_name, product_spec);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "pool_lcuuid='%s' and product_spec_lcuuid='%s'",
             pool_lcuuid, product_spec_lcuuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "pool_product_specification", "id", cond);
    if (id_s[0]) {
        fprintf(stderr, "Error: product_specification '%s' already in pool '%s'\n",
                product_spec, pool_name);
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s'", pool_lcuuid, product_spec_lcuuid);
    db_insert("pool_product_specification", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "pool_lcuuid='%s' and product_spec_lcuuid='%s'",
             pool_lcuuid, product_spec_lcuuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "pool_product_specification", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int pool_remove_product_spec(int opts, argument *args)
{
    char *pool_name = 0;
    char *product_spec = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char pool_lcuuid[BUFSIZE];
    char product_spec_lcuuid[BUFSIZE];

    if (!(pool_name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No pool name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_lcuuid, 0, sizeof pool_lcuuid);
    db_select(&pool_lcuuid, BUFSIZE, 1, "pool", "lcuuid", cond);
    if (!pool_lcuuid[0]) {
        fprintf(stderr, "Error: pool '%s' not exist\n", pool_name);
        return -1;
    }

    if (!(product_spec = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s'", product_spec);
    memset(product_spec_lcuuid, 0, sizeof product_spec_lcuuid);
    db_select(&product_spec_lcuuid, BUFSIZE, 1,
              "product_specification", "lcuuid", cond);
    if (!product_spec_lcuuid[0]) {
        fprintf(stderr, "Error: product_specification '%s' not exist\n",
                product_spec);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "pool_lcuuid='%s' and product_spec_lcuuid='%s'",
            pool_lcuuid, product_spec_lcuuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "pool_product_specification", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: pool '%s' don't have product_specification '%s'\n",
                pool_name, product_spec);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "pool_lcuuid='%s' and product_spec_lcuuid='%s'",
             pool_lcuuid, product_spec_lcuuid);
    db_delete("pool_product_specification", cond);

    return 0;
}

static int pool_list_product_spec(int opts, argument *args)
{
    char *default_params = "l.id,l.plan_name,l.charge_mode,l.price,l.lcuuid";
    char *pool_name = 0;
    char pool_lcuuid[BUFSIZE];
    char cond[BUFSIZE];

    if (!(pool_name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No pool name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", pool_name);
    memset(pool_lcuuid, 0, sizeof pool_lcuuid);
    db_select(&pool_lcuuid, BUFSIZE, 1, "pool", "lcuuid", cond);
    if (!pool_lcuuid[0]) {
        fprintf(stderr, "Error: pool '%s' not exist\n", pool_name);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "r.pool_lcuuid='%s'", pool_lcuuid);
    db_dump_pool_ps_cond("l", "r", default_params, cond);

    return 0;
}

static int pack_hwdev_intf_curl_post(char *buf, int buf_len,
                             int if_index,
                             char *mac,
                             int speed,
                             char *network_device_lcuuid,
                             int network_device_port
                             )
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_IF_INDEX, root);
    tmp->int_value = if_index;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_IF_MAC, root);
    tmp->text_value = mac;
    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_IF_SPEED, root);
    tmp->int_value = speed;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_NETWORK_DEVICE_LCUUID, root);
    tmp->text_value = network_device_lcuuid;
    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_NETWORK_DEVICE_PORT, root);
    tmp->int_value = network_device_port;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}


static int pack_hwdev_intf_curl_patch_service(char *buf, int buf_len,
                             int  if_state,
                             char *if_type,
                             uint32_t if_subtype
                             )
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    nx_json *subtype = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_IF_STATE, root);
    tmp->int_value = if_state;
    if (MNTNCT_HWDEV_INTF_STATE_ATTACHED == if_state) {
        tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_IF_TYPE, root);
        tmp->text_value = if_type;
        subtype = create_json(NX_JSON_OBJECT, MNTNCT_API_HWDEV_IF_SUBTYPE, root);
        tmp = create_json(NX_JSON_BOOL, "IS_L2_INTERFACE", subtype);
        tmp->int_value = if_subtype;
    }

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_hwdev_curl_post(char *buf, int buf_len,
                             char *name,
                             int userid,
                             int order_id,
                             int pool_id,
                             char *rack_lcuuid,
                             char *domain,
                             char *role,
                             char *product_s_lcuuid,
                             char *brand,
                             char *mgmt_ip,
                             char *user_name,
                             char *user_passwd
                             )
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_NAME, root);
    tmp->text_value = name;
    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_USERID, root);
    tmp->int_value = userid;
    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_ORDER_ID, root);
    tmp->int_value = order_id;
    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_HWDEV_POOL_ID, root);
    tmp->int_value = pool_id;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_RACK_LCUUID, root);
    tmp->text_value = rack_lcuuid;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN, root);
    tmp->text_value = domain;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_ROLE, root);
    tmp->text_value = role;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_PRODUCT_SPECIFICATION, root);
    tmp->text_value = product_s_lcuuid;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_BRAND, root);
    tmp->text_value = brand;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_MGMT_IP, root);
    tmp->text_value = mgmt_ip;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_USER_NAME, root);
    tmp->text_value = user_name;
    tmp = create_json(NX_JSON_STRING, MNTNCT_API_HWDEV_USER_PASSWD, root);
    tmp->text_value = user_passwd;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int pack_ip_curl_post(char *buf, int buf_len,
                             char *ip,
                             int netmask,
                             char *gateway,
                             char *pool_name,
                             char *domain,
                             char *product_spec_lcuuid,
                             int isp,
                             int vlantag)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_IP_IP, root);
    tmp->text_value = ip;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_IP_NETMASK, root);
    tmp->int_value = netmask;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_IP_GATEWAY, root);
    tmp->text_value = gateway;

    if (NULL != pool_name) {
        tmp = create_json(NX_JSON_STRING, MNTNCT_API_IP_POOL_NAME, root);
        tmp->text_value = pool_name;
    }

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN, root);
    tmp->text_value = domain;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_IP_PRODUCT_SPEC_LCUUID, root);
    tmp->text_value = product_spec_lcuuid;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_IP_ISP, root);
    tmp->int_value = isp;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_IP_VLANTAG, root);
    tmp->int_value = vlantag;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

/* ip */
static int ip_list(int opts, argument *args)
{
    char *default_params = "ip.id,ip.ip,ip.netmask,ip.userid,ip.vifid,vlantag,gateway,poolid,ip.isp,name AS isp_name";
    char *params = get_arg(args, "params");
    char *isp = get_arg(args, "ISP");
    char *pool_name = get_arg(args, "pool_name");
    char *vlantag = get_arg(args, "vlantag");
    char *domain_name = get_arg(args, "domain");
    char poolid[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    int isp_i;

    if (pool_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", pool_name);
        db_select(&poolid, BUFSIZE, 1, "pool", "id", cond);
        if (!poolid[0]) {
            fprintf(stderr, "Error: Pool with name '%s' not found\n", pool_name);
            return -1;
        }
    }
    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not found\n", domain_name);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (isp) {
        isp_i = atoi(isp);
        if (isp_i < 1 || isp_i > 8) {
            fprintf(stderr, "Error: ISP must in [1,8]\n");
            return -1;
        }
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and isp=%d", isp_i);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (poolid[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and poolid=%s", poolid);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (vlantag) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and vlantag=%s", vlantag);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (domain[0]) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and domain='%s'", domain);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond_minimal("ip_resource", "id", cond);
        } else {
            db_dump_cond_minimal("ip_resource", params, cond);
        }
    } else {
        if (params) {
            db_dump_cond("ip_resource", params, cond);
        } else {
            db_dump_ip_cond("ip", "isp", default_params,
                            "isp.userid=0 and isp.isp!=0");
        }
    }

    return 0;
}

static int ip_add(int opts, argument *args)
{
    char *ip = 0;
    char *netmask = 0;
    char *gateway = 0;
    char *pool_name = 0;
    char *isp = 0;
    int isp_i;
    char *vlan_tag = 0;
    char *product_spec = 0;
    char *domain_name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    int masklen;
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    char product_spec_lcuuid[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    int err;

    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No ip specified\n");
        return -1;
    }
    if (!(netmask = get_arg(args, "netmask"))) {
        fprintf(stderr, "Error: No netmask specified\n");
        return -1;
    } else {
        if (strlen(netmask) <= 2) {
            masklen = atoi(netmask);
        } else if (check_netmask(netmask, &masklen)) {
            fprintf(stderr, "Error: Netmask '%s' invalid\n", netmask);
            return -1;
        }
        if (masklen < 0 || masklen > 32) {
            fprintf(stderr, "Error: Netmask '%s' invalid\n", netmask);
            return -1;
        }
    }
    if (!(gateway = get_arg(args, "gateway"))) {
        fprintf(stderr, "Error: No gateway ip specified\n");
        return -1;
    }
    pool_name = get_arg(args, "pool_name");

    if (!(isp = get_arg(args, "ISP"))) {
        fprintf(stderr, "Error: No ISP specified\n");
        return -1;
    }
    isp_i = atoi(isp);
    if (isp_i < 1 || isp_i > 8) {
        fprintf(stderr, "Error: ISP must in [1,8]\n");
        return -1;
    }
    if (!(vlan_tag = get_arg(args, "vlantag"))) {
        vlan_tag = "0";
    }

    if (!(product_spec = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s'", product_spec);
    memset(product_spec_lcuuid, 0, sizeof product_spec_lcuuid);
    db_select(&product_spec_lcuuid, BUFSIZE, 1,
              "product_specification", "lcuuid", cond);
    if (!product_spec_lcuuid[0]) {
        fprintf(stderr, "Error: product_specification '%s' not exist\n",
                product_spec);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_IP_POST);
    strcat(url, url2);
    err = pack_ip_curl_post(post, MNTNCT_CMD_LEN, ip,
            masklen, gateway, pool_name, domain, product_spec_lcuuid, isp_i, atoi(vlan_tag));
    if (err) {
        fprintf(stderr, "Error: Pack ip post curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_POST, post, NULL);
    if (err) {
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ip_resource", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int ip_remove(int opts, argument *args)
{
    char *id = 0;
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char vif_id[BUFSIZE];
    char user_id[BUFSIZE];

    id = get_arg(args, "id");
    ip = get_arg(args, "ip");
    if (!id && !ip) {
        fprintf(stderr, "Error: No id or ipspecified\n");
        return -1;
    }
    if (ip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "ip_resource", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: ip '%s' not exist\n", ip);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: id and ip mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "ip_resource", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: ip with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    memset(vif_id, 0, sizeof vif_id);
    db_select(&vif_id, BUFSIZE, 1, "ip_resource", "vifid", cond);
    if (atoi(vif_id) != 0) {
        fprintf(stderr, "Error: ip with id '%s' in use\n", id);
        return -1;
    }
    memset(user_id, 0, sizeof user_id);
    db_select(&user_id, BUFSIZE, 1, "ip_resource", "userid", cond);
    if (atoi(user_id) != 0) {
        fprintf(stderr, "Error: ip with id '%s' assigned to the user '%s'\n", id, user_id);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("ip_resource", cond);

    return 0;
}

static int pack_isp_curl_put(char *buf,
                             int buf_len,
                             int isp,
                             char *name,
                             int bandwidth,
                             char *domain)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    if (name) {
        tmp = create_json(NX_JSON_STRING, MNTNCT_API_ISP_NAME, root);
        tmp->text_value = name;
    }

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_ISP_BANDWIDTH, root);
    tmp->int_value = bandwidth;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_ISP_DOMAIN, root);
    tmp->text_value = domain;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_ISP_ISP, root);
    tmp->int_value = isp;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int isp_config(int opts, argument *args)
{
    const char *isp = 0;
    const char *bandwidth = 0;
    char *name = 0;
    char *domain_name = 0;
    int isp_i, bandwidth_i;
    char cond[BUFSIZE] = {0};
    char domain[UUID_LENGTH] = {0};
    char uuid[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char put[MNTNCT_CMD_LEN] = {0};
    int err;

    if (!(isp = get_arg(args, "ISP"))) {
        fprintf(stderr, "Error: No ISP specified\n");
        return -1;
    }
    isp_i = atoi(isp);
    if (isp_i < 1 || isp_i > 8) {
        fprintf(stderr, "Error: ISP must in [1,8]\n");
        return -1;
    }

    bandwidth = get_arg(args, "bandwidth");
    if (!bandwidth) {
        bandwidth = "100";  /* 默认100Mbps带宽 */
    }
    bandwidth_i = atoi(bandwidth);
    if (bandwidth_i <= 0) {
        fprintf(stderr, "Error: bandwidth must be positive integer\n");
        return -1;
    }

    name = get_arg(args, "name");
    if (!name && has_arg(args, "name")) {
        name = "";
    }

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(cond, BUFSIZE, "isp=%s and domain='%s'", isp, domain);
    db_select(&uuid, UUID_LENGTH, 1, "vl2", "lcuuid", cond);
    if (!uuid[0]) {
        snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_ISP_PUT_C,
                MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    } else {
        snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_ISP_PUT,
                MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION,
                uuid);
    }

    err = pack_isp_curl_put(
            put, MNTNCT_CMD_LEN, isp_i, name, bandwidth_i, domain);
    if (err) {
        fprintf(stderr, "Error: Pack isp put curl failed, put=[%s]\n", put);
        return -1;
    }

    err = call_curl_api(url, API_METHOD_PUT, put, NULL);
    if (err) {
        fprintf(stderr, "Error: request talker failed, put=[%s]\n", put);
        return -1;
    }

    snprintf(cond, BUFSIZE, "userid=0 AND isp!=0");
    db_dump_cond("vl2", "isp,name,state,bandwidth >> 20 as 'bandwidth(Mbps)'", cond);
    printf("state:\n\t0. Disabled 2. Enable\n");

    return 0;
}

static int isp_list(int opts, argument *args)
{
    char *default_params = "isp,name,bandwidth >> 20 as 'bandwidth(Mbps)',state,domain";
    char *params = get_arg(args, "params");

    if (!params) {
        params = default_params;
    }
    db_dump_cond("vl2", params, "isp!=0 AND userid=0");
    printf("state:\n\t0. Disabled 2. Enable\n");

    return 0;
}

/* vgateway */
static int vgateway_list(int opts, argument *args)
{
    char *default_params = "id,name,lcuuid,state,gw_launch_server,role,specification";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        db_dump_minimal("vnet", "label");
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("vnet", params);
    }
    fprintf(stderr, "state:\n\t1. Creating 3. Error 7. Running 11. Migrating\n");
    fprintf(stderr, "role:\n\t7. VGateway 11. Valve\n");

    return 0;
}

/* vm */
static int vm_list(int opts, argument *args)
{
    char *default_params = "id,name,label,state,os,vcpu_num,mem_size,"
                           "vdi_user_size,launch_server,lcuuid";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        db_dump_minimal("vm", "label");
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("vm", params);
    }

    return 0;
}

static int vm_start(int opts, argument *args)
{
    char cond[BUFSIZE];
    char vm[LC_NAMESIZE];
    char *name_label;
    lc_mbuf_t msg;
    struct msg_vm_opt *data;

    if (!(name_label = get_arg(args, "name-label"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    }

    printf("starting vm '%s'\n", name_label);
    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "label='%s'", name_label);
    db_select(&vm, LC_NAMESIZE, 1, "vm", "id", cond);
    if (strlen(vm) == 0) {
        fprintf(stderr, "Error: vm '%s' not found\n", name_label);
        return -1;
    }
    snprintf(cond, BUFSIZE, "id='%s'", vm);
    db_update("vm", "state=3", cond);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_VM_START;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_vm_opt);

    data = (struct msg_vm_opt *)&msg.data;
    data->vm_id = atoi(vm);

    send_message(&msg);

    return 0;
}

static int vm_stop(int opts, argument *args)
{
    char cond[BUFSIZE];
    char vm[LC_NAMESIZE];
    char *name_label;
    lc_mbuf_t msg;
    struct msg_vm_opt *data;

    if (!(name_label = get_arg(args, "name-label"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    }

    printf("stopping vm '%s'\n", name_label);
    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "label='%s'", name_label);
    db_select(&vm, LC_NAMESIZE, 1, "vm", "id", cond);
    if (strlen(vm) == 0) {
        fprintf(stderr, "Error: vm '%s' not found\n", name_label);
        return -1;
    }
    snprintf(cond, BUFSIZE, "id='%s'", vm);
    db_update("vm", "state=8", cond);

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_VM_STOP;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_vm_opt);

    data = (struct msg_vm_opt *)&msg.data;
    data->vm_id = atoi(vm);

    send_message(&msg);

    return 0;
}

static int check_migrate_hosts(char *local_host, char *remote_host, int device_type)
{
    int i, err = 0;
    int brids[LC_BR_ID_NUM] = {0};

    char cond[BUFSIZE] = {0};
    char local_result[MNTNCT_CMD_RESULT_LEN] = {0};
    char remote_result[MNTNCT_CMD_RESULT_LEN] = {0};

    char local_user[LC_NAMESIZE] = {0};
    char local_password[LC_NAMESIZE] = {0};
    char remote_user[LC_NAMESIZE] = {0};
    char remote_password[LC_NAMESIZE] = {0};

    snprintf(cond, BUFSIZE, "ip='%s'", local_host);
    db_select(&local_user, LC_NAMESIZE, 1, "host_device", "user_name", cond);
    db_select(&local_password, LC_NAMESIZE, 1, "host_device", "user_passwd", cond);
    snprintf(cond, BUFSIZE, "ip='%s'", remote_host);
    db_select(&remote_user, LC_NAMESIZE, 1, "host_device", "user_name", cond);
    db_select(&remote_password, LC_NAMESIZE, 1, "host_device", "user_passwd", cond);

    if (!local_user[0] || !local_password[0] || !remote_user[0] || !remote_password[0]) {
        fprintf(stderr, "load local/remote host failed\n");
        return -1;
    }

    /* check network config */

    brids[0] = LC_DATA_BR_ID;
    if (device_type == LC_VM_TYPE_GW) {
        brids[1] = LC_ULNK_BR_ID;
        brids[2] = LC_CTRL_BR_ID;
    }
    for (i = 0; i < LC_BR_ID_NUM && brids[i]; ++i) {
        snprintf(cond, BUFSIZE, OVS_GET_BR_NAME,
                local_password, local_user, local_host, brids[i]);
        err = call_system_output(cond, local_result, MNTNCT_CMD_RESULT_LEN);

        snprintf(cond, BUFSIZE, OVS_GET_BR_NAME,
                remote_password, remote_user, remote_host, brids[i]);
        err = call_system_output(cond, remote_result, MNTNCT_CMD_RESULT_LEN);

        chomp(local_result);
        chomp(remote_result);

        if (local_result[0] == 0 || strcmp(local_result, remote_result) != 0) {
            fprintf(stderr, "Bridge-%d inconsistent, local-%s [%s], remote-%s [%s].\n",
                    brids[i], local_host, local_result, remote_host, remote_result);
            return -1;
        } else {
            fprintf(stderr, "Bridge-%d check passed %s-%s [%s].\n",
                    brids[i], local_host, remote_host, local_result);
        }
    }

    /* check CPU, ... */
    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "ip='%s'", local_host);
    memset(local_result, 0, sizeof(local_result));
    db_select(&local_result, LC_NAMESIZE, 1, "host_device", "cpu_info", cond);
    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "ip='%s'", remote_host);
    memset(remote_result, 0, sizeof(remote_result));
    db_select(&remote_result, LC_NAMESIZE, 1, "host_device", "cpu_info", cond);
    if (strcmp(local_result, remote_result)) {
        fprintf(stderr, "CPU mismatch on local and remote host.\n");
        return -1;
    }

    return 0;
}

static int vm_migrate(int opts, argument *args)
{
    char cond[BUFSIZE];
    char values[BIG_BUFSIZE];
    char vm[LC_NAMESIZE];
    char host[LC_NAMESIZE];
    char local_host[LC_NAMESIZE];
    char host_htype[LC_NAMESIZE];
    char username[LC_NAMESIZE];
    char password[LC_NAMESIZE];
    char power_state[LC_NAMESIZE];
    char vm_state[LC_NAMESIZE];
    char exec_result[MNTNCT_CMD_RESULT_LEN] = {0};
    char *name_label, *remote_host, *remote_sr;
    int  host_htype_int = 0;
    int  err = 0;
    lc_mbuf_t msg;
    struct msg_vm_migrate_opt *data;

    if (!(name_label = get_arg(args, "name-label"))) {
        fprintf(stderr, "Error: No name label specified\n");
        return -1;
    } else {
        memset(cond, 0, BUFSIZE);
        snprintf(cond, BUFSIZE, "label='%s'", name_label);
        memset(vm, 0, sizeof(vm));
        db_select(&vm, LC_NAMESIZE, 1, "vm", "id", cond);
        if (strlen(vm) == 0) {
            fprintf(stderr, "Error: vm '%s' not found\n", name_label);
            return -1;
        }
    }

    if (!(remote_host = get_arg(args, "remote-host"))) {
        fprintf(stderr, "Error: No remote host specified\n");
        return -1;
    } else {
        memset(cond, 0, BUFSIZE);
        snprintf(cond, BUFSIZE, "ip='%s'", remote_host);
        memset(host, 0, sizeof(host));
        db_select(&host, LC_NAMESIZE, 1, "host_device", "id", cond);
        if (strlen(host) == 0) {
            fprintf(stderr, "Error: host '%s' not found\n", remote_host);
            return -1;
        }
    }

    if (!(remote_sr = get_arg(args, "remote-sr"))) {
        fprintf(stderr, "Error: No remote sr specified\n");
        return -1;
    } else {
        memset(cond, 0, BUFSIZE);
        snprintf(cond, BUFSIZE, "ip='%s'", remote_host);
        memset(host_htype, 0, sizeof(host_htype));
        db_select(&host_htype, LC_NAMESIZE, 1, "host_device", "htype", cond);
        if (strlen(host_htype) == 0) {
            fprintf(stderr, "Error: unknown host '%s' htype\n", remote_host);
            return -1;
        }
        host_htype_int = atoi(host_htype);
    }

    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "label='%s'", name_label);
    memset(vm_state, 0, sizeof(vm_state));
    db_select(&vm_state, LC_NAMESIZE, 1, "vm", "state", cond);
    if (strlen(vm_state) == 0 || LC_VM_STATE_RUNNING != atoi(vm_state)) {
        fprintf(stderr, "Error: vm '%s' state is not running in DB\n",
                name_label);
        return -1;
    }

    printf("migrating vm '%s' to '%s': '%s'\n",
           name_label, remote_host, remote_sr);

    memset(cond, 0, BUFSIZE);
    snprintf(cond, BUFSIZE, "label='%s'", name_label);
    memset(local_host, 0, sizeof(local_host));
    db_select(&local_host, LC_NAMESIZE, 1, "vm", "launch_server", cond);

    snprintf(cond, BUFSIZE, "ip='%s'", local_host);
    memset(username, 0, sizeof(username));
    db_select(&username, LC_NAMESIZE, 1, "host_device", "user_name", cond);
    memset(password, 0, sizeof(password));
    db_select(&password, LC_NAMESIZE, 1, "host_device", "user_passwd", cond);

    if (host_htype_int == HOST_HTYPE_KVM) {
        memset(cond, 0, BUFSIZE);
        snprintf(cond, BUFSIZE, KVM_VM_MIGRATE_STR,
                 password, username, local_host, name_label, remote_host);
        err = call_system_output(cond, exec_result, MNTNCT_CMD_RESULT_LEN);
        if (err) {
            memset(cond, 0, BUFSIZE);
            snprintf(cond, BUFSIZE, "label='%s'", name_label);
            memset(values, 0, sizeof values);
            snprintf(values, BUFSIZE, "state=%d", LC_VM_STATE_ERROR);
            db_update("vm", values, cond);
            fprintf(stderr, "Error: vm '%s' migrate failed\n",
                    name_label);
            return -1;
        }
    } else {

        if (check_migrate_hosts(local_host, remote_host, LC_VM_TYPE_VM)) {
            return -1;
        }
        memset(power_state, 0, sizeof(power_state));
        get_param(power_state, LC_NAMESIZE, XEN_VM_STATE_STR,
                local_host, username, password, name_label);

        if (strcmp(power_state, "running") != 0) {
            fprintf(stderr, "Error: vm '%s' state is not running\n",
                    name_label);
            return -1;
        }
    }

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_VM_MIGRATE;
    msg.hdr.magic  = lc_msg_get_path(msg.hdr.type, MSG_MG_START);
    msg.hdr.length = sizeof(struct msg_vm_migrate_opt);

    data = (struct msg_vm_migrate_opt *)&msg.data;
    data->vm_id = atoi(vm);
    strncpy(data->des_server, remote_host, MAX_HOST_ADDRESS_LEN - 1);
    strncpy(data->des_sr_name, remote_sr, LC_NAMESIZE - 1);

    send_message(&msg);

    return 0;
}

static int vm_import(int opts, argument *args)
{
    char *vm_params = "state,name,label,os,flag,poolid,launch_server,"
                      "vdi_sys_uuid,vdi_sys_sr_uuid,vdi_sys_sr_name,vdi_sys_size,"
                      "vdi_user_uuid,vdi_user_sr_uuid,vdi_user_sr_name,vdi_user_size,"
                      "mem_size,vcpu_num,tport,lport,uuid,domain,userid,lcuuid,"
                      "product_specification_lcuuid";
    char *fdb_vm_params = "state,name,os,flag,launch_server,mem_size,vcpu_num,"
                          "sys_disk_size,user_disk_size,userid,lcuuid,domain,"
                          "product_specification_lcuuid, create_time";
    char *vif_params = "ifindex,state,mac,devicetype,deviceid,domain";
    char *launch_server = 0;
    char *label = 0;
    char *uuid = 0;
    char *os = 0;
    char *domain_name = 0;
    char *product_specification = 0;
    char *user_email = 0;
    char values[BIG_BUFSIZE];
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char vif_id_s[BUFSIZE];
    char vm_uuid[UUID_LENGTH];
    char lcuuid[UUID_LENGTH];
    char pool_id[BUFSIZE];
    char domain[BUFSIZE];
    char ps_lcuuid[UUID_LENGTH];
    char user_id[BUFSIZE];
    char ctrl_vif_subnetid[BUFSIZE];
    char ctrl_vif_bandw[BUFSIZE];
    char ctrl_vif_ip[MAX_HOST_ADDRESS_LEN];
    char vdi_sys_sr_name[BUFSIZE];
    char create_time[MNTNCT_CMD_RESULT_LEN];
    char username[LC_NAMESIZE];
    char password[LC_NAMESIZE];
    uint32_t state = 0;
    uint32_t tport = 0;
    uint32_t ctrl_vif_netmask = 0;
    int i, err;
    char url[MNTNCT_CMD_LEN] = {0};
    char call_result[CURL_BUFFER_SIZE] = {0};
    const nx_json *json = 0;
    const nx_json *json_data = 0;
    agexec_msg_t *p_req = NULL;
    agexec_msg_t *p_res = NULL;
    agexec_vm_t *p_req_vm = NULL;
    agexec_vm_t *p_res_vm = NULL;

    /* generate lcuuid */
    memset(lcuuid, 0, sizeof lcuuid);
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID (check talker)\n");
        return -1;
    }

    /* launch_server & poolid */
    if (!(launch_server = get_arg(args, "launch-server"))) {
        fprintf(stderr, "Error: No launch server specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", launch_server);
    memset(pool_id, 0, sizeof pool_id);
    db_select(&pool_id, BUFSIZE, 1, "compute_resource", "poolid", cond);
    if (!pool_id[0]) {
        fprintf(stderr, "Error: Server '%s' not exist\n", launch_server);
        return -1;
    }

    /* uuid */
    if (!(uuid = get_arg(args, "uuid"))) {

        if ((label = get_arg(args, "label"))) {

            snprintf(cond, BUFSIZE, "ip='%s'", launch_server);
            memset(username, 0, sizeof(username));
            db_select(&username, LC_NAMESIZE, 1, "host_device", "user_name", cond);
            memset(password, 0, sizeof(password));
            db_select(&password, LC_NAMESIZE, 1, "host_device", "user_passwd", cond);

            memset(vm_uuid, 0, sizeof(vm_uuid));
            if (get_param(vm_uuid, LC_NAMESIZE, XEN_VM_UUID_STR,
                          launch_server, username, password, label)) {
                fprintf(stderr, "Error: Get vm %s uuid failed\n", label);
                return -1;
            }
            if (!vm_uuid[0]) {
                fprintf(stderr, "Error: VM %s not exist\n", label);
                return -1;
            }
            uuid = vm_uuid;
        } else {
            fprintf(stderr, "Error: No uuid specified\n");
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "uuid='%s'", uuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "vm", "id", cond);
    if (id_s[0]) {
        fprintf(stderr, "Error: VM '%s' already exist\n", uuid);
        return -1;
    }

    /* os */
    if (!(os = get_arg(args, "os"))) {
        fprintf(stderr, "Error: No os specified\n");
        return -1;
    }

    /* domain */
    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    /* product_specification_lcuuid */
    if (!(product_specification = get_arg(args, "product-specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s' and domain='%s'",
             product_specification, domain);
    memset(ps_lcuuid, 0, sizeof ps_lcuuid);
    db_select(&ps_lcuuid, UUID_LENGTH, 1, "product_specification", "lcuuid", cond);
    if (!ps_lcuuid[0]) {
        fprintf(stderr, "Error: Product_specification '%s' not exist\n",
                product_specification);
        return -1;
    }

    /* userid */
    if (!(user_email = get_arg(args, "user-email"))) {
        fprintf(stderr, "Error: No user_email specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "email='%s'", user_email);
    memset(user_id, 0, sizeof user_id);
    db_select(&user_id, BUFSIZE, 1, "user", "id", cond);
    if (!user_id[0]) {
        fprintf(stderr, "Error: User with email '%s' not exist\n", user_email);
        return -1;
    }

    /* tport */
    memset(url, 0, MNTNCT_CMD_LEN);
    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_VM_VNC_TPORT_POST,
             MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    memset(call_result, 0, CURL_BUFFER_SIZE);
    err = call_curl_api(url, API_METHOD_POST, "", &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    json = nx_json_parse(call_result, 0);
    if (json) {
        json_data = nx_json_get(nx_json_get(json, "DATA"), "TPORT");
        if (json_data && json_data->type == NX_JSON_INTEGER) {
            tport = json_data->int_value;
        } else {
            fprintf(stderr, "Error: Invalid alloc tport response\n%s\n",
                    call_result);
            return -1;
        }
    } else {
        fprintf(stderr, "Error: Invalid alloc tport response\n%s\n", call_result);
        return -1;
    }

    /* ctrl_ip & netmask */
    memset(url, 0, MNTNCT_CMD_LEN);
    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_VM_CTRL_IP_POST,
             MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION, domain);
    memset(call_result, 0, CURL_BUFFER_SIZE);
    err = call_curl_api(url, API_METHOD_POST, "", &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    json = nx_json_parse(call_result, 0);
    if (json) {
        json_data = nx_json_get(nx_json_get(json, "DATA"), "IP");
        if (json_data && json_data->type == NX_JSON_STRING) {
            memcpy(ctrl_vif_ip, json_data->text_value, MAX_HOST_ADDRESS_LEN);
        } else {
            fprintf(stderr, "Error: Invalid alloc ctrl_ip response\n%s\n",
                    call_result);
            return -1;
        }
        json_data = nx_json_get(nx_json_get(json, "DATA"), "NETMASK");
        if (json_data && json_data->type == NX_JSON_INTEGER) {
            ctrl_vif_netmask = json_data->int_value;
        } else {
            fprintf(stderr, "Error: Invalid alloc ctrl_vif_netmask response\n%s\n",
                    call_result);
            return -1;
        }
    } else {
        fprintf(stderr, "Error: Invalid alloc ctrl_ip response\n%s\n", call_result);
        return -1;
    }

    /* ctrl_vif_subnetid */
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "net_type=%d", LC_VL2_NET_TYPE_CTRL);
    memset(ctrl_vif_subnetid, 0, sizeof ctrl_vif_subnetid);
    db_select(&ctrl_vif_subnetid, BUFSIZE, 1, "vl2", "id", cond);
    if (!ctrl_vif_subnetid[0]) {
        fprintf(stderr, "Error: Ctrl plane subnet not exist\n");
        return -1;
    }

    /* ctrl_vif_bandw */
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "param_name='ctrl_plane_bandwidth'and domain='%s'",
             domain);
    memset(ctrl_vif_bandw, 0, sizeof ctrl_vif_bandw);
    db_select(&ctrl_vif_bandw, BUFSIZE, 1, "domain_configuration", "value", cond);
    if (!ctrl_vif_bandw[0]) {
        fprintf(stderr, "Error: Ctrl plane bandwidth not exist\n");
        return -1;
    }

    /* create_time */
    memset(create_time, 0, sizeof create_time);
    err = call_system_output("date +'%F %T'", create_time, MNTNCT_CMD_RESULT_LEN);
    chomp(create_time);
    if (err || !create_time[0]) {
        fprintf(stderr, "Error: get create_time failed\n");
    }

    /* import msg to agexec */
    p_req = calloc(1, sizeof(agexec_msg_t) + sizeof(agexec_vm_t));
    if (!p_req) {
        fprintf(stderr, "Error: malloc() failed\n");
        return -1;
    }

    p_req_vm = (agexec_vm_t *) p_req->data;
    strncpy(p_req_vm->vm_uuid, uuid, AGEXEC_LEN_UUID);
    for (i = 0; i < LC_VM_IFACE_CTRL; ++i) {
        p_req_vm->vifs[i].network_type = LC_DATA_BR_ID;
        p_req_vm->vifs[i].in_use = VIF_IN_USE_Y;
    }
    p_req_vm->vifs[LC_VM_IFACE_CTRL].network_type = LC_CTRL_BR_ID;
    p_req_vm->vifs[LC_VM_IFACE_CTRL].in_use = VIF_IN_USE_Y;

    p_req->hdr.object_type = AGEXEC_OBJ_VM;
    p_req->hdr.length = sizeof(agexec_vm_t);
    p_req->hdr.action_type = AGEXEC_ACTION_GET;

    if (lc_agexec_exec(p_req, &p_res, launch_server)) {
        fprintf(stderr, "Error: lc_agexec_exec() failed\n");
        free(p_req);
        return -1;
    }

    free(p_req);

    p_res_vm = (agexec_vm_t *)p_res->data;
    printf("uuid: %s\nname: %s\nname-label: %s\n"
           "power-state: %s\nmemory: %d\ncpu: %d\n",
           p_res_vm->vm_uuid,
           p_res_vm->vm_name_label,
           p_res_vm->vm_name_label,
           p_res_vm->vm_snapshot_label,
           p_res_vm->mem_size,
           p_res_vm->vcpu_num
           );
    printf("sys:\n\tuuid: %s\n\tsr-uuid: %s\n\tsize: %"PRId64"\n",
           p_res_vm->vdi_sys_uuid,
           p_res_vm->vdi_sys_sr_uuid,
           p_res_vm->vdi_sys_size);
    printf("user:\n\tuuid: %s\n\tsr-uuid: %s\n\tsize: %"PRId64"\n",
           p_res_vm->vdi_user_uuid,
           p_res_vm->vdi_user_sr_uuid,
           p_res_vm->vdi_user_size);
    for (i = 0; i < 7; ++i) {
        if (p_res_vm->vifs[i].in_use == VIF_IN_USE_Y) {
            printf("vif#%d mac: %s\n",
                   i,
                   p_res_vm->vifs[i].dl_addr);
        }
    }

    if (!p_res_vm->vm_name_label[0]) {
        fprintf(stderr, "Error: VM '%s' not exist in server '%s'\n",
                uuid, launch_server);
        free(p_res);
        return -1;
    }
    if (strcmp(p_res_vm->vm_snapshot_label, "running") == 0) {
        state = 4;
    } else if (strcmp(p_res_vm->vm_snapshot_label, "halted") == 0) {
        state = 9;
    } else {
        state = 11;
    }

    /* vdi_sys_sr_name */
    /* assume sys_disk and user_disk in the same sr */
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "uuid='%s'", p_res_vm->vdi_sys_sr_uuid);
    memset(vdi_sys_sr_name, 0, sizeof vdi_sys_sr_name);
    db_select(&vdi_sys_sr_name, BUFSIZE, 1, "storage", "name", cond);
    if (!vdi_sys_sr_name[0]) {
        fprintf(stderr, "Error: Server '%s' sr '%s' not exist\n",
                launch_server, p_res_vm->vdi_sys_sr_uuid);
    }

    /* insert vm */
    memset(values, 0, sizeof values);
    snprintf(values, BIG_BUFSIZE,
             "%d,'%s','%s','%s',%d,%s,'%s',"
             "'%s','%s','%s',%"PRId64","
             "'%s','%s','%s',%"PRId64","
             "%d,%d,%d,%d,'%s','%s',%s,'%s','%s'",
             state, p_res_vm->vm_name_label, p_res_vm->vm_name_label,
             os, LC_VM_FLAGS_FROM_TEMPLATE, pool_id, launch_server,
             p_res_vm->vdi_sys_uuid, p_res_vm->vdi_sys_sr_uuid,
             vdi_sys_sr_name, p_res_vm->vdi_sys_size,
             p_res_vm->vdi_user_uuid, p_res_vm->vdi_sys_sr_uuid,
             vdi_sys_sr_name, p_res_vm->vdi_user_size,
             p_res_vm->mem_size, p_res_vm->vcpu_num, tport, (tport+10000),
             uuid, domain,user_id, lcuuid, ps_lcuuid);
    db_insert("vm", vm_params, values);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "vm", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: db_insert() failed\n");
        free(p_res);
        return -1;
    }
    /* insert fdb_vm */
    memset(values, 0, sizeof values);
    snprintf(values, BIG_BUFSIZE,
             "%d,'%s','%s',%d,'%s',"
             "%d,%d,%"PRId64",%"PRId64",%s,'%s','%s','%s','%s'",
             state, p_res_vm->vm_name_label, os, LC_VM_FLAGS_FROM_TEMPLATE,
             launch_server, p_res_vm->mem_size, p_res_vm->vcpu_num,
             p_res_vm->vdi_sys_size, p_res_vm->vdi_user_size,
             user_id, lcuuid, domain, ps_lcuuid, create_time);
    db_insert("fdb_vm", fdb_vm_params, values);
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "fdb_vm", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: db_insert() failed\n");
        free(p_res);
        return -1;
    }
    /* insert vinterface */
    for (i = 0; i <= LC_VM_IFACE_CTRL; ++i) {
        memset(values, 0, sizeof values);
        snprintf(values, BUFSIZE, "%d,%d,'%s',%d,%s,'%s'",
                 i, LC_VIF_STATE_DETACHED, p_res_vm->vifs[i].dl_addr,
                 LC_VIF_DEVICE_TYPE_VM,id_s, domain);
        db_insert("vinterface", vif_params, values);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "mac='%s'", p_res_vm->vifs[i].dl_addr);
        memset(vif_id_s, 0, sizeof vif_id_s);
        db_select(&vif_id_s, BUFSIZE, 1, "vinterface", "id", cond);
        if (!vif_id_s[0]) {
            fprintf(stderr, "Error: db_insert() failed\n");
            free(p_res);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "mac='%s'", p_res_vm->vifs[LC_VM_IFACE_CTRL].dl_addr);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE,
             "iftype=%u, state=%u, ip='%s', netmask=%u, subnetid=%s, bandw=%s",
             LC_VIF_IFTYPE_CTRL, LC_VIF_STATE_ATTACHED, ctrl_vif_ip, ctrl_vif_netmask,
             ctrl_vif_subnetid, ctrl_vif_bandw);
    db_update("vinterface", values, cond);

    free(p_res);

    return 0;
}

static int lcm_config(int opts, argument *args)
{
    int shmid;
    key_t shmkey;
    char *server;
    char *lcm_server;

    if (!(server = get_arg(args, "lcm_server"))) {
        fprintf(stderr, "Error: No server specified\n");
        return -1;
    }

    printf("config lcm_server '%s'\n", server);

    shmkey = ftok(LCMD_SHM_ID, 'a');
    shmid = shmget(shmkey, MAX_HOST_ADDRESS_LEN, 0666|IPC_CREAT);

    if (shmid != -1) {
        printf("Get shared memory for lcmd %d", shmid);
    } else {
        fprintf(stderr, "Get shared memory for lcmd error!");
        return -1;
    }

    if ((lcm_server = (char *)shmat(shmid, (const void*)0, 0)) == (void *)-1) {
        fprintf(stderr, "%s:shmat error:%s\n", __FUNCTION__,strerror(errno));
        return -1;
    }

    snprintf(lcm_server, MAX_HOST_ADDRESS_LEN, "%s", server);

    return 0;
}

static int template_os_config(int opts, argument *args)
{
    int err = 0, userid = 0;
    char *server, *template, *ttype, *vendor, *domain_name;
    char *userid_str;
    char domain[BUFSIZE] = {0};
    char cmd[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char local_result[MNTNCT_CMD_RESULT_LEN] = {0};

    if (!(domain_name = get_arg(args, "domain"))) {
        printf("Error: No domain specified\n");
        return -1;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: domain '%s' not exist\n", domain_name);
            return -1;
        }
    }
    if (!(server = get_arg(args, "server_ip"))) {
        server = "";
    }
    if (!(template = get_arg(args, "name"))) {
        printf("Error: No template specified\n");
        return -1;
    }
    if (!(ttype = get_arg(args, "ttype"))) {
        printf("Error: No ttype specified\n");
        return -1;
    }
    if (!(vendor = get_arg(args, "vendor"))) {
        printf("Error: No vendor specified\n");
        return -1;
    }

    if ((userid_str = get_arg(args, "userid"))) {
        userid = atoi(userid_str);
    }

    if (strlen(server)) {
        snprintf(cmd, BUFSIZE, LC_SET_OS_TEMPLATE_STR,
                 server, template, strlwr(ttype), vendor, domain);
    } else {
        snprintf(cmd, BUFSIZE, LC_SET_LOCAL_OS_TEMPLATE_STR,
                 template, strlwr(ttype), vendor, domain, userid);
    }
    err = call_system_output(cmd, local_result, MNTNCT_CMD_RESULT_LEN);
    return 0;
}

#define TEMPLATE_OS_STATE_ONLINE 0
#define TEMPLATE_OS_STATE_OFFLINE 1

static int template_os_modify(int opts, argument *args)
{
    int err = 0;
    int state_i = 0;
    char *server, *template, *ttype, *state, *new_name, *domain_name;
    char domain[BUFSIZE] = {0};
    char cmd[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char values[BUFSIZE] = {0};
    char local_result[MNTNCT_CMD_RESULT_LEN] = {0};

    if (!(template = get_arg(args, "name"))) {
        printf("Error: No template specified\n");
        return -1;
    }
    if (!(domain_name = get_arg(args, "domain"))) {
        printf("Error: No domain specified\n");
        return -1;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: domain '%s' not exist\n", domain_name);
            return -1;
        }
    }

    if ((server = get_arg(args, "server_ip"))) {
        if (!(ttype = get_arg(args, "ttype"))) {
            snprintf(cmd, BUFSIZE, LC_MOD_OS_TEMPLATE_STR,
                    server, template);
        } else {
            snprintf(cmd, BUFSIZE, LC_MOD_OS_TEMPLATE_BY_TTYPE_STR,
                    server, template, strlwr(ttype));
        }
        err = call_system_output(cmd, local_result, MNTNCT_CMD_RESULT_LEN);
    }

    if ((state = get_arg(args, "state"))) {
        if (strcasecmp(state, "online") == 0 ||
            strcasecmp(state, "0") == 0) {

            state_i = TEMPLATE_OS_STATE_ONLINE;
        } else if (strcasecmp(state, "offline") == 0 ||
                   strcasecmp(state, "1") == 0) {

            state_i = TEMPLATE_OS_STATE_OFFLINE;
        } else {

            fprintf(stderr, "Error: Unknown state %s\n", state);
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "state=%d", state_i);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s' and domain='%s'", template, domain);
        db_update("template_os", values, cond);
    }

    if ((new_name = get_arg(args, "new_name"))) {
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "name='%s'", new_name);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s' and domain='%s'", template, domain);
        db_update("template_os", values, cond);
    }
    return 0;
}

static int template_os_delete(int opts, argument *args)
{
    int err = 0;
    char *template, *ttype;
    char cmd[BUFSIZE] = {0};
    char local_result[MNTNCT_CMD_RESULT_LEN] = {0};

    if (!(template = get_arg(args, "name"))) {
        printf("Error: No template specified\n");
        return -1;
    }
    if (!(ttype = get_arg(args, "ttype"))) {
        snprintf(cmd, BUFSIZE, LC_UNSET_OS_TEMPLATE_STR,
                template);
    } else {
        snprintf(cmd, BUFSIZE, LC_UNSET_OS_TEMPLATE_BY_TTYPE_STR,
                template, strlwr(ttype));
    }
    err = call_system_output(cmd, local_result, MNTNCT_CMD_RESULT_LEN);
    return 0;
}

static int template_os_list(int opts, argument *args)
{
    char *default_params = "id,state,name,ttype,server_ip,vendor,time,userid,domain";
    char *params = get_arg(args, "params");
    char *name = get_arg(args, "name");
    char *domain_name = get_arg(args, "domain");
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    int cont = 0;

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not found\n", domain);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    if (name) {
        memset(buffer, 0, sizeof buffer);
        if (cont) {
            snprintf(buffer, BUFSIZE, " and name='%s'", name);
        } else {
            snprintf(buffer, BUFSIZE, "name='%s'", name);
            cont = 1;
        }
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (domain[0]) {
        memset(buffer, 0, sizeof buffer);
        if (cont) {
            snprintf(buffer, BUFSIZE, " and domain='%s'", domain);
        } else {
            snprintf(buffer, BUFSIZE, "domain='%s'", domain);
            cont = 1;
        }
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            if (cond[0]) {
                db_dump_cond_minimal("template_os", "id", cond);
            } else {
                db_dump_minimal("template_os", "id");
            }
        } else {
            if (cond[0]) {
                db_dump_cond_minimal("template_os", params, cond);
            } else {
                db_dump_minimal("template_os", params);
            }
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("template_os", params, cond);
        } else {
            db_dump("template_os", params);
        }
    }

    return 0;
}

/* vcenter */
static int vcenter_list(int opts, argument *args)
{
    char *default_params = "id,ip,username,in_mode,rackid,description";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond_minimal("vc_server", "id", cond);
        } else {
            db_dump_cond_minimal("vc_server", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("vc_server", params, cond);
    }

    return 0;
}

static int vcenter_add(int opts, argument *args)
{
    char *params = "ip,username,passwd,in_mode,rackid,description";
    char *ip = 0;
    char *username = 0;
    char *passwd = 0;
    char *in_mode = 0;
    char *rack_name = 0;
    char *domain = 0;
    char *description = 0;
    char dummy[BUFSIZE];
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char rack_id[BUFSIZE];
    char domain_uuid[UUID_LENGTH];
    char rack_uuid[UUID_LENGTH];

    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No ip specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "vc_server", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: vCenter '%s' already exists\n", ip);
        return -1;
    }
    if (!(in_mode = get_arg(args, "in_mode"))) {
        in_mode = "gateway";
    }
    if (strcmp(in_mode, "gateway") == 0) {
        if (!(domain = get_arg(args, "domain"))) {
            fprintf(stderr, "Error: No domain specified\n");
            return -1;
        }
        memset(cond, 0x00, sizeof(cond));
        snprintf(cond, BUFSIZE, "name='%s'", domain);
        memset(domain_uuid, 0x00, sizeof(domain_uuid));
        db_select(&domain_uuid, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain_uuid[0]) {
            fprintf(stderr, "Error: Domain does not exist\n");
            return -1;
        }
        memset(rack_uuid, 0x00, sizeof(rack_uuid));
        if (!generate_uuid(rack_uuid, UUID_LENGTH)) {
            fprintf(stderr, "Error: failed to generate rack_uuid\n");
            return -1;
        }
        memset(values, 0x00, sizeof(values));
        snprintf(values, BUFSIZE, "'%s','%s',%d,'%s'",
            rack_uuid, domain_uuid, 20, rack_uuid);
        db_insert("rack", "name,domain,server_num,lcuuid", values);
        memset(cond, 0x00, sizeof(cond));
        snprintf(cond, BUFSIZE, "name='%s'", rack_uuid);
        memset(rack_id, 0x00, sizeof(rack_id));
        db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
        if (!rack_id[0]) {
            fprintf(stderr, "Error: rack create failed\n");
            return -1;
        }
    }
    else if (strcmp(in_mode, "access") == 0) {
        if (!(rack_name = get_arg(args, "rack_name"))) {
            fprintf(stderr, "Error: No rack name specified\n");
            return -1;
        }
        memset(cond, 0x00, sizeof(cond));
        snprintf(cond, BUFSIZE, "name='%s'", rack_name);
        memset(rack_id, 0x00, sizeof(rack_id));
        db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
        if (!rack_id[0]) {
            fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
            return -1;
        }
        memset(cond, 0x00, sizeof(cond));
        snprintf(cond, BUFSIZE, "rackid=%s", rack_id);
        memset(dummy, 0x00, sizeof(dummy));
        db_select(&dummy, BUFSIZE, 1, "vc_server", "ip", cond);
        if (dummy[0]) {
            fprintf(stderr, "Error: Rack is used in vcenter(%s)\n", dummy);
            return -1;
        }
    }
    else {
        fprintf(stderr, "Error: in_mode must be \"gateway\" or \"access\"\n");
        return -1;
    }
    if (!(username = get_arg(args, "username"))) {
        fprintf(stderr, "Error: No username specified\n");
        return -1;
    }
    if (!(passwd = get_arg(args, "passwd"))) {
        fprintf(stderr, "Error: No password specified\n");
        return -1;
    }
    if (!(description = get_arg(args, "description"))) {
        description = "";
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s','%s','%s',%s,'%s'",
            ip, username, passwd, in_mode, rack_id, description);
    db_insert("vc_server", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "vc_server", "id", cond);
    printf("vcenter id = %s, rack id = %s\n", id_s, rack_id);

    return 0;
}

static int vcenter_remove(int opts, argument *args)
{
    char *id = 0;
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];

    id = get_arg(args, "id");
    ip = get_arg(args, "ip");
    if (!id && !ip) {
        fprintf(stderr, "Error: No id or ip specified\n");
        return -1;
    }
    if (ip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "vc_server", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: vCenter '%s' not exist\n", ip);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: vCenter id and ip mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "vc_server", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: vCenter with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "server_id=%s", id);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "vc_datacenter", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: vCenter with id '%s' not empty\n", id);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("vc_server", cond);

    return 0;
}

/* vcdatacenter */
static int vcdatacenter_list(int opts, argument *args)
{
    char *default_params = "id,server_id,datacenter_name,vswitch_name,"
                           "vm_folder,vm_templatefolder,description";
    char *params = get_arg(args, "params");
    char *server_ip = get_arg(args, "server_ip");
    char *datacenter_name = get_arg(args, "datacenter_name");
    char server_id[BUFSIZE] = {0};
    char cond[BUFSIZE];
    char buffer[BUFSIZE];

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (server_ip) {
        snprintf(cond, BUFSIZE, "ip='%s'", server_ip);
        memset(server_id, 0, sizeof server_id);
        db_select(&server_id, BUFSIZE, 1, "vc_server", "id", cond);
        if (!server_id[0]) {
            fprintf(stderr, "Error: vCenter with ip '%s' not exist\n", server_ip);
            return -1;
        }
        memset(cond, 0, sizeof cond);
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and server_id=%s", server_id);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (datacenter_name) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and datacenter_name='%s'", datacenter_name);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond_minimal("vc_datacenter", "id", cond);
        } else {
            db_dump_cond_minimal("vc_datacenter", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("vc_datacenter", params, cond);
    }

    return 0;
}

static int vcdatacenter_add(int opts, argument *args)
{
    char *params = "server_id,datacenter_name,vswitch_name,"
                   "vm_folder,vm_templatefolder,description";
    char *server_ip = 0;
    char *datacenter_name = 0;
    char *vswitch_name = 0;
    char *vm_folder = 0;
    char *vm_templatefolder = 0;
    char *description = 0;
    char server_id[BUFSIZE] = {0};
    char dummy[BUFSIZE];
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id_s[BUFSIZE];

    if (!(server_ip = get_arg(args, "server_ip"))) {
        fprintf(stderr, "Error: No vCenter id specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", server_ip);
    memset(server_id, 0, sizeof server_id);
    db_select(&server_id, BUFSIZE, 1, "vc_server", "id", cond);
    if (!server_id[0]) {
        fprintf(stderr, "Error: vCenter with ip '%s' not exist\n", server_ip);
        return -1;
    }
    if (!(datacenter_name = get_arg(args, "datacenter_name"))) {
        fprintf(stderr, "Error: No vDatacenter name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "datacenter_name='%s'", datacenter_name);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "vc_datacenter", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: vDatacenter '%s' already exists\n",
                datacenter_name);
        return -1;
    }
    if (!(vswitch_name = get_arg(args, "vswitch_name"))) {
        fprintf(stderr, "Error: No vswitch_name specified\n");
        return -1;
    }
    if (!(vm_folder = get_arg(args, "vm_folder"))) {
        vm_folder = "/";
    }
    if (!(vm_templatefolder = get_arg(args, "vm_templatefolder"))) {
        vm_templatefolder = "/";
    }
    if (!(description = get_arg(args, "description"))) {
        description = "";
    }
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s','%s','%s','%s','%s'",
            server_id, datacenter_name, vswitch_name,
            vm_folder,vm_templatefolder,description);
    db_insert("vc_datacenter", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "datacenter_name='%s'", datacenter_name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "vc_datacenter", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int vcdatacenter_remove(int opts, argument *args)
{
    char *id = 0;
    char *datacenter_name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];

    id = get_arg(args, "id");
    datacenter_name = get_arg(args, "datacenter_name");
    if (!id && !datacenter_name) {
        fprintf(stderr, "Error: No id or datacenter name specified\n");
        return -1;
    }
    if (datacenter_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "datacenter_name='%s'", datacenter_name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "vc_datacenter", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: vDatacenter '%s' not exist\n",
                    datacenter_name);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr,
                    "Error: vDatacenter id and datacenter name mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "vc_datacenter", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: vDatacenter with id '%s' not exist\n", id);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "vc_datacenter_id=%s", id);
    memset(dummy, 0, sizeof dummy);
    db_update("cluster", "vc_datacenter_id=0", cond);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("vc_datacenter", cond);

    return 0;
}

static int print_log_level(char *component)
{
    int shmid;
    int *lp = 0;
    char *level = 0;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    if (!component[0]) {
        return -1;
    }
    if (strcasecmp(component, "lcmd") == 0) {
        shmkey = ftok(LCMD_LOG_LEVEL_ID, 'l');
    } else if (strcasecmp(component, "vmdriver") == 0) {
        sprintf(cmd, "touch "LC_VM_SYS_LEVEL_SHM_ID);
        system(cmd);
        shmkey = ftok(LC_VM_SYS_LEVEL_SHM_ID, 'a');
    } else if (strcasecmp(component, "lcpd") == 0) {
        shmkey = ftok(LC_LCPD_SYS_LEVEL_SHM_ID, 'a');
    } else {
        fprintf(stderr, "No component %s\n", component);
        return -1;
    }
    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT);

    if (shmid == -1) {
        fprintf(stderr, "Get shared memory for %s log level error!\n",
                component);
        return -1;
    }

    if ((lp = (int *)shmat(shmid, (const void*)0, 0))
         == (void *)-1) {
        fprintf(stderr, "%s:shmat error:%s\n", __FUNCTION__, strerror(errno));
        return -1;
    }

    switch (*lp) {
    case LOG_DEBUG:
        level = "DEBUG";
        break;
    case LOG_INFO:
        level = "INFO";
        break;
    case LOG_WARNING:
        level = "WARNING";
        break;
    case LOG_ERR:
        level = "ERR";
        break;
    case LOG_EMERG:
        level = "NONE";
        break;
    default:
        break;
    }
    printf("%-10s: %s\n", component, level);

    return 0;
}

static int log_level_list(int opts, argument *args)
{
    char *component = 0;

    if (!(component = get_arg(args, "component"))) {
        component = "";
    }

    if (!component[0]) {
        print_log_level("lcmd");
        print_log_level("vmdriver");
        print_log_level("lcpd");
    } else {
        print_log_level(component);
    }

    return 0;
}

static int set_log_level(char *component, char *level)
{
    int shmid;
    int *lp = 0;
    key_t shmkey;
    char cmd[100];

    memset(cmd, 0x00, sizeof(cmd));

    if (!component[0]) {
        return -1;
    }
    if (strcasecmp(component, "lcmd") == 0) {
        shmkey = ftok(LCMD_LOG_LEVEL_ID, 'l');
    } else if (strcasecmp(component, "vmdriver") == 0) {
        sprintf(cmd, "touch "LC_VM_SYS_LEVEL_SHM_ID);
        system(cmd);
        shmkey = ftok(LC_VM_SYS_LEVEL_SHM_ID, 'a');
    } else if (strcasecmp(component, "lcpd") == 0) {
        shmkey = ftok(LC_LCPD_SYS_LEVEL_SHM_ID, 'a');
    } else {
        fprintf(stderr, "No component %s\n", component);
        return -1;
    }
    shmid = shmget(shmkey, sizeof(int), 0666|IPC_CREAT);

    if (shmid == -1) {
        fprintf(stderr, "Get shared memory for %s log level error!\n",
                component);
        return -1;
    }

    if ((lp = (int *)shmat(shmid, (const void*)0, 0))
         == (void *)-1) {
        fprintf(stderr, "%s:shmat error:%s\n", __FUNCTION__, strerror(errno));
        return -1;
    }

    if (strcasecmp(level, "DEBUG") == 0){
        *lp = LOG_DEBUG;
    } else if (strcasecmp(level, "INFO") == 0){
        *lp = LOG_INFO;
    } else if (strcasecmp(level, "WARNING") == 0){
        *lp = LOG_WARNING;
    } else if (strcasecmp(level, "ERR") == 0){
        *lp = LOG_ERR;
    } else if (strcasecmp(level, "NONE") == 0){
        *lp = LOG_EMERG;
    } else {
        fprintf(stderr, "No level %s\n", level);
        return -1;
    }

    return 0;
}

/*hwdev*/
static int hwdev_list(int opts, argument *args)
{
    char *default_params = "id,state,name,label,brand,mgmt_ip,sys_os,vnc_port,"
                           "rack_name,poolid,userid,epc_id,lcuuid";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("third_party_device", "mgmt_ip");
        } else {
            db_dump_minimal("third_party_device", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("third_party_device", params);
    }

    return 0;
}

static int hwdev_add(int opts, argument *args)
{
    char *name =0;
    char *epc_user_email = 0;
    char *order_id = 0;
    char *pool_name = 0;
    char *rack_name = 0;
    char *role = 0;
    char *product_s_name = 0;
    char *brand = 0;
    char *mgmt_ip = 0;
    char *user_name = 0;
    char *user_passwd = 0;
    char user_id[BUFSIZE] = { 0 };
    char rack_id[BUFSIZE] = { 0 };
    char pool_id[BUFSIZE] = { 0 };
    char domain[BUFSIZE] = { 0 };
    char cond[BUFSIZE];
    char rack_lcuuid[UUID_LENGTH];
    char product_s_lcuuid[UUID_LENGTH];
    char url[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int url_len = 0, used_for_public = 0;
    int err;
    char call_result[CURL_BUFFER_SIZE];
    int hwdev_id_int;
    const nx_json * json = 0;

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No third party's name specified\n");
        return -1;
    }
    epc_user_email = get_arg(args, "EPC_user_email");
    order_id = get_arg(args, "order_id");
    pool_name = get_arg(args, "pool_name");
    if (pool_name) {
        if (epc_user_email || order_id) {
            fprintf(stderr, "Error: pool_name and (EPC_user_email, order_id) "
                    "cannot be specified together\n");
            return -1;
        }
        used_for_public = 1;
    } else if (epc_user_email || order_id) {
        if (!epc_user_email) {
            fprintf(stderr, "Error: No related EPC_user_email with third party specified\n");
            return -1;
        }
        if (!order_id) {
            fprintf(stderr, "Error: No order id specified\n");
            return -1;
        }
    } else {
        fprintf(stderr, "Error: No pool_name or (EPC_user_email, order_id) "
                "should be specified\n");
        return -1;
    }
    if (!(rack_name = get_arg(args, "rack_name"))) {
        fprintf(stderr, "Error: No related rack_name with third party specified\n");
        return -1;
    }
    if (!(role = get_arg(args, "role"))) {
        fprintf(stderr, "Error: No third party's role specified\n");
        return -1;
    }
    if (!(product_s_name = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification (plan_name) specified\n");
        return -1;
    }
    if (!(brand = get_arg(args, "brand"))) {
        fprintf(stderr, "Error: No brand specified\n");
        return -1;
    }
    if (!(mgmt_ip = get_arg(args, "mgmt_ip"))) {
        fprintf(stderr, "Error: No mgmt_ip specified, "
                "use 0.0.0.0 if there is no mgmt_ip\n");
        return -1;
    }
    if (!(user_name = get_arg(args, "user_name"))) {
        fprintf(stderr, "Error: No user_name specified\n");
        return -1;
    }
    if (!(user_passwd = get_arg(args, "user_passwd"))) {
        fprintf(stderr, "Error: No user_passwd specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", rack_name);
    memset(rack_id, 0, sizeof rack_id);
    db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
    if (!rack_id[0]) {
        fprintf(stderr, "Error: Rack with name '%s' not exist\n", rack_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", rack_name);
    memset(rack_lcuuid, 0, UUID_LENGTH);
    db_select(&rack_lcuuid, UUID_LENGTH, 1, "rack", "lcuuid", cond);
    if (!rack_lcuuid[0]) {
        fprintf(
            stderr,
            "Error: Rack with name '%s' not exist or lcuuid is empty\n",
            rack_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", rack_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, BUFSIZE, 1, "rack", "domain", cond);
    if (!domain[0]) {
        fprintf(
            stderr,
            "Error: Rack with name '%s' not exist or domain is empty\n",
            rack_name);
        return -1;
    }
    if (!used_for_public) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "email='%s'", epc_user_email);
        memset(user_id, 0, sizeof user_id);
        db_select(&user_id, BUFSIZE, 1, "user", "id", cond);
        if (!user_id[0]) {
            fprintf(stderr, "Error: EPC_user with email '%s' not exist\n", epc_user_email);
            return -1;
        }
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", pool_name);
        memset(pool_id, 0, sizeof pool_id);
        db_select(&pool_id, BUFSIZE, 1, "pool", "id", cond);
        if (!pool_id[0]) {
            fprintf(stderr, "Error: Pool with name '%s' not exist\n", pool_name);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s'", product_s_name);
    memset(product_s_lcuuid, 0, UUID_LENGTH);
    db_select(&product_s_lcuuid, UUID_LENGTH, 1, "product_specification", "lcuuid", cond);
    if (!product_s_lcuuid[0]) {
        fprintf(stderr,
            "Error: product_specification with plan_name '%s' not exist\n",
            product_s_name);
        return -1;
    }

    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_POST);
    if (!used_for_public) {
        err = pack_hwdev_curl_post(
                post, MNTNCT_CMD_LEN, name, atoi(user_id), atoi(order_id), 0,
                rack_lcuuid, domain, role, product_s_lcuuid,
                brand, mgmt_ip, user_name, user_passwd);
    } else {
        err = pack_hwdev_curl_post(
                post, MNTNCT_CMD_LEN, name, 0, 0, atoi(pool_id),
                rack_lcuuid, domain, role, product_s_lcuuid,
                brand, mgmt_ip, user_name, user_passwd);
    }
    if (err) {
        fprintf(stderr, "Error: Pack third-party device post curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_POST, post, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    json = nx_json_parse(call_result, 0);
    if (json && nx_json_get(
                nx_json_get(json, "DATA"), "ID")->type == NX_JSON_INTEGER) {
        hwdev_id_int = nx_json_get(nx_json_get(json, "DATA"), "ID")->int_value;
    } else {
        fprintf(stderr, "Error: Invalid JSON response\n%s", call_result);
        return -1;
    }
    printf("id=%d\n", hwdev_id_int);
    if (used_for_public) {
        printf("NOW execute 'mt hwdev.connect hwdev_id=%d if_index=0 ...' "
               "to configure hwdev boot interface\n", hwdev_id_int);
    }
    return 0;
}

static int hwdev_connect(int opts, argument *args)
{
    char *hwdev_id = 0;
    char *if_index = 0;
    char *if_mac = 0;
    char *switch_id = 0;
    char *switch_port = 0;
    char *speed = 0;
    char switch_lcuuid[UUID_LENGTH];
    char hwdev_lcuuid[UUID_LENGTH];
    char cond[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int url_len = 0;
    int err;
    char call_result[CURL_BUFFER_SIZE];
    int vif_id_int, speed_int, i;
    const nx_json * json = 0;

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(hwdev_id = get_arg(args, "hwdev_id"))) {
        fprintf(stderr, "Error: No hwdev_id pecified\n");
        return -1;
    }
    if (!(if_index = get_arg(args, "if_index"))) {
        fprintf(stderr, "Error: No if_index specified\n");
        return -1;
    }
    if (!(if_mac = get_arg(args, "if_mac"))) {
        fprintf(stderr, "Error: No if_mac specified\n");
        return -1;
    }
    for (i = 0; i < strlen(if_mac); i++) {
        if (if_mac[i] >= 'A' && if_mac[i] <= 'Z') {
            if_mac[i] = if_mac[i] + ('a' - 'A');
        }
    }
    if (!(switch_id = get_arg(args, "switch_id"))) {
        fprintf(stderr, "Error: No switch_id specified\n");
        return -1;
    }
    if (!(switch_port = get_arg(args, "switch_port"))) {
        fprintf(stderr, "Error: No switch_port specified\n");
        return -1;
    }
    if (!(speed = get_arg(args, "speed"))) {
        fprintf(stderr, "Error: No speed specified\n");
        return -1;
    }
    if (strcmp(speed, "1000Mbps") == 0) {
        speed_int = 1000;
    } else if (strcmp(speed, "10000Mbps") == 0) {
        speed_int = 10000;
    } else {
        fprintf(stderr, "Error: speed must be either 1000Mbps or 10000Mbps\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", switch_id);
    memset(switch_lcuuid, 0, UUID_LENGTH);
    db_select(&switch_lcuuid, BUFSIZE, 1, "network_device", "lcuuid", cond);
    if (!switch_lcuuid[0]) {
        fprintf(stderr, "Error: switch with id=%s not exist\n", switch_id);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    memset(hwdev_lcuuid, 0, UUID_LENGTH);
    db_select(&hwdev_lcuuid, BUFSIZE, 1, "third_party_device", "lcuuid", cond);
    if (!hwdev_lcuuid[0]) {
        fprintf(stderr, "Error: hwdev with id=%s not exist\n", hwdev_id);
        return -1;
    }
    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_INTF_POST, hwdev_lcuuid);
    err = pack_hwdev_intf_curl_post(
            post, MNTNCT_CMD_LEN, atoi(if_index), if_mac, speed_int,
            switch_lcuuid, atoi(switch_port));
    if (err) {
        fprintf(stderr, "Error: Pack third-party device interface interface post curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_POST, post, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    json = nx_json_parse(call_result, 0);
    if (json && nx_json_get(nx_json_get(json, "DATA"),
                            "ID")->type == NX_JSON_INTEGER) {
        vif_id_int = nx_json_get(nx_json_get(json, "DATA"), "ID")->int_value;
    } else {
        fprintf(stderr, "Error: Invalid JSON response\n%s", call_result);
        return -1;
    }
    printf("interface_id=%d\n", vif_id_int);
    return 0;
}

static int hwdev_connect_list(int opts, argument *args)
{
    char *default_params = "l.id,l.name,l.state,l.lcuuid,concat(r.id) vifid,r.ifindex,"
                           "r.mac,concat(r.ofport) network_device_port,"
                           "concat(r.netdevice_lcuuid) network_device_lcuuid,r.domain";
    char *params = get_arg(args, "params");
    char *hwdev_name = get_arg(args, "hwdev_name");
    char cond[BUFSIZE];
    char buffer[BUFSIZE];

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (hwdev_name) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and l.name='%s'", hwdev_name);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (!params) {
        params = default_params;
    }
    db_dump_hwdev_cond(params, cond);

    return 0;

}

static int hwdev_attach(int opts, argument *args)
{
    char *hwdev_id = 0;
    char *if_index = 0;
    char *if_state = "attached";
    int if_state_int = 0;
    char *if_type = 0;
    char vif_state[BUFSIZE];
    char ip[BUFSIZE];
    char vlantag[BUFSIZE];
    char hwdev_lcuuid[UUID_LENGTH];
    char cond[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int url_len = 0;
    int err, i;
    char call_result[CURL_BUFFER_SIZE];

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(hwdev_id = get_arg(args, "hwdev_id"))) {
        fprintf(stderr, "Error: No hwdev_id pecified\n");
        return -1;
    }
    if (!(if_index = get_arg(args, "if_index"))) {
        fprintf(stderr, "Error: No if_index specified\n");
        return -1;
    }

    if (strcmp(if_state, "attached") ==0) {
        if_state_int = MNTNCT_HWDEV_INTF_STATE_ATTACHED;
    } else if (strcmp(if_state, "detached") ==0) {
        if_state_int = MNTNCT_HWDEV_INTF_STATE_DETACHED;
    } else {
        fprintf(
            stderr,
            "Error: only 'attached' or 'detached' can be assigned to if_type\n");
        return -1;
    }

    if (!(if_type = get_arg(args, "if_type"))) {
        fprintf(stderr, "Error: No if_type specified\n");
        return -1;
    }

    for (i = 0; i <strlen(if_type); i++ ) {
        if_type[i] = toupper(if_type[i]);
    }
    if (strcmp(if_type, "SERVICE") !=0) {
        fprintf(stderr, "Error: only 'service' can be assigned to if_state\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    memset(hwdev_lcuuid, 0, UUID_LENGTH);
    db_select(&hwdev_lcuuid, BUFSIZE, 1, "third_party_device", "lcuuid", cond);
    if (!hwdev_lcuuid[0]) {
        fprintf(stderr, "Error: hwdev with id=%s not exist\n", hwdev_id);
        return -1;
    }
    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_INTF_PATCH, hwdev_lcuuid, if_index);
    err = pack_hwdev_intf_curl_patch_service(
            post, MNTNCT_CMD_LEN, if_state_int, if_type, VIF_SUBTYPE_SERVICE_VENDOR);
    if (err) {
        fprintf(stderr, "Error: Pack third-party device interface interface patch curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_PATCH, post, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "devicetype=%d AND deviceid=%s and ifindex=%s",
        MNTNCT_VIF_DEVICE_TYPE_THIRDHW, hwdev_id, if_index);
    while (1) {
        memset(vif_state, 0, BUFSIZE);
        db_select(&vif_state, BUFSIZE, 1, "vinterface", "state", cond);
        if (if_state_int == atoi(vif_state)) {
            break;
        }
        printf("Waiting for vif %s state to complete ...\n", if_state);
        sleep(5);
    }
    db_select(&ip, BUFSIZE, 1, "vinterface", "ip", cond);
    db_select(&vlantag, BUFSIZE, 1, "vinterface", "vlantag", cond);
    printf("vlantag=%s, ip=%s\n", vlantag, ip);
    return 0;
}


static int hwdev_detach(int opts, argument *args)
{
    char *hwdev_id = 0;
    char *if_index = 0;
    char *if_state = "detached";
    char *if_type = 0;
    int if_state_int = 0;
    char vif_state[BUFSIZE];
    char hwdev_lcuuid[UUID_LENGTH];
    char cond[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int url_len = 0;
    int err;
    char call_result[CURL_BUFFER_SIZE];

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(hwdev_id = get_arg(args, "hwdev_id"))) {
        fprintf(stderr, "Error: No hwdev_id pecified\n");
        return -1;
    }
    if (!(if_index = get_arg(args, "if_index"))) {
        fprintf(stderr, "Error: No if_index specified\n");
        return -1;
    }

    if (strcmp(if_state, "attached") ==0) {
        if_state_int = MNTNCT_HWDEV_INTF_STATE_ATTACHED;
    } else if (strcmp(if_state, "detached") ==0) {
        if_state_int = MNTNCT_HWDEV_INTF_STATE_DETACHED;
    } else {
        fprintf(
            stderr,
            "Error: only 'attached' or 'detached' can be assigned to if_state\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    memset(hwdev_lcuuid, 0, UUID_LENGTH);
    db_select(&hwdev_lcuuid, BUFSIZE, 1, "third_party_device", "lcuuid", cond);
    if (!hwdev_lcuuid[0]) {
        fprintf(stderr, "Error: hwdev with id=%s not exist\n", hwdev_id);
        return -1;
    }
    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_INTF_PATCH, hwdev_lcuuid, if_index);
    err = pack_hwdev_intf_curl_patch_service(
            post, MNTNCT_CMD_LEN, if_state_int, if_type, 0);
    if (err) {
        fprintf(stderr, "Error: Pack third-party device interface interface patch curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_PATCH, post, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "devicetype=%d AND deviceid=%s and ifindex=%s",
        MNTNCT_VIF_DEVICE_TYPE_THIRDHW, hwdev_id, if_index);
    while (1) {
        memset(vif_state, 0, BUFSIZE);
        db_select(&vif_state, BUFSIZE, 1, "vinterface", "state", cond);
        if (if_state_int == atoi(vif_state)) {
            break;
        }
        printf("Waiting for vif %s state to complete ...\n", if_state);
        sleep(5);
    }
    return 0;
}

static int hwdev_disconnect(int opts, argument *args)
{
    char *hwdev_id = 0;
    char *if_index = 0;
    char hwdev_lcuuid[UUID_LENGTH];
    char cond[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    int url_len = 0;
    int err;
    char call_result[CURL_BUFFER_SIZE];
    char vif_id[BUFSIZE];

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(hwdev_id = get_arg(args, "hwdev_id"))) {
        fprintf(stderr, "Error: No hwdev_id pecified\n");
        return -1;
    }
    if (!(if_index = get_arg(args, "if_index"))) {
        fprintf(stderr, "Error: No if_index specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    memset(hwdev_lcuuid, 0, UUID_LENGTH);
    db_select(&hwdev_lcuuid, BUFSIZE, 1, "third_party_device", "lcuuid", cond);
    if (!hwdev_lcuuid[0]) {
        fprintf(stderr, "Error: hwdev with id=%s not exist\n", hwdev_id);
        return -1;
    }
    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_INTF_DELETE, hwdev_lcuuid, if_index);

    err = call_curl_api(url, API_METHOD_DEL, NULL, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "devicetype=%d AND deviceid=%s and ifindex=%s",
        MNTNCT_VIF_DEVICE_TYPE_THIRDHW, hwdev_id, if_index);
    while (1) {
        memset(vif_id, 0, BUFSIZE);
        db_select(&vif_id, BUFSIZE, 1, "vinterface", "id", cond);
        if (vif_id[0] == 0) {
            break;
        }
        printf("Waiting for vif disconnect to complete ... vifid=%s\n", vif_id);
        sleep(5);
    }
    return 0;
}

static int hwdev_remove(int opts, argument *args)
{
    char cond[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    int url_len = 0;
    int err;
    char call_result[CURL_BUFFER_SIZE];
    char *hwdev_id;
    char id[BUFSIZE];
    char hwdev_lcuuid[UUID_LENGTH];

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(hwdev_id = get_arg(args, "id"))) {
        fprintf(stderr, "Error: No related EPC_user with third party specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    memset(hwdev_lcuuid, 0, UUID_LENGTH);
    db_select(&hwdev_lcuuid, BUFSIZE, 1, "third_party_device", "lcuuid", cond);
    if (!hwdev_lcuuid[0]) {
        fprintf(stderr, "Error: hwdev with id=%s not exist\n", hwdev_id);
        return -1;
    }

    url_len += snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX,
            MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    url_len += snprintf(url + url_len, MNTNCT_CMD_LEN - url_len,
                        MNTNCT_API_HWDEV_DELETE, hwdev_lcuuid);

    err = call_curl_api(url, API_METHOD_DEL, NULL, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", hwdev_id);
    while (1) {
        memset(id, 0, BUFSIZE);
        db_select(&id, BUFSIZE, 1, "third_party_device", "id", cond);
        if (id[0] == 0) {
            break;
        }
        printf("Waiting for hwdev delete to complete ... vifid=%s\n", id);
        sleep(5);
    }
    return 0;
}

static int live_x_add(int opts, argument *args)
{
    char *params = "state,name,description,bandwidth,lcuuid";
    char *name = 0;
    char *bandwidth = 0;
    char connector_name[BUFSIZE] = {0};
    char cond[BUFSIZE];
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char lcuuid[BUFSIZE];

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name specified\n");
        return -1;
    }
    if (!(bandwidth = get_arg(args, "bandwidth"))) {
        fprintf(stderr, "Error: No bandwidth specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(connector_name, 0, sizeof connector_name);
    db_select(&connector_name, BUFSIZE, 1, "fdb_template_liveconnector", "name", cond);
    if (connector_name[0]) {
        fprintf(stderr, "Error: live connector with name '%s' already exist\n", name);
        return -1;
    }
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "%d,'%s','%s',%s,'%s'",
            1, name, "", bandwidth, lcuuid);
    db_insert("fdb_template_liveconnector", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "fdb_template_liveconnector", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int live_x_remove(int opts, argument *args)
{
    char *name = 0;
    char cond[BUFSIZE];
    char connector_name[BUFSIZE];

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(connector_name, 0, sizeof connector_name);
    db_select(&connector_name, BUFSIZE, 1, "fdb_template_liveconnector", "name", cond);
    if (!connector_name[0]) {
        fprintf(stderr, "Error: live connector with name '%s' not exist\n", name);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    db_delete("fdb_template_liveconnector", cond);

    return 0;
}

static int live_x_list(int opts, argument *args)
{
    char *default_params = "id,state,name,description,bandwidth,lcuuid";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("fdb_template_liveconnector", "name");
        } else {
            db_dump_minimal("fdb_template_liveconnector", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("fdb_template_liveconnector", params);
    }

    return 0;
}

/* ofs */
static int torswitch_list(int opts, argument *args)
{
    char *default_params = "id,rackid,mip,brand,tunnel_ip,username,password,name";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("network_device", "mip");
        } else {
            db_dump_minimal("network_device", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("network_device", params);
    }

    return 0;
}

static int host_port_torswitch_config(int opts, argument *args)
{
    char *host_ip = 0;
    char *port_name = 0;
    char *ofs_port = 0;
    char *type = 0;
    char *ofs_id = 0;
    char ofs_id_s[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char id_s[BUFSIZE];
    char columns[BUFSIZE];
    int type_int = 0;

    if (!(host_ip = get_arg(args, "host_ip"))) {
        fprintf(stderr, "Error: No host ip specified\n");
        return -1;
    }

    if (!(port_name  = get_arg(args, "port_name"))) {
        fprintf(stderr, "Error: No port name on the host spcified.\n");
        return -1;
    }

    if (!(ofs_port  = get_arg(args, "torswitch_port"))) {
        fprintf(stderr, "Error: No port on the torswitch to connect the host specified\n");
        return -1;
    }

    type = get_arg(args, "port_type");
    if (type) {
        if ( !strcmp(type, "CTRL")) {
            type_int = 0;
        } else if ( !strcmp(type, "DATA")) {
            type_int = 1;
        } else if (!strcmp(type, "DATA_PUBLIC")) {
            type_int = 2;
        }
    }

    if (!(ofs_id = get_arg(args, "torswitch_id"))) {
        fprintf(stderr, "Error: No id of torswitch specified\n");
        return -1;
    }

    snprintf(cond, BUFSIZE, "id='%s'", ofs_id);
    memset(ofs_id_s, 0, sizeof(ofs_id_s));
    db_select(&ofs_id_s, BUFSIZE, 1, "network_device", "id", cond);
    if (!ofs_id_s[0]) {
        fprintf(stderr, "Error: Network device with is '%s' not exist\n", ofs_id);
        return -1;
    }

    /*
    memset(port_list,0,sizeof(port_list));
    snprintf(cond, BUFSIZE, "id='%s'", ofs_id);

    if (0 == type_int){
        sprintf (port_type_s, "%s", CTRL_PLANE);
        db_select(&port_list, BUFSIZE, 1, "network_device",
                   "crl_plane_port_list", cond);
        ret = port_determine_scope(port_list, ofs_port);
        if (ret != 1){
            fprintf(stderr, "Error:Port:%s is beyond the control "\
                    "plane port list scope\n", ofs_port);
            return -1;
        }
    } else if ( 1 == type_int ) {
        sprintf (port_type_s, "%s", DATA_PLANE);
        db_select(&port_list, BUFSIZE, 1, "network_device", "data_plane_port_list", cond);
        ret = port_determine_scope(port_list, ofs_port);
        if (ret != 1) {
            fprintf(stderr, "Error:Port:%s is beyond the data plane port list scope\n", ofs_port);
            return -1;
        }
    } else {
        sprintf (port_type_s, "%s", DATA_PUBLIC);
    }
    */

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_ip='%s' && name='%s'", host_ip, port_name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "host_port_config", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: Host port config info with host_ip '%s'"\
                "and name %s not exists.\n", host_ip,port_name);
        return -1;
    }
    memset(columns, 0, sizeof(columns));
    snprintf(columns, BUFSIZE, "type='%d',torswitch_port='%s',torswitch_id='%s'",
             type_int, ofs_port, ofs_id);
    db_update("host_port_config", columns, cond);

    printf("%s\n", id_s);

    /*
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "module_name='LCPD' && param_name='ctrl_plane_vlan'");
    memset(ctrl_vlanid_s, 0, sizeof ctrl_vlanid_s);
    db_select(&ctrl_vlanid_s, BUFSIZE, 1, "sys_configuration", "value", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: Get ctrl_plane_vlan of sys_configuration "\
                "fail!\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id='%s'", ofs_id);
    memset(ofs_ip_s, 0, sizeof ofs_ip_s);
    db_select(&ofs_ip_s, BUFSIZE, 1, "network_device", "mip", cond);
    if (!ofs_ip_s[0]) {
        fprintf(stderr, "Error: Get mip of network_device fail with %s \n", cond);
        return -1;
    }

    // send info to lcpd for notice mac of port connected to remote ofs switch

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_HOST_PIF_CONFIG;
    msg.hdr.magic  = MSG_MG_UI2KER;
    msg.hdr.length = sizeof(struct msg_host_port_ofs_config_opt);

    msg_host_port_ofs_info = (struct msg_host_port_ofs_config_opt *)&msg.data;
    msg_host_port_ofs_info->host_port_ofs_config_id = atoi (id_s);
    msg_host_port_ofs_info->ctrl_vlan_vid = atoi (ctrl_vlanid_s);
    strcpy (msg_host_port_ofs_info->port_type, port_type_s);
    strcpy (msg_host_port_ofs_info->ofs_ip, ofs_ip_s);
    send_message(&msg);
    */

    return 0;
}

static int host_port_torswitch_remove(int opts, argument *args)
{
    char *host_ip = 0;
    char *port_name = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];

    host_ip = get_arg(args, "host_ip");
    port_name = get_arg(args, "port_name");
    if (!host_ip && !port_name) {
        fprintf(stderr, "Error: No host_ip and port name specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "host_ip='%s'&& name='%s'", host_ip,port_name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "host_port_config", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: Host port config info  host_ip:'%s'  "\
                "and name %s not exist\n", host_ip,port_name);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id_s);
    db_delete("host_port_config", cond);

    return 0;
}

static int host_port_torswitch_list(int opts, argument *args)
{
    char *default_params = "id,type,name,mac,host_ip,torswitch_port,torswitch_id";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("host_port_config", "id");
        } else {
            db_dump_minimal("host_port_config", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("host_port_config", params);
    }

    return 0;
}

static int torswitch_add(int opts, argument *args)
{
    char *params = "name,rackid,mip,tunnel_ip,username,password,enable,brand,lcuuid,domain";
    char *switch_name = 0;
    char *rackid = 0;
    char *mip = 0;
    char *brand = 0;
    char *tunnel_ip;
    char *username;
    char *password;
    char *enable;

    char rack_id[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    char dummy[BUFSIZE];
    char lcuuid[UUID_LENGTH] = {0};

    if (!(switch_name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No switch name specified\n");
        return -1;
    }

    if (!(rackid = get_arg(args, "rackid"))) {
        fprintf(stderr, "Error: No rackid specified\n");
        return -1;
    }

    if (!(brand  = get_arg(args, "switch_brand"))) {
        fprintf(stderr, "Error: No brand specified\n");
        return -1;
    }

    if (!(mip = get_arg(args, "mip"))) {
        fprintf(stderr, "Error: No manage ip specified\n");
        return -1;
    }
    if (!(tunnel_ip = get_arg(args, "tunnel_ip"))) {
        tunnel_ip = "";
    }
    if (!(username = get_arg(args, "username"))) {
        username = "";
    }
    if (!(password = get_arg(args, "password"))) {
        password = "";
    }
    if (!(enable = get_arg(args, "enable"))) {
        enable = "";
    }

    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }

    if (strcmp(rackid, "0") != 0) {
        snprintf(cond, BUFSIZE, "id='%s'", rackid);
        memset(rack_id, 0, sizeof(rack_id));
        db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
        if (!rack_id[0]) {
            fprintf(stderr, "Error: Rack with is '%s' not exist\n", rackid);
            return -1;
        }
        snprintf(cond, BUFSIZE, "id='%s'", rackid);
        memset(domain, 0, sizeof(domain));
        db_select(&domain, BUFSIZE, 1, "rack", "domain", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Rack with is '%s' not exist\n", rackid);
            return -1;
        }
    } else {
        snprintf(domain, sizeof(domain), ANY_LCUUID);
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "mip='%s'", mip);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "network_device", "id", cond);
    if (dummy[0]) {
        fprintf(stderr, "Error: Network device with mip: '%s' already exists\n", mip);
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s','%s','%s','%s','%s','%s','%s','%s','%s'",
             switch_name, rackid, mip, tunnel_ip, username, password, enable, brand,
             lcuuid, domain);
    db_insert("network_device", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "mip='%s'", mip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "network_device", "id", cond);
    printf("%s\n", id_s);

    /*
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "module_name='LCPD' && param_name='ctrl_plane_vlan'");
    memset (value_s, 0, sizeof value_s );
    db_select (&value_s, BUFSIZE, 1, "sys_configuration", "value", cond);

    ofs_ctrl_vlan = atoi ( value_s );

    // send info to lcpd for ofs switch config port info

    memset(&msg, 0, sizeof(msg));
    msg.hdr.type   = LC_MSG_OFS_PORT_CONFIG;
    msg.hdr.magic  = MSG_MG_UI2KER;
    msg.hdr.length = sizeof(struct msg_ofs_port_config_opt);

    msg_ofs_port_info = (struct msg_ofs_port_config_opt *)&msg.data;
    msg_ofs_port_info->uplink_port0 = atoi(uplink_port0);
    msg_ofs_port_info->uplink_port1 = atoi(uplink_port1);
    msg_ofs_port_info->ctrl_vlan_vid = ofs_ctrl_vlan;
    strcpy(msg_ofs_port_info->ofs_ip, mip);
    strcpy(msg_ofs_port_info->ofs_crl_port_list, crl_plane_port_list);
    strcpy(msg_ofs_port_info->ofs_data_port_list, data_plane_port_list);
    send_message(&msg);
    */

    return 0;
}

static int torswitch_remove(int opts, argument *args)
{
    char *id = 0;
    char *mip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];

    id = get_arg(args, "id");
    mip = get_arg(args, "mip");
    if (!id && !mip) {
        fprintf(stderr, "Error: No id or mip specified\n");
        return -1;
    }
    if (mip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "mip='%s'", mip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "network_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Network device mip:'%s' not exist\n", mip);
            return -1;
        }
        if (id && strcmp(id, id_s)) {
            fprintf(stderr, "Error: Network_device id and mip mismatch\n");
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "network_device", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: Network_device with id '%s' not exist\n", id);
            return -1;
        }
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("network_device", cond);

    return 0;
}

static int log_level_set(int opts, argument *args)
{
    char *component = 0;
    char *log_level = 0;

    if (!(log_level = get_arg(args, "log-level"))) {
        fprintf(stderr, "Error: No log level specified\n");
        return -1;
    }
    if (!(component = get_arg(args, "component"))) {
        component = "";
    }

    if (!component[0]) {
        set_log_level("lcmd", log_level);
        set_log_level("vmdriver", log_level);
        set_log_level("lcpd", log_level);
    } else {
        set_log_level(component, log_level);
    }

    return 0;
}

static int nsp_vgateway_upper_limit_config(int opts, argument *args)
{
    char *upper_limit = 0;
    int upper_limit_init = 0;
    char values[BUFSIZE];
    char cond[BUFSIZE];

    if (!(upper_limit = get_arg(args, "upper-limit"))) {
        fprintf(stderr, "Error: No upper limit specified\n");
        return -1;
    }

    upper_limit_init = atoi(upper_limit);
    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "value=%d", upper_limit_init);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "param_name='nsp_vgateway_upper_limit'");
    db_update("sys_configuration", values, cond);
    printf("Success\n");

    return 0;
}

static int user_list(int opts, argument *args)
{
    char *default_params = "id,username,description,email";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("user", "username");
        } else {
            db_dump_minimal("user", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("user", params);
    }

    return 0;
}

static int product_specification_list(int opts, argument *args)
{
    char *default_params = "id,name,plan_name,charge_mode,price,state,lcuuid,domain";
    char *params = get_arg(args, "params");
    char *domain_name = get_arg(args, "domain");
    char cond[BUFSIZE] = {0};
    char domain[BUFSIZE] = {0};

    memset(cond, 0, sizeof cond);
    if (domain_name) {
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        memset(cond, BUFSIZE, sizeof cond);
        snprintf(cond, BUFSIZE, "domain='%s'", domain);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("product_specification", "plan_name");
        } else {
            db_dump_minimal("product_specification", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("product_specification", params, cond);
        } else {
            db_dump("product_specification", params);
        }

        printf("state:\n\t1. online 0. offline\n");
    }

    return 0;
}

#define PRODUCT_TYPE_VM 1
#define PRODUCT_TYPE_VGW 2
#define PRODUCT_TYPE_TPD 3
#define PRODUCT_TYPE_IP 4
#define PRODUCT_TYPE_BW 5
#define PRODUCT_TYPE_WEB_VUL_SCANNER 6
#define PRODUCT_TYPE_SYS_VUL_SCANNER 7
#define PRODUCT_TYPE_NAS_STORAGE 8
#define PRODUCT_TYPE_LB 9
#define PRODUCT_TYPE_CLOUD_DISK 10
#define PRODUCT_TYPE_SNAPSHOT 11

#define VM_MEM_SIZE_MIN 512

#define PRODUCT_SPEC_STATE_OFFLINE 0
#define PRODUCT_SPEC_STATE_ONLINE 1

#define PRODUCT_SPEC_STR "\
计费模板名称：\n\
name:\n\
\n\
计费对象类别：1. 实例 2. 服务\n\
type:\n\
\n\
计费模式：1. 按秒计费 2. 按分计费 3. 按时计费 \
4. 按天计费 5. 按次计费 6. 按时按量计费\n\
charge_mode:\n\
\n\
计费模板单价：\n\
price:\n\
\n"

#define VM_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
CPU核数：\n\
vcpu_num:\n\
\n\
内存大小(M)：\n\
mem_size:\n\
\n\
系统盘大小(G)：\n\
sys_disk_size:\n\
\n\
用户盘大小(G)：\n\
user_disk_size:\n\
\n"

#define VGW_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
公网口数(最多8个公网口)：\n\
wans:\n\
\n\
私网口数(最多24个私网口)：\n\
lans:\n\
\n\
网口速率：1000/10000(Mbps)\n\
rate:\n\
\n"

#define TPD_CONTENT_STR "\
模板描述：\n\
description:\n\
\n"

#define IP_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
ISP id：\n\
isp:\n\
\n"

#define BW_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
ISP id：\n\
isp:\n\
\n"

#define SYS_VUL_SCANNER_CONTENT_STR "\
模板描述：\n\
description:\n\
\n"

#define WEB_VUL_SCANNER_CONTENT_STR "\
模板描述：\n\
description:\n\
\n"

#define NAS_STORAGE_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
服务提供商：\n\
service_vendor:\n\
\n"

#define CLOUD_DISK_CONTENT_STR "\
模板描述：\n\
description:\n\
\n\
服务提供商：\n\
service_vendor:\n\
\n"

#define SNAPSHOT_CONTENT_STR "\
模板描述：\n\
description:\n\
\n"

static int product_specification_add(int opts, argument *args)
{
    char *params = "lcuuid,name,type,charge_mode,price,content,product_type,plan_name";
    char values[BUFSIZE];
    char *type = 0;
    int type_i = 0;
    char tmpfile[BUFSIZE];
    char cmd[BUFSIZE];
    FILE *fp = 0;
    char line[BUFSIZE];
    char *p = 0;
    uint32_t i = 0;
    char *key = 0;
    char *value = 0;
    uint32_t len;
    char lcuuid[UUID_LENGTH];
    char cnf_name[BUFSIZE];
    char cnf_plan_name[BUFSIZE];
    char cnf_type[BUFSIZE];
    int cnf_type_i = 0;
    char cnf_charge_mode[BUFSIZE];
    int cnf_charge_mode_i = 0;
    char cnf_price[BUFSIZE];
    double cnf_price_f = 0;
    char cnf_description[BUFSIZE];
    char cnf_vcpu_num[BUFSIZE];
    int cnf_vcpu_num_i = 0;
    char cnf_mem_size[BUFSIZE];
    int cnf_mem_size_i = 0;
    char cnf_sys_disk_size[BUFSIZE];
    int cnf_sys_disk_size_i = 0;
    char cnf_user_disk_size[BUFSIZE];
    int cnf_user_disk_size_i = 0;
    char cnf_wans[BUFSIZE];
    int cnf_wans_i = 0;
    char cnf_lans[BUFSIZE];
    int cnf_lans_i = 0;
    char cnf_rate[BUFSIZE];
    int cnf_rate_i = 0;
    char cnf_isp[BUFSIZE];
    int cnf_isp_i = 0;
    char cnf_service_vendor[BUFSIZE];
    char content[BUFSIZE];
    char content2[BUFSIZE];
    nx_json *root = 0;
    nx_json *ele = 0;
    nx_json *obj = 0;

    memset(lcuuid, 0, sizeof lcuuid);
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID (check talker)\n");
        return -1;
    }

    if (!(type = get_arg(args, "product-type"))) {
        fprintf(stderr, "Error: no product type specified\n");
        return -1;
    }
    if (strcasecmp(type, "vm") == 0 ||
        strcasecmp(type, "1") == 0) {

        type_i = PRODUCT_TYPE_VM;

    } else if (strcasecmp(type, "vgateway") == 0 ||
               strcasecmp(type, "vgw") == 0 ||
               strcasecmp(type, "2") == 0) {

        type_i = PRODUCT_TYPE_VGW;

    } else if (strcasecmp(type, "thirdpartydevice") == 0 ||
               strcasecmp(type, "third-party-device") == 0 ||
               strcasecmp(type, "third_party_device") == 0 ||
               strcasecmp(type, "tpd") == 0 ||
               strcasecmp(type, "3") == 0) {

        type_i = PRODUCT_TYPE_TPD;

    } else if (strcasecmp(type, "ip") == 0 ||
               strcasecmp(type, "4") == 0) {

        type_i = PRODUCT_TYPE_IP;

    } else if (strcasecmp(type, "bandwidth") == 0 ||
               strcasecmp(type, "bw") == 0 ||
               strcasecmp(type, "5") == 0) {

        type_i = PRODUCT_TYPE_BW;

    } else if (strcasecmp(type, "sys_vul_scanner") == 0 ||
               strcasecmp(type, "6") == 0) {

        type_i = PRODUCT_TYPE_SYS_VUL_SCANNER;

    } else if (strcasecmp(type, "web_vul_scanner") == 0 ||
               strcasecmp(type, "7") == 0) {

        type_i = PRODUCT_TYPE_WEB_VUL_SCANNER;

    } else if (strcasecmp(type, "nas_storage") == 0 ||
               strcasecmp(type, "8") == 0) {

        type_i = PRODUCT_TYPE_NAS_STORAGE;

    } else if (strcasecmp(type, "load_balancer") == 0 ||
               strcasecmp(type, "9") == 0) {

        type_i = PRODUCT_TYPE_CLOUD_DISK;

    } else if (strcasecmp(type, "cloud_disk") == 0 ||
               strcasecmp(type, "10") == 0) {

        type_i = PRODUCT_TYPE_LB;

    } else if (strcasecmp(type, "vm-snapshot") == 0 ||
               strcasecmp(type, "11") == 0) {

        type_i = PRODUCT_TYPE_SNAPSHOT;

    } else {
        fprintf(stderr, "Error: unknown product type %s\n", type);
        return -1;
    }

    memset(tmpfile, 0, sizeof tmpfile);
    snprintf(tmpfile, BUFSIZE, "/tmp/mntnct-%x", getpid());
    fp = fopen(tmpfile, "w");
    if (!fp) {
        fprintf(stderr, "Error: open %s error\n", tmpfile);
        return -1;
    }
    fprintf(fp, PRODUCT_SPEC_STR);
    switch (type_i) {
    case PRODUCT_TYPE_VM:
    case PRODUCT_TYPE_LB:
        fprintf(fp, VM_CONTENT_STR);
        break;
    case PRODUCT_TYPE_VGW:
        fprintf(fp, VGW_CONTENT_STR);
        break;
    case PRODUCT_TYPE_TPD:
        fprintf(fp, TPD_CONTENT_STR);
        break;
    case PRODUCT_TYPE_IP:
        fprintf(fp, IP_CONTENT_STR);
        break;
    case PRODUCT_TYPE_BW:
        fprintf(fp, BW_CONTENT_STR);
        break;
    case PRODUCT_TYPE_SYS_VUL_SCANNER:
        fprintf(fp, SYS_VUL_SCANNER_CONTENT_STR);
        break;
    case PRODUCT_TYPE_WEB_VUL_SCANNER:
        fprintf(fp, WEB_VUL_SCANNER_CONTENT_STR);
        break;
    case PRODUCT_TYPE_NAS_STORAGE:
        fprintf(fp, NAS_STORAGE_CONTENT_STR);
        break;
    case PRODUCT_TYPE_CLOUD_DISK:
        fprintf(fp, CLOUD_DISK_CONTENT_STR);
    case PRODUCT_TYPE_SNAPSHOT:
        fprintf(fp, SNAPSHOT_CONTENT_STR);
    default:
        break;
    }
    fclose(fp);

    memset(cmd, 0, sizeof cmd);
    snprintf(cmd, BUFSIZE, "/usr/bin/vim %s", tmpfile);
    system(cmd);

    fp = fopen(tmpfile, "r");
    if (!fp) {
        fprintf(stderr, "Error: open %s error\n", tmpfile);
        return -1;
    }
    memset(line, 0, sizeof line);
    memset(cnf_name, 0, sizeof cnf_name);
    memset(cnf_plan_name, 0, sizeof cnf_plan_name);
    memset(cnf_type, 0, sizeof cnf_type);
    memset(cnf_charge_mode, 0, sizeof cnf_charge_mode);
    memset(cnf_price, 0, sizeof cnf_price);
    memset(cnf_description, 0, sizeof cnf_description);
    memset(cnf_vcpu_num, 0, sizeof cnf_vcpu_num);
    memset(cnf_mem_size, 0, sizeof cnf_mem_size);
    memset(cnf_sys_disk_size, 0, sizeof cnf_sys_disk_size);
    memset(cnf_user_disk_size, 0, sizeof cnf_user_disk_size);
    memset(cnf_wans, 0, sizeof cnf_wans);
    memset(cnf_lans, 0, sizeof cnf_lans);
    memset(cnf_rate, 0, sizeof cnf_rate);
    memset(cnf_isp, 0, sizeof cnf_isp);
    memset(cnf_service_vendor, 0, sizeof cnf_service_vendor);
    while (fgets(line, BUFSIZE - 1, fp)) {
        /* parse key value */
        len = strlen(line);
        p = line;
        while (*p && isspace(*p)) {
            ++p;
        }
        key = p;
        while (*p && *p != ':') {
            ++p;
        }
        if (!*p || !*(p+1)) {
            continue;
        }
        value = p + 1;
        --p;
        while (*p && isspace(*p)) {
            --p;
        }
        *(p + 1) = 0;
        while (*value && isspace(*value)) {
            ++value;
        }
        p = line + len - 1;
        while (*p && isspace(*p)) {
            --p;
        }
        *(p + 1) = 0;

        if (strcmp(key, "name") == 0) {
            strncpy(cnf_name, value, BUFSIZE - 1);
            strncpy(cnf_plan_name, value, BUFSIZE - 1);
        } else if (strcmp(key, "type") == 0) {
            strncpy(cnf_type, value, BUFSIZE - 1);
        } else if (strcmp(key, "charge_mode") == 0) {
            strncpy(cnf_charge_mode, value, BUFSIZE - 1);
        } else if (strcmp(key, "price") == 0) {
            strncpy(cnf_price, value, BUFSIZE - 1);
        } else if (strcmp(key, "description") == 0) {
            strncpy(cnf_description, value, BUFSIZE - 1);
        } else if (strcmp(key, "vcpu_num") == 0) {
            strncpy(cnf_vcpu_num, value, BUFSIZE - 1);
        } else if (strcmp(key, "mem_size") == 0) {
            strncpy(cnf_mem_size, value, BUFSIZE - 1);
        } else if (strcmp(key, "sys_disk_size") == 0) {
            strncpy(cnf_sys_disk_size, value, BUFSIZE - 1);
        } else if (strcmp(key, "user_disk_size") == 0) {
            strncpy(cnf_user_disk_size, value, BUFSIZE - 1);
        } else if (strcmp(key, "wans") == 0) {
            strncpy(cnf_wans, value, BUFSIZE - 1);
        } else if (strcmp(key, "lans") == 0) {
            strncpy(cnf_lans, value, BUFSIZE - 1);
        } else if (strcmp(key, "rate") == 0) {
            strncpy(cnf_rate, value, BUFSIZE - 1);
        } else if (strcmp(key, "isp") == 0) {
            strncpy(cnf_isp, value, BUFSIZE - 1);
        } else if (strcmp(key, "service_vendor") == 0) {
            strncpy(cnf_service_vendor, value, BUFSIZE - 1);
        }

        memset(line, 0, sizeof line);
    }
    fclose(fp);

    /* validation */
    if (!cnf_name[0]) {
        fprintf(stderr, "Error: name not specified\n");
        return -1;
    }
    if (!cnf_type[0]) {
        fprintf(stderr, "Error: type not specified\n");
        return -1;
    }
    cnf_type_i = atoi(cnf_type);
    if (cnf_type_i != 1 && cnf_type_i != 2) {
        fprintf(stderr, "Error: unknown type %s\n", cnf_type);
        return -1;
    }
    if (!cnf_charge_mode[0]) {
        fprintf(stderr, "Error: charge mode not specified\n");
        return -1;
    }
    cnf_charge_mode_i = atoi(cnf_charge_mode);
    if (cnf_charge_mode_i < 1 || cnf_charge_mode_i > 6) {
        fprintf(stderr, "Error: unknown charge mode %s\n", cnf_charge_mode);
        return -1;
    }
    if (!cnf_price[0]) {
        fprintf(stderr, "Error: price not specified\n");
        return -1;
    }
    cnf_price_f = atof(cnf_price);
    if (cnf_price_f < 0) {
        fprintf(stderr, "Error: invalid price %s\n", cnf_price);
        return -1;
    }
    memset(cnf_price, 0, sizeof cnf_price);
    snprintf(cnf_price, BUFSIZE, "%.2f", cnf_price_f);

    switch (type_i) {
    case PRODUCT_TYPE_VM:
    case PRODUCT_TYPE_LB:
        if (!cnf_vcpu_num[0]) {
            fprintf(stderr, "Error: cpu number not specified\n");
            return -1;
        }
        cnf_vcpu_num_i = atoi(cnf_vcpu_num);
        if (cnf_vcpu_num_i < 1) {
            fprintf(stderr, "Error: invalid cpu number %s\n", cnf_vcpu_num);
            return -1;
        }
        if (!cnf_mem_size[0]) {
            fprintf(stderr, "Error: memory size not specified\n");
            return -1;
        }
        cnf_mem_size_i = atoi(cnf_mem_size);
        if (cnf_mem_size_i < 1) {
            fprintf(stderr, "Error: invalid memory size %s\n", cnf_mem_size);
            return -1;
        }
        if (cnf_mem_size_i < VM_MEM_SIZE_MIN) {
            fprintf(stderr, "Error: invalid memory size %s, the minimum memory size is %u\n",
                    cnf_mem_size, VM_MEM_SIZE_MIN);
            return -1;
        }
        if (0 != (cnf_mem_size_i % VM_MEM_SIZE_MIN)) {
            fprintf(stderr, "Error: invalid memory size %s, must be a multiple of %u\n",
                    cnf_mem_size, VM_MEM_SIZE_MIN);
            return -1;
        }
        if (!cnf_sys_disk_size[0]) {
            fprintf(stderr, "Error: system disk size not specified\n");
            return -1;
        }
        cnf_sys_disk_size_i = atoi(cnf_sys_disk_size);
        if (cnf_sys_disk_size_i < 0) {
            fprintf(stderr, "Error: invalid system disk size %s\n",
                    cnf_sys_disk_size);
            return -1;
        }
        if (!cnf_user_disk_size[0]) {
            fprintf(stderr, "Error: user disk size not specified\n");
            return -1;
        }
        cnf_user_disk_size_i = atoi(cnf_user_disk_size);
        if (cnf_user_disk_size_i < 0) {
            fprintf(stderr, "Error: invalid user disk size %s\n",
                    cnf_user_disk_size);
            return -1;
        }
        break;
    case PRODUCT_TYPE_VGW:
        if (!cnf_wans[0]) {
            fprintf(stderr, "Error: WAN number not specified\n");
            return -1;
        }
        cnf_wans_i = atoi(cnf_wans);
        if ((cnf_wans_i < 0) || (cnf_wans_i > 8)) {
            fprintf(stderr, "Error: invalid WAN number %s. "
                    "number of WANs must in [1,8]\n", cnf_wans);
            return -1;
        }
        if (!cnf_lans[0]) {
            fprintf(stderr, "Error: LAN number not specified\n");
            return -1;
        }
        cnf_lans_i = atoi(cnf_lans);
        if ((cnf_lans_i < 0) || (cnf_lans_i > 24)) {
            fprintf(stderr, "Error: invalid LAN number %s."
                    "number of LANs must in [1,24]\n", cnf_lans);
            return -1;
        }
        if (!cnf_rate[0]) {
            fprintf(stderr, "Error: interface speed not specified\n");
            return -1;
        }
        cnf_rate_i = atoi(cnf_rate);
        if (cnf_rate_i != 1000 && cnf_rate_i != 10000) {
            fprintf(stderr, "Error: invalid interface speed %s\n", cnf_rate);
            return -1;
        }
        break;
    case PRODUCT_TYPE_TPD:
        break;
    case PRODUCT_TYPE_IP:
        if (!cnf_isp[0]) {
            fprintf(stderr, "Error: ISP not specified\n");
            return -1;
        }
        cnf_isp_i = atoi(cnf_isp);
        if (cnf_isp_i < 1 || cnf_isp_i > 8) {
            fprintf(stderr, "Error: invalid ISP %s\n", cnf_isp);
            return -1;
        }
        break;
    case PRODUCT_TYPE_BW:
        if (!cnf_isp[0]) {
            fprintf(stderr, "Error: ISP not specified\n");
            return -1;
        }
        cnf_isp_i = atoi(cnf_isp);
        if (cnf_isp_i < 1 || cnf_isp_i > 8) {
            fprintf(stderr, "Error: invalid ISP %s\n", cnf_isp);
            return -1;
        }
        break;
    case PRODUCT_TYPE_NAS_STORAGE:
        if (!cnf_service_vendor[0]) {
            fprintf(stderr, "Error: service vendor not specified\n");
            return -1;
        }
        break;
    case PRODUCT_TYPE_CLOUD_DISK:
        if (!cnf_service_vendor[0]) {
            fprintf(stderr, "Error: service vendor not specified\n");
            return -1;
        }
        break;
    default:
        break;
    }

    /* make json */
    root = calloc(1, sizeof(nx_json));
    if (!root) {
        fprintf(stderr, "Error: malloc() failed\n");
        return -1;
    }
    root->type = NX_JSON_OBJECT;
    switch (type_i) {
    case PRODUCT_TYPE_VM:
    case PRODUCT_TYPE_LB:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;

        obj = create_json(NX_JSON_OBJECT, "compute_size", root);
        ele = create_json(NX_JSON_INTEGER, "vcpu_num", obj);
        ele->int_value = cnf_vcpu_num_i;
        ele = create_json(NX_JSON_INTEGER, "mem_size", obj);
        ele->int_value = cnf_mem_size_i;
        ele = create_json(NX_JSON_INTEGER, "sys_disk_size", obj);
        ele->int_value = cnf_sys_disk_size_i;

        ele = create_json(NX_JSON_INTEGER, "user_disk_size", root);
        ele->int_value = cnf_user_disk_size_i;
        break;
    case PRODUCT_TYPE_VGW:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;

        obj = create_json(NX_JSON_OBJECT, "vgateway_info", root);
        ele = create_json(NX_JSON_INTEGER, "wans", obj);
        ele->int_value = cnf_wans_i;
        ele = create_json(NX_JSON_INTEGER, "lans", obj);
        ele->int_value = cnf_lans_i;
        ele = create_json(NX_JSON_INTEGER, "rate", obj);
        ele->int_value = cnf_rate_i;
        break;
    case PRODUCT_TYPE_TPD:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        break;
    case PRODUCT_TYPE_IP:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        ele = create_json(NX_JSON_INTEGER, "isp", root);
        ele->int_value = cnf_isp_i;
        break;
    case PRODUCT_TYPE_BW:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        ele = create_json(NX_JSON_INTEGER, "isp", root);
        ele->int_value = cnf_isp_i;
        break;
    case PRODUCT_TYPE_SYS_VUL_SCANNER:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        break;
    case PRODUCT_TYPE_WEB_VUL_SCANNER:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        break;
    case PRODUCT_TYPE_NAS_STORAGE:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        ele = create_json(NX_JSON_STRING, "service_vendor", root);
        ele->text_value = cnf_service_vendor;
        break;
    case PRODUCT_TYPE_CLOUD_DISK:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        ele = create_json(NX_JSON_STRING, "service_vendor", root);
        ele->text_value = cnf_service_vendor;
        break;
    case PRODUCT_TYPE_SNAPSHOT:
        ele = create_json(NX_JSON_STRING, "description", root);
        ele->text_value = cnf_description;
        break;
    default:
        break;
    }

    dump_json_msg(root, content, BUFSIZE);
    nx_json_free(root);

    /* escape quote in content */
    memset(content2, 0, sizeof content2);
    p = content;
    i = 0;
    while (*p && i < BUFSIZE - 1) {
        if (*p == '\"') {
            if (i + 2 > BUFSIZE) {
                fprintf(stderr, "Error: buffer too small\n");
                return -1;
            }
            content2[i] = '\\';
            content2[i+1] = *p;
            ++p;
            i += 2;
        } else {
            content2[i++] = *p++;
        }
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s',%s,%s,%s,'%s',%d,'%s'",
             lcuuid, cnf_name, cnf_type, cnf_charge_mode, cnf_price,
             content2, type_i, cnf_plan_name);
    db_insert("product_specification", params, values);

    return 0;
}

static int product_specification_remove(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char *lcuuid = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    lcuuid = get_arg(args, "lcuuid");
    if (!id && !name && !lcuuid) {
        fprintf(stderr, "Error: No id, name label or lcuuid specified\n");
        return -1;
    }
    if (lcuuid) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: product spec '%s' not exist\n", lcuuid);
            return -1;
        }
        id = id_s;
    } else if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: product spec '%s' not exist\n", name);
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr,
                    "Error: product spec with id '%s' not exist\n", id);
            return -1;
        }
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", id);
    db_delete("product_specification", cond);

    return 0;
}

static int product_specification_update(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char *lcuuid = 0;
    char *price = 0;
    char *state = 0;
    int state_i = 0;
    char cond[BUFSIZE];
    char values[BUFSIZE] = {0};
    char id_s[BUFSIZE];

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    lcuuid = get_arg(args, "lcuuid");
    if (!id && !name && !lcuuid) {
        fprintf(stderr, "Error: No id, name label or lcuuid specified\n");
        return -1;
    }
    if (lcuuid) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: product spec '%s' not exist\n", lcuuid);
            return -1;
        }
        id = id_s;
    } else if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: product spec '%s' not exist\n", name);
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "product_specification", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr,
                    "Error: product spec with id '%s' not exist\n", id);
            return -1;
        }
    }

    price = get_arg(args, "price");
    if (price) {
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "price=%s", price);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("product_specification", values, cond);
    }
    state = get_arg(args, "state");
    if (state) {
        if (strcasecmp(state, "offline") == 0 ||
            strcasecmp(state, "0") == 0) {

            state_i = PRODUCT_SPEC_STATE_OFFLINE;
        } else if (strcasecmp(state, "online") == 0 ||
                   strcasecmp(state, "1") == 0) {

            state_i = PRODUCT_SPEC_STATE_ONLINE;
        } else {

            fprintf(stderr, "Error: Unknown state %s\n", state);
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "state=%d", state_i);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("product_specification", values, cond);
    }

    return 0;
}

static int license_generate(int opts, argument *args)
{
    char *user_name       = NULL;
    char *rack_serial_num = NULL;
    char *server_num      = NULL;
    char *activation_time = NULL;
    char *lease           = NULL;
    int   err             = 0;
    int   year            = 0;
    int   month           = 0;
    int   day             = 0;
    char  local_time[MNTNCT_CMD_RESULT_LEN];
    char  expire_time[MNTNCT_CMD_RESULT_LEN];
    char  values[BUFSIZE];
    char  license[BUFSIZE];

    if (!(user_name = get_arg(args, "user_name"))) {
        fprintf(stderr, "Error: No user name specified\n");
        return -1;
    } else {
        if (LICENSE_UER_NAME_LEN < strlen(user_name)) {
            fprintf(stderr, "Error: User name len out of range\n");
            return -1;
        }
    }

    if (!(rack_serial_num = get_arg(args, "rack_serial_num"))) {
        fprintf(stderr, "Error: No rack serial num specified\n");
        return -1;
    }

    if (!(server_num = get_arg(args, "server_num"))) {
        fprintf(stderr, "Error: No server num specified\n");
        return -1;
    }

    if (!(activation_time = get_arg(args, "activation_time"))) {
        memset(local_time, 0, MNTNCT_CMD_RESULT_LEN);
        err = call_system_output("date +'%F'",
                                 local_time, MNTNCT_CMD_RESULT_LEN);
        chomp(local_time);
        if (err || !local_time[0]) {
            fprintf(stderr, "Error: get activation time failed\n");
            return -1;
        } else {
            activation_time = local_time;
        }
    }

    if (!(lease = get_arg(args, "lease"))) {
        fprintf(stderr, "Error: No lease specified\n");
        return -1;
    }

    /* Get expiry date */
    sscanf(activation_time, "%d-%d-%d", &year, &month, &day);
    day = LICENSE_FIRST_DAY_OF_MON;
    month += (atoi(lease) + 1);
    if (0 < (month / LICENSE_MON_NUM)) {
        year  += month / LICENSE_MON_NUM;
        month %= LICENSE_MON_NUM;
    }
    memset(expire_time, 0, sizeof expire_time);
    snprintf(expire_time, BUFSIZE, "%04d-%02d-%02d", year, month, day);
    if (!expire_time[0]) {
        fprintf(stderr, "Error: calc expire date failed\n");
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "%s#%s#%s#%s#%s#%s#",
             user_name, rack_serial_num, server_num,
             activation_time, lease, expire_time);
    memset(license, 0, sizeof license);
    if (aes_cbc_128_encrypt(license, values)) {
        fprintf(stderr, "Error: License generate failed\n");
        return -1;
    }
    str_replace(license, '-', '\n');
    printf("%s\n", license);

    return 0;
}

static int pack_domain_patch(char *buf, int buf_len,
                             char *param_name,
                             char *value)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_PARAM_NAME, root);
    tmp->text_value = param_name;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_VALUE, root);
    tmp->text_value = value;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int tunnel_protocol_config(int opts, argument *args)
{
    char *type = 0;
    char *domain_name = 0;
    char cond[BUFSIZE] = {0};
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;

    if (!(type = get_arg(args, "type"))) {
        fprintf(stderr, "Error: No type specified\n");
        return -1;
    }

    if (strcmp(type, "GRE") && strcmp(type, "VXLAN")) {
        fprintf(stderr, "Error: Type '%s' invalid\n", type);
        return -1;
    }

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_PATCH, domain);
    strcat(url, url2);
    err = pack_domain_patch(post, MNTNCT_CMD_LEN, "tunnel_protocol", type);
    if (err) {
        fprintf(stderr, "Error: Pack domain patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    printf("Success\n");
    return 0;
}

static int tunnel_protocol_list(int opts, argument *args)
{
    char *default_params = "id,param_name,value,domain";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE] = {0};
    char *domain_name = get_arg(args, "domain");
    char domain[UUID_LENGTH] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        snprintf(cond, BUFSIZE, "param_name='tunnel_protocol' AND domain='%s'", domain);
    } else {
        snprintf(cond, BUFSIZE, "param_name='tunnel_protocol'");
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond_minimal("domain_configuration", "value", cond);
        } else {
            db_dump_cond_minimal("domain_configuration", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("domain_configuration", params, cond);
    }

    return 0;
}

static int service_charge_add(int opts, argument *args)
{
    char *service_type = 0;
    char *instance_type = 0;
    char *instance_name = 0;
    char *product_spec_name = 0;
    char *epc_user = 0;
    char *start_time = 0;
    char *end_time = 0;
    int   year = 0;
    int   month = 0;
    int   day = 0;
    int   hour = 0;
    int  service_type_int = 0;
    int  instance_type_int = 0;
    char user_id[BUFSIZE] = {0};
    char instance_lcuuid[BUFSIZE] = {0};
    char product_spec_lcuuid[BUFSIZE] = {0};
    char start_time_char[BUFSIZE] = {0};
    char end_time_char[BUFSIZE] = {0};
    char lcuuid[UUID_LENGTH] = {0};
    char cond[BUFSIZE] = {0};
    char key_values[BUFSIZE] = {0};

    if (!(service_type = get_arg(args, "service_type"))) {
        fprintf(stderr, "Error: No service_type specified\n");
        return -1;
    }
    if (strcmp(service_type, "Vul_Scanner")) {
        fprintf(stderr, "Error: service_type '%s' invalid\n", service_type);
        return -1;
    }
    service_type_int = SERVICE_VUL_SCANNER;
    if (!(instance_type = get_arg(args, "instance_type"))) {
        fprintf(stderr, "Error: No instance_type specified\n");
        return -1;
    }
    if (strcmp(instance_type, "VM")) {
        fprintf(stderr, "Error: instance_type '%s' invalid\n", instance_type);
        return -1;
    }
    instance_type_int = SERVICE_INSTANCE_TYPE;
    if (!(instance_name = get_arg(args, "instance_name"))) {
        fprintf(stderr, "Error: No instance_name specified\n");
        return -1;
    }
    if (!(product_spec_name = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    if (!(epc_user = get_arg(args, "EPC_user"))) {
        fprintf(stderr, "Error: No EPC_user specified\n");
        return -1;
    }

    if (!(start_time = get_arg(args, "start_time"))) {
        memset(start_time_char, 0, sizeof start_time_char);
    } else {
        sscanf(start_time, "%d-%d-%d-%d", &year, &month, &day, &hour);
        if (0 == year || 0 == month || 0 == day || 0 == hour) {
            fprintf(stderr, "Error: start_time format error\n");
            return -1;
        }
        memset(start_time_char, 0, sizeof start_time_char);
        snprintf(start_time_char, BUFSIZE, "'%04d-%02d-%02d %02d:00:00'",
                 year, month, day, hour);
    }

    if (!(end_time = get_arg(args, "end_time"))) {
        memset(end_time_char, 0, sizeof end_time_char);
    } else {
        year = month = day = hour = 0;
        sscanf(end_time, "%d-%d-%d-%d", &year, &month, &day, &hour);
        if (0 == year || 0 == month || 0 == day || 0 == hour) {
            fprintf(stderr, "Error: end_time format error\n");
            return -1;
        }
        memset(end_time_char, 0, sizeof end_time_char);
        snprintf(end_time_char, BUFSIZE, "'%04d-%02d-%02d %02d:00:00'",
                 year, month, day, hour);
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "username='%s'", epc_user);
    memset(user_id, 0, sizeof user_id);
    db_select(&user_id, BUFSIZE, 1, "user", "id", cond);
    if (!user_id[0]) {
        fprintf(stderr, "Error: EPC_user with name '%s' not exist\n", epc_user);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "userid=%s and name='%s'", user_id, instance_name);
    memset(instance_lcuuid, 0, sizeof instance_lcuuid);
    db_select(&instance_lcuuid, BUFSIZE, 1, "vm", "lcuuid", cond);
    if (!instance_lcuuid[0]) {
        fprintf(stderr, "Error: instance with name '%s' not exist\n", instance_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s'", product_spec_name);
    memset(product_spec_lcuuid, 0, sizeof product_spec_lcuuid);
    db_select(&product_spec_lcuuid, BUFSIZE, 1,
              "product_specification", "lcuuid", cond);
    if (!product_spec_lcuuid[0]) {
        fprintf(stderr, "Error: product_specification with name '%s' not exist\n",
                product_spec_name);
        return -1;
    }
    if (!generate_uuid(lcuuid, UUID_LENGTH)) {
        fprintf(stderr, "Error: failed to generate LCUUID\n");
        return -1;
    }
    memset(key_values, 0, sizeof key_values);
    snprintf(key_values, BUFSIZE,
            "type:%d,instance_type:%d,instance_name:'%s',instance_lcuuid:'%s',"
            "product_specification_lcuuid:'%s',cost:'0',userid:%s,lcuuid:'%s',"
            "start_time:new Date(%s),end_time:new Date(%s)",
            service_type_int, instance_type_int, instance_name, instance_lcuuid,
            product_spec_lcuuid, user_id, lcuuid, start_time_char, end_time_char);
    mongo_insert("charge_detail", key_values);
    printf("%s\n", lcuuid);

    return 0;
}

static int service_charge_del(int opts, argument *args)
{
    char *service_type = 0;
    char *instance_type = 0;
    char *instance_name = 0;
    char *product_spec_name = 0;
    char *epc_user = 0;
    char *start_time = 0;
    int   year = 0;
    int   month = 0;
    int   day = 0;
    int   hour = 0;
    int  service_type_int = 0;
    int  instance_type_int = 0;
    char user_id[BUFSIZE] = {0};
    char instance_lcuuid[BUFSIZE] = {0};
    char product_spec_lcuuid[BUFSIZE] = {0};
    char start_time_char[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char sort[BUFSIZE] = {0};

    if (!(service_type = get_arg(args, "service_type"))) {
        fprintf(stderr, "Error: No service_type specified\n");
        return -1;
    }
    if (strcmp(service_type, "Vul_Scanner")) {
        fprintf(stderr, "Error: service_type '%s' invalid\n", service_type);
        return -1;
    }
    service_type_int = SERVICE_VUL_SCANNER;
    if (!(instance_type = get_arg(args, "instance_type"))) {
        fprintf(stderr, "Error: No instance_type specified\n");
        return -1;
    }
    if (strcmp(instance_type, "VM")) {
        fprintf(stderr, "Error: instance_type '%s' invalid\n", instance_type);
        return -1;
    }
    instance_type_int = SERVICE_INSTANCE_TYPE;
    if (!(instance_name = get_arg(args, "instance_name"))) {
        fprintf(stderr, "Error: No instance_name specified\n");
        return -1;
    }
    if (!(product_spec_name = get_arg(args, "product_specification"))) {
        fprintf(stderr, "Error: No product_specification specified\n");
        return -1;
    }
    if (!(epc_user = get_arg(args, "EPC_user"))) {
        fprintf(stderr, "Error: No EPC_user specified\n");
        return -1;
    }

    if (!(start_time = get_arg(args, "start_time"))) {
        memset(start_time_char, 0, sizeof start_time_char);
    } else {
        sscanf(start_time, "%d-%d-%d-%d", &year, &month, &day, &hour);
        if (0 == year || 0 == month || 0 == day || 0 == hour) {
            fprintf(stderr, "Error: start_time format error\n");
            return -1;
        }
        memset(start_time_char, 0, sizeof start_time_char);
        snprintf(start_time_char, BUFSIZE, "'%04d-%02d-%02d %02d:00:00'",
                 year, month, day, hour);
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "username='%s'", epc_user);
    memset(user_id, 0, sizeof user_id);
    db_select(&user_id, BUFSIZE, 1, "user", "id", cond);
    if (!user_id[0]) {
        fprintf(stderr, "Error: EPC_user with name '%s' not exist\n", epc_user);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "userid=%s and name='%s'", user_id, instance_name);
    memset(instance_lcuuid, 0, sizeof instance_lcuuid);
    db_select(&instance_lcuuid, BUFSIZE, 1, "vm", "lcuuid", cond);
    if (!instance_lcuuid[0]) {
        fprintf(stderr, "Error: instance with name '%s' not exist\n", instance_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "plan_name='%s'", product_spec_name);
    memset(product_spec_lcuuid, 0, sizeof product_spec_lcuuid);
    db_select(&product_spec_lcuuid, BUFSIZE, 1,
              "product_specification", "lcuuid", cond);
    if (!product_spec_lcuuid[0]) {
        fprintf(stderr, "Error: product_specification with name '%s' not exist\n",
                product_spec_name);
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE,
            "type:%d,instance_type:%d,instance_name:'%s',instance_lcuuid:'%s',"
            "product_specification_lcuuid:'%s',userid:%s,"
            "start_time:{\\$lte:new Date(%s)}",
            service_type_int, instance_type_int, instance_name, instance_lcuuid,
            product_spec_lcuuid, user_id, start_time_char);
    memset(sort, 0, sizeof sort);
    snprintf(sort, BUFSIZE, "start_time:-1");
    mongo_delete_select("charge_detail", cond, sort);

    return 0;
}

static int check_vlantag_ranges(char const * const ranges, int rackid)
{
    char *p = NULL;
    char *q = NULL;
    char *r = NULL;
    char cond[BIG_BUFSIZE] = {0};
    char vlantag[BUFSIZE] = {0};
    int min, max;
    int len = 0;

    if (NULL == ranges) {
        return -1;
    }

    r = strdup(ranges);
    if (NULL == r) {
        return -1;
    }

    memset(cond, 0, sizeof cond);
    len = snprintf(cond, BIG_BUFSIZE, "(rackid=%d or rackid=0) ", rackid);
    p = strtok(r, ",");
    while(p) {
        memset(vlantag, 0, sizeof vlantag);
        q = strchr(p, '-');
        if (q) {
            if (sscanf(p, "%d-%d", &min, &max) <= 1) {
                return -1;
            }
            if (min < 0 || max < 0 || min >= max) {
                return -1;
            }
            len += snprintf(cond + len, BIG_BUFSIZE - len, "and (vlantag<%d or vlantag>%d)", min, max);
        } else {
            if (sscanf(p, "%d", &min) < 1) {
                return -1;
            }
            if (min < 0) {
                return -1;
            }
            len += snprintf(cond + len, BIG_BUFSIZE - len, "and vlantag!=%d", min);
        }
        p = strtok(NULL, ",");
    }

    db_select(&vlantag, BUFSIZE, 1, "vl2_vlan", "vlantag", cond);
    if (vlantag[0]) {
        fprintf(stderr, "Error: Already contains vlantag is not within ranges %s\n", ranges);
        db_dump_cond("vl2_vlan", "*", cond);
        return -1;
    }

    return 0;
}

static int vlantag_ranges_config(int opts, argument *args)
{
    char *params = "rack,ranges";
    char *rack_name = 0;
    char *ranges = 0;
    char rack_id[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char values[BUFSIZE] = {0};
    char old_ranges[BUFSIZE] = {0};
    char *p, *q;
    int ret = 0;

    if (!(rack_name = get_arg(args, "rack_name"))) {
        fprintf(stderr, "Error: No rack name specified\n");
        return -1;
    } else {
        if (strlen(rack_name) > NAME_LENGTH) {
            fprintf(stderr, "Error: Illegal rack name\n");
            return -1;
        }
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", rack_name);
        memset(rack_id, 0, sizeof rack_id);
        db_select(&rack_id, BUFSIZE, 1, "rack", "id", cond);
        if (!rack_id[0]) {
            fprintf(stderr, "Error: rack '%s' can not found\n", rack_name);
            return -1;
        }
    }

    if (!(ranges = get_arg(args, "ranges"))) {
        fprintf(stderr, "Error: No vlantag ranges specified\n");
        return -1;
    }

    p = ranges;
    q = p;
    while (*p) {
        if (!isdigit(*p) && *p != '-' && *p != ',') {
            ++p;
            continue;
        }
        *q++ = *p++;
    }
    *q = 0;

    ret = check_vlantag_ranges(ranges, atoi(rack_id));
    if (ret) {
        fprintf(stderr, "Error: Invalid vlantag ranges specified\n");
        return -1;
    }

    snprintf(cond, BUFSIZE, "rack=%s", rack_id);
    db_select(&old_ranges, BUFSIZE, 1, "vlan", "ranges", cond);
    if (old_ranges[0]) {
        snprintf(values, BUFSIZE, "ranges='%s'", ranges);
        db_update("vlan", values, cond);
    } else {
        snprintf(values, BUFSIZE,
                "%s,'%s'", rack_id, ranges);
        db_insert("vlan", params, values);
    }
    return 0;
}

static int pack_vlan_curl_patch(char *buf, int buf_len,
                                char *vl2_lcuuid,
                                char *rack_lcuuid,
                                int vlantag)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_VLAN_VL2_LCUUID, root);
    tmp->text_value = vl2_lcuuid;

    if (rack_lcuuid && rack_lcuuid[0]) {
        tmp = create_json(NX_JSON_STRING, MNTNCT_API_VLAN_RACK_LCUUID, root);
        tmp->text_value = rack_lcuuid;
    }

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_IP_VLANTAG, root);
    tmp->int_value = vlantag;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int vlan_modify(int opts, argument *args)
{
    char *rack_name = NULL;
    char *vl2id = NULL;
    char *vlantag = NULL;
    char rack_lcuuid[BUFSIZE] = {0};
    char vl2_lcuuid[BUFSIZE] = {0};
    char cond[BUFSIZE] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    char state[BUFSIZE];
    int err = 0;

    if (!(vl2id = get_arg(args, "vl2id"))) {
        fprintf(stderr, "Error: No vl2id specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", vl2id);
    memset(vl2_lcuuid, 0, sizeof vl2_lcuuid);
    db_select(&vl2_lcuuid, BUFSIZE, 1, "vl2", "lcuuid", cond);
    if (!vl2_lcuuid[0]) {
        fprintf(stderr, "Error: vl2 %s can not found\n", vl2id);
        return -1;
    }

    if ((rack_name = get_arg(args, "rack_name")) != NULL) {
        if (strlen(rack_name) > NAME_LENGTH) {
            fprintf(stderr, "Error: Illegal rack name\n");
            return -1;
        }
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", rack_name);
        memset(rack_lcuuid, 0, sizeof rack_lcuuid);
        db_select(&rack_lcuuid, BUFSIZE, 1, "rack", "lcuuid", cond);
        if (!rack_lcuuid[0]) {
            fprintf(stderr, "Error: rack '%s' can not found\n", rack_name);
            return -1;
        }
    }

    if (!(vlantag = get_arg(args, "vlantag"))) {
        fprintf(stderr, "Error: No vlantag specified\n");
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_VLAN_PATCH);
    strcat(url, url2);
    err = pack_vlan_curl_patch(post, MNTNCT_CMD_LEN, vl2_lcuuid,
            rack_lcuuid, atoi(vlantag));
    if (err) {
        fprintf(stderr, "Error: Pack vlan patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "id=%s", vl2id);
    while (1) {
        memset(state, 0, sizeof(state));
        db_select(&state, BUFSIZE, 1, "vl2", "state", cond);
        if (atoi(state) == MNTNCT_VL2_STATE_FINISH) {
            break;
        }
        if (atoi(state) == MNTNCT_VL2_STATE_ABNORMAL) {
            fprintf(stderr, "Error: vlan modify failed\n");
            return -1;
        }
        printf("Waiting for vlan to complete ...\n");
        sleep(5);
    }

    return 0;
}

static int pack_ctrl_ip_post(char *buf, int buf_len,
                             char *type,
                             char *ip_min,
                             char *ip_max,
                             int netmask,
                             char *domain)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_CTRL_IP_TYPE, root);
    tmp->text_value = type;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_CTRL_IP_IP_MIN, root);
    tmp->text_value = ip_min;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_CTRL_IP_IP_MAX, root);
    tmp->text_value = ip_max;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_CTRL_IP_NETMASK, root);
    tmp->int_value = netmask;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_CTRL_IP_DOMAIN, root);
    tmp->text_value = domain;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int ip_ranges_config(int opts, argument *args)
{
    char *type = NULL;
    char *ip_min = NULL;
    char *ip_max = NULL;
    char *netmask = NULL;
    char *domain_name = 0;
    char cond[BUFSIZE] = {0};
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;
    int masklen;

    if (!(type = get_arg(args, "type"))) {
        fprintf(stderr, "Error: No type specified\n");
        return -1;
    }

    if (!(ip_min = get_arg(args, "ip_min"))) {
        fprintf(stderr, "Error: No ip_min specified\n");
        return -1;
    }

    if (!(ip_max = get_arg(args, "ip_max"))) {
        fprintf(stderr, "Error: No ip_max specified\n");
        return -1;
    }

    if (!(netmask = get_arg(args, "netmask"))) {
        fprintf(stderr, "Error: No netmask specified\n");
        return -1;
    } else {
        if (strlen(netmask) <= 2) {
            masklen = atoi(netmask);
        } else if (check_netmask(netmask, &masklen)) {
            fprintf(stderr, "Error: Netmask '%s' invalid\n", netmask);
            return -1;
        }
        if (masklen < 0 || masklen > 32) {
            fprintf(stderr, "Error: Netmask '%s' invalid\n", netmask);
            return -1;
        }
    }

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_CTRL_IP_POST);
    strcat(url, url2);
    err = pack_ctrl_ip_post(post, MNTNCT_CMD_LEN, type,
            ip_min, ip_max, masklen, domain);
    if (err) {
        fprintf(stderr, "Error: Pack ctrl ip post curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_POST, post, NULL);
    if (err) {
        return -1;
    }

    return 0;
}

static int vul_scanner_config(int opts, argument *args)
{
    char *id = 0;
    char *task_id = 0;
    char *service_vendor_ip = 0;
    char id_s[BUFSIZE];
    char cond[BUFSIZE] = {0};
    char value[BUFSIZE] = {0};

    if (!(id = get_arg(args, "id"))) {
        fprintf(stderr, "Error: No id specified\n");
        return -1;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "vul_scanner", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: vul scanner item %s not exist\n", id);
            return -1;
        }
    }

    if (!(task_id = get_arg(args, "task_id"))) {
        fprintf(stderr, "Error: No task_id specified\n");
        return -1;
    }
    if (!(service_vendor_ip = get_arg(args, "service_vendor_ip"))) {
        fprintf(stderr, "Error: No service_vendor_ip specified\n");
        return -1;
    }
    snprintf(cond, BUFSIZE, "id=%s", id);
    snprintf(value, BUFSIZE, "task_id=%s, service_vendor_ip='%s'",
             task_id, service_vendor_ip);
    db_update("vul_scanner", value, cond);

    db_dump_cond("vul_scanner", "id, task_id, service_vendor_ip", cond);

    return 0;
}

static int vul_scanner_list(int opts, argument *args)
{
    char *default_params = 0;
    char *task_type = 0;
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];

    if (!(task_type = get_arg(args, "task_type"))) {
        fprintf(stderr, "Error: No task_type specified\n");
        return -1;
    }

    if (strcasecmp(task_type, "system") == 0 ||
        strcasecmp(task_type, "1") == 0) {

        default_params = "l.id,r.name vm_name,l.vm_lcuuid,l.vm_service_ip,"
                         "l.start_time, l.service_vendor_ip";
        memset(cond, 0, sizeof cond);
        strncpy(cond, "true", BUFSIZE - 1);
        if (!params) {
            params = default_params;
        }
        db_dump_vul_cond("l", "r", params, cond);

    } else if (strcasecmp(task_type, "web") == 0 ||
               strcasecmp(task_type, "2") == 0) {

        default_params = "id, scan_target, start_time, service_vendor_ip, order_id";
        memset(cond, 0, sizeof cond);
        strncpy(cond, "task_type=2", BUFSIZE - 1);
        if (!params) {
            params = default_params;
        }
        db_dump_cond("vul_scanner", params, cond);
    } else {

        fprintf(stderr, "Error: Unknown task_type %s\n", task_type);
        return -1;
    }

    return 0;
}

/* ipmi */
static int ipmi_list(int opts, argument *args)
{
    char *default_params = "id,ipmi_ip";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("ipmi_info", "ipmi_ip");
        } else {
            db_dump_minimal("ipmi_info", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("ipmi_info", params);
    }

    return 0;
}

#define IPMI_INFO_DEVICE_TYPE_HOST 1
static int ipmi_config(int opts, argument *args)
{
    char *params = "ipmi_ip,ipmi_user_name,ipmi_passwd,device_id,device_type";
    char values[BUFSIZE];
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char host_id_s[BUFSIZE];
    char cmd[BUFSIZE];
    char exec_result[MNTNCT_CMD_RESULT_LEN];
    char *host_ip = 0;
    char *ipmi_ip = 0;
    char *username = 0;
    char *password = 0;
    int  err = 0;

    if (!(host_ip = get_arg(args, "host"))) {
        fprintf(stderr, "Error: No host specified\n");
        return -1;
    }

    if (!(ipmi_ip = get_arg(args, "ipmi_ip"))) {
        fprintf(stderr, "Error: No IP specified\n");
        return -1;
    }

    if (!(username = get_arg(args, "username"))) {
        fprintf(stderr, "Error: No username specified\n");
        return -1;
    }

    if (!(password = get_arg(args, "password"))) {
        fprintf(stderr, "Error: No password specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", host_ip);
    memset(host_id_s, 0, sizeof host_id_s);
    db_select(&host_id_s, BUFSIZE, 1, "host_device", "id", cond);
    if (!host_id_s[0]) {
        fprintf(stderr, "Error: Host '%s' not exist\n", host_ip);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "device_id=%s and device_type=%d",
             host_id_s, IPMI_INFO_DEVICE_TYPE_HOST);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    if (id_s[0]) {
        fprintf(stderr, "Error: Host '%s' ipmi_info already exist\n", host_ip);
        return -1;
    }

    memset(cmd, 0, sizeof cmd);
    snprintf(cmd, BUFSIZE, LC_CHECK_IPMI_CONFIG_STR,
             ipmi_ip, username, password);
    memset(exec_result, 0, sizeof exec_result);
    err = call_system_output(cmd, exec_result, MNTNCT_CMD_RESULT_LEN);
    if (err) {
        fprintf(stderr, "Error: Check ipmi config failed\n");
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s','%s','%s',%s, %d",
             ipmi_ip, username, password, host_id_s,
             IPMI_INFO_DEVICE_TYPE_HOST);
    db_insert("ipmi_info", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ipmi_ip='%s'", ipmi_ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int ipmi_remove(int opts, argument *args)
{
    char *host_ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];

    if (!(host_ip = get_arg(args, "host"))) {
        fprintf(stderr, "Error: No host specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", host_ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "host_device", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: Host '%s' not exist\n", host_ip);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "device_id=%s and device_type=%d",
             id_s, IPMI_INFO_DEVICE_TYPE_HOST);
    db_delete("ipmi_info", cond);
    return 0;
}

#define IPMI_START 1
#define IPMI_STOP 2
#if 0
static int ipmi_operate(char *ip, uint32_t op)
{
    Header__Header head = HEADER__HEADER__INIT;
    Talker__Message body = TALKER__MESSAGE__INIT;
    Talker__PMOpsReq msg = TALKER__PMOPS_REQ__INIT;
    uint32_t head_len, body_len, len;
    uint8_t *buf = 0;
    uint8_t *recv_buf = 0;
    size_t recv_buf_len;
    Talker__Message *p_recv_body = 0;

    assert(op == IPMI_START || op == IPMI_STOP);
    msg.pm_ipmi_ip = ip;
    if (op == IPMI_START) {
        msg.pm_ops = TALKER__PMOPS__PM_START;
    } else {
        msg.pm_ops = TALKER__PMOPS__PM_STOP;
    }
    msg.has_pm_ops = 1;
    body.pm_ops_req = &msg;
    body_len = talker__message__get_packed_size(&body);

    head.sender = HEADER__MODULE__MNTNCT;
    head.type = HEADER__TYPE__UNICAST;
    head.length = body_len;
    head.seq = 0;
    head_len = header__header__get_packed_size(&head);

    len = head_len + body_len;
    buf = calloc(1, len);
    if (!buf) {
        fprintf(stderr, "Error: malloc() failed\n");
        return -1;
    }
    header__header__pack(&head, buf);
    talker__message__pack(&body, buf + head_len);

    if (lc_bus_init()) {
        fprintf(stderr, "Error: lc_bus_init() failed\n");
        free(buf);
        return -1;
    }

    /* to avoid multiple processes cosuming mntnct queue */
    if (acquire_mntnct_queue_lock()) {
        lc_bus_exit();
        free(buf);
        return -1;
    }

    if (lc_bus_init_consumer(0, LC_BUS_QUEUE_MNTNCT)) {
        fprintf(stderr, "Error: lc_bus_init_consumer() failed\n");
        lc_bus_exit();
        release_mntnct_queue_lock();
        free(buf);
        return -1;
    }

    if (lc_bus_publish_unicast((void *)buf, len, LC_BUS_QUEUE_PMADAPTER)) {
        fprintf(stderr, "Error: lc_bus_publish_unicast() failed\n");
        lc_bus_exit();
        release_mntnct_queue_lock();
        free(buf);
        return -1;
    }
    free(buf);

    while (1) {
        if (recv_message(&recv_buf, &recv_buf_len)) {
            fprintf(stderr, "Error: recv_message() failed\n");
        }
        p_recv_body = talker__message__unpack(0, recv_buf_len, recv_buf);
        if (!p_recv_body) {
            fprintf(stderr, "Error: unpack message error\n");
            continue;
        }
        if (!p_recv_body->pm_ops_reply) {
            fprintf(stderr, "Error: incorrect message\n");
            talker__message__free_unpacked(p_recv_body, 0);
            continue;
        }
        break;
    }
    lc_bus_exit();
    release_mntnct_queue_lock();

    if (p_recv_body->pm_ops_reply->result != 0) {
        fprintf(stderr, "Error: ipmi operation failed (%d)\n",
                p_recv_body->pm_ops_reply->result);
    } else {
        printf("Operation successful\n");
    }
    talker__message__free_unpacked(p_recv_body, 0);

    return 0;
}

static int ipmi_start(int opts, argument *args)
{
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char values[BUFSIZE];
    int ret;

    if (!(ip = get_arg(args, "ipmi_ip"))) {
        fprintf(stderr, "Error: No IP specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ipmi_ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: No IPMI %s\n", ip);
        return -1;
    }
    ret = ipmi_operate(ip, IPMI_START);
    if (!ret) {
        memset(values, 0, sizeof values);
        snprintf(values, BUFSIZE, "state=%d", DB_IPMI_STATE_RUNNING);
        db_update("ipmi_info", values, cond);
    }
    printf("Start %s\n", ret ? "failed" : "successful");

    return ret;
}

static int ipmi_stop(int opts, argument *args)
{
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char values[BUFSIZE];
    int ret;

    if (!(ip = get_arg(args, "ipmi_ip"))) {
        fprintf(stderr, "Error: No IP specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ipmi_ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: No IPMI %s\n", ip);
        return -1;
    }
    ret = ipmi_operate(ip, IPMI_STOP);
    if (!ret) {
        memset(values, 0, sizeof values);
        snprintf(values, BUFSIZE, "state=%d", DB_IPMI_STATE_STOPPED);
        db_update("ipmi_info", values, cond);
    }
    printf("Stop %s\n", ret ? "failed" : "successful");

    return ret;
}

char *os_types[] = {
    "CentOS_6_5", "1",
    "XenServer_6_2", "2",
    0, 0
};

static int ipmi_deploy(int opts, argument *args)
{
    char *os_type = 0;
    int i_os_type = 0;
    int i;
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char values[BUFSIZE];
    char *ipmi_mac = 0;
    char *hostname = 0;
    char *os_password = 0;
    char *os_ip = 0;
    char *os_netmask = 0;
    char *os_gateway = 0;
    char *os_dns = 0;
    char *os_device_name = 0;
    Header__Header head = HEADER__HEADER__INIT;
    Talker__Message body = TALKER__MESSAGE__INIT;
    Talker__PMDeployReq msg = TALKER__PMDEPLOY_REQ__INIT;
    Talker__PMNic nic = TALKER__PMNIC__INIT;
    uint32_t head_len, body_len, len;
    uint8_t *buf = 0;
    uint8_t *recv_buf = 0;
    size_t recv_buf_len;
    Talker__Message *p_recv_body = 0;
    int ret;
    int masklen;

    if (!(os_type = get_arg(args, "os_type"))) {
        fprintf(stderr, "Error: No os_type specified\n");
        fprintf(stderr, "Supported systems are: ");
        for (i = 0; os_types[i]; i += 2) {
            fprintf(stderr, "%s", os_types[i]);
            if (os_types[i+2]) {
                fprintf(stderr, ", ");
            }
        }
        fprintf(stderr, "\n");
        return -1;
    }
    for (i = 0; os_types[i]; i += 2) {
        if (!strcmp(os_types[i], os_type)) {
            i_os_type = atoi(os_types[i+1]);
            break;
        }
    }
    if (!os_types[i]) {
        fprintf(stderr, "Error: os_type %s not supported\n", os_type);
        fprintf(stderr, "Supported systems are: ");
        for (i = 0; os_types[i]; i += 2) {
            fprintf(stderr, "%s", os_types[i]);
            if (os_types[i+2]) {
                fprintf(stderr, ", ");
            }
        }
        fprintf(stderr, "\n");
        return -1;
    }

    if (!(ip = get_arg(args, "ipmi_ip"))) {
        fprintf(stderr, "Error: No IP specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ipmi_ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: No IPMI %s\n", ip);
        return -1;
    }

    if (!(ipmi_mac = get_arg(args, "ipmi_mac"))) {
        fprintf(stderr, "Error: No ipmi_mac specified\n");
        return -1;
    }

    if (!(hostname = get_arg(args, "hostname"))) {
        fprintf(stderr, "Error: No hostname specified\n");
        return -1;
    }

    if (!(os_password = get_arg(args, "os_password"))) {
        fprintf(stderr, "Error: No os_password specified\n");
        return -1;
    }

    if (!(os_ip = get_arg(args, "os_ip"))) {
        fprintf(stderr, "Error: No os_ip specified\n");
        return -1;
    }

    if (!(os_netmask = get_arg(args, "os_netmask"))) {
        fprintf(stderr, "Error: No os_netmask specified\n");
        return -1;
    } else {
        if (strlen(os_netmask) <= 2) {
            masklen = atoi(os_netmask);
            if (check_masklen(masklen, &os_netmask)) {
                fprintf(stderr, "Error: os_netmask '%s' invalid\n", os_netmask);
                return -1;
            }
        } else if (check_netmask(os_netmask, NULL)) {
            fprintf(stderr, "Error: os_netmask '%s' invalid\n", os_netmask);
            return -1;
        }
    }

    if (!(os_gateway = get_arg(args, "os_gateway"))) {
        fprintf(stderr, "Error: No os_gateway specified\n");
        return -1;
    }

    if (!(os_dns = get_arg(args, "os_dns"))) {
        fprintf(stderr, "Error: No os_dns specified\n");
        return -1;
    }
    if (!(os_device_name = get_arg(args, "os_device_name"))){
        fprintf(stderr, "Error: No os_device_name specified\n");
        return -1;
    }

    nic.device_name = os_device_name;
    nic.ipaddr = os_ip;
    nic.netmask = os_netmask;
    nic.gateway = os_gateway;
    nic.dns = os_dns;
    msg.pm_ipmi_ip = ip;
    msg.os_type = i_os_type;
    msg.has_os_type = 1;
    msg.root_passwd = os_password;
    msg.hostname = hostname;
    msg.pm_ipmi_mac = ipmi_mac;
    msg.pm_nic = &nic;
    body.pm_deploy_req = &msg;
    body_len = talker__message__get_packed_size(&body);

    head.sender = HEADER__MODULE__MNTNCT;
    head.type = HEADER__TYPE__UNICAST;
    head.length = body_len;
    head.seq = 0;
    head_len = header__header__get_packed_size(&head);

    len = head_len + body_len;
    buf = calloc(1, len);
    if (!buf) {
        fprintf(stderr, "Error: malloc() failed\n");
        return -1;
    }
    header__header__pack(&head, buf);
    talker__message__pack(&body, buf + head_len);

    if (lc_bus_init()) {
        fprintf(stderr, "Error: lc_bus_init() failed\n");
        free(buf);
        return -1;
    }

    /* to avoid multiple processes cosuming mntnct queue */
    if (acquire_mntnct_queue_lock()) {
        lc_bus_exit();
        free(buf);
        return -1;
    }

    if (lc_bus_init_consumer(0, LC_BUS_QUEUE_MNTNCT)) {
        fprintf(stderr, "Error: lc_bus_init_consumer() failed\n");
        lc_bus_exit();
        release_mntnct_queue_lock();
        free(buf);
        return -1;
    }

    if (lc_bus_publish_unicast((void *)buf, len, LC_BUS_QUEUE_PMADAPTER)) {
        fprintf(stderr, "Error: lc_bus_publish_unicast() failed\n");
        lc_bus_exit();
        release_mntnct_queue_lock();
        free(buf);
        return -1;
    }
    free(buf);

    while (1) {
        if (recv_message(&recv_buf, &recv_buf_len)) {
            fprintf(stderr, "Error: recv_message() failed\n");
        }
        p_recv_body = talker__message__unpack(0, recv_buf_len, recv_buf);
        if (!p_recv_body) {
            fprintf(stderr, "Error: unpack message error\n");
            continue;
        }
        if (!p_recv_body->pm_ops_reply) {
            fprintf(stderr, "Error: incorrect message\n");
            talker__message__free_unpacked(p_recv_body, 0);
            continue;
        }
        break;
    }
    lc_bus_exit();
    release_mntnct_queue_lock();

    if (p_recv_body->pm_ops_reply->result != 0) {
        fprintf(stderr, "Error: ipmi deploy failed (%d)\n",
                p_recv_body->pm_ops_reply->result);
    } else {
        memset(values, 0, sizeof values);
        snprintf(values, BUFSIZE, "action=%d,create_time=SYSDATE()", DB_IPMI_ACTION_DEPLOYING);
        db_update("ipmi_info", values, cond);
        printf("Deployment started\n");
    }
    talker__message__free_unpacked(p_recv_body, 0);

    return ret;
}

static int ipmi_console(int opts, argument *args)
{
    char *ip = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char cmd[BUFSIZE];

    if (!(ip = get_arg(args, "ipmi_ip"))) {
        fprintf(stderr, "Error: No IP specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ipmi_ip='%s'", ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "ipmi_info", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr, "Error: No IPMI %s\n", ip);
        return -1;
    }
    memset(cmd, 0, sizeof cmd);
    db_select(&cmd, BUFSIZE, 1, "ipmi_info",
              "group_concat('ipmitool -H ',ipmi_ip,' -I lanplus "
              "-U ',ipmi_user_name,' -P ',ipmi_passwd,' sol activate')",
              cond);
    printf("Press '~.' to exit\n");
    system(cmd);

    return 0;
}
#endif

static int pack_service_register_curl_post(char *buf, int buf_len,
                                char *service_vendor,
                                int service_type_int,
                                char *service_ip,
                                int service_port_int,
                                char *service_user_name,
                                char *service_user_passwd,
                                char *control_ip,
                                int control_port_int)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_SERVICE_VENDOR, root);
    tmp->text_value = service_vendor;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_SERVICE_TYPE, root);
    tmp->int_value = service_type_int;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_SERVICE_IP, root);
    tmp->text_value = service_ip;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_SERVICE_PORT, root);
    tmp->int_value = service_port_int;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_SERVICE_USER_NAME, root);
    tmp->text_value = service_user_name;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_SERVICE_USER_PASSWD, root);
    tmp->text_value = service_user_passwd;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_CONTROL_IP, root);
    tmp->text_value = control_ip;

    tmp = create_json(NX_JSON_INTEGER, MNTNCT_API_CONTROL_PORT, root);
    tmp->int_value = control_port_int;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int service_vendor_add(int opts, argument *args)
{
    char *params = "service_vendor, service_type, service_ip, service_port, "\
                   "service_user_name, service_user_passwd, control_ip, control_port, lcuuid";
    char *service_vendor = 0;
    char *service_type = 0;
    char *service_ip = 0;
    char *service_user_name = 0;
    char *service_user_passwd = 0;
    int  service_type_int = 0;
    int  service_port_int = 0;
    char *control_ip = 0;
    int  control_port_int = 0;
    char values[BUFSIZE];
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char call_result[CURL_BUFFER_SIZE];
    char lcuuid[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err;
    const nx_json *json = 0;

    memset(call_result, 0, CURL_BUFFER_SIZE);
    if (!(service_vendor = get_arg(args, "service_vendor"))) {
        fprintf(stderr, "Error: No service_vendor specified\n");
        return -1;
    } else if (strlen(service_vendor) > NAME_LENGTH) {
        fprintf(stderr, "Error: service_vendor too long\n");
        return -1;
    }

    if (!(service_type = get_arg(args, "service_type"))) {
        fprintf(stderr, "Error: No service_type specified\n");
        return -1;
    }
    if (strcasecmp(service_type, "vul_scanner") == 0 ||
        strcasecmp(service_type, "1") == 0) {

        service_type_int = SERVICE_TYPE_VUL_SCANNER;
        control_port_int = CONTROL_PORT_VUL_SCANNER;

    } else if (strcasecmp(service_type, "nas_storage") == 0 ||
               strcasecmp(service_type, "2") == 0) {

        service_type_int = SERVICE_TYPE_NAS_STORAGE;
    } else if (strcasecmp(service_type, "cloud_disk") == 0 ||
               strcasecmp(service_type, "3") == 0) {

        service_type_int = SERVICE_TYPE_CLOUD_DISK;
    } else if (strcasecmp(service_type, "backup") == 0 ||
               strcasecmp(service_type, "4") == 0) {

        service_type_int = SERVICE_TYPE_BACKUP;
        control_port_int = CONTROL_PORT_BACKUP;
    } else {

        fprintf(stderr, "Error: Unknown service type %s\n", service_type);
        return -1;
    }

    if (!(service_ip = get_arg(args, "service_ip"))) {
        fprintf(stderr, "Error: No service_ip specified\n");
        return -1;
    }
    if (!(service_user_name = get_arg(args, "service_user_name"))) {
        fprintf(stderr, "Error: No service_user_name specified\n");
        return -1;
    }
    if (!(service_user_passwd = get_arg(args, "service_user_passwd"))) {
        fprintf(stderr, "Error: No service_user_passwd specified\n");
        return -1;
    }
    if (!(control_ip = get_arg(args, "control_ip"))) {
        fprintf(stderr, "Error: No control_ip specified\n");
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_SERVICE_REGISTER_POST,
             MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);

    err = pack_service_register_curl_post(post, MNTNCT_CMD_LEN, service_vendor,
            service_type_int, service_ip, service_port_int, service_user_name,
            service_user_passwd, control_ip, control_port_int);

    if (err) {
        fprintf(stderr, "Error: Pack service register post curl failed\n");
        return -1;
    }

    err = call_curl_api(url, API_METHOD_POST, post, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    json = nx_json_parse(call_result, 0);
    if (json) {
        json = nx_json_get(nx_json_get(json, "DATA"), "LCUUID");
        if (json && json->type == NX_JSON_STRING) {
            memcpy(lcuuid, json->text_value, UUID_LENGTH);
        }
        else {
            fprintf(stderr, "Error: Invalid JSON response\n%s\n", call_result);
            return -1;
        }
    } else {
        fprintf(stderr, "Error: Invalid JSON response\n%s\n", call_result);
        return -1;
    }

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s',%d,'%s',%d,'%s','%s','%s','%d','%s'",
            service_vendor, service_type_int, service_ip, service_port_int,
            service_user_name, service_user_passwd, control_ip, control_port_int, lcuuid);
    db_insert("service_vendor", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE,
             "service_vendor='%s' and service_type=%d "\
             "and service_ip='%s' and service_port=%d",
             service_vendor, service_type_int, service_ip, service_port_int);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "service_vendor", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int service_vendor_remove(int opts, argument *args)
{
    char *service_vendor = 0;
    char *service_type = 0;
    char *service_ip = 0;
    int service_type_int = 0;
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char lcuuid[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char call_result[CURL_BUFFER_SIZE] = {0};
    int err;

    if (!(service_vendor = get_arg(args, "service_vendor"))) {
        fprintf(stderr, "Error: No service_vendor specified\n");
        return -1;
    } else if (strlen(service_vendor) > NAME_LENGTH) {
        fprintf(stderr, "Error: service_vendor too long\n");
        return -1;
    }

    if (!(service_type = get_arg(args, "service_type"))) {
        fprintf(stderr, "Error: No service_type specified\n");
        return -1;
    }
    if (strcasecmp(service_type, "vul_scanner") == 0 ||
        strcasecmp(service_type, "1") == 0) {

        service_type_int = SERVICE_TYPE_VUL_SCANNER;

    } else if (strcasecmp(service_type, "nas_storage") == 0 ||
               strcasecmp(service_type, "2") == 0) {

        service_type_int = SERVICE_TYPE_NAS_STORAGE;

    } else if (strcasecmp(service_type, "backup") == 0 ||
               strcasecmp(service_type, "4") == 0) {

        service_type_int = SERVICE_TYPE_BACKUP;

    } else {

        fprintf(stderr, "Error: Unknown service type %s\n", service_type);
        return -1;
    }
    if (!(service_ip = get_arg(args, "service_ip"))) {
        fprintf(stderr, "Error: No service_ip specified\n");
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "service_vendor='%s' and service_type=%d and service_ip='%s'",
            service_vendor, service_type_int, service_ip);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "service_vendor", "id", cond);
    if (!id_s[0]) {
        fprintf(stderr,
                "Error: service_vendor not exist\n");
        return -1;
    }

    db_select(&lcuuid, BUFSIZE, 1, "service_vendor", "lcuuid", cond);
    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX MNTNCT_API_SERVICE_REGISTER_DELETE,
             MNTNCT_API_DEST_IP, MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION, lcuuid);

    err = call_curl_api(url, API_METHOD_DEL, NULL, &call_result[0]);
    if (err) {
        fprintf(stderr, "Error: call result [%s]\n", call_result);
        return -1;
    }
    db_delete("service_vendor", cond);

    return 0;
}

static int service_vendor_list(int opts, argument *args)
{
    char *default_params = "id,service_vendor,service_type,service_ip,service_port,control_ip,control_port";
    char *params = get_arg(args, "params");

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_minimal("service_vendor", "service_ip");
        } else {
            db_dump_minimal("service_vendor", params);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump("service_vendor", params);
    }

    return 0;
}

static int ctrl_plane_bandwidth_config(int opts, argument *args)
{
    char *ctrl_plane_bandwidth = 0;
    uint64_t ctrl_plane_bandwidth_init = 0;
    char values[BUFSIZE];
    char cond[BUFSIZE];
    char *domain_name = 0;
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;

    if (!(ctrl_plane_bandwidth = get_arg(args, "bandwidth"))) {
        fprintf(stderr, "Error: No bandwidth specified\n");
        return -1;
    }

    sscanf(ctrl_plane_bandwidth, "%lu", &ctrl_plane_bandwidth_init);
    ctrl_plane_bandwidth_init <<= 20L;

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "%lu", ctrl_plane_bandwidth_init);

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_PATCH, domain);
    strcat(url, url2);
    err = pack_domain_patch(post, MNTNCT_CMD_LEN, "ctrl_plane_bandwidth", values);
    if (err) {
        fprintf(stderr, "Error: Pack domain patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    printf("Success\n");
    return 0;
}

static int ctrl_plane_bandwidth_list(int opts, argument *args)
{
    char *default_params = "id,param_name,value,domain";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];
    char *domain_name = get_arg(args, "domain");
    char domain[UUID_LENGTH] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        snprintf(cond, BUFSIZE, "param_name='ctrl_plane_bandwidth' AND domain='%s'", domain);
    } else {
        snprintf(cond, BUFSIZE, "param_name='ctrl_plane_bandwidth'");
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond("domain_configuration", "id,value", cond);
        } else {
            db_dump_cond("domain_configuration", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("domain_configuration", params, cond);
    }

    return 0;
}


static int service_plane_bandwidth_config(int opts, argument *args)
{
    char *serv_plane_bandwidth = 0;
    uint64_t serv_plane_bandwidth_init = 0;
    char values[BUFSIZE];
    char cond[BUFSIZE];
    char *domain_name = 0;
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;

    if (!(serv_plane_bandwidth = get_arg(args, "bandwidth"))) {
        fprintf(stderr, "Error: No bandwidth specified\n");
        return -1;
    }

    sscanf(serv_plane_bandwidth, "%lu", &serv_plane_bandwidth_init);
    serv_plane_bandwidth_init <<= 20L;

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "%lu", serv_plane_bandwidth_init);

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_PATCH, domain);
    strcat(url, url2);
    err = pack_domain_patch(post, MNTNCT_CMD_LEN, "serv_plane_bandwidth", values);
    if (err) {
        fprintf(stderr, "Error: Pack domain patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    printf("Success\n");
    return 0;
}

static int service_plane_bandwidth_list(int opts, argument *args)
{
    char *default_params = "id,param_name,value,domain";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];
    char *domain_name = get_arg(args, "domain");
    char domain[UUID_LENGTH] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        snprintf(cond, BUFSIZE, "param_name='serv_plane_bandwidth' AND domain='%s'", domain);
    } else {
        snprintf(cond, BUFSIZE, "param_name='serv_plane_bandwidth'");
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond("domain_configuration", "id,value", cond);
        } else {
            db_dump_cond("domain_configuration", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("domain_configuration", params, cond);
    }

    return 0;
}

static int ctrl_plane_vlan_config(int opts, argument *args)
{
    char *ctrl_plane_vlan = 0;
    char cond[BUFSIZE];
    char *domain_name = 0;
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;

    if (!(ctrl_plane_vlan = get_arg(args, "vlan"))) {
        fprintf(stderr, "Error: No vlan specified\n");
        return -1;
    }

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_PATCH, domain);
    strcat(url, url2);
    err = pack_domain_patch(post, MNTNCT_CMD_LEN, "ctrl_plane_vlan", ctrl_plane_vlan);
    if (err) {
        fprintf(stderr, "Error: Pack domain patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    printf("Success\n");
    return 0;
}

static int ctrl_plane_vlan_list(int opts, argument *args)
{
    char *default_params = "id,param_name,value,domain";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];
    char *domain_name = get_arg(args, "domain");
    char domain[UUID_LENGTH] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        snprintf(cond, BUFSIZE, "param_name='ctrl_plane_vlan' AND domain='%s'", domain);
    } else {
        snprintf(cond, BUFSIZE, "param_name='ctrl_plane_vlan'");
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond("domain_configuration", "id,value", cond);
        } else {
            db_dump_cond("domain_configuration", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("domain_configuration", params, cond);
    }

    return 0;
}

static int service_plane_vlan_config(int opts, argument *args)
{
    char *serv_plane_vlan = 0;
    char cond[BUFSIZE];
    char *domain_name = 0;
    char domain[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0;

    if (!(serv_plane_vlan = get_arg(args, "vlan"))) {
        fprintf(stderr, "Error: No vlan specified\n");
        return -1;
    }

    if (!(domain_name = get_arg(args, "domain"))) {
        fprintf(stderr, "Error: No domain specified\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", domain_name);
    memset(domain, 0, sizeof domain);
    db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
    if (!domain[0]) {
        fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
            MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_PATCH, domain);
    strcat(url, url2);
    err = pack_domain_patch(post, MNTNCT_CMD_LEN, "serv_plane_vlan", serv_plane_vlan);
    if (err) {
        fprintf(stderr, "Error: Pack domain patch curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_PATCH, post, NULL);
    if (err) {
        return -1;
    }

    printf("Success\n");
    return 0;
}

static int service_plane_vlan_list(int opts, argument *args)
{
    char *default_params = "id,param_name,value,domain";
    char *params = get_arg(args, "params");
    char cond[BUFSIZE];
    char *domain_name = get_arg(args, "domain");
    char domain[UUID_LENGTH] = {0};

    if (domain_name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", domain_name);
        memset(domain, 0, sizeof domain);
        db_select(&domain, UUID_LENGTH, 1, "domain", "lcuuid", cond);
        if (!domain[0]) {
            fprintf(stderr, "Error: Domain '%s' not exist\n", domain_name);
            return -1;
        }
        snprintf(cond, BUFSIZE, "param_name='serv_plane_vlan' AND domain='%s'", domain);
    } else {
        snprintf(cond, BUFSIZE, "param_name='serv_plane_vlan'");
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            db_dump_cond("domain_configuration", "id,value", cond);
        } else {
            db_dump_cond("domain_configuration", params, cond);
        }
    } else {
        if (!params) {
            params = default_params;
        }
        db_dump_cond("domain_configuration", params, cond);
    }

    return 0;
}

static int invitation_list(int opts, argument *args)
{
    char *params = "code, duedate" ;
    db_dump("invitationcode", params);
    return 0;
}

static int invitation_add(int opts, argument *args)
{
    char *params = "code, duedate";
    char *code = 0;
    char *expdays_char = 0;
    int expdays = 0;
    int duetimestamp = (int)time(NULL);
    char cond[BUFSIZE];
    char dummy[BUFSIZE];
    char values[BUFSIZE];

    if (!(code = get_arg(args, "code"))) {
        fprintf(stderr, "Error: No code specified\n");
        return -1;
    } else if (strlen(code) > 8 || strlen(code) < 6) {
        fprintf(stderr, "Error: Code length is out of range from 6 to 8\n");
        return -1;
    }
    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "code='%s'", code);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "invitationcode", "code", cond);
    if (dummy[0]){
        fprintf(stderr, "Error: Code '%s' already exists\n", code);
        return -1;
    }

    if (!(expdays_char = get_arg(args, "expdays"))){
        fprintf(stderr, "Warning: expdays auto reset to 30\n");
        expdays = 30;
    } else{
        expdays = atoi(expdays_char);
        if (expdays < 1 || expdays > 365){
            fprintf(stderr, "Warning: expdays outof range 1-365, auto reset to 30\n");
            expdays = 30;
        }
    }
    duetimestamp += expdays * 86400;

    memset(values, 0, sizeof values);
    snprintf(values, BUFSIZE, "'%s', '%d'", code, duetimestamp);
    db_insert("invitationcode", params, values);

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "code='%s'", code);
    memset(dummy, 0, sizeof dummy);
    db_select(&dummy, BUFSIZE, 1, "invitationcode", "code", cond);
    printf("%s\n", dummy);

    return 0;
}

static int pack_domain_post(char *buf, int buf_len,
                            char *name,
                            char *ip,
                            char *role,
                            char *public_ip)
{
    nx_json *root = NULL;
    nx_json *tmp = NULL;
    int len = 0;

    if (!buf) {
        return -1;
    }

    root = (nx_json *)malloc(sizeof(nx_json));
    if (NULL == root) {
        return -1;
    }
    memset(root, 0, sizeof(nx_json));
    root->type = NX_JSON_OBJECT;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_NAME, root);
    tmp->text_value = name;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_IP, root);
    tmp->text_value = ip;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_ROLE, root);
    tmp->text_value = role;

    tmp = create_json(NX_JSON_STRING, MNTNCT_API_DOMAIN_PUBLICIP, root);
    tmp->text_value = public_ip;

    len = dump_json_msg(root, buf, buf_len);
    nx_json_free(root);
    return 0;
}

static int bss_domain_opt(int op, char *name, int role, char *ip, char *public_ip, char *uuid)
{
    char cond[BUFSIZE];
    char lcuuid[UUID_LENGTH] = {0};
    char *params = "name,ip,lcuuid,role,public_ip";
    char values[BUFSIZE];
    char id_s[BUFSIZE];
    FILE *fp;
    char cmd[512];

    if (op == 0 ) {/*Add domain*/
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(lcuuid, 0, sizeof lcuuid);
        bss_db_select(&lcuuid, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (lcuuid[0]) {
            fprintf(stderr, "Error: domain '%s' already exist\n", name);
            return -1;
        }

        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(lcuuid, 0, sizeof lcuuid);
        bss_db_select(&lcuuid, BUFSIZE, 1, "domain", "ip", cond);
        if (lcuuid[0]) {
            fprintf(stderr, "Error: domain with ip '%s' already exist\n", ip);
            return -1;
        }

        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "public_ip='%s'", public_ip);
        memset(lcuuid, 0, sizeof lcuuid);
        bss_db_select(&lcuuid, BUFSIZE, 1, "domain", "public_ip", cond);
        if (lcuuid[0]) {
            fprintf(stderr, "Error: domain with public_ip '%s' already exist\n", ip);
            return -1;
        }
/*
        if (!generate_uuid(lcuuid, UUID_LENGTH)) {
            fprintf(stderr, "Error: failed to generate LCUUID\n");
            return -1;
        }
*/
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, 512, "cat /proc/sys/kernel/random/uuid");
        if ((fp = popen(cmd, "r")) == 0) {
            printf("Generate UUID error!\n");
            return -1;
        }
        fgets(lcuuid, UUID_LENGTH - 2, fp);
        chomp(lcuuid);
        pclose(fp);

        memset(values, 0, sizeof values);
        snprintf(values, BUFSIZE, "'%s','%s','%s','%d','%s'",
                 name, ip, lcuuid, role, public_ip);
        bss_db_insert("domain", params, values);

        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        bss_db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        printf("BSS domain %s(%s) added successfully\n", name, id_s);
    } else if (op ==  1) {/*Modify domain*/
        /*TODO: TBD*/
    } else if (op == 2 ){ /*Delete domain*/
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(lcuuid, 0, sizeof lcuuid);
        bss_db_select(&lcuuid, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!lcuuid[0]) {
            fprintf(stderr, "Error: nonexisting bss domain '%s'\n", name);
            return -1;
        }
        bss_db_delete("domain", cond);
        printf("BSS domain %s deleted successfully\n", name);
    }
    return 0;
}

static int domain_add(int opts, argument *args)
{
    char *name = 0;
    char *ip = 0;
    char *role = 0;
    char *public_ip = 0;
    char role_str[BUFSIZE];
    char cond[BUFSIZE];
    char id_s[BUFSIZE];
    char lcuuid[UUID_LENGTH] = {0};
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    char post[MNTNCT_CMD_LEN] = {0};
    int err = 0, bss_role = 0;

    if (!(name = get_arg(args, "name"))) {
        fprintf(stderr, "Error: No name specified\n");
        return -1;
    } else if (strlen(name) > NAME_LENGTH) {
        fprintf(stderr, "Error: name too long\n");
        return -1;
    }

    memset(role_str, 0, BUFSIZE);
    if (!(role = get_arg(args, "role"))) {
        fprintf(stderr, "Error: No role specified\n");
        return -1;
    }else if (strcasecmp(role, "BSS") == 0 ||
        strcasecmp(role, "1") == 0) {
        memcpy(role_str, "1", BUFSIZE);
        bss_role = 1;
    } else if (strcasecmp(role, "OSS") == 0 ||
               strcasecmp(role, "2") == 0) {
        memcpy(role_str, "2", BUFSIZE);
    } else {

        fprintf(stderr, "Error: Unknown role %s\n", role);
        return -1;
    }

    if (!(ip = get_arg(args, "ip"))) {
        fprintf(stderr, "Error: No ip specified\n");
        return -1;
    }

    if (!(public_ip = get_arg(args, "public_ip"))) {
        fprintf(stderr, "Error: No public_ip specified\n");
        return -1;
    }

    /* On BSS side, workaround to add domain because
     * talker isn't resident, it will result in failing to add bss domain.
     * */
    if (bss_role) { /*BSS role*/
        return bss_domain_opt(0, name, bss_role, ip, public_ip, NULL);
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(lcuuid, 0, sizeof lcuuid);
    db_select(&lcuuid, BUFSIZE, 1, "domain", "lcuuid", cond);
    if (lcuuid[0]) {
        fprintf(stderr, "Error: domain '%s' already exist\n", name);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "ip='%s'", ip);
    memset(lcuuid, 0, sizeof lcuuid);
    db_select(&lcuuid, BUFSIZE, 1, "domain", "ip", cond);
    if (lcuuid[0]) {
        fprintf(stderr, "Error: domain with ip '%s' already exist\n", ip);
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "public_ip='%s'", public_ip);
    memset(lcuuid, 0, sizeof lcuuid);
    db_select(&lcuuid, BUFSIZE, 1, "domain", "public_ip", cond);
    if (lcuuid[0]) {
        fprintf(stderr, "Error: domain with public_ip '%s' already exist\n", public_ip);
        return -1;
    }

    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
        MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_POST);
    strcat(url, url2);
    err = pack_domain_post(post, MNTNCT_CMD_LEN, name, ip, role_str, public_ip);
    if (err) {
        fprintf(stderr, "Error: Pack domain post curl failed\n");
        return -1;
    }
    err = call_curl_api(url, API_METHOD_POST, post, NULL);
    if (err) {
        return -1;
    }

    memset(cond, 0, sizeof cond);
    snprintf(cond, BUFSIZE, "name='%s'", name);
    memset(id_s, 0, sizeof id_s);
    db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
    printf("%s\n", id_s);

    return 0;
}

static int domain_remove(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char *lcuuid = 0;
    char *role = 0;
    char cond[BUFSIZE];
    char lcuuid_s[BUFSIZE];
    char url[MNTNCT_CMD_LEN] = {0};
    char url2[MNTNCT_CMD_LEN] = {0};
    int err = 0, bss_role = 0;

    id = get_arg(args, "id");
    name = get_arg(args, "name");
    lcuuid = get_arg(args, "lcuuid");
    role = get_arg(args, "role");
    if (!role) {
        fprintf(stderr, "Error: No role specified\n");
        return -1;
    }
    if (strcasecmp(role, "BSS") == 0 ||
        strcasecmp(role, "1") == 0) {
        if (!name) {
            fprintf(stderr, "Error: No BSS name specified\n");
            return -1;
        }
        return bss_domain_opt(2, name, bss_role, NULL, NULL, NULL);
    }
    if (!id && !name && !lcuuid && !role) {
        fprintf(stderr, "Error: No id, name or lcuuid or role specified\n");
        return -1;
    }
    if (lcuuid) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
        memset(lcuuid_s, 0, sizeof lcuuid_s);
        db_select(&lcuuid_s, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!lcuuid_s[0]) {
            fprintf(stderr, "Error: domain '%s' not exist\n", lcuuid);
            return -1;
        }
    } else if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(lcuuid_s, 0, sizeof lcuuid_s);
        db_select(&lcuuid_s, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!lcuuid_s[0]) {
            fprintf(stderr, "Error: domain '%s' not exist\n", name);
            return -1;
        }
        lcuuid = lcuuid_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(lcuuid_s, 0, sizeof lcuuid_s);
        db_select(&lcuuid_s, BUFSIZE, 1, "domain", "lcuuid", cond);
        if (!lcuuid_s[0]) {
            fprintf(stderr,
                    "Error: domain with id '%s' not exist\n", id);
            return -1;
        }
        lcuuid = lcuuid_s;
    }
    snprintf(url, MNTNCT_CMD_LEN, MNTNCT_API_PREFIX, MNTNCT_API_DEST_IP,
        MNTNCT_API_DEST_PORT, MNTNCT_API_VERSION);
    snprintf(url2, MNTNCT_CMD_LEN, MNTNCT_API_DOMAIN_DELETE, lcuuid);
    strcat(url, url2);
    err = call_curl_api(url, API_METHOD_DEL, NULL, NULL);
    if (err) {
        return -1;
    }

    return 0;
}

static int domain_list(int opts, argument *args)
{
    char *default_params = "id, name, ip, role, lcuuid, public_ip";
    char *params = get_arg(args, "params");
    char *name = get_arg(args, "name");
    char cond[BUFSIZE];

    memset(cond, 0, sizeof cond);
    if (name) {
        snprintf(cond, BUFSIZE, "name='%s'", name);
    }
    if (!params) {
        params = default_params;
    }
    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (cond[0]) {
            db_dump_cond_minimal("domain", params, cond);
        } else {
            db_dump_minimal("domain", params);
        }
    } else {
        if (cond[0]) {
            db_dump_cond("domain", params, cond);
        } else {
            db_dump("domain", params);
        }
        printf("role:\n\t1. BSS 2. OSS\n");
    }

    return 0;
}

static int domain_update(int opts, argument *args)
{
    char *id = 0;
    char *name = 0;
    char *ip = 0;
    char *public_ip = 0;
    char *lcuuid = 0;
    char *role_str = 0;
    int role_int = 0;
    char cond[BUFSIZE];
    char values[BUFSIZE] = {0};
    char id_s[BUFSIZE];

    id = get_arg(args, "id");
    lcuuid = get_arg(args, "lcuuid");
    if (!id && !lcuuid) {
        fprintf(stderr, "Error: No id or lcuuid specified\n");
        return -1;
    }
    if (lcuuid) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "lcuuid='%s'", lcuuid);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: domain '%s' not exist\n", lcuuid);
            return -1;
        }
        id = id_s;
    } else {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        if (!id_s[0]) {
            fprintf(stderr, "Error: domain with id '%s' not exist\n", id);
            return -1;
        }
    }

    name = get_arg(args, "name");
    ip = get_arg(args, "ip");
    role_str = get_arg(args, "role");
    public_ip = get_arg(args, "public_ip");
    if (name) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "name='%s'", name);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        if (id_s[0]) {
            fprintf(stderr, "Error: domain '%s' already exist\n", name);
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "name='%s'", name);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("domain", values, cond);
    } else if (ip) {
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "ip='%s'", ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        if (id_s[0]) {
            fprintf(stderr, "Error: domain with ip '%s' already exist\n", ip);
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "ip='%s'", ip);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("domain", values, cond);
    } else if (role_str) {
        if(strcasecmp(role_str, "BSS") == 0 ){
            role_int = 1;
        }else if(strcasecmp(role_str, "OSS") == 0 ){
            role_int = 2;
        }else{
            fprintf(stderr, "Error: role is not BSS or OSS\n");
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "role='%d'", role_int);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("domain", values, cond);
    } else if (public_ip){
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "public_ip='%s'", public_ip);
        memset(id_s, 0, sizeof id_s);
        db_select(&id_s, BUFSIZE, 1, "domain", "id", cond);
        if (id_s[0]) {
            fprintf(stderr, "Error: domain with public_ip '%s' already exist\n", public_ip);
            return -1;
        }
        memset(values, 0, BUFSIZE);
        snprintf(values, BUFSIZE, "public_ip='%s'", public_ip);
        memset(cond, 0, sizeof cond);
        snprintf(cond, BUFSIZE, "id=%s", id);
        db_update("domain", values, cond);
    } else {
        fprintf(stderr, "Error: Nothing to update\n");
        return -1;
    }

    return 0;
}

static int vif_list(int opts, argument *args)
{
    char *default_params = "id,ifindex,iftype,flag,ip,netmask,gateway,mac,bandw,broadc_bandw,broadc_ceil_bandw,subnetid,vlantag,devicetype,deviceid,lcuuid,domain";
    char *params = get_arg(args, "params");
    char *ip = get_arg(args, "ip");
    char *mac = get_arg(args, "mac");
    char *subnetid = get_arg(args, "subnetid");
    char *vlantag = get_arg(args, "vlantag");
    char cond[BUFSIZE] = {0};
    char buffer[BUFSIZE] = {0};

    memset(cond, 0, sizeof cond);
    strncpy(cond, "true", BUFSIZE - 1);
    if (ip) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and ip='%s'", ip);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (mac) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and mac='%s'", mac);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (subnetid) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and subnetid=%s", subnetid);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }
    if (vlantag) {
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, BUFSIZE, " and vlantag=%s", vlantag);
        strncat(cond, buffer, BUFSIZE - strlen(cond) - 1);
    }

    if (opts & MNTNCT_OPTION_MINIMAL) {
        if (!params || strchr(params, ',')) {
            if (cond[0]) {
                db_dump_cond_minimal("vinterface", "id", cond);
            } else {
                db_dump_minimal("vinterface", "id");
            }
        } else {
            if (cond[0]) {
                db_dump_cond_minimal("vinterface", params, cond);
            } else {
                db_dump_minimal("vinterface", params);
            }
        }
    } else {
        if (!params) {
            params = default_params;
        }
        if (cond[0]) {
            db_dump_cond("vinterface", params, cond);
        } else {
            db_dump("vinterface", params);
        }
    }

    return 0;
}
