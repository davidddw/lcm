import datetime
import re
import simplejson as json
import subprocess
import uuid
import chardet
from schematics.exceptions import ValidationError
from schematics.models import Model
from schematics.transforms import whitelist
from schematics.types import StringType, DecimalType, IntType
from schematics.types.compound import ListType, ModelType
import copy
import decimal

import data

DIALOG = '/usr/bin/dialog'
BW_PRICE_WEIGHT = '{"1":0,"2":0,"3":0,"4":0,"5":0,"6":0,"7":0,"8":0}'


def call_dialog(args):
    assert isinstance(args, list)
    real_args = [DIALOG, '--ascii-lines',
                 '--backtitle', 'Product Specification Utilities']
    real_args.extend(args)
    child = subprocess.Popen(real_args, stderr=subprocess.PIPE)
    _, err = child.communicate()
    return child.returncode, err


def msgbox(msg):
    width = 2 * len(msg)
    args = ['--title', 'Error', '--msgbox', msg, '5', '%d' % width]
    call_dialog(args)


def menu(title, info, items, default=''):
    assert isinstance(title, basestring)
    assert isinstance(info, basestring)
    assert isinstance(items, list)
    menulen = len(items) + 1
    windowlen = menulen + 7
    itemlist = []
    maxwidth = 21
    infolen = len(info) + 5
    if infolen > maxwidth:
        maxwidth = infolen
    default_id = 0
    for i in range(len(items)):
        itemlist.append('%d' % i)
        assert isinstance(items[i], basestring)
        itemlist.append(items[i])
        width = len(itemlist[-2]) + len(itemlist[-1]) + 10
        if width > maxwidth:
            maxwidth = width
        if items[i] == default:
            default_id = i
    args = ['--title', title,
            '--default-item', str(default_id),
            '--menu', info, '%d' % windowlen, '%d' % maxwidth, '%d' % menulen]
    args.extend(itemlist)
    ret, out = call_dialog(args)
    if ret == 0:
        return items[int(out)]
    else:
        return None


def inputbox(title, info, width, string):
    assert isinstance(title, basestring)
    assert isinstance(info, basestring)
    assert isinstance(string, basestring)
    args = ['--title', title, '--inputbox', info, '9', '%d' % width, string]
    ret, out = call_dialog(args)
    if ret == 0:
        return out
    else:
        return None

display_type_to_db_type = {
    'CPU': 'cpu',
    'Memory': 'memory',
    'Disk': 'disk',
    'Load Balancer': 'load-balancer',
    'Virtual Gateway': 'vgw',
    'Bandwidth': 'bandwidth',
    'IP Address': 'ip',
    'NAS Storage': 'nas-storage',
    'Cloud Disk': 'cloud-disk',
    'Third Party Device': 'tpd',
    'System Vulnerability Scanner': 'sys-vul-scanner',
    'Website Vulnerability Scanner': 'web-vul-scanner',
    'VM Snapshot': 'vm-snapshot',
    'Backup Storage Pool': 'backup-storage-pool',
    'Normal Recovery': 'normal-recovery',
    'Disaster Recovery': 'disaster-recovery',
    'Emergency Recovery': 'emergency-recovery',
    'Virtual Firewall': 'vfw',
    'Bandwidth Sharer': 'valve',
    'Backup': 'backup',
    'BlockDevice': 'block-device',
    'BlockDevice Snapshot': 'block-device-snapshot',
}

db_type_to_display_type = (
    dict((v, k) for k, v in display_type_to_db_type.items()))

display_type_to_int_type = {
    'Virtual Gateway': 2,
    'Third Party Device': 3,
    'IP Address': 4,
    'Bandwidth': 5,
    'Website Vulnerability Scanner': 6,
    'System Vulnerability Scanner': 7,
    'NAS Storage': 8,
    'Load Balancer': 9,
    'Cloud Disk': 10,
    'VM Snapshot': 11,
    'Backup Storage Pool': 12,
    'Normal Recovery': 13,
    'Disaster Recovery': 14,
    'Emergency Recovery': 15,
    'Virtual Firewall': 16,
    'Bandwidth Sharer': 17,
    'Backup': 18,
    'BlockDevice': 19,
    'BlockDevice Snapshot': 20,
}

display_type_to_db_name = {
    'CPU': 'cpu',
    'Memory': 'memory',
    'Disk': 'disk'
}

ps_charge_mode = {
    'per-second': 1,
    'per-minute': 2,
    'per-hour': 3,
    'per-day': 4,
    'per-use': 5
}


def get_type(default=''):
    choices = [
        'CPU',
        'Memory',
        'Disk',
        'Load Balancer',
        'Virtual Gateway',
        'Bandwidth',
        'IP Address',
        'NAS Storage',
        'Cloud Disk',
        'Third Party Device',
        'System Vulnerability Scanner',
        'Website Vulnerability Scanner',
        'VM Snapshot',
        'Backup Storage Pool',
        'Normal Recovery',
        'Disaster Recovery',
        'Emergency Recovery',
        'Virtual Firewall',
        'Bandwidth Sharer',
        'Backup',
        'BlockDevice',
        'BlockDevice Snapshot'
    ]
    return menu('Product Types', 'Product type is', choices, default)


def get_charge_mode(default=''):
    choices = [
        'per-day',
        'per-second',
        'per-minute',
        'per-hour',
        'per-use'
    ]
    return menu('Charge Modes', 'The product will be charged', choices,
                default)


def get_bit_rate(default=1000):
    default_str = '%d Mbps' % default
    choices = [
        '1000 Mbps',
        '2000 Mbps',
        '10000 Mbps'
    ]
    selection = menu('Bit Rate', 'Interface bit rate is', choices,
                     default_str)
    if selection == '1000 Mbps':
        return 1000
    elif selection == '2000 Mbps':
        return 2000
    elif selection == '10000 Mbps':
        return 10000
    else:
        return None


def show_bw_weight(df_json):
    return ':'.join([str(df_json["1"]), str(df_json["2"]), str(df_json["3"]),
                     str(df_json["4"]), str(df_json["5"]), str(df_json["6"]),
                     str(df_json["7"]) + ":" + str(df_json["8"])])


def edit_bw_weight(df_json=json.loads(BW_PRICE_WEIGHT)):
    default_item = 'ISP 1'
    while True:
        args = ['--title', 'Bandwidth Weight',
                '--default-item', default_item,
                '--ok-label', 'Edit',
                '--extra-button', '--extra-label', 'Save',
                '--menu', 'Input weight for each ISP', '15', '50', '8',
                'ISP 1', str(df_json["1"]), 'ISP 2', str(df_json["2"]),
                'ISP 3', str(df_json["3"]), 'ISP 4', str(df_json["4"]),
                'ISP 5', str(df_json["5"]), 'ISP 6', str(df_json["6"]),
                'ISP 7', str(df_json["7"]), 'ISP 8', str(df_json["8"])]
        ret, out = call_dialog(args)
        if ret == 0:
            op = 'edit'
        elif ret == 3:
            op = 'save'
        else:
            op = 'cancel'
        if op == 'cancel':
            return None
        elif op == 'save':
            return df_json
        else:
            default_item = out
            if out == 'ISP 1':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 1',
                                  60, str(df_json["1"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["1"] = int(tmpstr)
            elif out == 'ISP 2':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 2',
                                  60, str(df_json["2"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["2"] = int(tmpstr)
            elif out == 'ISP 3':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 3',
                                  60, str(df_json["3"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["3"] = int(tmpstr)
            elif out == 'ISP 4':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 4',
                                  60, str(df_json["4"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["4"] = int(tmpstr)
            elif out == 'ISP 5':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 5',
                                  60, str(df_json["5"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["5"] = int(tmpstr)
            elif out == 'ISP 6':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 6',
                                  60, str(df_json["6"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["6"] = int(tmpstr)
            elif out == 'ISP 7':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 7',
                                  60, str(df_json["7"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["7"] = int(tmpstr)
            elif out == 'ISP 8':
                tmpstr = inputbox(out, 'Input bandwidth weight of ISP 8',
                                  60, str(df_json["8"]))
                if tmpstr is not None and int(tmpstr) >= 0:
                    df_json["8"] = int(tmpstr)

    return None


def get_service_vendor(default=''):
    choices = [
        'YUNSHAN',
        'NSFOCUS',
        'TOPSEC',
        'EISOO'
    ]
    return menu('Service Vendor', 'Service Vendor is', choices,
                default)

block_device_type_name_to_id = {
    'Capacity': 0,
    'Performance': 1
}

block_device_type_id_to_name = {
    0: 'Capacity',
    1: 'Performance'
}

def get_block_device_type(default=''):
    choices = ['Capacity', 'Performance']
    selection = menu('BlockDevice type', 'BlockDevice type is', choices,
                     default)
    return block_device_type_name_to_id.get(selection, None)


def get_domain_name(default=''):
    choices = []
    for it in data.Domain.select():
        choices.append(it.name)
    return menu('Domain', 'Domain is', choices, default)


def domain_lcuuid_to_name(lcuuid=None):
    if lcuuid is not None:
        domain_info = data.Domain.get(data.Domain.lcuuid == lcuuid)
    else:
        domain_info = None

    if domain_info is None:
        return ''
    return domain_info.name


def domain_name_to_lcuuid(name=None):
    if name is not None:
        domain_info = data.Domain.get(data.Domain.name == name)
    else:
        domain_info = None

    if domain_info is None:
        return ''
    return domain_info.lcuuid


# single decimal: +5.3
DOMAIN_SINGLE = re.compile(r'^\s*\+?(\d*\.?\d+)\s*$')
# multiple decimal: +5.3, +8.2
DOMAIN_MULTIPLE = \
    re.compile(r'^\s*(\+?(\d*\.?\d+)\s*,\s*)+\+?(\d*\.?\d+)\s*,?\s*$')
# domain: (4, 9] [1, 5]
DOMAIN_DOMAIN = re.compile(
    r'^\s*([\(\[])\s*\+?(\d*\.?\d+)\s*,\s*\+?(\d*\.?\d+)\s*([\)\]])')
# domain to infinity: (1, Inf) [2, +Inf)
DOMAIN_TO_INF = re.compile(
    r'^\s*([\(\[])\s*\+?(\d*\.?\d+)\s*,\s*\+?[Ii]nf\s*\)')


def validate_domain(value):
    """matches single digit, (4, 55] and [0, +Inf)
    """
    if value is None or value == '':
        return value
    if re.match(DOMAIN_SINGLE, value) is not None:
        return value
    if re.match(DOMAIN_MULTIPLE, value) is not None:
        return value
    if re.match(DOMAIN_TO_INF, value) is not None:
        return value
    # match domain
    m = re.match(DOMAIN_DOMAIN, value)
    if m is not None and int(m.group(2)) < int(m.group(3)):
        return value
    raise ValidationError('Invalid domain %s' % value)


class Piece(Model):
    """piece of a piecewise linear function
    """
    domain = StringType(default='', required=True,
                        validators=[validate_domain])
    k = DecimalType(default=0)
    b = DecimalType(default=0)

    def edit(self):
        tmp = Piece(dict(domain=self.domain, k=self.k, b=self.b))
        while True:
            args = ['--title', 'Piece',
                    '--form', 'Input domain, k and b.',
                    '11', '31', '3',
                    'domain', '1', '2',
                    tmp.domain, '1', '9', '16', '15',
                    'k', '2', '2',
                    '%g' % tmp.k, '2', '9', '16', '15',
                    'b', '3', '2',
                    '%g' % tmp.b, '3', '9', '16', '15']
            ret, out = call_dialog(args)
            if ret != 0:
                return False
            outlist = out.split('\n')
            if outlist[0] == '':
                msgbox('Invalid input!')
                continue
            tmp.domain = outlist[0]
            if outlist[1] == '':
                tmp.k = decimal.Decimal('0')
            else:
                tmp.k = decimal.Decimal(outlist[1])
            if outlist[2] == '':
                tmp.b = decimal.Decimal('0')
            else:
                tmp.b = decimal.Decimal(outlist[2])
            try:
                tmp.validate()
                self.domain = tmp.domain
                self.k = tmp.k
                self.b = tmp.b
                return True
            except Exception:
                msgbox('Invalid input!')


# ProductSpec code start
class BwWeight(Model):
    one = IntType(serialized_name='1', default=0)
    two = IntType(serialized_name='2', default=0)
    three = IntType(serialized_name='3', default=0)
    four = IntType(serialized_name='4', default=0)
    five = IntType(serialized_name='5', default=0)
    six = IntType(serialized_name='6', default=0)
    seven = IntType(serialized_name='7', default=0)
    eight = IntType(serialized_name='8', default=0)


class ComputeSize(Model):
    vcpu_num = IntType(default=0)
    mem_size = IntType(default=0)
    sys_disk_size = IntType(default=50)


class VGatewayInfo(Model):
    wans = IntType(default=0)
    lans = IntType(default=0)
    rate = IntType(default=1000)
    conn_max = IntType(default=0)
    new_conn_per_sec = IntType(default=0)


class ValveInfo(Model):
    wans = IntType(default=0)
    lans = IntType(default=0)
    rate = IntType(default=1000)
    ips = IntType(default=0)
    bw_weight = ModelType(BwWeight)


class TpdInfo(Model):
    cpu_num = IntType(default=0)
    cpu_info = StringType(default='')
    mem_size = IntType(default=0)
    mem_info = StringType(default='')
    disk_size = IntType(default=0)
    disk_info = StringType(default='')
    nic_num = IntType(default=0)
    nic_info = StringType(default='')


class PSContent(Model):
    description = StringType(default='')
    compute_size = ModelType(ComputeSize)
    vgateway_info = ModelType(VGatewayInfo)
    valve_info = ModelType(ValveInfo)
    tpd_info = ModelType(TpdInfo)
    isp = IntType(default=0)
    service_vendor = StringType(default='')
    block_device_type = IntType(default=0)
    user_disk_size = IntType(default=0)

    class Options:
        serialize_when_none = False
        roles = {
            'default': whitelist('description'),
            'vm':
                whitelist('description', 'compute_size', 'user_disk_size'),
            'load-balancer':
                whitelist('description', 'compute_size', 'user_disk_size'),
            'virtual-firewall':
                whitelist('description', 'compute_size',
                          'service_vendor', 'user_disk_size'),
            'virtual-gateway': whitelist('description', 'vgateway_info'),
            'bandwidth-sharer': whitelist('description', 'valve_info'),
            'third-party-device': whitelist('description', 'tpd_info'),
            'bandwidth': whitelist('description', 'isp'),
            'ip-address': whitelist('description', 'isp'),
            'nas-storage': whitelist('description', 'service_vendor'),
            'cloud-disk': whitelist('description', 'service_vendor'),
            'backup': whitelist('description', 'service_vendor'),
            'block-device': whitelist('description', 'block_device_type'),
            'block-device-snapshot':
                whitelist('description', 'block_device_type'),
        }


class ProductSpec(Model):
    name = StringType(default='')
    lcuuid = StringType(default='')
    content = ModelType(PSContent)
# ProductSpec code end


class Price(Model):
    name = StringType(required=True, default='')
    type = StringType(required=True, default='')
    description = StringType(default='')
    map = ListType(ModelType(Piece), default=[])
    charge_mode = StringType(required=True, default='')
    domain = StringType(required=True, default='')
    # ProductSpec code start
    product_spec = ModelType(ProductSpec)
    # ProductSpec code end

    def edit(self):
        tmp = Price()
        tmp.domain = self.domain
        tmp.name = self.name
        tmp.type = self.type
        tmp.charge_mode = self.charge_mode
        tmp.description = self.description
        tmp.map = copy.deepcopy(self.map)
        # ProductSpec code start
        tmp.product_spec = copy.deepcopy(self.product_spec)
        tps = tmp.product_spec
        # ProductSpec code end
        default_item = 'Domain'
        while True:
            op = None
            out = None
            if tmp.domain is None or tmp.domain == '':
                show_domain = '<select>'
            else:
                show_domain = tmp.domain
            if tmp.name is None or tmp.name == '':
                show_name = '<edit>'
            else:
                show_name = tmp.name
            if tmp.type is None or tmp.type == '':
                show_type = '<select>'
            else:
                show_type = tmp.type
            if tmp.description is None or tmp.description == '':
                show_description = '<edit>'
            else:
                show_description = tmp.description
            if len(tmp.map) == 0:
                show_price = '<select>'
            elif (len(tmp.map) == 1 and tmp.map[0].domain == '[0, +Inf)' and
                    decimal.Decimal(tmp.map[0].b) == 0):
                show_price = '%g' % tmp.map[0].k
            else:
                show_price = '<dedicated field>'
            if tmp.charge_mode is None or tmp.charge_mode == '':
                show_charge_mode = '<select>'
            else:
                show_charge_mode = tmp.charge_mode
            args = ['--title', 'Product Specification',
                    '--default-item', default_item,
                    '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Save',
                    '--menu', 'Product', '13', '50', '6',
                    'Domain', show_domain, 'Type', show_type,
                    'Name', show_name, 'Description', show_description,
                    'Price', show_price,
                    'Charge Mode', show_charge_mode]
            # ProductSpec code start
            if (tmp.type in display_type_to_db_type.keys() and
                    tmp.type not in ['CPU', 'Memory', 'Disk']):
                extra_fields = []
                if tps.name is None or tps.name == '':
                    show_display_name = '<edit>'
                else:
                    show_display_name = tps.name
                extra_fields.extend(['Display Name', show_display_name])
                if tps.content is None:
                    tps.content = PSContent()
                if tmp.type == 'Load Balancer':
                    if tps.content.compute_size is None:
                        tps.content.compute_size = ComputeSize()
                    extra_fields.extend(
                        ['CPU', str(tps.content.compute_size.vcpu_num)])
                    extra_fields.extend(
                        ['Memory', str(tps.content.compute_size.mem_size)])
                elif tmp.type == 'Virtual Firewall':
                    if tps.content.compute_size is None:
                        tps.content.compute_size = ComputeSize()
                    if tps.content.service_vendor == '':
                        show_service_vendor = '<select>'
                    else:
                        show_service_vendor = tps.content.service_vendor
                    extra_fields.extend(['Service Vendor',
                                         '%s' % show_service_vendor])
                    extra_fields.extend(
                        ['CPU', str(tps.content.compute_size.vcpu_num)])
                    extra_fields.extend(
                        ['Memory', str(tps.content.compute_size.mem_size)])
                elif tmp.type == 'Virtual Gateway':
                    if tps.content.vgateway_info is None:
                        tps.content.vgateway_info = VGatewayInfo()
                    extra_fields.extend(
                        ['WAN Interfaces',
                         str(tps.content.vgateway_info.wans)])
                    extra_fields.extend(
                        ['LAN Interfaces',
                         str(tps.content.vgateway_info.lans)])
                    extra_fields.extend(
                        ['Bit Rate',
                         '%d Mbps' % tps.content.vgateway_info.rate])
                    extra_fields.extend(
                        ['Max Connections',
                         str(tps.content.vgateway_info.conn_max)])
                    extra_fields.extend(
                        ['Max Connections Per Second',
                         str(tps.content.vgateway_info.new_conn_per_sec)])
                elif tmp.type == 'Bandwidth Sharer':
                    if tps.content.valve_info is None:
                        tps.content.valve_info = ValveInfo()
                        tps.content.valve_info.bw_weight = BwWeight()
                    extra_fields.extend(
                        ['WAN Interfaces',
                         str(tps.content.valve_info.wans)])
                    extra_fields.extend(
                        ['LAN Interfaces',
                         str(tps.content.valve_info.lans)])
                    extra_fields.extend(
                        ['Bit Rate',
                         '%d Mbps' % tps.content.valve_info.rate])
                    extra_fields.extend(
                        ['IP Number',
                         str(tps.content.valve_info.ips)])
                    extra_fields.extend(
                        ['Bandwidth Weight',
                         show_bw_weight(
                             tps.content.valve_info.bw_weight.to_primitive())])
                elif tmp.type == 'Third Party Device':
                    if tps.content.tpd_info is None:
                        tps.content.tpd_info = TpdInfo()
                    tpd_info_ext = tps.content.tpd_info.cpu_num
                    extra_fields.extend(
                        ['CPU Number',
                         '%s' % tpd_info_ext if tpd_info_ext > 0 else '<edit>'])
                    tpd_info_ext = str(tps.content.tpd_info.cpu_info)
                    extra_fields.extend(
                        ['CPU Info', tpd_info_ext if tpd_info_ext else '<edit>'])
                    tpd_info_ext = tps.content.tpd_info.mem_size
                    extra_fields.extend(
                        ['Memory Size',
                         '%s' % tpd_info_ext if tpd_info_ext > 0 else '<edit>'])
                    tpd_info_ext = str(tps.content.tpd_info.mem_info)
                    extra_fields.extend(
                        ['Memory Info', tpd_info_ext if tpd_info_ext else '<edit>'])
                    tpd_info_ext = tps.content.tpd_info.disk_size
                    extra_fields.extend(
                        ['Disk Size',
                         '%s' % tpd_info_ext if tpd_info_ext > 0 else '<edit>'])
                    tpd_info_ext = str(tps.content.tpd_info.disk_info)
                    extra_fields.extend(
                        ['Disk Info', tpd_info_ext if tpd_info_ext else '<edit>'])
                    tpd_info_ext = tps.content.tpd_info.nic_num
                    extra_fields.extend(
                        ['NIC Number',
                         '%s' % tpd_info_ext if tpd_info_ext > 0 else '<edit>'])
                    tpd_info_ext = str(tps.content.tpd_info.nic_info)
                    extra_fields.extend(
                        ['NIC Info', tpd_info_ext if tpd_info_ext else '<edit>'])
                elif tmp.type in ['Bandwidth', 'IP Address']:
                    extra_fields.extend(
                        ['ISP ID', str(tps.content.isp)])
                elif tmp.type in ['NAS Storage', 'Cloud Disk', 'Backup']:
                    if tps.content.service_vendor == '':
                        show_service_vendor = '<select>'
                    else:
                        show_service_vendor = tps.content.service_vendor
                    extra_fields.extend(['Service Vendor',
                                         '%s' % show_service_vendor])
                elif tmp.type in ['BlockDevice', 'BlockDevice Snapshot']:
                    show_block_device_type = block_device_type_id_to_name.get(
                        tps.content.block_device_type, '')
                    extra_fields.extend(['BlockDevice Type',
                                         '%s' % show_block_device_type])
                args.extend(extra_fields)
                extra_len = len(extra_fields) / 2
                # window height
                args[11] = str(int(args[11]) + extra_len)
                # menu height
                args[13] = str(int(args[13]) + extra_len)
            # ProductSpec code end
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'save'
            else:
                op = 'cancel'
            if op == 'cancel':
                return False
            elif op == 'save':
                try:
                    tmp.validate()
                    if tmp.domain == '':
                        raise Exception('Domain empty!')
                    if tmp.name == '':
                        raise Exception('Name empty!')
                    self.domain = tmp.domain
                    self.name = tmp.name
                    self.type = tmp.type
                    self.description = tmp.description
                    self.map = tmp.map
                    self.charge_mode = tmp.charge_mode
                    # ProductSpec code start
                    if tmp.type not in ['CPU', 'Memory', 'Disk']:
                        tps.content.description = tmp.description
                        self.product_spec = tmp.product_spec
                    # ProductSpec code end
                    return True
                except Exception as e:
                    msgbox('Invalid product info!\n[%r]' % e)
            else:
                default_item = out
                if out == 'Domain' and (tmp.domain is None or
                                        tmp.domain == ''):
                    tmpstr = get_domain_name(tmp.domain)
                    if tmpstr is not None:
                        tmp.domain = tmpstr
                elif out == 'Name' and (tmp.name is None or tmp.name == ''):
                    tmpstr = inputbox('Name', 'Input name', 30, tmp.name)
                    if tmpstr is not None:
                        coding = chardet.detect(tmpstr)
                        if coding['encoding'] == 'ascii':
                            tmp.name = tmpstr
                        else:
                            msgbox("Please enter the \'Name\' using english")
                elif out == 'Type':
                    tmpstr = get_type(tmp.type)
                    if tmpstr is not None:
                        tmp.type = tmpstr
                        if tmp.type in ['CPU', 'Memory', 'Disk']:
                            tmp.name = display_type_to_db_name[tmp.type]
                elif out == 'Description':
                    tmpstr = inputbox('Description',
                                      'Input description', 60, tmp.description)
                    if tmpstr is not None:
                        tmp.description = tmpstr
                elif out == 'Price':
                    tmp.simple_edit_map()
                elif out == 'Charge Mode':
                    tmpstr = get_charge_mode(tmp.charge_mode)
                    if tmpstr is not None:
                        tmp.charge_mode = tmpstr
                # productspec code start
                elif out == 'Display Name':
                    tmpstr = inputbox('Display Name',
                                      'Input display name', 60, tps.name)
                    if tmpstr is not None:
                        tps.name = tmpstr
                elif out == 'CPU':
                    tmpstr = inputbox(out, 'Input CPU', 60,
                                      str(tps.content.compute_size.vcpu_num))
                    if tmpstr is not None and int(tmpstr) >= 0:
                        tps.content.compute_size.vcpu_num = int(tmpstr)
                elif out == 'Memory':
                    tmpstr = inputbox(out, 'Input memory', 60,
                                      str(tps.content.compute_size.mem_size))
                    if tmpstr is not None and int(tmpstr) >= 0:
                        tps.content.compute_size.mem_size = int(tmpstr)
                elif out == 'WAN Interfaces':
                    if tmp.type == 'Virtual Gateway':
                        tmpstr = inputbox(
                            out, 'Input number of WAN interfaces', 60,
                            str(tps.content.vgateway_info.wans))
                        if tmpstr is not None and int(tmpstr) >= 0:
                            tps.content.vgateway_info.wans = int(tmpstr)
                    elif tmp.type == 'Bandwidth Sharer':
                        tmpstr = inputbox(
                            out, 'Input number of WAN interfaces', 60,
                            str(tps.content.valve_info.wans))
                        if tmpstr is not None and int(tmpstr) >= 0:
                            tps.content.valve_info.wans = int(tmpstr)
                elif out == 'LAN Interfaces':
                    if tmp.type == 'Virtual Gateway':
                        tmpstr = inputbox(
                            out, 'Input number of LAN interfaces', 60,
                            str(tps.content.vgateway_info.lans))
                        if tmpstr is not None and int(tmpstr) >= 0:
                            tps.content.vgateway_info.lans = int(tmpstr)
                    elif tmp.type == 'Bandwidth Sharer':
                        tmpstr = inputbox(
                            out, 'Input number of LAN interfaces', 60,
                            str(tps.content.valve_info.lans))
                        if tmpstr is not None and int(tmpstr) >= 0:
                            tps.content.valve_info.lans = int(tmpstr)
                elif out == 'Bit Rate':
                    if tmp.type == 'Virtual Gateway':
                        tmpint = get_bit_rate(tps.content.vgateway_info.rate)
                        if tmpint is not None:
                            tps.content.vgateway_info.rate = tmpint
                    elif tmp.type == 'Bandwidth Sharer':
                        tmpint = get_bit_rate(tps.content.valve_info.rate)
                        if tmpint is not None:
                            tps.content.valve_info.rate = tmpint
                elif out == 'Max Connections':
                    tmpstr = inputbox(
                        out, 'Input connection limit of this vgateway',
                        60, str(tps.content.vgateway_info.conn_max))
                    if tmpstr is not None and int(tmpstr) >= 0:
                        tps.content.vgateway_info.conn_max = int(tmpstr)
                elif out == 'Max Connections Per Second':
                    tmpstr = inputbox(
                        out, 'Input allowed new conn per sec of this vgateway',
                        60, str(tps.content.vgateway_info.new_conn_per_sec))
                    if tmpstr is not None and int(tmpstr) >= 0:
                        tps.content.vgateway_info.new_conn_per_sec = \
                            int(tmpstr)
                elif out == 'IP Number':
                    tmpstr = inputbox(
                        out, 'Input number of IPs', 60,
                        str(tps.content.valve_info.ips))
                    if tmpstr is not None and int(tmpstr) >= 0:
                        tps.content.valve_info.ips = int(tmpstr)
                elif out == 'Bandwidth Weight':
                    tmpjson = edit_bw_weight(
                        tps.content.valve_info.bw_weight.to_primitive())
                    if tmpjson is not None:
                        tps.content.valve_info.bw_weight = BwWeight(tmpjson)
                elif out == 'CPU Number':
                    tmpstr = inputbox(
                        out, 'Input the total number of hardware CPUs', 60,
                        str(tps.content.tpd_info.cpu_num))
                    if tmpstr and int(float(tmpstr)) >= 0:
                        tps.content.tpd_info.cpu_num = int(float(tmpstr))
                elif out == 'CPU Info':
                    tmpstr = inputbox(
                        out, 'Input detailed hardware CPU information', 60,
                        tps.content.tpd_info.cpu_info)
                    if tmpstr is not None:
                        tps.content.tpd_info.cpu_info = tmpstr
                elif out == 'Memory Size':
                    tmpstr = inputbox(
                        out, 'Input the total size (MB) of hardware memories', 60,
                        str(tps.content.tpd_info.mem_size))
                    if tmpstr and int(float(tmpstr)) >= 0:
                        tps.content.tpd_info.mem_size = int(float(tmpstr))
                elif out == 'Memory Info':
                    tmpstr = inputbox(
                        out, 'Input detailed hardware memory information', 60,
                        tps.content.tpd_info.mem_info)
                    if tmpstr is not None:
                        tps.content.tpd_info.mem_info = tmpstr
                elif out == 'Disk Size':
                    tmpstr = inputbox(
                        out, 'Input the total size (GB) of hardware disks', 60,
                        str(tps.content.tpd_info.disk_size))
                    if tmpstr and int(float(tmpstr)) >= 0:
                        tps.content.tpd_info.disk_size = int(float(tmpstr))
                elif out == 'Disk Info':
                    tmpstr = inputbox(
                        out, 'Input detailed hardware disk information', 60,
                        tps.content.tpd_info.disk_info)
                    if tmpstr is not None:
                        tps.content.tpd_info.disk_info = tmpstr
                elif out == 'NIC Number':
                    tmpstr = inputbox(
                        out, 'Input the total number of hardware NICs', 60,
                        str(tps.content.tpd_info.nic_num))
                    if tmpstr and int(float(tmpstr)) >= 0:
                        tps.content.tpd_info.nic_num = int(float(tmpstr))
                elif out == 'NIC Info':
                    tmpstr = inputbox(
                        out, 'Input detailed hardware NIC information', 60,
                        tps.content.tpd_info.nic_info)
                    if tmpstr is not None:
                        tps.content.tpd_info.nic_info = tmpstr
                elif out == 'ISP ID':
                    tmpstr = inputbox(out, 'Input ISP ID',
                                      60, str(tps.content.isp))
                    if tmpstr is not None:
                        tps.content.isp = int(tmpstr)
                elif out == 'Service Vendor':
                    tmpstr = get_service_vendor(tps.content.service_vendor)
                    if tmpstr is not None:
                        tps.content.service_vendor = tmpstr
                elif out == 'BlockDevice Type':
                    tmpint = get_block_device_type(
                        tps.content.block_device_type)
                    if tmpint is not None:
                        tps.content.block_device_type = tmpint
                # ProductSpec code end

    def simple_edit_map(self):
        tmp = copy.deepcopy(self.map)
        while True:
            if len(tmp) == 0:
                show_price = '<edit>'
            elif (len(tmp) == 1 and tmp[0].domain == '[0, +Inf)' and
                    decimal.Decimal(tmp[0].b) == 0):
                show_price = '%g' % tmp[0].k
            else:
                show_price = '<dedicated field>'
            args = ['--title', 'Price',
                    '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Save',
                    '--menu', 'Per unit price', '10', '50', '3',
                    'Price', show_price, 'Advanced', '']
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'save'
            else:
                op = 'cancel'
            if op == 'cancel':
                return False
            elif op == 'save':
                self.map = tmp
                return True
            else:
                if out == 'Price':
                    if (show_price == '<edit>' or
                            show_price == '<dedicated field>'):
                        tmpstr = inputbox('Price', 'Input price', 30, '')
                    else:
                        tmpstr = inputbox('Price', 'Input price',
                                          30, show_price)
                    if tmpstr is not None and tmpstr != '':
                        p = Piece()
                        p.domain = '[0, +Inf)'
                        p.k = decimal.Decimal(tmpstr)
                        p.b = decimal.Decimal('0')
                        tmp = [p]
                    self.map = tmp
                    return True
                elif out == 'Advanced':
                    return self.edit_map()

    def edit_map(self):
        tmp = copy.deepcopy(self.map)
        while True:
            op = None
            out = None
            if len(tmp) == 0:
                args = ['--title', 'Formula', '--yes-label', 'Add',
                        '--no-label', 'Save',
                        '--yesno', '<no pieces>', '8', '40']
                ret, _ = call_dialog(args)
                if ret == 0:
                    op = 'add'
                elif ret == 1:
                    op = 'save'
                else:
                    op = 'cancel'
            else:
                menulen = len(tmp) + 1
                windowlen = menulen + 7
                maxwidth = 39
                items = []
                for it in tmp:
                    items.append(it.domain)
                    items.append('%gx + %g' % (it.k, it.b))
                    items.append('off')
                    width = len(items[-3]) + len(items[-2]) + 14
                    if width > maxwidth:
                        maxwidth = width
                args = ['--title', 'Formula', '--ok-label', 'Edit',
                        '--extra-button', '--extra-label', 'Delete',
                        '--cancel-label', 'Add',
                        '--help-button', '--help-label', 'Save',
                        '--separate-output',
                        '--checklist', 'Pieces', '%d' % windowlen,
                        '%d' % maxwidth, '%d' % menulen]
                args.extend(items)
                ret, out = call_dialog(args)
                if ret == 0:
                    op = 'edit'
                elif ret == 3:
                    op = 'delete'
                elif ret == 1:
                    op = 'add'
                elif ret == 2:
                    op = 'save'
                else:
                    op = 'cancel'

            if op == 'cancel':
                return False
            elif op == 'save':
                self.map = tmp
                return True
            elif op == 'add':
                p = Piece()
                if p.edit() is True:
                    tmp.append(p)
            elif op == 'edit':
                to_edit = out.rstrip('\n').split('\n')
                if not to_edit or to_edit[0] == '':
                    continue
                if len(to_edit) != 1:
                    msgbox('Only one piece can be modified at a time.')
                    continue
                for it in tmp:
                    if it.domain == to_edit[0]:
                        it.edit()
                        break

            elif op == 'delete':
                to_delete = out.rstrip('\n').split('\n')
                if not to_delete or to_delete[0] == '':
                    continue
                quoted_to_delete = ["'%s'" % it for it in to_delete]
                if len(to_delete) == 1:
                    msgline = quoted_to_delete[0]
                else:
                    msgline = ', '.join(quoted_to_delete[0:-1])
                    msgline += ' and '
                    msgline += quoted_to_delete[-1]
                msgline = 'Confirm removing piece ' + msgline + '?'
                args = ['--title', 'Confirm', '--defaultno',
                        '--yesno', msgline, '8', '40']
                ret, _ = call_dialog(args)
                if ret == 0:
                    tmp = [it for it in tmp if it.domain not in to_delete]


def prices_list():
    prices = []
    pss = data.ProductSpec.select()
    for it in data.UnitPrice.select():
        try:
            price = Price()
            price.map = json.loads(it.price).get('map', None)
            price.domain = domain_lcuuid_to_name(it.domain)
            price.name = it.name
            price.type = db_type_to_display_type.get(it.type, '')
            price.charge_mode = it.charge_mode
            price.description = it.description
            # ProductSpec code start
            if (price.type in display_type_to_db_type.keys() and
                    price.type not in ['CPU', 'Memory', 'Disk']):
                ps_info = None
                for it2 in pss:
                    if (it2.plan_name == it.name and it2.domain == it.domain):
                        ps_info = it2
                        break
                if ps_info is not None:
                    price.product_spec = ProductSpec()
                    price.product_spec.name = it2.name
                    price.product_spec.lcuuid = it2.lcuuid
                    price.product_spec.content = PSContent(
                        json.loads(ps_info.content))
            # ProductSpec code end
            price.validate()
        except Exception as e:
            print('%r: Error parsing price [%s] for %s:%s' %
                  (e, it.price, it.domain, it.name))
            continue
        prices.append(price)

    while True:
        op = None
        out = None
        if len(prices) == 0:
            args = ['--title', 'Unit Prices', '--yes-label', 'Add',
                    '--no-label', 'Exit',
                    '--yesno', '<n/a>', '8', '40']
            ret, _ = call_dialog(args)
            if ret == 0:
                op = 'add'
            else:
                op = 'cancel'
        else:
            items = []
            for it in prices:
                items.append(it.domain + ' ' + it.name)
                desc = '%s, charged %s, %s' % (it.type,
                                               it.charge_mode,
                                               it.description)
                items.append(desc)
                items.append('off')
            menulen = len(prices) + 1
            windowlen = menulen + 7
            args = ['--title', 'Unit Prices', '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Delete',
                    '--cancel-label', 'Add',
                    '--help-button', '--help-label', 'Exit',
                    '--separate-output',
                    '--checklist', 'Contents', '%d' % windowlen,
                    '100', '%d' % menulen]
            args.extend(items)
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'delete'
            elif ret == 1:
                op = 'add'
            else:
                op = 'cancel'

        if op == 'cancel':
            return
        elif op == 'add':
            p = Price()
            while True:
                name_collide = False
                if p.edit() is True:
                    for it in prices:
                        if it.domain == p.domain and it.name == p.name:
                            msgbox('Name collides!')
                            name_collide = True
                            break
                else:
                    break
                if name_collide is False:
                    prices.append(p)
                    price_map = p.to_primitive()['map']
                    data.UnitPrice.insert(
                        domain=domain_name_to_lcuuid(p.domain),
                        name=p.name,
                        type=display_type_to_db_type.get(p.type, ''),
                        charge_mode=p.charge_mode,
                        description=p.description,
                        price=json.dumps(dict(map=price_map))).execute()
                    # ProductSpec code start
                    if p.type not in ['CPU', 'Memory', 'Disk']:
                        ps_info = data.ProductSpec()
                        ps_info.name = p.product_spec.name
                        ps_info.lcuuid = uuid.uuid4()
                        if p.type in ['Load Balancer', 'Virtual Gateway',
                                      'Third Party Device', 'Bandwidth',
                                      'IP Address', 'Virtual Firewall',
                                      'Bandwidth Sharer']:
                            # instance
                            ps_info.type = 1
                            ps_info.price = decimal.Decimal('0')
                        else:
                            # service
                            ps_info.type = 2
                            ps_info.price = p.map[0].k
                        ps_info.charge_mode = ps_charge_mode.get(
                            p.charge_mode, 0)
                        ps_info.state = 1
                        if p.type == 'Load Balancer':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='load-balancer'))
                        elif p.type == 'Virtual Firewall':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='virtual-firewall'))
                        elif p.type == 'Virtual Gateway':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='virtual-gateway'))
                        elif p.type == 'Bandwidth Sharer':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='bandwidth-sharer'))
                        elif p.type == 'Third Party Device':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='third-party-device'))
                        elif p.type == 'Bandwidth':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='bandwidth'))
                        elif p.type == 'IP Address':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='ip-address'))
                        elif p.type == 'NAS Storage':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='nas-storage'))
                        elif p.type == 'Cloud Disk':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='cloud-disk'))
                        elif p.type == 'Backup':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='backup'))
                        elif p.type == 'BlockDevice':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='block-device'))
                        elif p.type == 'BlockDevice Snapshot':
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive(
                                    role='block-device-snapshot'))
                        else:
                            ps_info.content = json.dumps(
                                p.product_spec.content.to_primitive())
                        ps_info.plan_name = p.name
                        ps_info.product_type = display_type_to_int_type.get(
                            p.type, 0)
                        ps_info.domain = domain_name_to_lcuuid(p.domain)
                        ps_info.save()
                    # ProductSpec code end
                    break
        elif op == 'edit':
            to_edit = out.rstrip('\n').split('\n')
            if not to_edit or to_edit[0] == '':
                continue
            if len(to_edit) != 1:
                msgbox('Only one piece can be modified at a time.')
                continue
            (to_edit_domain, to_edit_name) = to_edit[0].split(' ', 1)
            for it in prices:
                if it.domain == to_edit_domain and it.name == to_edit_name:
                    while True:
                        name_collide = False
                        if it.edit() is True:
                            for it2 in prices:
                                if it2 != it and it2.domain == it.domain and \
                                        it2.name == it.name:
                                    msgbox('Name collides!')
                                    name_collide = True
                                    break
                        else:
                            break
                        if name_collide is False:
                            price_map = it.to_primitive()['map']
                            domain = domain_name_to_lcuuid(it.domain)
                            data.UnitPrice.update(
                                type=display_type_to_db_type.get(it.type, ''),
                                charge_mode=it.charge_mode,
                                description=it.description,
                                price=json.dumps(dict(map=price_map))).where(
                                    data.UnitPrice.domain == domain,
                                    data.UnitPrice.name == it.name).execute()
                            # ProductSpec code start
                            if it.type not in ['CPU', 'Memory', 'Disk']:
                                ps_info = data.ProductSpec()
                                ps_info.name = it.product_spec.name
                                if it.type in ['Load Balancer',
                                               'Virtual Gateway',
                                               'Third Party Device',
                                               'Bandwidth', 'IP Address',
                                               'Virtual Firewall',
                                               'Bandwidth Sharer']:
                                    # instance
                                    ps_info.type = 1
                                    ps_info.price = decimal.Decimal('0')
                                else:
                                    # service
                                    ps_info.type = 2
                                    ps_info.price = it.map[0].k
                                ps_info.charge_mode = ps_charge_mode.get(
                                    it.charge_mode, 0)
                                ps_info.state = 1
                                if it.type == 'Load Balancer':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='load-balancer'))
                                elif it.type == 'Virtual Firewall':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='virtual-firewall'))
                                elif it.type == 'Virtual Gateway':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='virtual-gateway'))
                                elif it.type == 'Bandwidth Sharer':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='bandwidth-sharer'))
                                elif it.type == 'Third Party Device':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='third-party-device'))
                                elif it.type == 'Bandwidth':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='bandwidth'))
                                elif it.type == 'IP Address':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='ip-address'))
                                elif it.type == 'NAS Storage':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='nas-storage'))
                                elif it.type == 'Cloud Disk':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='cloud-disk'))
                                elif it.type == 'Backup':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='backup'))
                                elif it.type == 'BlockDevice':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='block-device'))
                                elif it.type == 'BlockDevice Snapshot':
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive(
                                            role='block-device-snapshot'))
                                else:
                                    ps_info.content = json.dumps(
                                        it.product_spec.content.to_primitive())
                                ps_info.product_type = (
                                    display_type_to_int_type.get(it.type,
                                                                 0))
                                data.ProductSpec.update(
                                    name=ps_info.name,
                                    type=ps_info.type,
                                    price=ps_info.price,
                                    charge_mode=ps_info.charge_mode,
                                    state=ps_info.state,
                                    content=ps_info.content,
                                    product_type=ps_info.product_type).where(
                                        data.ProductSpec.domain == domain,
                                        data.ProductSpec.plan_name == it.name
                                    ).execute()
                            # ProductSpec code end
                            break
                    break
        elif op == 'delete':
            to_delete = out.rstrip('\n').split('\n')
            if not to_delete or to_delete[0] == '':
                continue
            quoted_to_delete = ["'%s'" % it for it in to_delete]
            if len(to_delete) == 1:
                msgline = quoted_to_delete[0]
            else:
                msgline = ', '.join(quoted_to_delete[0:-1])
                msgline += ' and '
                msgline += quoted_to_delete[-1]
            msgline = 'Confirm removing price for ' + msgline + '?'
            args = ['--title', 'Confirm', '--defaultno',
                    '--yesno', msgline, '8', '60']
            ret, _ = call_dialog(args)
            if ret == 0:
                prices = [it for it in prices
                          if (it.domain + ' ' + it.name) not in to_delete]
                for domain_and_name in to_delete:
                    (domain_name, name) = domain_and_name.split(' ', 1)
                    domain = domain_name_to_lcuuid(domain_name)
                    data.UnitPrice.delete(
                        ).where(data.UnitPrice.domain == domain,
                                data.UnitPrice.name == name).execute()
                    # ProductSpec code start
                    data.ProductSpec.delete(
                        ).where(data.ProductSpec.domain == domain,
                                data.ProductSpec.plan_name == name).execute()
                    # ProductSpec code end


class CPU(Model):
    num = StringType(required=True, default='',
                     validators=[validate_domain])


class Memory(Model):
    size = StringType(required=True, default='',
                      validators=[validate_domain])


class Disk(Model):
    size = StringType(required=True, default='',
                      validators=[validate_domain])


class VMContent(Model):
    cpu = ModelType(CPU)
    memory = ModelType(Memory)
    disk = ModelType(Disk)

    def edit(self):
        tmp = VMContent()
        tmp.cpu = copy.deepcopy(self.cpu)
        tmp.memory = copy.deepcopy(self.memory)
        tmp.disk = copy.deepcopy(self.disk)
        while True:
            show_cpu = tmp.cpu.num
            show_memory = tmp.memory.size
            show_disk = tmp.disk.size
            args = ['--title', 'VM Content',
                    '--form', 'Input range of CPU num, memory and disk size.',
                    '11', '40', '3',
                    'CPU', '1', '2',
                    show_cpu, '1', '14', '16', '15',
                    'Memory (GB)', '2', '2',
                    show_memory, '2', '14', '16', '15',
                    'Disk (GB)', '3', '2',
                    show_disk, '3', '14', '16', '15']
            ret, out = call_dialog(args)
            if ret != 0:
                return False
            outlist = out.split('\n')
            if outlist[0] != '':
                tmp.cpu.num = outlist[0]
            else:
                tmp.cpu.num = None
            if outlist[1] != '':
                tmp.memory.size = outlist[1]
            else:
                tmp.memory.size = None
            if outlist[1] != '':
                tmp.disk.size = outlist[2]
            else:
                tmp.disk.size = None
            try:
                tmp.validate()
                self.cpu = tmp.cpu
                self.memory = tmp.memory
                self.disk = tmp.disk
                return True
            except Exception:
                msgbox('Invalid input!')


class VMCoupon(Model):
    cpu = DecimalType(default=1, min_value=0)
    memory = DecimalType(default=1, min_value=0)
    disk = DecimalType(default=1, min_value=0)

    def edit(self):
        tmp = VMCoupon()
        tmp.cpu = self.cpu
        tmp.memory = self.memory
        tmp.disk = self.disk
        while True:
            show_cpu = '%g' % tmp.cpu
            show_memory = '%g' % tmp.memory
            show_disk = '%g' % tmp.disk
            args = ['--title', 'VM Coupon',
                    '--form', 'Input discount of CPU num, memory and disk.',
                    '11', '40', '3',
                    'CPU', '1', '2',
                    show_cpu, '1', '9', '8', '7',
                    'Memory', '2', '2',
                    show_memory, '2', '9', '8', '7',
                    'Disk', '3', '2',
                    show_disk, '3', '9', '8', '7']
            ret, out = call_dialog(args)
            if ret != 0:
                return False
            outlist = out.split('\n')
            if outlist[0] != '':
                tmp.cpu = decimal.Decimal(outlist[0])
            else:
                tmp.cpu = decimal.Decimal('1')
            if outlist[1] != '':
                tmp.memory = decimal.Decimal(outlist[1])
            else:
                tmp.memory = decimal.Decimal('1')
            if outlist[2] != '':
                tmp.disk = decimal.Decimal(outlist[2])
            else:
                tmp.disk = decimal.Decimal('1')
            try:
                tmp.validate()
                self.cpu = tmp.cpu
                self.memory = tmp.memory
                self.disk = tmp.disk
                return True
            except Exception:
                msgbox('Invalid input!')


class VMPlan(Model):
    domain = StringType(required=True, default='')
    name = StringType(required=True, default='')
    description = StringType(default='')
    content = ModelType(VMContent, required=True)
    coupon = ModelType(VMCoupon, required=True)
    product_spec = ModelType(ProductSpec)

    def edit(self):
        tmp = VMPlan()
        tmp.domain = self.domain
        tmp.name = self.name
        tmp.description = self.description
        tmp.content = copy.deepcopy(self.content)
        tmp.coupon = copy.deepcopy(self.coupon)
        default_item = 'Domain'
        # ProductSpec code start
        tmp.product_spec = copy.deepcopy(self.product_spec)
        tps = tmp.product_spec
        # ProductSpec code end
        while True:
            op = None
            out = None
            if tmp.domain is None or tmp.domain == '':
                show_domain = '<select>'
            else:
                show_domain = tmp.domain
            if tmp.name is None or tmp.name == '':
                show_name = '<edit>'
            else:
                show_name = tmp.name
            if tmp.description is None or tmp.description == '':
                show_description = '<edit>'
            else:
                show_description = tmp.description
            args = ['--title', 'VM Specification',
                    '--default-item', default_item,
                    '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Save',
                    '--menu', 'VM', '12', '50', '5',
                    'Domain', show_domain,
                    'Name', show_name, 'Description', show_description,
                    'Content', '<dedicated field>',
                    'Coupon', '<dedicated field>']
            # ProductSpec code start
            if tps.content is None:
                tps.content = PSContent()
            extra_fields = []
            if tps.name is None or tps.name == '':
                show_display_name = '<edit>'
            else:
                show_display_name = tps.name
            extra_fields.extend(['Display Name', show_display_name])
            args.extend(extra_fields)
            extra_len = len(extra_fields) / 2

            # window height
            args[11] = str(int(args[11]) + extra_len)
            # menu height
            args[13] = str(int(args[13]) + extra_len)

            # ProductSpec code end
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'save'
            else:
                op = 'cancel'
            if op == 'cancel':
                return False
            elif op == 'save':
                try:
                    tmp.validate()
                    self.domain = tmp.domain
                    self.name = tmp.name
                    self.description = tmp.description
                    self.content = tmp.content
                    self.coupon = tmp.coupon
                    # ProductSpec code start
                    tps.content.description = tmp.description
                    if tps.content.compute_size is None:
                        tps.content.compute_size = ComputeSize()
                    tps.content.compute_size.vcpu_num = int(
                        tmp.content.cpu.num)
                    tps.content.compute_size.mem_size = int(
                        tmp.content.memory.size)
                    tps.content.compute_size.sys_disk_size = 50
                    tps.content.user_disk_size = int(tmp.content.disk.size)
                    self.product_spec = tmp.product_spec
                    # ProductSpec code end
                    return True
                except Exception as e:
                    msgbox('Invalid VM Plan info!\n[%r]' % e)
            else:
                default_item = out
                if out == 'Domain' and (tmp.domain is None or
                                        tmp.domain == ''):
                    tmpstr = get_domain_name(tmp.domain)
                    if tmpstr is not None:
                        tmp.domain = tmpstr
                if out == 'Name' and (tmp.name is None or tmp.name == ''):
                    tmpstr = inputbox('Name', 'Input name', 30, tmp.name)
                    if tmpstr is not None:
                        coding = chardet.detect(tmpstr)
                        if coding['encoding'] == 'ascii':
                            tmp.name = tmpstr
                        else:
                            msgbox("Please enter the \'Name\' using english")
                elif out == 'Description':
                    tmpstr = inputbox('Description',
                                      'Input description', 60, tmp.description)
                    if tmpstr is not None:
                        tmp.description = tmpstr
                elif out == 'Content':
                    tmp.content.edit()
                elif out == 'Coupon':
                    tmp.coupon.edit()
                # ProductSpec code start
                elif out == 'Display Name':
                    tmpstr = inputbox('Display Name',
                                      'Input display name', 60, tps.name)
                    if tmpstr is not None:
                        tps.name = tmpstr
                # ProductSpec code end


def vm_plan_list():
    plans = []
    pss = data.ProductSpec.select()
    for it in data.PricingPlan.select().where(data.PricingPlan.type == 'vm'):
        try:
            content_json = json.loads(it.content)
            coupon_json = json.loads(it.coupon)
            plan = VMPlan()
            plan.domain = domain_lcuuid_to_name(it.domain)
            plan.name = it.name
            plan.description = it.description
            plan.content = content_json
            plan.coupon = coupon_json
            # ProductSpec code start
            ps_info = None
            for it2 in pss:
                if (it2.plan_name == it.name and it2.domain == it.domain):
                    ps_info = it2
                    break
            if ps_info is not None:
                plan.product_spec = ProductSpec()
                plan.product_spec.name = it2.name
                plan.product_spec.lcuuid = it2.lcuuid
                plan.product_spec.content = PSContent(
                    json.loads(ps_info.content))
            # ProductSpec code end
            plan.validate()
        except Exception as e:
            print('%r: Error parsing price [%s] for %s:%s' %
                  (e, it.coupon, it.domain, it.name))
            continue
        plans.append(plan)

    while True:
        op = None
        out = None
        if len(plans) == 0:
            args = ['--title', 'VM Plans', '--yes-label', 'Add',
                    '--no-label', 'Exit',
                    '--yesno', '<n/a>', '8', '40']
            ret, _ = call_dialog(args)
            if ret == 0:
                op = 'add'
            else:
                op = 'cancel'
        else:
            items = []
            for it in plans:
                items.append(it.domain + ' ' + it.name)
                items.append(it.description)
                items.append('off')
            menulen = len(plans) + 1
            windowlen = menulen + 7
            args = ['--title', 'VM Plans', '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Delete',
                    '--cancel-label', 'Add',
                    '--help-button', '--help-label', 'Exit',
                    '--separate-output',
                    '--checklist', 'Contents', '%d' % windowlen,
                    '100', '%d' % menulen]
            args.extend(items)
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'delete'
            elif ret == 1:
                op = 'add'
            else:
                op = 'cancel'

        if op == 'cancel':
            return
        elif op == 'add':
            p = VMPlan()
            while True:
                name_collide = False
                if p.edit() is True:
                    for it in plans:
                        if it.domain == p.name and it.name == p.name:
                            msgbox('Name collides!')
                            name_collide = True
                            break
                else:
                    break
                if name_collide is False:
                    plans.append(p)
                    content = p.content.to_primitive()
                    if p.content.cpu.num == '':
                        del content['cpu']
                    if p.content.memory.size == '':
                        del content['memory']
                    if p.content.disk.size == '':
                        del content['disk']
                    data.PricingPlan.insert(
                        domain=domain_name_to_lcuuid(p.domain),
                        name=p.name,
                        type='vm',
                        description=p.description,
                        content=json.dumps(content),
                        coupon=json.dumps(p.coupon.to_primitive()),
                        expiry_type='never').execute()
                    # ProductSpec code start
                    ps_info = data.ProductSpec()
                    ps_info.name = p.product_spec.name
                    ps_info.lcuuid = uuid.uuid4()
                    ps_info.type = 1
                    ps_info.charge_mode = 4
                    ps_info.state = 1
                    ps_info.price = decimal.Decimal('0')
                    ps_info.content = json.dumps(
                        p.product_spec.content.to_primitive(role='vm'))
                    ps_info.domain = domain_name_to_lcuuid(p.domain)
                    ps_info.plan_name = p.name
                    ps_info.product_type = 1
                    ps_info.save()
                    # ProductSpec code end
                    break
        elif op == 'edit':
            to_edit = out.rstrip('\n').split('\n')
            if not to_edit or to_edit[0] == '':
                continue
            if len(to_edit) != 1:
                msgbox('Only one piece can be modified at a time.')
                continue
            (to_edit_domain, to_edit_name) = to_edit[0].split(' ')
            for it in plans:
                if it.domain == to_edit_domain and it.name == to_edit_name:
                    while True:
                        name_collide = False
                        if it.edit() is True:
                            for it2 in plans:
                                if it2 != it and it2.domain == it.domain and \
                                        it2.name == it.name:
                                    msgbox('Name collides!')
                                    name_collide = True
                                    break
                        else:
                            break
                        if name_collide is False:
                            content = it.content.to_primitive()
                            if it.content.cpu.num == '':
                                del content['cpu']
                            if it.content.memory.size == '':
                                del content['memory']
                            if it.content.disk.size == '':
                                del content['disk']
                            domain = domain_name_to_lcuuid(it.domain)
                            data.PricingPlan.update(
                                domain=domain,
                                name=it.name,
                                type='vm',
                                description=it.description,
                                content=json.dumps(content),
                                coupon=json.dumps(it.coupon.to_primitive()),
                                expiry_type='never').where(
                                    data.PricingPlan.domain == domain,
                                    data.PricingPlan.name == it.name).execute()
                            # ProductSpec code start
                            ps_info = data.ProductSpec()
                            ps_info.name = it.product_spec.name
                            ps_info.type = 1
                            ps_info.charge_mode = 4
                            ps_info.state = 1
                            ps_info.price = decimal.Decimal('0')
                            ps_info.content = json.dumps(
                                it.product_spec.content.to_primitive(
                                    role='vm'))
                            ps_info.product_type = 1
                            data.ProductSpec.update(
                                name=ps_info.name,
                                type=ps_info.type,
                                price=ps_info.price,
                                charge_mode=ps_info.charge_mode,
                                state=ps_info.state,
                                content=ps_info.content,
                                product_type=ps_info.product_type).where(
                                    data.ProductSpec.domain == domain,
                                    data.ProductSpec.plan_name == it.name
                                ).execute()
                            # ProductSpec code end
                            break
                    break
        elif op == 'delete':
            to_delete = out.rstrip('\n').split('\n')
            if not to_delete or to_delete[0] == '':
                continue
            quoted_to_delete = ["'%s'" % it for it in to_delete]
            if len(to_delete) == 1:
                msgline = quoted_to_delete[0]
            else:
                msgline = ', '.join(quoted_to_delete[0:-1])
                msgline += ' and '
                msgline += quoted_to_delete[-1]
            msgline = 'Confirm removing price for ' + msgline + '?'
            args = ['--title', 'Confirm', '--defaultno',
                    '--yesno', msgline, '8', '40']
            ret, _ = call_dialog(args)
            if ret == 0:
                plans = [it for it in plans
                         if (it.domain + ' ' + it.name) not in to_delete]
                for domain_name in to_delete:
                    (domain_name, name) = domain_name.split(' ')
                    domain = domain_name_to_lcuuid(domain_name)
                    data.PricingPlan.delete(
                        ).where(data.PricingPlan.domain == domain,
                                data.PricingPlan.name == name).execute()
                    # ProductSpec code start
                    data.ProductSpec.delete(
                        ).where(data.ProductSpec.domain == domain,
                                data.ProductSpec.plan_name == name).execute()
                    # ProductSpec code end


class PromotionRule(Model):
    name = StringType(required=True, default='')
    domain = StringType(required=True, default='')
    description = StringType(default='')
    purchase_type = StringType(default='')
    purchase_quantity = IntType(default=0)
    present_type = StringType(required=True, default='')
    present_quantity = IntType(required=True, default=0)
    present_days = IntType(required=True, default=0)
    event_start_time = StringType(default='')
    event_end_time = StringType(default='')

    def edit(self):
        tmp = PromotionRule()
        tmp.domain = self.domain
        tmp.name = self.name
        tmp.description = self.description
        tmp.purchase_type = self.purchase_type
        tmp.purchase_quantity = self.purchase_quantity
        tmp.present_type = self.present_type
        tmp.present_quantity = self.present_quantity
        tmp.present_days = self.present_days
        tmp.event_start_time = self.event_start_time
        tmp.event_end_time = self.event_end_time

        default_item = 'Domain'

        while True:
            op = None
            out = None
            if tmp.domain is None or tmp.domain == '':
                show_domain = '<select>'
            else:
                show_domain = tmp.domain
            if tmp.name is None or tmp.name == '':
                show_name = '<edit>'
            else:
                show_name = tmp.name
            if tmp.purchase_type is None or tmp.purchase_type == '':
                show_purchase_type = '<select>'
            else:
                show_purchase_type = tmp.purchase_type
            if tmp.purchase_quantity <= 0:
                show_purchase_quantity = '<edit>'
            else:
                show_purchase_quantity = str(tmp.purchase_quantity)
            if tmp.present_type is None or tmp.present_type == '':
                show_present_type = '<select>'
            else:
                show_present_type = tmp.present_type
            if tmp.present_quantity <= 0:
                show_present_quantity = '<edit>'
            else:
                show_present_quantity = str(tmp.present_quantity)
            if tmp.present_days <= 0:
                show_present_days = '<edit>'
            else:
                show_present_days = str(tmp.present_days)
            if tmp.event_start_time is None or tmp.event_start_time == '':
                show_event_start_time = '<edit>'
            else:
                show_event_start_time = tmp.event_start_time
            if tmp.event_end_time is None or tmp.event_end_time == '':
                show_event_end_time = '<edit>'
            else:
                show_event_end_time = tmp.event_end_time
            if tmp.description is None or tmp.description == '':
                show_description = '<edit>'
            else:
                show_description = tmp.description
            args = ['--title', 'Promotion Specification',
                    '--default-item', default_item,
                    '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Save',
                    '--menu', 'Promotion', '12', '50', '5',
                    'Domain', show_domain,
                    'Name', show_name,
                    'Description', show_description,
                    'PurchaseType', show_purchase_type,
                    'PurchaseQuantity', show_purchase_quantity,
                    'PresentType', show_present_type,
                    'PresentQuantity', show_present_quantity,
                    'PresentDays', show_present_days,
                    'EventStartTime', show_event_start_time,
                    'EventEndTime', show_event_end_time]

            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'save'
            else:
                op = 'cancel'
            if op == 'cancel':
                return False
            elif op == 'save':
                try:
                    tmp.validate()
                    self.domain = tmp.domain
                    self.name = tmp.name
                    self.description = tmp.description
                    self.purchase_type = tmp.purchase_type
                    self.purchase_quantity = tmp.purchase_quantity
                    self.present_type = tmp.present_type
                    self.present_quantity = tmp.present_quantity
                    self.present_days = tmp.present_days
                    self.event_start_time = tmp.event_start_time
                    self.event_end_time = tmp.event_end_time
                    return True
                except Exception as e:
                    msgbox('Invalid Promotion info!\n[%r]' % e)
            else:
                default_item = out
                if out == 'Domain' and (tmp.domain is None or
                                        tmp.domain == ''):
                    tmpstr = get_domain_name(tmp.domain)
                    if tmpstr is not None:
                        tmp.domain = tmpstr
                if out == 'Name' and (tmp.name is None or tmp.name == ''):
                    tmpstr = inputbox('Name', 'Input name', 30, tmp.name)
                    if tmpstr is not None:
                        coding = chardet.detect(tmpstr)
                        if coding['encoding'] == 'ascii':
                            tmp.name = tmpstr
                        else:
                            msgbox("Please enter the \'Name\' using english")
                if out == 'PurchaseType' and (tmp.purchase_type is None or
                                              tmp.purchase_type == ''):
                    tmpstr = get_type(tmp.purchase_type)
                    if tmpstr is not None:
                        tmp.purchase_type = tmpstr
                if out == 'PurchaseQuantity':
                    tmpstr = inputbox('Purchase quantity',
                                      'Input purchase quantity',
                                      30,
                                      str(tmp.purchase_quantity))
                    if tmpstr is not None:
                        tmp.purchase_quantity = int(tmpstr)
                if out == 'PresentType' and (tmp.present_type is None or
                                             tmp.present_type == ''):
                    tmpstr = get_type(tmp.present_type)
                    if tmpstr is not None:
                        tmp.present_type = tmpstr
                if out == 'PresentQuantity':
                    tmpstr = inputbox('Present quantity',
                                      'Input present quantity',
                                      30,
                                      str(tmp.purchase_quantity))
                    if tmpstr is not None:
                        tmp.present_quantity = int(tmpstr)
                if out == 'PresentDays':
                    tmpstr = inputbox('Present Days',
                                      'Input Present Days',
                                      30,
                                      str(tmp.present_days))
                    if tmpstr is not None:
                        tmp.present_days = str(tmpstr)
                if out == 'EventStartTime' and (tmp.event_start_time is None or
                                                tmp.event_start_time == ''):
                    tmpstr = inputbox(
                        'Event Start Time',
                        'Input Event Start Time, e.g. 2015-01-01-00',
                        60,
                        tmp.event_start_time)
                    if tmpstr is not None:
                        tmp.event_start_time = tmpstr
                if out == 'EventEndTime' and (tmp.event_end_time is None or
                                              tmp.event_end_time == ''):
                    tmpstr = inputbox(
                        'Event End Time',
                        'Input Event End Time, e.g. 2015-02-01-00',
                        60,
                        tmp.event_end_time)
                    if tmpstr is not None:
                        tmp.event_end_time = tmpstr
                elif out == 'Description':
                    tmpstr = inputbox('Description',
                                      'Input description', 60, tmp.description)
                    if tmpstr is not None:
                        tmp.description = tmpstr


def promotion_rules_list():
    rules = []
    promotion_rules = data.PromotionRules.select()

    for it in promotion_rules:
        try:
            rule = PromotionRule()
            rule.domain = domain_lcuuid_to_name(it.domain)
            rule.name = it.name
            rule.description = it.description
            rule.purchase_type = it.purchase_type
            rule.purchase_quantity = it.purchase_quantity
            rule.present_type = it.present_type
            rule.present_quantity = it.present_quantity
            rule.present_days = it.present_days
            rule.event_start_time = it.event_start_time.strftime("%Y-%m-%d-%H")
            rule.event_end_time = it.event_end_time.strftime("%Y-%m-%d-%H")

            rule.validate()
        except Exception as e:
            print('%r: Error parsing promotion_rule [%s] for %s:%s' %
                  (e, it.present_type, it.domain, it.name))
            continue
        rules.append(rule)

    while True:
        op = None
        out = None
        if len(rules) == 0:
            args = ['--title', 'Promotion Rules', '--yes-label', 'Add',
                    '--no-label', 'Exit',
                    '--yesno', '<n/a>', '8', '40']
            ret, _ = call_dialog(args)
            if ret == 0:
                op = 'add'
            else:
                op = 'cancel'
        else:
            items = []
            for it in rules:
                items.append(it.domain + ' ' + it.name)
                desc = it.description
                items.append(desc)
                items.append('off')
            menulen = len(rules) + 1
            windowlen = menulen + 7
            args = ['--title', 'Promotion Rules', '--ok-label', 'Edit',
                    '--extra-button', '--extra-label', 'Delete',
                    '--cancel-label', 'Add',
                    '--help-button', '--help-label', 'Exit',
                    '--separate-output',
                    '--checklist', 'Contents', '%d' % windowlen,
                    '100', '%d' % menulen]
            args.extend(items)
            ret, out = call_dialog(args)
            if ret == 0:
                op = 'edit'
            elif ret == 3:
                op = 'delete'
            elif ret == 1:
                op = 'add'
            else:
                op = 'cancel'

        if op == 'cancel':
            return
        elif op == 'add':
            p = PromotionRule()
            while True:
                name_collide = False
                if p.edit() is True:
                    for it in rules:
                        if it.domain == p.domain and it.name == p.name:
                            msgbox('Name collides!')
                            name_collide = True
                            break
                else:
                    break
                if name_collide is False:
                    rules.append(p)
                    data.PromotionRules.insert(
                        domain=domain_name_to_lcuuid(p.domain),
                        name=p.name,
                        purchase_type=display_type_to_db_type.get(
                            p.purchase_type, ''),
                        purchase_quantity=p.purchase_quantity,
                        present_type=display_type_to_db_type.get(
                            p.present_type, ''),
                        present_quantity=p.present_quantity,
                        present_days=p.present_days,
                        event_start_time=datetime.datetime.strptime(
                            p.event_start_time, '%Y-%m-%d-%H'),
                        event_end_time=datetime.datetime.strptime(
                            p.event_end_time, '%Y-%m-%d-%H'),
                        description=p.description).execute()
                    break
        elif op == 'edit':
            to_edit = out.rstrip('\n').split('\n')
            if not to_edit or to_edit[0] == '':
                continue
            if len(to_edit) != 1:
                msgbox('Only one piece can be modified at a time.')
                continue
            (to_edit_domain, to_edit_name) = to_edit[0].split(' ')
            for it in rules:
                if it.domain == to_edit_domain and it.name == to_edit_name:
                    while True:
                        name_collide = False
                        if it.edit() is True:
                            for it2 in rules:
                                if it2 != it and it2.domain == it.domain and \
                                        it2.name == it.name:
                                    msgbox('Name collides!')
                                    name_collide = True
                                    break
                        else:
                            break
                        if name_collide is False:
                            domain = domain_name_to_lcuuid(it.domain)
                            data.PromotionRules.update(
                                domain=domain,
                                name=it.name,
                                purchase_type=display_type_to_db_type.get(
                                    it.purchase_type, ''),
                                purchase_quantity=it.purchase_quantity,
                                present_type=display_type_to_db_type.get(
                                    it.present_type, ''),
                                present_quantity=it.present_quantity,
                                present_days=it.present_days,
                                event_start_time=datetime.datetime.strptime(
                                    it.event_start_time, '%Y-%m-%d-%H'),
                                event_end_time=datetime.datetime.strptime(
                                    it.event_end_time, '%Y-%m-%d-%H'),
                                description=it.description
                            ).where(data.PromotionRules.domain == domain,
                                    data.PromotionRules.name == it.name).\
                                execute()
                            break
                    break
        elif op == 'delete':
            to_delete = out.rstrip('\n').split('\n')
            if not to_delete or to_delete[0] == '':
                continue
            quoted_to_delete = ["'%s'" % it for it in to_delete]
            if len(to_delete) == 1:
                msgline = quoted_to_delete[0]
            else:
                msgline = ', '.join(quoted_to_delete[0:-1])
                msgline += ' and '
                msgline += quoted_to_delete[-1]
            msgline = 'Confirm removing promotion for ' + msgline + '?'
            args = ['--title', 'Confirm', '--defaultno',
                    '--yesno', msgline, '8', '60']
            ret, _ = call_dialog(args)
            if ret == 0:
                prices = [it for it in rules
                          if (it.domain + ' ' + it.name) not in to_delete]
                for domain_and_name in to_delete:
                    (domain_name, name) = domain_and_name.split(' ')
                    domain = domain_name_to_lcuuid(domain_name)
                    data.PromotionRules.delete(
                        ).where(data.PromotionRules.domain == domain,
                                data.PromotionRules.name == name).execute()


def main_dialog():
    default_item = 'Unit Prices'
    while True:
        itemlist = ['Unit Prices', 'VM Plans', 'Promotion Rules']
        args = ['--title', 'Product Specification Utilities',
                '--default-item', default_item,
                '--ok-label', 'Select',
                '--cancel-label', 'Exit',
                '--menu', 'Menu', '20', '40', '10']
        for it in itemlist:
            args.append(it)
            args.append('')
        ret, out = call_dialog(args)
        if ret == 0:
            default_item = out
            if out == 'Unit Prices':
                prices_list()
            elif out == 'VM Plans':
                vm_plan_list()
            elif out == 'Promotion Rules':
                promotion_rules_list()
        else:
            break

if __name__ == '__main__':
    main_dialog()
