from schematics.exceptions import ValidationError
from schematics.models import Model
from schematics.types.base import (StringType, IPv4Type, UUIDType, IntType)
from schematics.types.compound import ListType
from const import MAX_CONNTRACKS, MAX_CONNTRACKS_PER_SEC


def string_enum(*sequential, **named):
    enums = dict(zip(sequential, sequential), **named)
    return type('StringEnum', (), enums)


class HostAdd(Model):
    """Host add model structure
    """
    _instance_types = ['VM', 'NSP']
    instance_types = string_enum(*_instance_types)
    instance_types_dict = dict(zip(_instance_types, [1, 3]))
    _hypervisor_types = ['XEN', 'VMWARE', 'KVM']
    hypervisor_types = string_enum(*_hypervisor_types)
    hypervisor_types_dict = dict(zip(_hypervisor_types, [1, 2, 3]))
    _nic_types = ['GIGABIT', 'XGIGABIT']
    nic_types = string_enum(*_nic_types)
    nic_types_dict = dict(zip(_nic_types, [1, 2]))

    ip = IPv4Type(required=True, serialized_name='IP')
    user_name = StringType(required=True, serialized_name='USER_NAME')
    user_passwd = StringType(required=True, serialized_name='USER_PASSWD')
    instance_type = StringType(required=True, serialized_name='INSTANCE_TYPE',
                               choices=_instance_types)
    rack_name = StringType(required=True, serialized_name='RACK_NAME')
    hypervisor_type = StringType(required=True,
                                 serialized_name='HYPERVISOR_TYPE',
                                 choices=_hypervisor_types)
    nic_type = StringType(required=True, serialized_name='NIC_TYPE',
                          choices=_nic_types)
    uplink_ip = IPv4Type(serialized_name='UPLINK_IP')
    uplink_netmask = IPv4Type(serialized_name='UPLINK_NETMASK')
    uplink_gateway = IPv4Type(serialized_name='UPLINK_GATEWAY')
    misc = StringType(serialized_name='MISC')
    brand = StringType(serialized_name='BRAND')
    model = StringType(serialized_name='MODEL')
    storage_link_ip = IPv4Type(serialized_name='STORAGE_LINK_IP')


class HostRescan(Model):
    """Host rescan model structure
    """
    ip = IPv4Type(required=True, serialized_name='IP')


class BackupPriceGet(Model):
    """ Backup Price Get structure
    """
    name = StringType(serialized_name='NAME', required=True)
    quantity = StringType(serialized_name='QUANTITY', required=True)
    domain = UUIDType(serialized_name='DOMAIN', required=True)


class BackupChargeAdd(Model):
    """Backup Charge add structure
    """
    name = StringType(required=True, serialized_name='NAME')
    userid = IntType(required=True, serialized_name='USERID')
    start_time = StringType(required=True, serialized_name='START_TIME')
    size = IntType(required=True, serialized_name='SIZE')
    domain = UUIDType(serialized_name='DOMAIN', required=True)
    charge_mode = IntType(required=True, serialized_name='CHARGE_MODE')
    price = IntType(required=True, serialized_name='PRICE')


class VmPlanRequest(Model):
    """VM plan price request structure
    """
    cpu = IntType(required=True, serialized_name='CPU')
    memory = IntType(required=True, serialized_name='MEMORY')
    disk = IntType(required=True, serialized_name='DISK')
    domain = UUIDType(serialized_name='DOMAIN')


class PriceRequest(Model):
    """Price request structure
    """
    name = StringType(serialized_name='NAME', required=True)
    quantity = StringType(serialized_name='QUANTITY', required=True)
    domain = UUIDType(serialized_name='DOMAIN', required=True)


class StorageAdd(Model):
    """Storage add model structure
    """
    _backends = ['CEPH_RBD']
    backends = string_enum(*_backends)
    backends_dict = dict(zip(_backends, [1]))

    _types = ['CAPACITY', 'PERFORMANCE']
    types = string_enum(*_types)

    name = StringType(required=True, serialized_name='NAME')
    uuid = UUIDType(serialized_name='UUID')
    disk_total = IntType(required=True, serialized_name='DISK_TOTAL')
    disk_used = IntType(required=True, serialized_name='DISK_USED')
    backend = StringType(required=True, serialized_name='BACKEND',
                         choices=_backends)
    hosts = ListType(IPv4Type, serialized_name='HOSTS', default=[])
    type = StringType(required=True, serialized_name='TYPE', choices=_types)
    domain = UUIDType(serialized_name='DOMAIN', required=True)


class PyagexecStorage(Model):
    pool_name = StringType(required=True, serialized_name='POOL_NAME')
    uuid = UUIDType(serialized_name='UUID')
    total_size = IntType(required=True, serialized_name='TOTAL_SIZE')
    used_size = IntType(required=True, serialized_name='USED_SIZE')

    def validate_used_size(self, data, value):
        if data['total_size'] < value or value < 0:
            raise ValidationError('0 <= used_size <= total_size')
        return value


class Conntrack(Model):
    conn_max = IntType(
        serialized_name='CONN_MAX', default=0,
        required=True, min_value=0, max_value=MAX_CONNTRACKS)
    new_conn_per_sec = IntType(
        serialized_name='NEW_CONN_PER_SEC', default=0,
        required=True, min_value=0, max_value=MAX_CONNTRACKS_PER_SEC)

    def validate_new_conn_per_sec(self, data, value):
        if data['conn_max'] != 0 and value > data['conn_max']:
            raise ValidationError(
                'conn_per_sec can not be larger than conn_max')
        return value


class VMImport(Model):
    name = StringType(serialized_name='NAME', required=True)
    os = StringType(serialized_name='OS', required=True)
    launch_server = StringType(serialized_name='LAUNCH_SERVER', required=True)
    userid = IntType(serialized_name='USERID', required=True)
    product_specification_lcuuid = UUIDType(
        serialized_name='PRODUCT_SPECIFICATION_LCUUID', required=True)
    domain = UUIDType(required=True, serialized_name='DOMAIN')


class VolImport(Model):
    name = StringType(serialized_name='NAME', required=True)
    storage_lcuuid = UUIDType(serialized_name='STORAGE_LCUUID', required=True)
    user_lcuuid = StringType(serialized_name='USER_LCUUID', required=True)
    product_specification_lcuuid = UUIDType(
        serialized_name='PRODUCT_SPECIFICATION_LCUUID', required=True)
