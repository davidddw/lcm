import requests
import sys
import os
import subprocess

import mtcmd
import models
import data
from const import TALKER_URL, PYAGEXEC_URL
from const import HOST_HTYPE_KVM
from utils import call_post_with_callback

vmcmd = mtcmd.Command()


@vmcmd.define(name='vm-agent.show',
              opt_params='launch-server',
              description='Show version of vagent in VMs',
              example='mt vm-agent.show launch-server=172.16.1.131')
def vm_vagent_show(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'vms/'
    if 'launch-server' in args:
        url += '?launch_server=%s' % args['launch-server']
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get VM'
        return -1
    data = resp.json()
    for vm in data['DATA']:
        ctrl_interface = vm['INTERFACES'][-1]
        if ctrl_interface['IF_INDEX'] != 6 or ctrl_interface['STATE'] != 1:
            continue
        print >>sys.stderr, 'Agent version of %s' % vm['NAME']
        url = PYAGEXEC_URL % vm['LAUNCH_SERVER']
        url += 'vagent/%s/' % ctrl_interface['CONTROL']['IP']
        try:
            resp = requests.get(url)
        except requests.exceptions.ConnectionError:
            print >>sys.stderr, 'Error: unable to connet to pyagexec'
            return -1
        if resp.status_code != 200:
            print >>sys.stderr, 'Error: request failed'
            continue
        json_data = resp.json()
        print >>sys.stderr, '  Version: %s' % json_data['DATA']['VERSION']
        print >>sys.stderr, '  Pack Time: %s' % json_data['DATA']['PACKTIME']
        print >>sys.stderr, '  Git Commit: %s' % json_data['DATA']['GITCOMMIT']

    return 0


@vmcmd.define(name='vm-agent.upgrade',
              opt_params='launch-server',
              description='Upgrade vagent in VMs',
              example='mt vm-agent.upgrade launch-server=172.16.1.131')
def vm_vagent_upgrade(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'vms/'
    if 'launch-server' in args:
        url += '?launch_server=%s' % args['launch-server']
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get VM'
        return -1
    data = resp.json()
    for vm in data['DATA']:
        ctrl_interface = vm['INTERFACES'][-1]
        if ctrl_interface['IF_INDEX'] != 6 or ctrl_interface['STATE'] != 1:
            continue
        print >>sys.stderr, 'Update agent for %s' % vm['NAME']
        url = PYAGEXEC_URL % vm['LAUNCH_SERVER']
        url += 'vagent/%s/upgrade/' % ctrl_interface['CONTROL']['IP']
        try:
            resp = requests.post(url)
        except requests.exceptions.ConnectionError:
            print >>sys.stderr, 'Error: unable to connet to pyagexec'
            return -1
        if resp.status_code != 200:
            print >>sys.stderr, 'Error: request failed'
            continue

    return 0


@vmcmd.define(name='vm.import',
              rqd_params='launch-server, label, os, user-email, '
                         'product-specification, domain',
              opt_params='',
              description='Import VM to database',
              example='mt vm.import launch-server=172.16.1.131 label=vm-1 '
                      'os=template_name user-email=x@yunshan.net.cn '
                      'product-specification=Basic domain=Beijing')
def vm_import(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'hosts/'
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get hosts'
        return -1
    r_data = resp.json()
    if 'DATA' not in r_data:
        print >>sys.stderr, 'Error: unable to get hosts'
        return -1

    launch_server_htype = None
    for it in r_data['DATA']:
        if args['launch-server'] == it.get('IP', None):
            launch_server_htype = it.get('HTYPE', None)
            break
    if launch_server_htype is None:
        print >>sys.stderr, 'Error: launch_server %s not found' % \
            args['launch-server']
        return -1
    if launch_server_htype != HOST_HTYPE_KVM:
        call_args = []
        for key, value in args.items():
            call_args.append('%s=%s' % (key, value))

        if os.path.isfile('/usr/local/bin/mntnct'):
            call_cmd = 'mntnct'
        else:
            call_cmd = (os.path.dirname(os.path.realpath(__file__)) +
                        '/mntnct')
        subprocess.call([call_cmd, 'vm.import'] + call_args)
        return

    url = TALKER_URL + 'domains/'
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get domains'
        return -1
    r_data = resp.json()
    if 'DATA' not in r_data:
        print >>sys.stderr, 'Error: unable to get domains'
        return -1

    domain_uuid = None
    for it in r_data['DATA']:
        if args['domain'] == it.get('NAME', None):
            domain_uuid = it.get('LCUUID', None)
            break
    if domain_uuid is None:
        print >>sys.stderr, 'Error: domain %s not found' % args['domain']
        return -1

    try:
        user = data.User.get(data.User.email == args['user-email'])
        userid = user.id
    except Exception:
        print >>sys.stderr, 'Error: user %s not found' % \
            args['user-email']
        return -1

    try:
        ps = data.OssProductSpec.get(
            data.OssProductSpec.plan_name == args['product-specification'],
            data.OssProductSpec.domain == domain_uuid)
        ps_lcuuid = ps.lcuuid
    except Exception:
        print >>sys.stderr, 'Error: product-specification %s not found' % \
            args['product-specification']
        return -1

    vm_info = models.VMImport()
    vm_info.name = args['label']
    vm_info.os = args['os']
    vm_info.launch_server = args['launch-server']
    vm_info.userid = userid
    vm_info.product_specification_lcuuid = ps_lcuuid
    vm_info.domain = domain_uuid

    resp = call_post_with_callback(TALKER_URL + 'vms/import/',
                                   data=vm_info.to_primitive())

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print >>sys.stdout, ''
    print >>sys.stdout, 'NAME: %s' % resp['DATA']['NAME']
    print >>sys.stdout, 'LCUUID: %s' % resp['DATA']['LCUUID']
    print >>sys.stdout, 'STATE: %d' % resp['DATA']['STATE']
    print >>sys.stdout, 'CPU: %d' % resp['DATA']['VCPU_NUM']
    print >>sys.stdout, 'MEM_SIZE: %d M' % resp['DATA']['MEM_SIZE']
    print >>sys.stdout, 'SYS_DISK_SIZE: %d G' % resp['DATA']['VDI_SYS_SIZE']
    print >>sys.stdout, 'SYS_SR_NAME: %s' % resp['DATA']['VDI_SYS_SR_NAME']

    return 0
