from sqlalchemy import create_engine
from sqlalchemy.orm import scoped_session, sessionmaker
from sqlalchemy.ext.declarative import declarative_base, declared_attr
from sqlalchemy import Column
from sqlalchemy.dialects.mysql import \
    BIGINT, CHAR, DATETIME, INTEGER, TIMESTAMP, VARCHAR
import re
from threading import current_thread
import datetime
import uuid
from time import time
from os import system
import sys
import random

DB_NAME = 'livecloud'
DB_USER = 'root'
DB_PASS = 'security421'
DB_VER = '2_2'
TABLE_SUFFIX = '_v' + DB_VER
DATE_PATTEN = '%Y-%m-%d %H:%M:%S'
WORKER_THREAD_NUM = 16
TOTAL_THREAD_NUM = WORKER_THREAD_NUM + 3
VIF_STATE_DETACHED = 2

MAX_NUM = 1001

engine = create_engine('mysql+mysqldb://%s:%s@localhost/%s?charset=utf8' %
                       (DB_USER, DB_PASS, DB_NAME),
                       echo=False, pool_size=TOTAL_THREAD_NUM,
                       pool_recycle=3600)

db_sessions = {}


def bin_to_ip(binaddr):
    """binary IP to string
    """
    return '%d.%d.%d.%d' % (binaddr >> 24,
                            (binaddr & 0x00FFFFFF) >> 16,
                            (binaddr & 0x0000FFFF) >> 8,
                            binaddr & 0x000000FF)


def bin_to_mac(bin):
    """binary MAC to string
    """
    local_mac = uuid.uuid1().hex[-12:]
    mac = [random.randint(0x00, 0xff), random.randint(0x00, 0xff)]
    s = [local_mac[0:2], local_mac[2:4], local_mac[4:6], local_mac[6:8]]
    for item in mac:
        s.append(str("%x" % item))
    return ( ':'.join(s) )


def get_db_session():
    """Get a DB session for thread

    Create session if not exist
    """
    id = current_thread().ident
    if id in db_sessions:
        return db_sessions[id]
    else:
        s = scoped_session(sessionmaker(autocommit=False,
                                        autoflush=False,
                                        bind=engine))
        db_sessions[id] = s
        return s


def shutdown_db_session(session=None):
    """Remove DB session
    """
    id = current_thread().ident
    if id in db_sessions:
        db_sessions[id].remove()
        del db_sessions[id]
    elif session is not None:
        session.remove()

Base = declarative_base()


class LCBase(object):

    @declared_attr
    def __tablename__(cls):

        if cls.__name__ == 'VInterface':
            name = 'vinterface'
        elif cls.__name__ == 'VGW':
            name = 'vnet'
        elif cls.__name__ == 'VMWaf':
            name = 'vmwaf'
        elif cls.__name__ == 'HADisk':
            name = 'vdisk'
        elif cls.__name__ == 'VL2NET':
            name = 'vl2_net'
        elif cls.__name__ == 'VL2VLAN':
            name = 'vl2_vlan'
        elif cls.__name__ == 'VInterfaceIp':
            name = 'vinterface_ip'
        elif cls.__name__ == 'SysConfiguration':
            name = 'sys_configuration'
        else:
            s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', cls.__name__)
            name = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()
        return name + TABLE_SUFFIX

    def __init__(self, dict_data=None):
        """Init instance from dict_data

        :param dict_data: json data to parse
        :type dict_data: dict
        """
        if dict_data is not None:
            for key, value in dict_data.items():
                if key == 'LCID':
                    self.lcid = value
                    continue
                real_key = key.lower()

                # validate json data
                try:
                    getattr(self, real_key)
                except:
                    continue

                setattr(self, real_key, value)


class Cluster(LCBase, Base):

    id = Column(INTEGER(), primary_key=True, nullable=False)
    name = Column(VARCHAR(length=64))
    location = Column(VARCHAR(length=256))
    admin = Column(VARCHAR(length=64))
    deploy_time = Column(DATETIME())
    platform = Column(VARCHAR(length=64))
    support = Column(VARCHAR(length=64))
    domain = Column(CHAR(length=64), primary_key=True, nullable=False)
    vc_datacenter_id = Column(INTEGER())
    lcuuid = Column(CHAR(length=64))


class VM(LCBase, Base):

    id = Column('id', INTEGER(), primary_key=True, nullable=False)
    state = Column('state', INTEGER(), nullable=False)
    pre_state = Column('pre_state', INTEGER(), nullable=False)
    errno = Column('errno', INTEGER())
    eventid = Column('eventid', INTEGER())
    action = Column('action', INTEGER())
    name = Column('name', CHAR(length=64))
    label = Column('label', CHAR(length=64))
    os = Column('os', CHAR(length=64))
    flag = Column('flag', INTEGER())
    template = Column('template', CHAR(length=64))
    import_from = Column('import', VARCHAR(length=256))
    poolid = Column('poolid', INTEGER())
    launch_server = Column('launch_server', CHAR(length=16))
    des_host = Column('des_host', CHAR(length=16))
    opaque_id = Column('opaque_id', VARCHAR(length=64))
    vdi_sys_uuid = Column('vdi_sys_uuid', CHAR(length=64))
    vdi_sys_sr_uuid = Column('vdi_sys_sr_uuid', CHAR(length=64))
    vdi_sys_sr_name = Column('vdi_sys_sr_name', CHAR(length=64))
    vdi_sys_size = Column('vdi_sys_size', INTEGER())
    vdi_user_uuid = Column('vdi_user_uuid', CHAR(length=64))
    vdi_user_sr_uuid = Column('vdi_user_sr_uuid', CHAR(length=64))
    vdi_user_sr_name = Column('vdi_user_sr_name', CHAR(length=64))
    vdi_user_size = Column('vdi_user_size', INTEGER())
    mem_size = Column('mem_size', INTEGER())
    vcpu_num = Column('vcpu_num', INTEGER())
    tport = Column('tport', INTEGER())
    lport = Column('lport', INTEGER())
    health_state = Column('health_state', INTEGER())
    cpu_start_time = Column('cpu_start_time', BIGINT())
    disk_start_time = Column('disk_start_time', BIGINT())
    traffic_start_time = Column('traffic_start_time', BIGINT())
    check_start_time = Column('check_start_time', BIGINT())
    uuid = Column('uuid', CHAR(length=64))
    domain = Column('domain', CHAR(length=64), primary_key=True,
                    nullable=False)
    exclude_server = Column('exclude_server', VARCHAR(length=128))
    thumbprint = Column('thumbprint', CHAR(length=64))
    vm_path = Column('vm_path', VARCHAR(length=128))
    userid = Column('userid', INTEGER())
    epc_id = Column('epc_id', INTEGER(), default=0)
    is_expired = Column('is_expired', INTEGER())
    expired_time = Column('expired_time', TIMESTAMP(),
                          default=datetime.datetime.now)
    lcuuid = Column('lcuuid', CHAR(length=64))
    product_specification_lcuuid = Column('product_specification_lcuuid',
                                          CHAR(length=64), default='')
    gateway = Column('gateway', CHAR(length=16), default='')
    role = Column('role', INTEGER())
    loopback_ips = Column('loopback_ips', CHAR(length=256), default='')


class ComputeResource(LCBase, Base):

    bandwidth_total = Column('bandwidth_total', INTEGER())
    bandwidth_usage = Column('bandwidth_usage', INTEGER())
    check_start_time = Column('check_start_time', BIGINT())
    cpu_alloc = Column('cpu_alloc', INTEGER(), default=0)
    cpu_info = Column('cpu_info', VARCHAR(length=128))
    cpu_start_time = Column('cpu_start_time', BIGINT())
    cpu_usage = Column('cpu_usage', VARCHAR(length=1024))
    cpu_used = Column('cpu_used', INTEGER(), default=0)
    dbr_peer_server = Column('dbr_peer_server', CHAR(length=16))
    disk_alloc = Column('disk_alloc', INTEGER(), default=0)
    disk_start_time = Column('disk_start_time', BIGINT())
    disk_total = Column('disk_total', INTEGER())
    disk_usage = Column('disk_usage', INTEGER())
    domain = Column('domain', CHAR(length=64), primary_key=True,
                    nullable=False)
    errno = Column('errno', INTEGER())
    eventid = Column('eventid', INTEGER())
    flag = Column('flag', INTEGER())
    ha_disk_total = Column('ha_disk_total', INTEGER())
    ha_disk_usage = Column('ha_disk_usage', INTEGER())
    health_state = Column('health_state', INTEGER())
    id = Column('id', INTEGER(), primary_key=True,
                nullable=False)
    ip = Column('ip', CHAR(length=16))
    linkespeed = Column('linkespeed', INTEGER())
    master_ip = Column('master_ip', CHAR(length=16))
    max_vm = Column('max_vm', INTEGER())
    mem_alloc = Column('mem_alloc', INTEGER(), default=0)
    mem_total = Column('mem_total', INTEGER(), default=0)
    mem_usage = Column('mem_usage', INTEGER(), default=0)
    name = Column('name', VARCHAR(length=64))
    poolid = Column('poolid', INTEGER())
    rackid = Column('rackid', INTEGER())
    role = Column('role', INTEGER())
    service_flag = Column('service_flag', INTEGER())
    state = Column('state', INTEGER())
    stats_time = Column('stats_time', BIGINT())
    traffic_start_time = Column('traffic_start_time', BIGINT())
    type = Column('type', INTEGER())
    uplink_gateway = Column('uplink_gateway', CHAR(length=16))
    uplink_ip = Column('uplink_ip', CHAR(length=16))
    uplink_netmask = Column('uplink_netmask', CHAR(length=16))
    user_name = Column('user_name', VARCHAR(length=64))
    user_passwd = Column('user_passwd', VARCHAR(length=64))
    uuid = Column('uuid', CHAR(length=64))
    lcuuid = Column('lcuuid', CHAR(length=64))


class Storage(LCBase, Base):

    # column definitions
    id = Column('id', INTEGER(), primary_key=True, nullable=False)
    name = Column('name', VARCHAR(length=64))
    rack_id = Column('rack_id', INTEGER())
    uuid = Column('uuid', CHAR(length=64))
    is_shared = Column('is_shared', INTEGER())
    disk_total = Column('disk_total', INTEGER())
    disk_used = Column('disk_used', INTEGER())
    disk_alloc = Column('disk_alloc', INTEGER(), default=0)


class VInterface(LCBase, Base):

    id = Column('id', INTEGER(), primary_key=True, nullable=False)
    name = Column('name', CHAR(length=64))
    ifindex = Column('ifindex', INTEGER(), nullable=False)
    state = Column(
        'state', INTEGER(),
        nullable=False, default=VIF_STATE_DETACHED)
    flag = Column('flag', INTEGER())
    errno = Column('errno', INTEGER())
    eventid = Column('eventid', INTEGER())
    ip = Column('ip', CHAR(length=16))
    netmask = Column('netmask', INTEGER())
    iftype = Column('iftype', INTEGER(), default=0)
    gateway = Column('gateway', CHAR(length=16))
    mac = Column('mac', CHAR(length=32))
    bandw = Column('bandw', INTEGER())
    ofport = Column('ofport', INTEGER(), default=0)
    subnetid = Column('subnetid', INTEGER(), default=0)
    vlantag = Column('vlantag', INTEGER(), default=0)
    devicetype = Column('devicetype', INTEGER())
    deviceid = Column('deviceid', INTEGER())
    port_id = Column('port_id', VARCHAR(length=64))
    pg_id = Column('pg_id', VARCHAR(length=64))
    netdevice_lcuuid = Column('netdevice_lcuuid', CHAR(length=64))
    domain = Column('domain', CHAR(length=64), primary_key=True,
                    nullable=False)
    lcuuid = Column('lcuuid', CHAR(length=64))


class VGW(LCBase, Base):

    id = Column('id', INTEGER(),
                primary_key=True, nullable=False)
    state = Column('state', INTEGER(), nullable=False)
    pre_state = Column('pre_state', INTEGER(), nullable=False)
    errno = Column('errno', INTEGER(), default=0)
    eventid = Column('eventid', INTEGER(), default=0)
    name = Column('name', CHAR(length=64))
    label = Column('label', CHAR(length=64))
    description = Column('description', VARCHAR(length=256))
    vl2_num = Column('vl2_num', INTEGER())
    gw_poolid = Column('gw_poolid', INTEGER())
    gw_launch_server = Column('gw_launch_server', CHAR(length=16))
    gwtemplate = Column('gwtemplate', CHAR(length=64))
    gwimport = Column('gwimport', VARCHAR(length=256))
    flag = Column('flag', INTEGER(), default=0)
    opaque_id = Column('opaque_id', INTEGER())
    vdi_sys_sr_name = Column('vdi_sys_sr_name', VARCHAR(length=64))
    tport = Column('tport', INTEGER())
    lport = Column('lport', INTEGER())
    uuid = Column('uuid', CHAR(length=64))
    domain = Column('domain', CHAR(length=64), primary_key=True,
                    nullable=False)
    health_state = Column('health_state', INTEGER())
    cpu_start_time = Column('cpu_start_time', BIGINT(), default=0)
    disk_start_time = Column('disk_start_time', BIGINT(), default=0)
    check_start_time = Column('check_start_time', BIGINT(), default=0)
    exclude_server = Column('exclude_server', VARCHAR(length=128))
    passwd = Column('passwd', CHAR(length=32))
    gw_des_server = Column('gw_des_server', CHAR(length=16))
    haflag = Column('haflag', INTEGER(), default=0)
    hastate = Column('hastate', INTEGER(), default=0)
    havrid = Column('havrid', INTEGER(), default=0)
    haswitchmode = Column('haswitchmode', INTEGER(), default=0)
    action = Column('action', INTEGER(), default=0)
    userid = Column('userid', INTEGER(), nullable=False)
    epc_id = Column('epc_id', INTEGER(), default=0)
    lcuuid = Column('lcuuid', CHAR(length=64))
    product_specification_lcuuid = Column('product_specification_lcuuid',
                                          CHAR(length=64), default='')


'''
VM
'''
def create_vm():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        vm = VM()
        vm.lcuuid = str(uuid.uuid4())
        vm.name = 'vm' + str(i)
        vm.state = i
        vm.pre_state = 2
        vm.label = vm.name
        vm.os = 'centos6.5'
        vm.flag = 1
        vm.template = 'template'
        vm.poolid = 1
        vm.launch_server = '172.16.1.112'
        vm.gateway = '192.168.0.1'
        vm.epc_id = 1
        vm.vcloudid = 1
        vm.vnetid = 1
        vm.vl2id = 1
        vm.vdi_sys_uuid = str(uuid.uuid4())
        vm.vdi_sys_sr_uuid = str(uuid.uuid4())
        vm.vdi_sys_sr_name = 'SR_Local'
        vm.vdi_sys_size = 30
        vm.vdi_user_size = 0
        vm.mem_size = 4096
        vm.vcpu_num = 4
        vm.tport = 25018
        vm.lport = 35018
        vm.uuid = str(uuid.uuid4())
        vm.userid = 4
        vm.product_specification_lcuuid = str(uuid.uuid4())
        vm.role = 2
        db_session.add(vm)

    db_session.flush()
    db_session.commit()


def get_vm():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vm' + str(i)
        vm = db_session.query(VM).\
            filter("state=%d and launch_server='172.16.1.112'" % i).first()
        if vm is None:
            system('echo -e failed find %s >> err_log' % name)


def update_vm():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vm' + str(i)
        vm = db_session.query(VM).\
            filter("state=%d and launch_server='172.16.1.112'" % i).first()
        if vm is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            vm.pre_state = 4

    db_session.flush()
    db_session.commit()


def delete_vm():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vm' + str(i)
        vm = db_session.query(VM).\
            filter("state=%d and launch_server='172.16.1.112'" % i).first()
        if vm is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            db_session.delete(vm)

    db_session.commit()


def mysql_vm():
    start = int(time())
    cmd = 'echo -e start time: %d >> runtime' % start
    system(cmd)

    create_vm()
    t = int(time())
    cmd = 'echo -e create end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_vm()
    t = int(time())
    cmd = 'echo -e get end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    update_vm()
    t = int(time())
    cmd = 'echo -e update end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    delete_vm()
    t = int(time())
    cmd = 'echo -e delete end: %d >> runtime' % (t - start)
    system(cmd)


def handle_vm():
    system('echo -e insert update search delete without index: >> runtime')
    mysql_vm()
    system('echo -e add index: >> runtime')
    system("mysql livecloud -e 'alter table vm_v2_2 add INDEX state_server_index(state, launch_server);'")
    mysql_vm()
    system("mysql livecloud -e 'alter table vm_v2_2 drop index state_server_index;'")


'''
COMPUTE_RESOURCE
'''
def create_cr():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        cr = ComputeResource()
        cr.lcuuid = str(uuid.uuid4())
        cr.bandwidth_total = 1024 * 1024
        cr.bandwidth_usage = 1024
        cr.cpu_alloc = 0
        cr.cpu_info = '12,GenuineIntel,Intel(R) Xeon(R) CPU E5-2420 0 @ 1.90GHz'
        cr.cpu_usage = '0,0.000#1,0.000#2,0.000#3,0.001#4,0.014#5,0.001#6,0.013#7,0.000#8,0.000#9,0.012#10,0.023#11,0.000#'
        cr.cpu_used = 22
        cr.disk_alloc = 0
        cr.disk_total = 0
        cr.disk_usage = 0
        cr.flag = 0
        cr.ha_disk_total = 0
        cr.ha_disk_usage = 0
        cr.health_state = 0
        cr.mem_alloc = 0
        cr.mem_total = 0
        cr.mem_usage = 0
        cr.name = 'xen' + str(i)
        cr.poolid = 1
        cr.rackid = 1
        cr.role = 1
        cr.state = 2
        cr.type = i % 10
        cr.user_name = 'root'
        cr.user_passwd = 'yunshan3302'
        cr.ip = bin_to_ip(i)
        cr.master_ip = cr.ip

        db_session.add(cr)

    db_session.flush()
    db_session.commit()


def get_cr():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        ip = bin_to_ip(i)
        cr = db_session.query(ComputeResource).\
            filter("ip='%s'" % ip).first()
        if cr is None:
            system('echo -e failed find %s >> err_log' % ip)


def get_cr_by_type():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        type = i % 10
        cr = db_session.query(ComputeResource).\
            filter("type=%d" % type).first()
        if cr is None:
            system('echo -e failed find %d >> err_log' % type)


def update_cr():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        ip = bin_to_ip(i)
        cr = db_session.query(ComputeResource).\
            filter("ip='%s'" % ip).first()
        if cr is None:
            system('echo -e failed find %s >> err_log' % ip)
        else:
            cr.state = 4

    db_session.flush()
    db_session.commit()


def delete_cr():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        ip = bin_to_ip(i)
        cr = db_session.query(ComputeResource).\
            filter("ip='%s'" % ip).first()
        if cr is None:
            system('echo -e failed find %s >> err_log' % ip)
        else:
            db_session.delete(cr)

    db_session.commit()


def mysql_cr():
    start = int(time())
    cmd = 'echo -e start time: %d >> runtime' % start
    system(cmd)

    create_cr()
    t = int(time())
    cmd = 'echo -e create end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_cr()
    t = int(time())
    cmd = 'echo -e get by ip end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_cr_by_type()
    t = int(time())
    cmd = 'echo -e get by type end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    update_cr()
    t = int(time())
    cmd = 'echo -e update end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    delete_cr()
    t = int(time())
    cmd = 'echo -e delete end: %d >> runtime' % (t - start)
    system(cmd)


def handle_cr():
    system('echo -e insert update search delete without index: >> runtime')
    mysql_cr()
    system('echo -e add index: >> runtime')
    system("mysql livecloud -e 'alter table compute_resource_v2_2 add UNIQUE index ip_index(ip);'")
    system("mysql livecloud -e 'alter table compute_resource_v2_2 add index type_index(type);'")
    mysql_cr()
    system("mysql livecloud -e 'alter table compute_resource_v2_2 drop index ip_index;'")
    system("mysql livecloud -e 'alter table compute_resource_v2_2 drop index type_index;'")


'''
STORAGE
'''
def create_storage():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        st = Storage()
        st.uuid = str(uuid.uuid4())
        st.name = 'st' + str(i)
        st.rack_id = 1
        st.is_shared = 0
        st.disk_total = 0
        st.disk_used = 0
        st.disk_alloc = 0
        db_session.add(st)

    db_session.flush()
    db_session.commit()


def get_storage():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'st' + str(i)
        st = db_session.query(Storage).\
            filter("name='%s'" % name).first()
        if st is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            st2 = db_session.query(Storage).\
                filter("uuid='%s'" % st.uuid).first()


def update_storage():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'st' + str(i)
        st = db_session.query(Storage).\
            filter("name='%s'" % name).first()
        if st is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            st.is_shared = 1

    db_session.flush()
    db_session.commit()


def delete_storage():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'st' + str(i)
        st = db_session.query(Storage).\
            filter("name='%s'" % name).first()
        if st is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            db_session.delete(st)

    db_session.commit()


def mysql_storage():
    start = int(time())
    cmd = 'echo -e start time: %d >> runtime' % start
    system(cmd)

    create_storage()
    t = int(time())
    cmd = 'echo -e create end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_storage()
    t = int(time())
    cmd = 'echo -e get end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    update_storage()
    t = int(time())
    cmd = 'echo -e update end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    delete_storage()
    t = int(time())
    cmd = 'echo -e delete end: %d >> runtime' % (t - start)
    system(cmd)


def handle_storage():
    system('echo -e insert update search delete without index: >> runtime')
    mysql_storage()
    system('echo -e add index: >> runtime')
    system("mysql livecloud -e 'alter table storage_v2_2 add UNIQUE INDEX uuid_index(uuid);'")
    mysql_storage()
    system("mysql livecloud -e 'alter table storage_v2_2 drop index uuid_index;'")

'''
VINTERFACE
'''
def create_vif():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        vif = VInterface()
        vif.name = 'vif' + str(i)
        vif.ifindex = i
        vif.state = 4
        vif.flag = 1
        vif.errno = 0
        vif.eventid = 0
        vif.ip = bin_to_ip(i)
        vif.netmask = 24
        vif.iftype = 0
        vif.gateway = '10.0.0.1'
        vif.mac = bin_to_mac(i)
        vif.bandw = 0
        vif.ofport = 0
        vif.subnetid = 1
        vif.vlantag = 0
        vif.devicetype = 1
        vif.deviceid = 1
        vif.port_id = ''
        vif.pg_id = ''
        vif.netdevice_lcuuid = str(uuid.uuid4())
        vif.lcuuid = str(uuid.uuid4())
        db_session.add(vif)

    db_session.flush()
    db_session.commit()


def get_vif():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vif' + str(i)
        vif = db_session.query(VInterface).\
            filter("name='%s'" % name).first()
        if vif is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            vif2 = db_session.query(VInterface).\
                filter("mac='%s'" % vif.mac).first()


def update_vif():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vif' + str(i)
        vif = db_session.query(VInterface).\
            filter("name='%s'" % name).first()
        if vif is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            vif.state = 7

    db_session.flush()
    db_session.commit()


def delete_vif():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vif' + str(i)
        vif = db_session.query(VInterface).\
            filter("name='%s'" % name).first()
        if vif is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            db_session.delete(vif)

    db_session.commit()


def mysql_vif():
    start = int(time())
    cmd = 'echo -e start time: %d >> runtime' % start
    system(cmd)

    create_vif()
    t = int(time())
    cmd = 'echo -e create end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_vif()
    t = int(time())
    cmd = 'echo -e get end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    update_vif()
    t = int(time())
    cmd = 'echo -e update end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    delete_vif()
    t = int(time())
    cmd = 'echo -e delete end: %d >> runtime' % (t - start)
    system(cmd)


def handle_vif():
    system('echo -e insert update search delete without index: >> runtime')
    mysql_vif()
    system('echo -e add index: >> runtime')
    system("mysql livecloud -e 'alter table vinterface_v2_2 add INDEX mac_index(mac);'")
    mysql_vif()
    system("mysql livecloud -e 'alter table vinterface_v2_2 drop index mac_index;'")

'''
VNET
'''
def create_vgw():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        vgw = VGW()
        vgw.state = i
        vgw.pre_state = 1
        vgw.errno = 0
        vgw.eventid = 0
        vgw.name = 'vgw' + str(i)
        vgw.label = vgw.name
        vgw.description = ''
        vgw.vl2_num = 0
        vgw.gw_poolid = 1
        vgw.gw_launch_server = '172.16.1.106'
        vgw.gwtemplate = 'vgateway'
        vgw.gwimport = 'import'
        vgw.flag = 0
        vgw.opaque_id = 0
        vgw.vdi_sys_sr_name = 'SR_Local'
        vgw.tport = 0
        vgw.lport = 0
        vgw.uuid = str(uuid.uuid4())
        vgw.health_state = 0
        vgw.cpu_start_time = 0
        vgw.disk_start_time = 0
        vgw.check_start_time = 0
        vgw.exclude_server = ''
        vgw.passwd = 'security421'
        vgw.gw_des_server = '172.16.1.106'
        vgw.haflag = 0
        vgw.hastate = 0
        vgw.havrid = 0
        vgw.haswitchmode = 0
        vgw.action = 0
        vgw.userid = 1
        vgw.epc_id = 1
        vgw.lcuuid = str(uuid.uuid4())
        vgw.product_specification_lcuuid = str(uuid.uuid4())
        db_session.add(vgw)

    db_session.flush()
    db_session.commit()


def get_vgw():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vgw' + str(i)
        vgw = db_session.query(VGW).\
            filter("state=%d and gw_launch_server='172.16.1.106'" % i).first()
        if vgw is None:
            system('echo -e failed find %s >> err_log' % name)


def update_vgw():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vgw' + str(i)
        vgw = db_session.query(VGW).\
            filter("state=%d and gw_launch_server='172.16.1.106'" % i).first()
        if vgw is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            vgw.pre_state = 4

    db_session.flush()
    db_session.commit()


def delete_vgw():
    db_session = get_db_session()

    for i in xrange(1, MAX_NUM):
        name = 'vgw' + str(i)
        vgw = db_session.query(VGW).\
            filter("state=%d and gw_launch_server='172.16.1.106'" % i).first()
        if vgw is None:
            system('echo -e failed find %s >> err_log' % name)
        else:
            db_session.delete(vgw)

    db_session.commit()


def mysql_vgw():
    start = int(time())
    cmd = 'echo -e start time: %d >> runtime' % start
    system(cmd)

    create_vgw()
    t = int(time())
    cmd = 'echo -e create end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    get_vgw()
    t = int(time())
    cmd = 'echo -e get end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    update_vgw()
    t = int(time())
    cmd = 'echo -e update end: %d >> runtime' % (t - start)
    system(cmd)
    start = t

    delete_vgw()
    t = int(time())
    cmd = 'echo -e delete end: %d >> runtime' % (t - start)
    system(cmd)


def handle_vgw():
    system('echo -e insert update search delete without index: >> runtime')
    mysql_vgw()
    system('echo -e add index: >> runtime')
    system("mysql livecloud -e 'alter table vnet_v2_2 add INDEX state_server_index(state, gw_launch_server);'")
    mysql_vgw()
    system("mysql livecloud -e 'alter table vnet_v2_2 drop index state_server_index;'")

system('rm -f runtime')
system('rm -f err_log')
system('touch runtime')

def print_help(cmd):
    print 'Usage: '
    print '       python %s compute_resource | vm | storage | vinterface | vgw' % cmd
    exit(1)

if len(sys.argv) < 2:
    print_help(sys.argv[0])

handle = {'compute_resource': handle_cr,
          'vm': handle_vm,
          'storage': handle_storage,
          'vinterface': handle_vif,
          'vgw': handle_vgw}

if sys.argv[1] not in handle:
    print_help(sys.argv[0])

handle[sys.argv[1]]()
