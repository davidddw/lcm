import requests
import sys
import commands
import mtcmd
import models
import json
from const import TALKER_URL, PYAGEXEC_URL, JSON_HEADER, \
    SCRIPT_SWITCH_CONF
from data import NetworkDevice

switchcmd = mtcmd.Command()


@switchcmd.define(
    name='switch-port-mirror.add',
    rqd_params='mip, src_ports_eth, dst_ports_eth, src_ports_po, dst_ports_po',
    description='Mirror switch ports for capturing',
    example='mt switch-port-mirror.add '
    'mip=172.17.1.111 src_ports_eth=47,48 dst_ports_eth=46 src_ports_po=10'
    ' dst_ports_po=none')
def switch_port_mirror_add(args):
    assert isinstance(args, dict)

    switch_info = NetworkDevice.get(NetworkDevice.mip == args['mip'])
    cmd = SCRIPT_SWITCH_CONF
    cmd += "add-mirror %s %s %s %s '%s' %s %s %s %s" % (
        switch_info.brand, switch_info.mip, switch_info.username,
        switch_info.password, switch_info.enable, args['src_ports_eth'],
        args['dst_ports_eth'], args['src_ports_po'], args['dst_ports_po'])
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output

    return 0


@switchcmd.define(
    name='switch-port-mirror.remove',
    rqd_params='mip',
    description='Remove switch ports mirroring',
    example='mt switch-port-mirror.remove '
    'switch_brand=Arista username=admin passwd=yunshan3302 '
    'enable_passwd=yunshan mip=172.17.1.111')
def switch_port_mirror_remove(args):
    assert isinstance(args, dict)

    switch_info = NetworkDevice.get(NetworkDevice.mip == args['mip'])
    cmd = SCRIPT_SWITCH_CONF
    cmd += "del-mirror %s %s %s %s '%s'" % (
        switch_info.brand, switch_info.mip, switch_info.username,
        switch_info.password, switch_info.enable)
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output

    return 0
