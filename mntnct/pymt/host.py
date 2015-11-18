import sys

import mtcmd
import models
from const import TALKER_URL
from utils import call_post_with_callback, call_patch_with_callback

hostcmd = mtcmd.Command()


@hostcmd.define(name='host.add',
                rqd_params='ip, user_name, user_passwd, type, rack_name, '
                           'htype, nic_type',
                opt_params='uplink_ip, uplink_netmask, uplink_gateway, misc, '
                           'brand, model, storage_link_ip',
                description='Add host device to LiveCloud',
                example='mt host.add ip=x.x.x.x user_name=USER '
                        'user_passwd=PASSWD type=VM/Gateway/NSP '
                        'rack_name=rackname htype=Xen/VMWare/KVM '
                        'nic_type=Gigabit/10Gigabit [ uplink_ip=x.x.x.x '
                        'uplink_netmask=x.x.x.x uplink_gateway=x.x.x.x '
                        'storage_link_ip=x.x.x.x]')
def host_add(args):
    assert isinstance(args, dict)
    # remove this assert after adding more type and htype
    assert (args['type'].lower() in ['vm', 'server', '1'] and
            args['htype'].lower() == 'kvm')

    host_info = models.HostAdd()

    host_info.ip = args['ip']
    host_info.user_name = args['user_name']
    host_info.user_passwd = args['user_passwd']
    if args['type'].lower() in ['vm', 'server', '1']:
        host_info.instance_type = host_info.instance_types.VM
        if args['htype'].lower() in ['xen', 'xenserver', 'xcp', '1']:
            host_info.hypervisor_type = host_info.hypervisor_types.XEN
        elif args['htype'].lower() in ['vmware', 'esxi', '2']:
            host_info.hypervisor_type = host_info.hypervisor_types.VMWARE
        elif args['htype'].lower() in ['kvm', '3']:
            host_info.hypervisor_type = host_info.hypervisor_types.KVM
    elif args['type'].lower() in ['nspgateway', 'nsp', '3']:
        host_info.instance_type = host_info.instance_types.NSP
        host_info.hypervisor_type = host_info.hypervisor_types.KVM
    host_info.rack_name = args['rack_name']
    if args['nic_type'].lower() == 'gigabit':
        host_info.nic_type = host_info.nic_types.GIGABIT
    elif args['nic_type'].lower() == '10gigabit':
        host_info.nic_type = host_info.nic_types.XGIGABIT
    host_info.uplink_ip = args.get('uplink_ip')
    host_info.uplink_netmask = args.get('uplink_netmask')
    host_info.uplink_gateway = args.get('uplink_gateway')
    host_info.misc = args.get('misc')
    host_info.brand = args.get('brand')
    host_info.model = args.get('model')
    host_info.storage_link_ip = args.get('storage_link_ip')

    if (host_info.storage_link_ip is None and
            host_info.hypervisor_type == host_info.hypervisor_types.KVM and
            host_info.instance_type == host_info.instance_types.VM):
        print >>sys.stderr, 'Error: No storage_link_ip specified'
        return -1

    resp = call_post_with_callback(TALKER_URL + 'hosts/',
                                   data=host_info.to_primitive())

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print resp['DATA']['LCUUID']
    if host_info.instance_type != host_info.instance_types.NSP:
        print >>sys.stderr, 'Please activate storage of this host before use'

    return 0


@hostcmd.define(name='host.rescan',
                rqd_params='ip, type, htype',
                opt_params='',
                description='Rescan host device in LiveCloud',
                example='mt host.rescan ip=x.x.x.x')
def host_rescan(args):
    assert isinstance(args, dict)

    resp = call_patch_with_callback(TALKER_URL + 'hosts/%s/' % args['ip'],
                                    data={'ACTION': 'RESCAN'})

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print resp['DATA']['LCUUID']
    print >>sys.stderr, 'Please activate storage of this host before use'

    return 0


@hostcmd.define(name='host.switch',
                rqd_params='ip',
                opt_params='',
                description='Switch VMs on host in LiveCloud',
                example='mt host.switch ip=x.x.x.x')
def host_switch(args):
    assert isinstance(args, dict)

    resp = call_patch_with_callback(TALKER_URL + 'hosts/%s/' % args['ip'],
                                    data={'ACTION': 'SWITCH'})

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print >>sys.stderr, 'VMs on host %s is switching, '\
        'you can view progress through seetalker' % resp['DATA']['IP']

    return 0
