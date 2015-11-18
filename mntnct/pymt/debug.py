# -*- coding: utf-8 -*-
import sys
import commands
import mtcmd
import requests
import json
from const import VINTERFACE_STATE_ATTACH, VINTERFACE_DEVICETYPE, \
    VINTERFACE_TYPE_CTRL, VINTERFACE_TYPE_SERV, \
    VINTERFACE_TYPE_WAN, VINTERFACE_TYPE_LAN, ROLE_VALVE, \
    VGATEWAY_POLICY_SNAT, VGATEWAY_POLICY_DNAT, VGATEWAY_POLICY_ACL, \
    HOST_TYPE_VM, HOST_TYPE_NSP, SCRIPT_SWITCH_CONF, VM_FLAG_ISOLATE, \
    MASK_MAX, VGATEWAY_POLICY_ENABLE, SWITCH_TYPE_TUNNEL, SDNCTRL_URL
from data import VM, VInterface, VGateway, IP, VInterfaceIP, HostDevice, \
    VgwPolicy, VgwVpn, VgwRoute, Vl2TunnelPolicy, RackTunnel, SysConfig, \
    DomainConfig, Rack, Epc, User, NetworkDevice, ThirdPartyDevice, VL2, \
    Vl2Net

debugcmd = mtcmd.Command()


def bin_to_ip(binaddr):
    """binary IP to string
    """
    return '%d.%d.%d.%d' % (binaddr >> 24,
                            (binaddr & 0x00FFFFFF) >> 16,
                            (binaddr & 0x0000FFFF) >> 8,
                            binaddr & 0x000000FF)


def ip_to_bin(ipaddr):
    """string IP to binary
    """
    (a, b, c, d) = [int(str) for str in ipaddr.split('.')]
    return (a << 24) + (b << 16) + (c << 8) + d


def netmask2masklen(netmask):
    """convert netmask to masklen
    """
    ip_bin = ip_to_bin(netmask)
    l = 0
    while ip_bin:
        l += ip_bin & 0x1
        ip_bin >>= 1
    return l


def ip_range_to_netmask(beg, end):
    xor = beg ^ end
    ps1 = xor + 1
    chk = xor & ps1
    if chk == 0:
        flip = MASK_MAX ^ xor
        return flip
    else:
        return 0


@debugcmd.define(name='debug.vgw_vif',
                 rqd_params='deviceid, ifindex',
                 description='检查NSP服务器上的vgateway_id为'
                 '<deviceid>的虚拟网关/带宽共享器的接口<ifindex>的'
                 '配置是否和数据库一致并且状态正确。'
                 '\n                          '
                 '比如，deviceid=256并且ifindex=1，即对接口256-w-1'
                 '执行检查。如果检查通过，则会输出如下信息：'
                 '\n                          '
                 'Info: VGATEWAY(7) 256 VINTERFACE 1 SUCCESS'
                 '\n                          '
                 '其中括号中如果是7则表示虚拟网关，如果是11则表示'
                 '带宽共享器。如果检查失败，则会输出Error信息。'
                 '\n                          '
                 '对于配置检查失败的情况，最简单的恢复方法是执行'
                 '/usr/local/livecloud/script/upgrade/'
                 'vgateway_migrate.sh脚本来原地迁移一下对应的虚'
                 '拟网关/带宽共享器，或者在WEB界面上重新执行一次'
                 '配置。',
                 example='mt debug.vgw_vif deviceid=256 ifindex=1')
def debug_vgw_vif(args):
    assert isinstance(args, dict)

    devicetype = VINTERFACE_DEVICETYPE['VGATEWAY']
    try:
        vif = VInterface.get((VInterface.devicetype == devicetype) &
                             (VInterface.deviceid == args['deviceid']) &
                             (VInterface.ifindex == args['ifindex']))
    except Exception:
        print >>sys.stderr, 'Error: VGATEWAY %s VINTERFACE %s not found' % \
            (args['deviceid'], args['ifindex'])
        return -1

    try:
        vdevice = VGateway.get(VGateway.id == args['deviceid'])
    except Exception:
        print >>sys.stderr, 'Error: VGATEWAY %s not found' % \
            args['deviceid']
        return -1
    launch_server = vdevice.gw_launch_server
    try:
        host = HostDevice.get(HostDevice.ip == launch_server)
    except Exception:
        print >>sys.stderr, 'Error: Host device %s not found' % launch_server
        return -1
    cmd = 'sshpass -p %s ssh -o ConnectTimeout=9 %s@%s ' % (host.user_passwd,
                                                            host.user_name,
                                                            launch_server)
    if vif.state != VINTERFACE_STATE_ATTACH:
        print >>sys.stderr, 'Error: VGATEWAY %s VINTERFACE %s not attached' % \
            (args['deviceid'], args['ifindex'])
        return -1
    cmd += 'sh /usr/local/livegate/script/debug.sh '
    if vdevice.role == ROLE_VALVE:
        if vif.iftype == VINTERFACE_TYPE_WAN:
            cmd += 'valve-wan-vport %d %d --state --mtu --ip ' % \
                (vif.deviceid, vif.ifindex)
            try:
                ips = IP.select().where(IP.vifid == vif.id)
            except Exception:
                print >>sys.stderr, 'Error: VINTERFACE %d-w-%d IP not found' % \
                    (vif.deviceid, vif.ifindex)
                return -1
            for ip in ips:
                cmd += '%s/%d ' % (ip.ip, ip.netmask)
            cmd += '--gateway %s --isp %d --vlan %d ' % \
                (ips[0].gateway, ips[0].isp, vif.vlantag)
            if vif.bandw == 0:
                wan_vif_num = VInterface.select().where(
                    (VInterface.devicetype == devicetype) &
                    (VInterface.deviceid == args['deviceid']) &
                    (VInterface.iftype == VINTERFACE_TYPE_WAN) &
                    (VInterface.state == VINTERFACE_STATE_ATTACH)).count()
                if wan_vif_num > 1:
                    vif.bandw = vif.ceil_bandw / wan_vif_num
            if vif.ceil_bandw != 0:
                vif.ceil_bandw = 128 if vif.ceil_bandw < 128 \
                    else vif.ceil_bandw
                vif.bandw = 128 if vif.bandw < 128 \
                    else vif.bandw
            if vif.broadc_ceil_bandw != 0:
                vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                    else vif.broadc_ceil_bandw
                vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                    else vif.broadc_bandw
            cmd += '--qos %d %d 128000 128000 %d %d --policy ' % \
                (vif.bandw, vif.ceil_bandw,
                 vif.broadc_bandw, vif.broadc_ceil_bandw)
        else:
            cmd += 'valve-lan-vport %d %d --state --mtu ' % \
                (vif.deviceid, vif.ifindex)
            cmd += '--vlan %d ' % vif.vlantag
            cmd += '--qos 0 0 0 0 0 0 --policy '
            try:
                rack = Rack.get(Rack.id == host.rackid)
            except Exception:
                pass
            if rack is not None \
                and rack.switch_type != SWITCH_TYPE_TUNNEL \
                    and host.uplink_ip != '':
                try:
                    vl2_tunnel_policies = Vl2TunnelPolicy.select().where(
                        Vl2TunnelPolicy.vl2id == vif.subnetid)
                except Exception:
                    print >>sys.stderr, 'Error: VL2 %s not found' % \
                        vif.subnetid
                    return -1
                for vl2_tunnel_policy in vl2_tunnel_policies:
                    if vl2_tunnel_policy.uplink_ip != host.uplink_ip:
                        cmd += '--tunnel %d ' % vif.subnetid
                        break
    else:
        if vif.iftype == VINTERFACE_TYPE_WAN:
            cmd += 'router-wan-vport %d %d --state --mac %s --mtu --ip ' % \
                (vif.deviceid, vif.ifindex, vif.mac)
            try:
                ips = IP.select().where(IP.vifid == vif.id)
            except Exception:
                print >>sys.stderr, 'Error: VINTERFACE %d-w-%d IP not found' % \
                    (vif.deviceid, vif.ifindex)
                return -1
            for ip in ips:
                cmd += '%s/%d ' % (ip.ip, ip.netmask)
            cmd += '--gateway %s --isp %d --vlan %d ' % \
                (ips[0].gateway, ips[0].isp, vif.vlantag)
            if vif.bandw != 0:
                vif.bandw = 128 if vif.bandw < 128 \
                    else vif.bandw
            if vif.broadc_ceil_bandw != 0:
                vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                    else vif.broadc_ceil_bandw
                vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                    else vif.broadc_bandw
            cmd += '--qos %d %d 128000 128000 %d %d --policy ' % \
                (vif.bandw, vif.bandw,
                 vif.broadc_bandw, vif.broadc_ceil_bandw)
            try:
                snats = VgwPolicy.select().where(
                    (VgwPolicy.vnetid == vif.deviceid) &
                    (VgwPolicy.policy_type == VGATEWAY_POLICY_SNAT) &
                    (VgwPolicy.if_index_2 == vif.ifindex) &
                    (VgwPolicy.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if snats is not None:
                for snat in snats:
                    cmd += '--snat %d %s %s %s %s %d ' % \
                        (snat.protocol, snat.min_addr_1, snat.max_addr_1,
                         snat.min_addr_2, snat.max_addr_2, snat.isp)
            try:
                dnats = VgwPolicy.select().where(
                    (VgwPolicy.vnetid == vif.deviceid) &
                    (VgwPolicy.policy_type == VGATEWAY_POLICY_DNAT) &
                    (VgwPolicy.if_index_1 == vif.ifindex) &
                    (VgwPolicy.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if dnats is not None:
                for dnat in dnats:
                    cmd += '--dnat %d %s %s %d %d %s %s %d %d %d ' % \
                        (dnat.protocol, dnat.min_addr_1, dnat.max_addr_1,
                         dnat.min_port_1, dnat.max_port_1,
                         dnat.min_addr_2, dnat.max_addr_2,
                         dnat.min_port_2, dnat.max_port_2, dnat.isp)
            try:
                vpns = VgwVpn.select().where(
                    (VgwVpn.vnetid == vif.deviceid) &
                    (VgwVpn.isp == ips[0].isp) &
                    (VgwVpn.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if vpns is not None:
                for vpn in vpns:
                    cmd += '--vpn %s %s %s %s %s %s %s %d %s ' % \
                        (vpn.local_ip_addr, vpn.local_net_addr,
                         vpn.local_net_mask,
                         vpn.remote_ip_addr, vpn.remote_net_addr,
                         vpn.remote_net_mask,
                         vpn.psk, vpn.isp, vpn.name)
            try:
                routes = VgwRoute.select().where(
                    (VgwRoute.vnetid == vif.deviceid) &
                    (VgwRoute.isp == ips[0].isp) &
                    (VgwRoute.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if routes is not None:
                for route in routes:
                    cmd += '--route %s %s %s %d ' % \
                        (route.dst_net_addr, route.dst_net_mask,
                         route.next_hop, route.isp)
        else:
            cmd += 'router-lan-vport %d %d --state --mac %s --mtu --ip ' % \
                (vif.deviceid, vif.ifindex, vif.mac)
            try:
                ips = VInterfaceIP.select().where(VInterfaceIP.vifid == vif.id)
            except Exception:
                print >>sys.stderr, 'Error: VINTERFACE %d-l-%d IP not found' % \
                    (vif.deviceid, vif.ifindex)
                return -1
            for ip in ips:
                cmd += '%s/%d ' % (ip.ip, netmask2masklen(ip.netmask))
            cmd += '--vlan %d ' % vif.vlantag
            if vif.bandw != 0:
                vif.bandw = 128 if vif.bandw < 128 \
                    else vif.bandw
            cmd += '--qos %d %d 128000 128000 %d %d --policy ' % \
                (128 if vif.bandw != 0 else 0, vif.bandw,
                 vif.broadc_bandw, vif.broadc_ceil_bandw)
            try:
                acls = VgwPolicy.select().where(
                    (VgwPolicy.vnetid == vif.deviceid) &
                    (VgwPolicy.policy_type == VGATEWAY_POLICY_ACL) &
                    ((VgwPolicy.if_index_1 == vif.ifindex) |
                     (VgwPolicy.if_index_2 == vif.ifindex) |
                     ((VgwPolicy.if_index_1 == 0) &
                      (VgwPolicy.if_index_1 == 0))) &
                    (VgwPolicy.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if acls is not None:
                for acl in acls:
                    cmd += '--acl %d %s %d %s %s %d %d %s %d %s %s %d %d %s ' % \
                        (acl.protocol, acl.if_type_1, acl.if_index_1,
                         acl.min_addr_1, acl.max_addr_1,
                         acl.min_port_1, acl.max_port_1,
                         acl.if_type_2, acl.if_index_2,
                         acl.min_addr_2, acl.max_addr_2,
                         acl.min_port_2, acl.max_port_2, acl.action)
            try:
                routes = VgwRoute.select().where(
                    (VgwRoute.vnetid == vif.deviceid) &
                    (VgwRoute.isp == ips[0].isp) &
                    (VgwRoute.state == VGATEWAY_POLICY_ENABLE))
            except Exception:
                pass
            if routes is not None:
                for route in routes:
                    cmd += '--route %s %s %s %d ' % \
                        (route.dst_net_addr, route.dst_net_mask,
                         route.next_hop, route.isp)
            try:
                rack = Rack.get(Rack.id == host.rackid)
            except Exception:
                pass
            if rack is not None \
                and rack.switch_type != SWITCH_TYPE_TUNNEL \
                    and host.uplink_ip != '':
                try:
                    vl2_tunnel_policies = Vl2TunnelPolicy.select().where(
                        Vl2TunnelPolicy.vl2id == vif.subnetid)
                except Exception:
                    print >>sys.stderr, 'Error: VL2 %s not found' % \
                        vif.subnetid
                    return -1
                for vl2_tunnel_policy in vl2_tunnel_policies:
                    if vl2_tunnel_policy.uplink_ip != host.uplink_ip:
                        cmd += '--tunnel %d ' % vif.subnetid
                        break
            cmd += '--conn_limit %d %d' % \
                (vdevice.conn_max, vdevice.new_conn_per_sec)
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        return -1
#    print 'Info: "%s" succeed' % cmd
    print 'Info: VGATEWAY(%d) %s VINTERFACE %s SUCCESS' % \
        (vdevice.role, args['deviceid'], args['ifindex'])
    return 0


@debugcmd.define(name='debug.vgw_server',
                 rqd_params='ip',
                 description='检查NSP服务器本身的配置是否正确，以'
                 '及该服务器上所承载的所有虚拟网关/带宽共享器的所'
                 '有接口的配置是否和数据库一致并且状态正确。'
                 '\n                          '
                 '如果服务器本身检查通过，则继续检查其上的虚拟网'
                 '关/带宽共享器的接口配置，不会输出提示信息。'
                 '如果检查失败，则会输出Error信息。'
                 '\n                          '
                 '对于配置检查失败的情况，最简单的恢复方法是执行'
                 '/usr/local/livecloud/script/nspbr_config.sh'
                 '脚本来恢复一下服务器的网络配置。不过，对于内核'
                 '版本不一致或者服务状态不正确的情况，则需要更新'
                 '内核或通过systemctl设置服务状态。'
                 '\n                          '
                 '此外，检查所有虚拟网关/带宽共享器的接口时，如'
                 '果检查通过，最后会输出Info信息，如果检查有错'
                 '误，则会输出Error信息，并会返回一个负值，提示'
                 '有多少处检查失败。'
                 '\n                          '
                 '可通过mt help debug.vgw_vif来查看如何处理这些'
                 '这些错误。',
                 example='mt debug.vgw_server ip=172.16.0.85')
def debug_vgw_server(args):
    assert isinstance(args, dict)

    try:
        host = HostDevice.get((HostDevice.ip == args['ip']) &
                              (HostDevice.type == HOST_TYPE_NSP))
    except Exception:
        print >>sys.stderr, 'Error: NSP Server %s not found' % args['ip']
        return -1
    cmd = 'sshpass -p %s ssh -o ConnectTimeout=9 %s@%s ' % (host.user_passwd,
                                                            host.user_name,
                                                            host.ip)
    try:
        sys_tunnel = SysConfig.get((SysConfig.module_name == "LCPD") &
                                   (SysConfig.param_name == "tunnel_protocol"))
    except Exception:
        print >>sys.stderr, 'Error: Tunnel protocol not found'
        return -1
    cmd += 'sh /usr/local/livegate/script/debug.sh server %s ' % \
        sys_tunnel.value
    try:
        rack = Rack.get(Rack.id == host.rackid)
    except Exception:
        pass
    try:
        rack_tunnels = RackTunnel.select().where(
            RackTunnel.local_uplink_ip == host.uplink_ip)
    except Exception:
        pass
    if rack is not None \
        and rack.switch_type != SWITCH_TYPE_TUNNEL \
            and rack_tunnels is not None:
        for rack_tunnel in rack_tunnels:
            cmd += '%s ' % rack_tunnel.remote_uplink_ip
    rc, output = commands.getstatusoutput(cmd)
    ret = 0
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        ret = -1
#        return -1
    try:
        vgws = VGateway.select().where(
            VGateway.gw_launch_server == host.ip)
    except Exception:
        print "Info: server %s SUCCESS" % host.ip
        return 0
    devicetype = VINTERFACE_DEVICETYPE['VGATEWAY']
    for vgw in vgws:
        try:
            vifs = VInterface.select().where(
                (VInterface.devicetype == devicetype) &
                (VInterface.deviceid == vgw.id) &
                (VInterface.state == VINTERFACE_STATE_ATTACH))
        except Exception:
            pass
        if vifs is not None:
            for vif in vifs:
                print "Info: ====== VGATEWAY %s VINTERFACE %s =====" % \
                    (vif.deviceid, vif.ifindex)
                ret += debug_vgw_vif(
                    {"deviceid": vif.deviceid, "ifindex": vif.ifindex})
                print "============================================\n"
    if ret:
        print >>sys.stderr, 'Error: "server %s failed (%d)" failed' % \
            (host.ip, ret)
        return -1
    print "Info: server %s SUCCESS" % host.ip
    return 0


@debugcmd.define(name='debug.vgw',
                 opt_params='vgw_name, epc_name, user_name',
                 description='检查对应名字<vgw_name>的虚拟网关/带'
                 '宽共享器的接口配置是否正确；'
                 '\n                          '
                 '检查弹性私有云<epc_name>中的所有虚拟网关/带宽共'
                 '享器的接口配置是否正确；'
                 '\n                          '
                 '检查属于用户<user_name>名下的所有虚拟网关/带宽'
                 '共享器的接口配置是否正确。',
                 example='mt debug.vgw vgw_name=router-1-1'
                 '\n                          '
                 'mt debug.vgw epc_name=beijing-autotest'
                 '\n                          '
                 'mt debug.vgw user_name=autotest')
def debug_vgw(args):
    assert isinstance(args, dict)

    if 'vgw_name' not in args \
        and 'epc_name' not in args \
            and 'user_name' not in args:
        print >>sys.stderr, 'Error: no parameter is found'
        return -1

    devicetype = VINTERFACE_DEVICETYPE['VGATEWAY']
    if 'vgw_name' in args:
        try:
            vgws = VGateway.select().where(VGateway.name == args['vgw_name'])
        except Exception:
            print >>sys.stderr, 'Error: vgateway %s not found' % \
                args['vgw_name']
            return -1
        if vgws.count() == 0:
            print >>sys.stderr, 'Error: vgateway %s not found' % \
                args['vgw_name']
            return -1
        for vgw in vgws:
            try:
                vifs = VInterface.select().where(
                    (VInterface.devicetype == devicetype) &
                    (VInterface.deviceid == vgw.id) &
                    (VInterface.state == VINTERFACE_STATE_ATTACH))
            except Exception:
                pass
            if vifs is not None and vifs.count() > 0:
                username = '<NULL>'
                if vgw.userid != 0:
                    username = User.get(User.id == vgw.userid).username
                epc_name = '<NULL>'
                if vgw.epc_id != 0:
                    epc_name = Epc.get(Epc.id == vgw.epc_id).name
                for vif in vifs:
                    print "Info: ===== USER-%s:%s EPC-%s:%s " \
                        "VGW-%s:%s VIF-%s =====" % \
                        (vgw.userid, username, vgw.epc_id, epc_name,
                         vgw.id, vgw.name, vif.ifindex)
                    debug_vgw_vif(
                        {"deviceid": vgw.id, "ifindex": vif.ifindex})
                    print "============================================\n"
    elif 'epc_name' in args:
        try:
            epcs = Epc.select().where(Epc.name == args['epc_name'])
        except Exception:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        if epcs.count() == 0:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        for epc in epcs:
            try:
                vgws = VGateway.select().where(VGateway.epc_id == epc.id)
            except Exception:
                continue
            for vgw in vgws:
                try:
                    vifs = VInterface.select().where(
                        (VInterface.devicetype == devicetype) &
                        (VInterface.deviceid == vgw.id) &
                        (VInterface.state == VINTERFACE_STATE_ATTACH))
                except Exception:
                    pass
                if vifs is not None and vifs.count() > 0:
                    username = '<NULL>'
                    if vgw.userid != 0:
                        username = User.get(User.id == vgw.userid).username
                    for vif in vifs:
                        print "Info: ===== USER-%s:%s EPC-%s:%s " \
                            "VGW-%s:%s VIF-%s =====" % \
                            (vgw.userid, username, epc.id, epc.name,
                             vgw.id, vgw.name, vif.ifindex)
                        debug_vgw_vif(
                            {"deviceid": vgw.id, "ifindex": vif.ifindex})
                        print "============================================\n"
    elif 'user_name' in args:
        try:
            users = User.select().where(User.username == args['user_name'])
        except Exception:
            print >>sys.stderr, 'Error: user %s not found' % \
                args['user_name']
            return -1
        if users.count() == 0:
            print >>sys.stderr, 'Error: user %s not found' % \
                args['user_name']
            return -1
        for user in users:
            try:
                vgws = VGateway.select().where(VGateway.userid == user.id)
            except Exception:
                continue
            for vgw in vgws:
                try:
                    vifs = VInterface.select().where(
                        (VInterface.devicetype == devicetype) &
                        (VInterface.deviceid == vgw.id) &
                        (VInterface.state == VINTERFACE_STATE_ATTACH))
                except Exception:
                    pass
                if vifs is not None and vifs.count() > 0:
                    epc_name = '<NULL>'
                    if vgw.epc_id != 0:
                        epc_name = Epc.get(Epc.id == vgw.epc_id).name
                    for vif in vifs:
                        print "Info: ===== USER-%s:%s EPC-%s:%s " \
                            "VGW-%s:%s VIF-%s =====" % \
                            (user.id, user.username, vgw.epc_id, epc_name,
                             vgw.id, vgw.name, vif.ifindex)
                        debug_vgw_vif(
                            {"deviceid": vgw.id, "ifindex": vif.ifindex})
                        print "============================================\n"
    print "DONE"
    return 0


@debugcmd.define(name='debug.vm_vif',
                 rqd_params='deviceid, ifindex',
                 description='检查KVM/XEN服务器上的vm_id为'
                 '<deviceid>的虚拟机的接口<ifindex>的配置是否和数'
                 '据库一致并且状态正确。'
                 '\n                          '
                 '比如，deviceid=16并且ifindex=0，即对虚拟机eth0'
                 '网卡对应的接口执行检查。如果检查通过，则会输出'
                 '如下信息：'
                 '\n                          '
                 'Info: VM 16 VINTERFACE 0 SUCCESS'
                 '\n                          '
                 '如果检查失败，则会输出Error信息。'
                 '\n                          '
                 '对于配置检查失败的情况，可以通过在WEB界面上重新'
                 '执行一次配置来进行恢复。',
                 example='mt debug.vm_vif deviceid=16 ifindex=0')
def debug_vm_vif(args):
    assert isinstance(args, dict)

    devicetype = VINTERFACE_DEVICETYPE['VM']
    try:
        vif = VInterface.get((VInterface.devicetype == devicetype) &
                             (VInterface.deviceid == args['deviceid']) &
                             (VInterface.ifindex == args['ifindex']))
    except Exception:
        print >>sys.stderr, 'Error: VM %s VINTERFACE %s not found' % \
            (args['deviceid'], args['ifindex'])
        return -1

    try:
        vdevice = VM.get(VM.id == args['deviceid'])
    except Exception:
        print >>sys.stderr, 'Error: VM %s not found' % \
            args['deviceid']
        return -1
    launch_server = vdevice.launch_server
    try:
        host = HostDevice.get(HostDevice.ip == launch_server)
    except Exception:
        print >>sys.stderr, 'Error: Host device %s not found' % launch_server
        return -1
    cmd = 'sshpass -p %s ssh -o ConnectTimeout=9 %s@%s ' % (host.user_passwd,
                                                            host.user_name,
                                                            launch_server)
    if vif.state != VINTERFACE_STATE_ATTACH:
        print >>sys.stderr, 'Error: VM %s VINTERFACE %s not attached' % \
            (args['deviceid'], args['ifindex'])
        return -1
    cmd += 'sh /usr/local/livecloud/pyagexec/script/debug.sh ' + \
        'vm-vport %s %d %s %d --state --mac %s --mtu --vlan %d ' % \
        (vdevice.label, vif.ifindex, vif.mac, vif.id, vif.mac, vif.vlantag)
    if (vdevice.flag & VM_FLAG_ISOLATE) == 0:
        flag_isolate = 'CLOSE'
    else:
        flag_isolate = 'OPEN'
    if vif.iftype == VINTERFACE_TYPE_WAN:
        if vif.bandw != 0:
            vif.bandw = 128 if vif.bandw < 128 \
                else vif.bandw
        if vif.broadc_ceil_bandw != 0:
            vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                else vif.broadc_ceil_bandw
            vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                else vif.broadc_bandw
        cmd += '--qos %d %d 128000 128000 %d %d --policy SRC_IP %s ' % \
            (vif.bandw, vif.bandw,
             vif.broadc_bandw, vif.broadc_ceil_bandw,
             flag_isolate)
        try:
            ips = IP.select().where(IP.vifid == vif.id)
        except Exception:
            print >>sys.stderr, 'Error: VM %s VINTERFACE %d IP not found' % \
                (args['deviceid'], args['ifindex'])
            return -1
        for ip in ips:
            cmd += '%s/%d,' % (ip.ip, ip.netmask)
        cmd += ' NONE'
    elif vif.iftype == VINTERFACE_TYPE_LAN:
        if vif.bandw != 0:
            vif.bandw = 128 if vif.bandw < 128 \
                else vif.bandw
        if vif.broadc_ceil_bandw != 0:
            vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                else vif.broadc_ceil_bandw
            vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                else vif.broadc_bandw
        cmd += '--qos %d %d 128000 128000 %d %d --policy SRC_MAC %s ' % \
            (128 if vif.bandw != 0 else 0, vif.bandw,
             vif.broadc_bandw, vif.broadc_ceil_bandw,
             flag_isolate)
        cmd += 'NONE NONE'
    elif vif.iftype == VINTERFACE_TYPE_CTRL:
        if vif.bandw != 0:
            vif.bandw = 128 if vif.bandw < 128 \
                else vif.bandw
        if vif.broadc_ceil_bandw != 0:
            vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                else vif.broadc_ceil_bandw
            vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                else vif.broadc_bandw
        cmd += '--qos %d %d 128000 128000 %d %d --policy SRC_DST_IP %s ' % \
            (128 if vif.bandw != 0 else 0, vif.bandw,
             vif.broadc_bandw, vif.broadc_ceil_bandw,
             flag_isolate)
        cmd += '%s %s' % (vif.ip, vdevice.launch_server)
    elif vif.iftype == VINTERFACE_TYPE_SERV:
        if vif.bandw != 0:
            vif.bandw = 128 if vif.bandw < 128 \
                else vif.bandw
        if vif.broadc_ceil_bandw != 0:
            vif.broadc_ceil_bandw = 128 if vif.broadc_ceil_bandw < 128 \
                else vif.broadc_ceil_bandw
            vif.broadc_bandw = 128 if vif.broadc_bandw < 128 \
                else vif.broadc_bandw
        cmd += '--qos %d %d 128000 128000 %d %d --policy SRC_DST_IP %s ' % \
            (128 if vif.bandw != 0 else 0, vif.bandw,
             vif.broadc_bandw, vif.broadc_ceil_bandw,
             flag_isolate)
        try:
            serv_ip_min = DomainConfig.get(
                DomainConfig.param_name == "service_provider_ip_min")
        except Exception:
            print >>sys.stderr, 'Error: VM %s Service IP MIN not found' % \
                (args['deviceid'])
            return -1
        try:
            serv_ip_max = DomainConfig.get(
                DomainConfig.param_name == "service_provider_ip_max")
        except Exception:
            print >>sys.stderr, 'Error: VM %s Service IP MAX not found' % \
                (args['deviceid'])
            return -1
        if serv_ip_min.value == '' or serv_ip_max.value == '':
            serv_ip = "255.255.255.255"
        else:
            int_ip_min = ip_to_bin(serv_ip_min.value)
            int_ip_max = ip_to_bin(serv_ip_max.value)
            netmask = ip_range_to_netmask(int_ip_min, int_ip_max)
            if netmask == 0:
                print >>sys.stderr, \
                    'Error: VM %s Service IP prefix not found' % \
                    (args['deviceid'])
                return -1
            masklen = netmask2masklen(bin_to_ip(netmask))
            serv_ip = "%s/%d" % (serv_ip_min.value, masklen)
        cmd += '%s %s' % (vif.ip, serv_ip)
    else:
        print >>sys.stderr, 'Error: VM %s VINTERFACE %s type %d not found' % \
            (args['deviceid'], args['ifindex'], vif.iftype)
        return -1
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        return -1
#    print 'Info: "%s" succeed' % cmd
    print 'Info: VM %s VINTERFACE %s SUCCESS' % \
        (args['deviceid'], args['ifindex'])
    return 0


@debugcmd.define(name='debug.vm_server',
                 rqd_params='ip',
                 description='检查KVM/XEN服务器本身的配置是否正确'
                 '，以及该服务器上所承载的所有虚拟机的所有接口的'
                 '配置是否和数据库一致并且状态正确。'
                 '\n                          '
                 '如果服务器本身检查通过，则继续检查其上的虚拟机'
                 '的接口配置，不会输出提示信息。'
                 '如果检查失败，则会输出Error信息。'
                 '\n                          '
                 '对于配置检查失败的情况，最简单的恢复方法是执行'
                 '/usr/local/livecloud/script/xxxbr_config.sh'
                 '脚本（对于KVM是nspbr_config.sh，对于XEN是'
                 'xenbr_config.sh）来恢复一下服务器的网络配置。不'
                 '过，对于内核版本不一致或者服务状态不正确的情况'
                 '，则需要更新内核或通过systemctl设置服务状态。'
                 '\n                          '
                 '此外，检查所有虚拟机的接口时，如果检查通过，最'
                 '后会输出Info信息，如果检查有错误，则会输出'
                 'Error信息，并会返回一个负值，提示有多少处检查失'
                 '败。'
                 '\n                          '
                 '可通过mt help debug.vm_vif来查看如何处理这些'
                 '这些错误。',
                 example='mt debug.vm_server ip=172.16.0.86')
def debug_vm_server(args):
    assert isinstance(args, dict)

    try:
        host = HostDevice.get((HostDevice.ip == args['ip']) &
                              (HostDevice.type == HOST_TYPE_VM))
    except Exception:
        print >>sys.stderr, 'Error: KVM/XEN Server %s not found' % args['ip']
        return -1
    cmd = 'sshpass -p %s ssh -o ConnectTimeout=9 %s@%s ' % (host.user_passwd,
                                                            host.user_name,
                                                            host.ip)
    cmd += 'sh /usr/local/livecloud/pyagexec/script/debug.sh server'
    rc, output = commands.getstatusoutput(cmd)
    ret = 0
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        ret = -1
#        return -1
    try:
        vms = VM.select().where(
            VM.launch_server == host.ip)
    except Exception:
        print "Info: server %s SUCCESS" % host.ip
        return 0
    devicetype = VINTERFACE_DEVICETYPE['VM']
    for vm in vms:
        try:
            vifs = VInterface.select().where(
                (VInterface.devicetype == devicetype) &
                (VInterface.deviceid == vm.id) &
                (VInterface.state == VINTERFACE_STATE_ATTACH))
        except Exception:
            pass
        if vifs is not None:
            for vif in vifs:
                print "Info: ====== VM %s VINTERFACE %s =====" % \
                    (vif.deviceid, vif.ifindex)
                ret += debug_vm_vif(
                    {"deviceid": vif.deviceid, "ifindex": vif.ifindex})
                print "======================================\n"
    if ret:
        print >>sys.stderr, 'Error: "server %s failed (%d)" failed' % \
            (host.ip, ret)
        return -1
    print "Info: server %s SUCCESS" % host.ip
    return 0


@debugcmd.define(name='debug.vm',
                 opt_params='vm_name, epc_name, user_name',
                 description='检查对应名字<vm_name>的虚拟机的接口'
                 '配置是否正确；'
                 '\n                          '
                 '检查弹性私有云<epc_name>中的所有虚拟机的接口配'
                 '置是否正确；'
                 '\n                          '
                 '检查属于用户<user_name>名下的所有虚拟机的接口配'
                 '置是否正确。',
                 example='mt debug.vm vm_name=vm-1-1'
                 '\n                          '
                 'mt debug.vm epc_name=beijing-autotest'
                 '\n                          '
                 'mt debug.vm user_name=autotest')
def debug_vm(args):
    assert isinstance(args, dict)

    if 'vm_name' not in args \
        and 'epc_name' not in args \
            and 'user_name' not in args:
        print >>sys.stderr, 'Error: no parameter is found'
        return -1

    devicetype = VINTERFACE_DEVICETYPE['VM']
    if 'vm_name' in args:
        try:
            vms = VM.select().where(VM.name == args['vm_name'])
        except Exception:
            print >>sys.stderr, 'Error: vm %s not found' % \
                args['vm_name']
            return -1
        if vms.count() == 0:
            print >>sys.stderr, 'Error: vm %s not found' % \
                args['vm_name']
            return -1
        for vm in vms:
            try:
                vifs = VInterface.select().where(
                    (VInterface.devicetype == devicetype) &
                    (VInterface.deviceid == vm.id) &
                    (VInterface.state == VINTERFACE_STATE_ATTACH))
            except Exception:
                pass
            if vifs is not None and vifs.count() > 0:
                username = '<NULL>'
                if vm.userid != 0:
                    username = User.get(User.id == vm.userid).username
                epc_name = '<NULL>'
                if vm.epc_id != 0:
                    epc_name = Epc.get(Epc.id == vm.epc_id).name
                for vif in vifs:
                    print "Info: ===== USER-%s:%s EPC-%s:%s " \
                        "VM-%s:%s VIF-%s =====" % \
                        (vm.userid, username, vm.epc_id, epc_name,
                         vm.id, vm.name, vif.ifindex)
                    debug_vm_vif(
                        {"deviceid": vm.id, "ifindex": vif.ifindex})
                    print "============================================\n"
    elif 'epc_name' in args:
        try:
            epcs = Epc.select().where(Epc.name == args['epc_name'])
        except Exception:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        if epcs.count() == 0:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        for epc in epcs:
            try:
                vms = VM.select().where(VM.epc_id == epc.id)
            except Exception:
                continue
            for vm in vms:
                try:
                    vifs = VInterface.select().where(
                        (VInterface.devicetype == devicetype) &
                        (VInterface.deviceid == vm.id) &
                        (VInterface.state == VINTERFACE_STATE_ATTACH))
                except Exception:
                    pass
                if vifs is not None and vifs.count() > 0:
                    username = '<NULL>'
                    if vm.userid != 0:
                        username = User.get(User.id == vm.userid).username
                    for vif in vifs:
                        print "Info: ===== USER-%s:%s EPC-%s:%s " \
                            "VM-%s:%s VIF-%s =====" % \
                            (vm.userid, username, epc.id, epc.name,
                             vm.id, vm.name, vif.ifindex)
                        debug_vm_vif(
                            {"deviceid": vm.id, "ifindex": vif.ifindex})
                        print "============================================\n"
    elif 'user_name' in args:
        try:
            users = User.select().where(User.username == args['user_name'])
        except Exception:
            print >>sys.stderr, 'Error: user %s not found' % \
                args['user_name']
            return -1
        if users.count() == 0:
            print >>sys.stderr, 'Error: user %s not found' % \
                args['user_name']
            return -1
        for user in users:
            try:
                vms = VM.select().where(VM.userid == user.id)
            except Exception:
                continue
            for vm in vms:
                try:
                    vifs = VInterface.select().where(
                        (VInterface.devicetype == devicetype) &
                        (VInterface.deviceid == vm.id) &
                        (VInterface.state == VINTERFACE_STATE_ATTACH))
                except Exception:
                    pass
                if vifs is not None and vifs.count() > 0:
                    epc_name = '<NULL>'
                    if vm.epc_id != 0:
                        epc_name = Epc.get(Epc.id == vm.epc_id).name
                    for vif in vifs:
                        print "Info: ===== USER-%s:%s EPC-%s:%s " \
                            "VM-%s:%s VIF-%s =====" % \
                            (user.id, user.username, vm.epc_id, epc_name,
                             vm.id, vm.name, vif.ifindex)
                        debug_vm_vif(
                            {"deviceid": vm.id, "ifindex": vif.ifindex})
                        print "============================================\n"
    print "DONE"
    return 0


@debugcmd.define(name='debug.vgw_dnat',
                 rqd_params='vgw_id',
                 opt_params='pretty_print',
                 description='测试虚拟网关DNAT服务连接是否正常，'
                             'pretty_print默认为true，可指定为false',
                 example='mt debug.vgw_dnat vgw_id=256 pretty_print=false')
def debug_vgw_nat(args):
    assert isinstance(args, dict)

    if 'vgw_id' not in args:
        print >>sys.stderr, 'Error: no parameter is found'
        return -1

    try:
        VGateway.get(VGateway.id == args['vgw_id'])
    except Exception:
        print >>sys.stderr, 'Error: VGATEWAY %s not found' % \
            args['vgw_id']
        return -1
    cmd = '/usr/local/livecloud/script/debug/dnat_test/dnat_test.sh %s %s' % \
        (args['vgw_id'], args.get('pretty_print', 'true'))
    rc, output = commands.getstatusoutput(cmd)
    print output
    return 0


@debugcmd.define(name='debug.logic-topo',
                 opt_params='epc_id, epc_name, user_id, user_name',
                 description='打印指定弹性私有云或指定用户的网络逻辑拓扑信息',
                 example='mt debug.logic-topo epc_name=beijing-autotest'
                 '\n                          '
                 'mt debug.logic-topo user_name=autotest')
def debug_logic_topo(args):
    assert isinstance(args, dict)

    if 'epc_id' not in args \
        and 'epc_name' not in args \
            and 'user_id' not in args \
                and 'user_name' not in args:
        print >>sys.stderr, 'Error: no parameter is found'
        return -1

    if 'epc_id' in args:
        try:
            vl2s = VL2.select().where(
                (VL2.epc_id == args['epc_id']) |
                (VL2.net_type != VINTERFACE_TYPE_LAN))
        except Exception:
            vl2s = None
            pass
        if vl2s is None or vl2s.count() == 0:
            print >>sys.stderr, 'Error: no vl2 is found in epc#%s' % \
                args['epc_id']
            return -1
        epc = Epc.get(Epc.id == args['epc_id'])
        print '>>> EPC#%s %s:' % (epc.id, epc.name)
        for vl2 in vl2s:
            try:
                vifs = VInterface.select().where(
                    VInterface.subnetid == vl2.id).order_by(
                    VInterface.devicetype)
            except Exception:
                vifs = None
                pass
            if vifs is None or vifs.count() == 0:
                continue
            vl2_nets = ''
            if vl2.net_type == VINTERFACE_TYPE_LAN:
                vl2nets = Vl2Net.select().where(
                    Vl2Net.vl2id == vl2.id).order_by(
                    Vl2Net.net_index)
                for vl2net in vl2nets:
                    vl2_nets += ' %s/%s' % (
                        vl2net.prefix, netmask2masklen(vl2net.netmask))
            else:
                vl2_nets = ' ISP#%s' % vl2.isp
            print '=== VL2#%-3s %s%s ===' % (vl2.id, vl2.name, vl2_nets)
            for vif in vifs:
                if vif.devicetype == VINTERFACE_DEVICETYPE['VM']:
                    try:
                        vdevice = VM.get(
                            (VM.id == vif.deviceid) &
                            (VM.epc_id == args['epc_id']))
                    except Exception:
                        continue
                    vdevice_launch_server = vdevice.launch_server
                    vdevice_type = 'VM#'
                    host = HostDevice.get(
                        HostDevice.ip == vdevice_launch_server)
                    rack = Rack.get(Rack.id == host.rackid)
                elif vif.devicetype == VINTERFACE_DEVICETYPE['TPD']:
                    try:
                        vdevice = ThirdPartyDevice.get(
                            (ThirdPartyDevice.id == vif.deviceid) &
                            (ThirdPartyDevice.epc_id == args['epc_id']))
                    except Exception:
                        continue
                    vdevice_launch_server = vdevice.mgmt_ip
                    vdevice_type = 'TPD'
                    rack = Rack.get(Rack.name == vdevice.rack_name)
                elif vif.devicetype == VINTERFACE_DEVICETYPE['VGATEWAY']:
                    try:
                        vdevice = VGateway.get(
                            (VGateway.id == vif.deviceid) &
                            (VGateway.epc_id == args['epc_id']))
                    except Exception:
                        continue
                    vdevice_launch_server = vdevice.gw_launch_server
                    vdevice_type = 'VGW'
                    host = HostDevice.get(
                        HostDevice.ip == vdevice_launch_server)
                    rack = Rack.get(Rack.id == host.rackid)
                else:
                    continue
                net_devs = ''
                try:
                    netdevs = NetworkDevice.select().where(
                        NetworkDevice.rackid == rack.id)
                except Exception:
                    pass
                if netdevs is not None and netdevs.count() > 0:
                    for netdev in netdevs:
                        net_devs += '-%s' % netdev.mip
                vif_ips = []
                if vif.iftype == VINTERFACE_TYPE_LAN:
                    vifips = VInterfaceIP.select().where(
                        VInterfaceIP.vifid == vif.id).order_by(
                        VInterfaceIP.net_index)
                    for vifip in vifips:
                        vif_ips.append(vifip.ip)
                elif vif.iftype == VINTERFACE_TYPE_WAN:
                    iprs = IP.select().where(IP.vifid == vif.id)
                    for ipr in iprs:
                        vif_ips.append(ipr.ip)
                else:
                    vif_ips.append(vif.ip)
                for vif_ip in vif_ips:
                    print '    %-15s %3s#%-4s %-16s IF-%-2s HOST-%-15s' \
                          ' RACK#%-2s %-8s ToR%s' % (
                              vif_ip, vdevice_type, vdevice.id, vdevice.name,
                              vif.ifindex, vdevice_launch_server,
                              rack.id, rack.name, net_devs)
        print ''
        return 0
    elif 'epc_name' in args:
        try:
            epcs = Epc.select().where(Epc.name == args['epc_name'])
        except Exception:
            epcs = None
            pass
        if epcs is None or epcs.count() == 0:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        for epc in epcs:
            debug_logic_topo({'epc_id': epc.id})
        return 0
    elif 'user_id' in args:
        try:
            epcs = Epc.select().where(Epc.userid == args['user_id'])
        except Exception:
            epcs = None
            pass
        if epcs is None or epcs.count() == 0:
            print >>sys.stderr, 'Error: no epc is found for user#%s' % \
                args['user_id']
            return -1
        for epc in epcs:
            debug_logic_topo({'epc_id': epc.id})
        return 0
    elif 'user_name' in args:
        try:
            users = User.select().where(User.username == args['user_name'])
        except Exception:
            users = None
            pass
        if users is None or users.count() == 0:
            print >>sys.stderr, 'Error: user %s not found' % \
                args['user_name']
            return -1
        for user in users:
            debug_logic_topo({'user_id': user.id})
        return 0

    return 1


@debugcmd.define(name='debug.epc',
                 opt_params='epc_id, epc_name, pretty_print',
                 description='效果等同于调用mt debug.logic-topo, mt debug.vm, '
                             'mt debug.vgw, mt debug.vgw_dnat',
                 example='mt debug.epc epc_id=2')
def debug_epc(args):
    assert isinstance(args, dict)

    if 'epc_id' not in args and 'epc_name' not in args:
        print >>sys.stderr, 'Error: no parameter is found'
        return -1

    if 'epc_name' in args:
        try:
            epcs = Epc.select().where(Epc.name == args['epc_name'])
        except Exception:
            epcs = None
        if epcs is None or epcs.count() == 0:
            print >>sys.stderr, 'Error: epc %s not found' % \
                args['epc_name']
            return -1
        for epc in epcs:
            debug_epc({'epc_id': epc.id})
        return 0

    debug_logic_topo({'epc_id': args['epc_id']})

    epc = None
    try:
        epc = Epc.get(Epc.id == args['epc_id'])
    except Exception as e:
        print >>sys.stderr, 'Error: epc %s not found' % args['epc_id']
        print >>sys.stderr, str(e)
    if not epc:
        return -1

    print 'VM:\n'
    debug_vm({'epc_name': epc.name})

    print '\nVGateway:\n'
    debug_vgw({'epc_name': epc.name})

    try:
        vgateways = VGateway.select().where(VGateway.epc_id == args['epc_id'])
    except Exception as e:
        print >>sys.stderr, 'Error: failed to find vgw in epc %s' % epc.name
        print >>sys.stderr, str(e)
        return -1
    for vgw in vgateways:
        print '\nNAT policy of vgateway %s:\n' % vgw.name
        debug_vgw_nat({
            'vgw_id': vgw.id,
            'pretty_print': args.get('pretty_print', 'true')})

    print "\nALL DONE!"
    return 0


@debugcmd.define(name='debug.help',
                 description='输出debug相关的帮助信息',
                 example='mt debug.help')
def debug_help(args):
    print "以下为vgateway相关debug帮助信息：\n" \
          " 1. mt debug.vgw_server ip=x.x.x.x\n" \
          "    描述：检查IP为x.x.x.x的NSP主机以及其上的虚拟网关/带宽共享器的" \
          "网络相关配置是否正确并与数据库一致。\n" \
          " 2. mt debug.vgw vgw_name=xxx\n" \
          "    描述：检查名字为xxx的虚拟网关的网络相关配置是否正确并与数据库" \
          "一致。\n" \
          " 3. mt debug.vgw epc_name=xxx\n" \
          "    描述：检查名字为xxx的弹性私有云中的所有虚拟网关的网络相关配置" \
          "是否正确并与数据库一致。\n" \
          " 4. mt debug.vgw user_name=xxx\n" \
          "    描述：检查名字为xxx的用户名下的所有虚拟网关的网络相关配置是否" \
          "正确并与数据库一致。\n" \
          " 5. mt debug.vgw_vif deviceid=xxx ifindex=yyy\n" \
          "    描述：检查ID为xxx的虚拟网关的接口yyy的网络相关配置是否正确并"  \
          "与数据库一致。\n" \
          " 6. mt debug.vgw_dnat vgw_id=xxx\n" \
          "    描述：检查ID为xxx的虚拟网关所配置的所有DNAT服务是否能够正常"  \
          "访问。\n" \
          "\n"\
          "以下为vm相关debug帮助信息：\n" \
          " 1. mt debug.vm_server ip=x.x.x.x\n" \
          "    描述：检查IP为x.x.x.x的KVM/XEN主机以及其上的虚拟机的网络相关"  \
          "配置是否正确并与数据库一致。\n" \
          " 2. mt debug.vm vm_name=xxx\n" \
          "    描述：检查名字为xxx的虚拟机的网络相关配置是否正确并与数据库一" \
          "致。\n" \
          " 3. mt debug.vm epc_name=xxx\n" \
          "    描述：检查名字为xxx的弹性私有云中的所有虚拟机的网络相关配置是" \
          "否正确并与数据库一致。\n" \
          " 4. mt debug.vm user_name=xxx\n" \
          "    描述：检查名字为xxx的用户名下的所有虚拟机的网络相关配置是否正" \
          "确并与数据库一致。\n" \
          " 5. mt debug.vm_vif deviceid=xxx ifindex=yyy\n" \
          "    描述：检查ID为xxx的虚拟机的接口yyy的网络相关配置是否正确并与"  \
          "数据库一致。\n" \
          "\n"\
          "以下为epc相关debug帮助信息：\n" \
          " 1. mt debug.logic-topo epc_id=xxx\n" \
          "    mt debug.logic-topo epc_name=xxx\n" \
          "    描述：打印ID或名字为xxx的弹性私有云中的所有虚拟网关/第三方硬" \
          "件设备/虚拟机的接口IP信息，按子网分组显示。\n"\
          " 2. mt debug.epc epc_id=xxx\n" \
          "    mt debug.epc epc_name=xxx\n" \
          "    描述：效果等同于调用mt debug.logic-topo, mt debug.vm, " \
          "mt debug.vgw, mt debug.vgw_dnat\n" \
          "\n"\
          "以下为switch相关的debug帮助信息：\n" \
          "mt debug.switch mip=xxx.xxx.xxx.xxx\n" \
          "   描述：获取交换机的调试信息，并检查交换机部分配置的正确性。" \
          "具体包括：\n"\
          "1. 输出交换机当前的mac表\n"\
          "2. 输出交换机各个接口的收发包速率\n"\
          "3. 检查第三方硬件所接的交换机接口配置正确性\n"\
          "       如果正确，则输出SUCCESS和交换机的brand,mip和port\n"\
          "       如果有错，则除输出FAILED和交换机信息外，还输出当前配置和期望配置\n"\
          "4. 检查交换机上VLAN与VNI的映射是否与数据库中VLAN与VL2ID的映射相一致，"\
          "检查所建立的隧道是否与数据库中的记录相一致"


@debugcmd.define(
    name='debug.switch',
    rqd_params='mip',
    description='获取交换机的调试信息，并检查交换机部分配置的正确性。'
    '具体包括：\n'
    '1. 输出交换机当前的mac表\n'
    '2. 输出交换机各个接口的收发包速率\n'
    '3. 检查第三方硬件所接的交换机接口配置正确性\n'
    '       如果正确，则输出SUCCESS和交换机的brand,mip和port\n'
    '       如果有错，则除输出FAILED和交换机信息外，还输出当前配置和期望配置\n'
    '4. 检查交换机上VLAN与VNI的映射是否与数据库中VLAN与VL2ID的映射相一致，'
    '检查所建立的隧道是否与数据库中的记录相一致',
    example='mt debug.switch mip=172.17.1.111')
def debug_switch(args):
    assert isinstance(args, dict)

    # Get mac address-table
    try:
        switch_info = NetworkDevice.get(NetworkDevice.mip == args['mip'])
    except Exception as e:
        print >>sys.stderr, "Failed to get switch_info: mip=%s" % args['mip']
        print >>sys.stderr, "%s" % str(e)
        return -1
    cmd = SCRIPT_SWITCH_CONF
    cmd += "show %s %s %s %s '%s' PLAIN MAC" % (
        switch_info.brand, switch_info.mip, switch_info.username,
        switch_info.password, switch_info.enable)
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        return rc
    print json.loads(output)['OUTPUT']

    # Get port rates
    print "RATES"
    print "------------------------------------------------------------------"
    cmd = SCRIPT_SWITCH_CONF
    cmd += "show %s %s %s %s '%s' PLAIN RATES" % (
        switch_info.brand, switch_info.mip, switch_info.username,
        switch_info.password, switch_info.enable)
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >>sys.stderr, 'Error: "%s" failed' % cmd
        print >>sys.stderr, 'Error:\n%s' % output
        return rc
    print json.loads(output)['OUTPUT']

    # Check port conf
    url_base = SDNCTRL_URL + 'switch/%s/' % args['mip']
    devicetype = VINTERFACE_DEVICETYPE['TPD']
    vifs = []
    try:
        vifs = VInterface.select().where(
            (VInterface.devicetype == devicetype) &
            (VInterface.state == VINTERFACE_STATE_ATTACH) &
            (VInterface.netdevice_lcuuid == switch_info.lcuuid))
    except Exception:
        print >>sys.stderr, 'Error: Getting vifs exception'
    for vif in vifs:
        url = url_base + "port/%d/status/" % vif.ofport
        try:
            resp = requests.get(url)
        except requests.exceptions.ConnectionError:
            print >>sys.stderr, 'Error: unable to connect to sdn controller'
            return -1
        if resp.status_code != 200:
            print >>sys.stderr,\
                'Port Check Error: switch %s port %d. "\
                "unable to get the status.\n%s\n' % (
                    args['mip'], vif.ofport, json.dumps(resp.json(), indent=2))
            return -1
        if resp.json()['OPT_STATUS'] == 'SUCCESS':
            print "Port Check SUCCESS: brand=%s, mip=%s, port=%d." % (
                switch_info.brand, args['mip'], vif.ofport)
        else:
            print "Port Check FAILED: brand=%s, mip=%s, port=%d.\n%s\n" % (
                switch_info.brand, args['mip'], vif.ofport,
                json.dumps(resp.json(), indent=2))

    # Check subnets conf
    if switch_info.brand.lower() != "arista":
        return 0
    url = SDNCTRL_URL + 'switch/%s/subnets/status/' % args['mip']
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connect to sdn controller'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr,\
            'Subnets Check Error: switch %s. "\
            "unable to get the status.\n%s\n' % (
                args['mip'], json.dumps(resp.json(), indent=2))
        return -1
    if resp.json()['OPT_STATUS'] == 'SUCCESS':
        print "Subnets Check SUCCESS: brand=%s, mip=%s." % (
            switch_info.brand, args['mip'])
    else:
        print "Subnets Check FAILED: brand=%s, mip=%s.\n%s\n" % (
            switch_info.brand, args['mip'], json.dumps(resp.json(), indent=2))
    return 0
