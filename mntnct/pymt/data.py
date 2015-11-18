from peewee import MySQLDatabase, Model as Model
from peewee import CharField, DateTimeField, IntegerField, TextField, \
    BigIntegerField
from peewee import DecimalField

database = MySQLDatabase('livecloud', host='localhost',
                         user='root', password='security421',
                         threadlocals=True)
bss_database = MySQLDatabase('livecloud_bss', host='localhost',
                             user='root', password='security421',
                             threadlocals=True)


def string_enum(*sequential, **named):
    enums = dict(zip(sequential, sequential), **named)
    return type('StringEnum', (), enums)


class Base(Model):
    class Meta:
        database = database


class ProductSpec(Base):
    lcuuid = CharField(unique=True)
    name = CharField(default='')
    type = IntegerField()
    charge_mode = IntegerField()
    state = IntegerField()
    price = DecimalField(max_digits=18, decimal_places=9)
    content = TextField()
    plan_name = CharField()
    product_type = IntegerField()
    domain = CharField()

    class Meta:
        database = bss_database
        db_table = 'product_specification'


class OssProductSpec(Base):
    lcuuid = CharField(unique=True)
    plan_name = CharField()
    domain = CharField()

    class Meta:
        db_table = 'product_specification_v2_2'


class ChargeDetail(Base):

    # column definitions
    instance_lcuuid = CharField(unique=True)
    obj_lcuuid = CharField(unique=True)
    product_specification_lcuuid = CharField(unique=True)
    lcuuid = CharField(unique=True)
    instance_name = CharField()
    instance_type = IntegerField()
    charge_mode = IntegerField()
    price = DecimalField(max_digits=18, decimal_places=9)
    cost = DecimalField(max_digits=18, decimal_places=9)
    start_time = DateTimeField()
    end_time = DateTimeField()
    last_check_time = ()
    type = IntegerField()
    size = IntegerField()
    userid = IntegerField()
    domain = CharField(unique=True)

    class Meta:
        database = bss_database
        db_table = 'charge_detail'


class VM(Base):
    id = IntegerField()
    state = IntegerField()
    pre_state = IntegerField()
    errno = IntegerField()
    eventid = IntegerField()
    action = IntegerField()
    name = CharField()
    label = CharField()
    os = CharField()
    flag = IntegerField()
    template = CharField()
    poolid = IntegerField()
    launch_server = CharField()
    des_host = CharField()
    vcloudid = IntegerField()
    vnetid = IntegerField()
    vl2id = IntegerField()
    opaque_id = CharField()
    vdi_sys_uuid = CharField(unique=True)
    vdi_sys_sr_uuid = CharField(unique=True)
    vdi_sys_sr_name = CharField()
    vdi_sys_size = IntegerField()
    vdi_user_uuid = CharField(unique=True)
    vdi_user_sr_uuid = CharField(unique=True)
    vdi_user_sr_name = CharField()
    vdi_user_size = IntegerField()
    mem_size = IntegerField()
    vcpu_num = IntegerField()
    tport = IntegerField()
    lport = IntegerField()
    ifid = IntegerField()
    health_state = IntegerField()
    cpu_start_time = BigIntegerField()
    disk_start_time = BigIntegerField()
    traffic_start_time = BigIntegerField()
    check_start_time = BigIntegerField()
    uuid = CharField(unique=True)
    domain = CharField(unique=True)
    exclude_server = CharField()
    thumbprint = CharField()
    vm_path = CharField()
    userid = IntegerField()
    epc_id = IntegerField()
    is_expired = IntegerField()
    expired_time = DateTimeField()
    lcuuid = CharField(unique=True)
    product_specification_lcuuid = CharField(unique=True)
    gateway = CharField()
    role = IntegerField()
    loopback_ips = CharField()

    class Meta:
        db_table = 'vm_v2_2'


class NasStorage(Base):
    id = IntegerField()
    name = CharField()
    lcuuid = CharField(unique=True)
    path = CharField()
    total_size = IntegerField()
    available_size = IntegerField()
    userid = IntegerField()
    domain = CharField()
    order_id = IntegerField()
    product_specification_lcuuid = CharField(unique=True)
    service_vendor_lcuuid = CharField(unique=True)
    protocol = CharField()
    vendor = CharField()
    create_time = DateTimeField()

    class Meta:
        db_table = 'nas_storage_v2_2'


class BssDomain(Base):
    id = IntegerField()
    name = CharField()
    lcuuid = CharField(unique=True)
    ip = CharField()
    public_ip = CharField()
    role = IntegerField()

    class Meta:
        database = bss_database
        db_table = 'domain'


class VInterface(Base):
    id = IntegerField()
    ifindex = IntegerField()
    state = IntegerField()
    iftype = IntegerField()
    ip = CharField()
    mac = CharField()
    bandw = BigIntegerField()
    ceil_bandw = BigIntegerField()
    broadc_bandw = BigIntegerField()
    broadc_ceil_bandw = BigIntegerField()
    ofport = IntegerField()
    subnetid = IntegerField()
    vlantag = IntegerField()
    devicetype = IntegerField()
    deviceid = IntegerField()
    lcuuid = CharField(unique=True)
    netdevice_lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'vinterface_v2_2'


class Syslog(Base):
    id = IntegerField()
    module = CharField()
    action = CharField()
    objecttype = CharField()
    objectname = CharField()
    objectid = IntegerField()
    loginfo = TextField()
    time = DateTimeField()
    level = IntegerField()

    class Meta:
        db_table = 'syslog_v2_2'


class VGateway(Base):
    id = IntegerField()
    state = IntegerField()
    name = CharField()
    label = CharField()
    epc_id = IntegerField()
    userid = IntegerField()
    gw_launch_server = CharField()
    role = IntegerField()
    conn_max = IntegerField()
    new_conn_per_sec = IntegerField()
    lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'vnet_v2_2'


class ThirdPartyDevice(Base):
    id = IntegerField()
    state = IntegerField()
    name = CharField()
    label = CharField()
    epc_id = IntegerField()
    userid = IntegerField()
    mgmt_ip = CharField()
    role = IntegerField()
    rack_name = CharField()
    lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'third_party_device_v2_2'


class IP(Base):
    id = IntegerField()
    ip = CharField()
    netmask = IntegerField()
    gateway = CharField()
    isp = IntegerField()
    vifid = IntegerField()
    vlantag = IntegerField()
    lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'ip_resource_v2_2'


class VInterfaceIP(Base):
    id = IntegerField()
    ip = CharField()
    net_index = IntegerField()
    netmask = CharField()
    gateway = CharField()
    isp = IntegerField()
    vifid = IntegerField()
    lcuuid = CharField(unique=True)

    class Meta:
        db_table = 'vinterface_ip_v2_2'


class IpResource(Base):
    id = IntegerField()
    ip = CharField()
    netmask = CharField()
    gateway = CharField()
    isp = IntegerField()
    vifid = IntegerField()
    lcuuid = CharField(unique=True)

    class Meta:
        db_table = 'ip_resource_v2_2'


class HostDevice(Base):
    id = IntegerField()
    rackid = IntegerField()
    name = CharField()
    ip = CharField()
    uplink_ip = CharField()
    user_name = CharField()
    user_passwd = CharField()
    type = IntegerField()
    htype = IntegerField()
    rackid = IntegerField()
    lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'host_device_v2_2'


class NetworkDevice(Base):
    id = IntegerField()
    rackid = IntegerField()
    name = CharField()
    mip = CharField()
    brand = CharField()
    tunnel_ip = CharField()
    tunnel_netmask = CharField()
    tunnel_gateway = CharField()
    username = CharField()
    password = CharField()
    enable = CharField()
    boot_time = BigIntegerField()
    lcuuid = CharField(unique=True)
    domain = CharField(unique=True)

    class Meta:
        db_table = 'network_device_v2_2'


class VgwPolicy(Base):
    id = IntegerField()
    state = IntegerField()
    vnetid = IntegerField()
    policy_type = IntegerField()
    isp = IntegerField()
    protocol = IntegerField()
    if_type_1 = CharField()
    if_index_1 = IntegerField()
    min_addr_1 = CharField()
    max_addr_1 = CharField()
    min_port_1 = IntegerField()
    max_port_1 = IntegerField()
    if_type_2 = CharField()
    if_index_2 = IntegerField()
    min_addr_2 = CharField()
    max_addr_2 = CharField()
    min_port_2 = IntegerField()
    max_port_2 = IntegerField()
    action = CharField()

    class Meta:
        db_table = 'vgw_policy_v2_2'


class VgwVpn(Base):
    id = IntegerField()
    state = IntegerField()
    vnetid = IntegerField()
    name = TextField()
    isp = IntegerField()
    local_ip_addr = CharField()
    local_net_addr = CharField()
    local_net_mask = CharField()
    remote_ip_addr = CharField()
    remote_net_addr = CharField()
    remote_net_mask = CharField()
    psk = CharField()

    class Meta:
        db_table = 'vgw_vpn_v2_2'


class VgwRoute(Base):
    id = IntegerField()
    state = IntegerField()
    vnetid = IntegerField()
    isp = IntegerField()
    dst_net_addr = CharField()
    dst_net_mask = CharField()
    next_hop = CharField()

    class Meta:
        db_table = 'vgw_route_v2_2'


class VL2(Base):
    id = IntegerField()
    name = CharField()
    epc_id = IntegerField()
    net_type = IntegerField()
    isp = IntegerField()

    class Meta:
        db_table = 'vl2_v2_2'


class Vl2Net(Base):
    id = IntegerField()
    vl2id = IntegerField()
    net_index = IntegerField()
    prefix = CharField()
    netmask = CharField()

    class Meta:
        db_table = 'vl2_net_v2_2'


class Vl2Vlan(Base):
    id = IntegerField()
    vl2id = IntegerField()
    rackid = IntegerField()
    vlantag = IntegerField()

    class Meta:
        db_table = 'vl2_vlan_v2_2'


class Vl2TunnelPolicy(Base):
    id = IntegerField()
    uplink_ip = CharField()
    vl2id = IntegerField()
    vlantag = IntegerField()

    class Meta:
        db_table = 'vl2_tunnel_policy_v2_2'


class Rack(Base):
    id = IntegerField()
    name = CharField()
    switch_type = IntegerField()

    class Meta:
        db_table = 'rack_v2_2'


class RackTunnel(Base):
    id = IntegerField()
    local_uplink_ip = CharField()
    remote_uplink_ip = CharField()
    switch_ip = CharField()

    class Meta:
        db_table = 'rack_tunnel_v2_2'


class Epc(Base):
    id = IntegerField()
    name = CharField()
    userid = IntegerField()

    class Meta:
        db_table = 'epc_v2_2'


class User(Base):
    id = IntegerField()
    username = CharField()
    email = CharField()
    useruuid = CharField()

    class Meta:
        db_table = 'user_v2_2'


class SysConfig(Base):
    id = IntegerField()
    module_name = CharField()
    param_name = CharField()
    value = TextField()

    class Meta:
        db_table = 'sys_configuration_v2_2'


class DomainConfig(Base):
    id = IntegerField()
    param_name = CharField()
    value = TextField()
    domain = CharField()

    class Meta:
        db_table = 'domain_configuration_v2_2'
