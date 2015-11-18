# -*- coding: utf-8 -*-
import sys
import commands
import mtcmd
from const import VINTERFACE_STATE_ATTACH, VINTERFACE_DEVICETYPE, \
    VINTERFACE_TYPE_WAN, VINTERFACE_CONFIG_VLANTAG_LEVEL, \
    VINTERFACE_CONFIG_BROADC_BANDW_LEVEL
from data import VInterface, VM, VGateway, IP, HostDevice, Syslog

vifcmd = mtcmd.Command()


def vif_syslog(**args):
    vifname = '%s_%s_VIF_%d' % (args['devicetype'],
                                args['deviceid'],
                                args['ifindex'])
    syslog_new = Syslog(
        module='MNTNCT',
        action='Config VINTERFACE',
        objecttype=args['viftype'],
        objectname=vifname,
        objectid=args['vifid'],
        loginfo='Config VINTERFACE ' + vifname + ': ' + args['loginfo'],
        level=args['level'])
    syslog_new.save()


@vifcmd.define(name='vif.config',
               rqd_params='devicetype, deviceid, ifindex',
               opt_params='vlantag, broadc_bandw, broadc_ceil_bandw',
               description='对于vif.config vlantag，是将指定<deviceid>的'
               '虚拟机/虚拟网关的接口<ifindex>的VLAN变更为指定的<vlantag>；'
               '\n                          '
               '对于vif.config broadc_bandw，是将指定<deviceid>的'
               '虚拟机/虚拟网关的接口<ifindex>的广播/组播带宽设置为'
               '指定的最小<broadc_bandw>以及最大<broadc_ceil_bandw>。',
               example='mt vif.config devicetype=VM '
               'deviceid=10 ifindex=0 vlantag=10'
               '\n                          '
               'mt vif.config devicetype=VGATEWAY '
               'deviceid=256 ifindex=1 broadc_bandw=128 broadc_ceil_bandw=256')
def vif_conifg(args):
    assert isinstance(args, dict)
    assert args['devicetype'] in VINTERFACE_DEVICETYPE.keys()

    devicetype = VINTERFACE_DEVICETYPE[args['devicetype']]
    try:
        vif = VInterface.get((VInterface.devicetype == devicetype) &
                             (VInterface.deviceid == args['deviceid']) &
                             (VInterface.ifindex == args['ifindex']))
    except Exception:
        print >>sys.stderr, 'Error: vif not found'
        return -1

    try:
        if args['devicetype'] == 'VM':
            vdevice = VM.get(VM.id == args['deviceid'])
        else:
            vdevice = VGateway.get(VGateway.id == args['deviceid'])
    except Exception:
        print >>sys.stderr, 'Error: VM/VGATEWAY %s not found' % \
            args['deviceid']
        return -1
    launch_server = vdevice.launch_server if args['devicetype'] == 'VM' else \
        vdevice.gw_launch_server
    try:
        host = HostDevice.get(HostDevice.ip == launch_server)
    except Exception:
        print >>sys.stderr, 'Error: Host device %s not found' % launch_server
        return -1
    cmd = 'sshpass -p %s ssh -o ConnectTimeout=9 %s@%s ' % (host.user_passwd,
                                                            host.user_name,
                                                            launch_server)

    if 'vlantag' in args:
        if vif.state != VINTERFACE_STATE_ATTACH:
            print >>sys.stderr, 'Error: vif is detached'
            return -1
        if vif.iftype != VINTERFACE_TYPE_WAN:
            print >>sys.stderr, 'Error: vif is not WAN'
            return -1

        if args['devicetype'] == 'VM':
            cmd += 'sh /usr/local/livecloud/pyagexec/script/vport.sh UPDATE '\
                'vlantag %s %s' % (vif.mac, args['vlantag'])
        else:
            cmd += 'sh /usr/local/livegate/script/router.sh update '\
                'vlantag %s %s' % (vif.mac, args['vlantag'])
        rc, output = commands.getstatusoutput(cmd)
        if rc:
            print >>sys.stderr, 'Error: "%s" failed' % cmd
            print >>sys.stderr, 'Error: %s' % output
            vif_syslog(
                viftype=vif.iftype, devicetype=args['devicetype'],
                deviceid=vif.deviceid, ifindex=vif.ifindex, vifid=vif.id,
                loginfo='vlantag config failed',
                level=VINTERFACE_CONFIG_VLANTAG_LEVEL)
            return -1
        ips = IP.select().where(IP.vifid == vif.id)
        for ip in ips:
            ip.vlantag = args['vlantag']
            ip.save()
        old_vlantag = vif.vlantag
        vif.vlantag = args['vlantag']
        vif.save()
        vif_syslog(
            viftype=vif.iftype, devicetype=args['devicetype'],
            deviceid=vif.deviceid, ifindex=vif.ifindex, vifid=vif.id,
            loginfo='vlantag config successful on launch_server %s,'
                    ' from %d to %s' % (launch_server, old_vlantag,
                                        args['vlantag']),
            level=VINTERFACE_CONFIG_VLANTAG_LEVEL)
        print "SUCCESS"
        return

    if 'broadc_bandw' in args or 'broadc_ceil_bandw' in args:
        if vif.state != VINTERFACE_STATE_ATTACH:
            print >>sys.stderr, 'Error: vif is detached'
            return -1
        if args['devicetype'] != 'VM' and vif.iftype != VINTERFACE_TYPE_WAN:
            print >>sys.stderr, 'Error: vif is not VGATEWAY WAN or VM DATA'
            return -1
        if 'broadc_bandw' not in args:
            print >>sys.stderr, 'Error: broadc_bandw is also required '\
                'for broadc_ceil_bandw'
            return -1
        if 'broadc_ceil_bandw' not in args:
            print >>sys.stderr, 'Error: broadc_ceil_bandw is also '\
                'required for broadc_bandw'
            return -1
        if int(args['broadc_bandw']) > int(args['broadc_ceil_bandw']):
            print >>sys.stderr, 'Error: broadc_bandw cannot be larger '\
                'than broadc_ceil_bandw'
            return -1

        if int(args['broadc_ceil_bandw']) > 0:
            if args['devicetype'] == 'VM':
                cmd += 'sh /usr/local/livecloud/pyagexec/script/vport.sh ADD-BROADCAST-QOS '\
                    '%s %s %s' % (vif.mac,
                                  args['broadc_bandw'],
                                  args['broadc_ceil_bandw'])
            else:
                cmd += 'sh /usr/local/livegate/script/router.sh add broadcast-qos '\
                    '%s %s %s %s' % (vif.deviceid, vif.ifindex,
                                     args['broadc_bandw'],
                                     args['broadc_ceil_bandw'])
        else:
            if args['devicetype'] == 'VM':
                cmd += 'sh /usr/local/livecloud/pyagexec/script/vport.sh DEL-BROADCAST-QOS '\
                    '%s' % vif.mac
            else:
                cmd += 'sh /usr/local/livegate/script/router.sh delete broadcast-qos '\
                    '%s %s' % (vif.deviceid, vif.ifindex)
        rc, output = commands.getstatusoutput(cmd)
        if rc:
            print >>sys.stderr, 'Error: "%s" failed' % cmd
            print >>sys.stderr, 'Error: %s' % output
            vif_syslog(
                viftype=vif.iftype, devicetype=args['devicetype'],
                deviceid=vif.deviceid, ifindex=vif.ifindex, vifid=vif.id,
                loginfo='broadcast bandwidth config failed',
                level=VINTERFACE_CONFIG_BROADC_BANDW_LEVEL)
            return -1
        vif.broadc_bandw = args['broadc_bandw']
        vif.broadc_ceil_bandw = args['broadc_ceil_bandw']
        vif.save()
        vif_syslog(
            viftype=vif.iftype, devicetype=args['devicetype'],
            deviceid=vif.deviceid, ifindex=vif.ifindex, vifid=vif.id,
            loginfo='broadcast bandwidth config successful on '
                    'launch_server %s' % launch_server,
            level=VINTERFACE_CONFIG_BROADC_BANDW_LEVEL)
        print "SUCCESS"
        return

    print "ERROR: No config parameter is given"
