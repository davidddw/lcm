# -*- coding: utf-8 -*-
import sys
import commands
import simplejson as json
import mtcmd
from const import HOST_TYPE_VM, HOST_TYPE_NSP, SCRIPT_SWITCH_CONF, \
    VINTERFACE_TYPE_WAN, VIF_DEVICE_TYPE_VGATEWAY, VIF_DEVICE_TYPE_VM, \
    SSH_OPTION_LIST, SSH_OPTION_STRING
from data import HostDevice, NetworkDevice, Rack, VGateway, VInterface, VM, \
    IpResource, VInterfaceIP
from utils import call_system_streaming

tracecmd = mtcmd.Command()
kvm_trace_script = '/usr/local/livecloud/pyagexec/script/trace.sh'
nsp_trace_script = '/usr/local/livegate/script/trace.sh'
kvm_probe_script = '/usr/local/livecloud/pyagexec/script/net_ghost.py'
nsp_probe_script = '/usr/local/livegate/script/net_ghost.py'


def display_long_string(s, prefix=1, suffix=1):
    # 字符串过长时只显示前prefix和后suffix个字符
    if len(s) > prefix + suffix + 2:
        if suffix:
            return s[:prefix] + '..' + s[-suffix:]
        else:
            return s[:prefix] + '..'
    return s


@tracecmd.define(name='trace.l2',
                 rqd_params='first_hop, dest_mac',
                 description='Trace dest_mac in a L2 Network, from first_hop. '
                             'first_hop must be an IP address of switch/host.',
                 example='mt trace.l2 first_hop=172.16.0.10 '
                         'dest_mac=01:23:45:67:89:ab')
def trace_l2(args):
    assert isinstance(args, dict)
    h_fmt = '%-5s %-9s %-4s %-10s %-15s %-10s %-10s %-11s %-16s %-14s'
    l_fmt = '%-5s %02d-%-6s %-4s %-10s %-15s %-10s %-10s %-11s %-16s %-14s'

    def get_physical_topo():
        matrix = {}

        # get switch and lldp info
        try:
            switchs = NetworkDevice.select()
        except Exception:
            print >> sys.stderr, 'Error: get network_device from db failed'
            return {}
        for switch in switchs:
            # get hostname
            cmd = '%s %s %s %s %s %s "%s" JSON HOSTNAME' \
                % (SCRIPT_SWITCH_CONF, 'show', switch.brand, switch.mip,
                   switch.username, switch.password, switch.enable)
            rc, output = commands.getstatusoutput(cmd)
            if rc:
                print >> sys.stderr, \
                    'Error: cmd [%s] failed: rc=%d out=%s' % \
                    (cmd.replace(switch.password, '*').replace(
                        '"%s"' % switch.enable, '*'), rc, output)
                continue
            hostname = json.loads(output).get('OUTPUT')
            if not hostname:
                continue

            # get lldp neighbour
            cmd = '%s %s %s %s %s %s "%s" JSON LLDP' \
                % (SCRIPT_SWITCH_CONF, 'show', switch.brand, switch.mip,
                   switch.username, switch.password, switch.enable)
            rc, output = commands.getstatusoutput(cmd)
            if rc:
                print >> sys.stderr, \
                    'Error: cmd [%s] failed: rc=%d out=%s' % \
                    (cmd.replace(switch.password, '*').replace(
                        '"%s"' % switch.enable, '*'), rc, output)
                continue
            edges = []
            if output:
                # OUTPUT:
                #    [{
                #        'LOCAL_PORT_NAME': 'Ethernet21',
                #        'REMOTE_PORT_NAME': 'eth22',
                #        'REMOTE_NODE': 'KVM-81'
                #    }]
                edges = json.loads(output).get('OUTPUT', [])

            switch.name = hostname
            matrix[switch.mip] = ('SWITCH', switch, edges)

        # get host info
        try:
            hosts = HostDevice.select()
        except Exception:
            print >> sys.stderr, 'Error: get host_device from db failed'
            return {}
        for host in hosts:
            matrix[host.name] = ('HOST', host)
            matrix[host.ip] = ('HOST', host)

        # get rack info
        try:
            racks = Rack.select()
        except Exception:
            print >> sys.stderr, 'Error: get rack from db failed'
            return {}
        for rack in racks:
            matrix[rack.id] = ('RACK', rack)

        return matrix  # {switch.mip/host.name/host.ip/rack.id: info}

    def trace_in_switch(switch, dest_mac):
        trace_result = []

        # show mac address-table address mac
        cmd = '%s %s %s %s %s %s "%s" JSON MAC %s' \
            % (SCRIPT_SWITCH_CONF, 'show', switch.brand, switch.mip,
               switch.username, switch.password, switch.enable, dest_mac)
        rc, output = commands.getstatusoutput(cmd)
        if rc:
            print >> sys.stderr, \
                'Error: cmd [%s] failed: rc=%d out=%s' % \
                (cmd.replace(switch.password, '*').replace(
                    '"%s"' % switch.enable, '*'), rc, output)

        entries = []
        if output:
            entries = json.loads(output).get('OUTPUT', [])
        for e in entries:
            if e['IS_PORT_CHANNEL']:
                # Port-Channel在本交换机上的成员端口
                cmd = '%s %s %s %s %s %s "%s" JSON PO_MEMBERS %s' \
                    % (SCRIPT_SWITCH_CONF, 'show',
                       switch.brand, switch.mip,
                       switch.username, switch.password, switch.enable,
                       e['PORT_NAME'])
                rc, output = commands.getstatusoutput(cmd)
                if rc or not output:
                    print >> sys.stderr, \
                        'Error: cmd [%s] failed: rc=%d out=%s' % \
                        (cmd.replace(switch.password, '*').replace(
                            '"%s"' % switch.enable, '*'), rc, output)
                members = json.loads(output).get('OUTPUT', [])
                for m in members:
                    trace_result.append([e['PORT_NAME'], m, '-'])
            else:
                trace_result.append([e['PORT_NAME'], e['PORT_NAME'], '-'])
        for tr in trace_result:
            # 每个swithport的出流量
            out_iface = tr[1]
            cmd = '%s %s %s %s %s %s "%s" JSON ETH_RATES %s' \
                % (SCRIPT_SWITCH_CONF, 'show',
                   switch.brand, switch.mip,
                   switch.username, switch.password, switch.enable,
                   out_iface)
            rc, output = commands.getstatusoutput(cmd)
            if rc or not output:
                print >> sys.stderr, \
                    'Error: cmd [%s] failed: rc=%d out=%s' % \
                    (cmd.replace(switch.password, '*').replace(
                        '"%s"' % switch.enable, '*'), rc, output)
            traffic = json.loads(output).get('OUTPUT', [])
            for t in traffic:
                if t['PORT_NAME'] == out_iface:
                    tr[-1] = ('%.1lfM' % (int(t['TX_BPS']) / (1 << 20)))

        return trace_result

    def trace_in_host(host, dest_mac):
        trace_result = []

        # trace_l2.sh
        if host.type == HOST_TYPE_VM:
            script = kvm_trace_script
        elif host.type == HOST_TYPE_NSP:
            script = nsp_trace_script
        else:
            print >> sys.stderr, "Error: unknown host type, %r" % host
            return trace_result

        cmd = 'sshpass -p %s ssh %s %s@%s sh %s %s' \
            % (host.user_passwd, SSH_OPTION_STRING, host.user_name,
               host.ip, script, dest_mac)
        rc, output = commands.getstatusoutput(cmd)
        # output:
        #    vm-64... nspbr1 DATA vnet31 PHYSICAL bond-eth25-eth27 eth25 0.0M/0
        #    ...
        if rc:
            print >> sys.stderr, \
                'Error: cmd [%s] failed, rc=%d out=%s' % \
                (cmd.replace(host.user_passwd, '*'), rc, output)
            return trace_result

        for line in output.split('\n'):
            if not line:
                continue
            dom_name, br_name, br_type, in_iface, \
                out_iface_type, out_port, out_iface, out_rate = line.split(' ')
            trace_result.append(
                (dom_name, br_name, br_type, in_iface,
                 out_iface_type, out_port, out_iface, out_rate))
        return trace_result

    def get_switch_neighbour(matrix, node_name, iface_name):
        for key, hop in matrix.items():
            if hop[0] != 'SWITCH':
                continue
            for edge in hop[2]:
                if edge['LOCAL_PORT_NAME'] == iface_name:
                    return (matrix.get(edge['REMOTE_NODE']),
                            edge['REMOTE_PORT_NAME'])

        return (None, '')

    def get_host_neighbour(matrix, node_name, iface_name):
        for key, hop in matrix.items():
            if hop[0] != 'SWITCH':
                continue
            for edge in hop[2]:
                if edge['REMOTE_NODE'] == node_name and \
                        edge['REMOTE_PORT_NAME'] == iface_name:
                    return (hop, edge['LOCAL_PORT_NAME'])

        return (None, '')

    def dfs(matrix, dest_mac, depth, path, curr_hop, in_port):
        if not curr_hop:
            return

        indent = (' ' * depth) + str(depth + 1)
        depth += 1

        if curr_hop[0] == 'SWITCH':
            switch = curr_hop[1]
            switch_type = 'ToR'
            if switch.rackid == 0:
                switch_type = 'Agg'
            rack_name = '%d' % switch.rackid
            if switch.rackid in matrix:
                rack_name = matrix[switch.rackid][1].name

            trace_result = trace_in_switch(switch, dest_mac)
            for tr in trace_result:
                port_name, iface_name, out_rate = tr
                print l_fmt % (
                    indent, switch.rackid,
                    display_long_string(rack_name, 1, 3), switch_type,
                    display_long_string(switch.name, 3, 5), switch.mip,
                    display_long_string(in_port, 3, 5),
                    display_long_string(iface_name, 3, 5), out_rate,
                    display_long_string(port_name, 3, 11)
                    if iface_name != port_name else '',  # 仅当iface!=port时显示
                    '')
                if iface_name == 'Vxlan1':
                    for hop in matrix.values():
                        if hop[0] != 'SWITCH' or hop[1].mip in path or \
                                hop[1].rackid == switch.rackid:
                            continue
                        dfs(matrix, dest_mac, depth, path + [hop[1].mip],
                            hop, 'Vxlan1')
                else:
                    next_hop, next_in_port = get_switch_neighbour(
                        matrix, switch.name, iface_name)
                    if next_hop:
                        next_ip = ''
                        if next_hop[0] == 'SWITCH':
                            next_ip = next_hop[1].mip
                        elif next_hop[0] == 'HOST':
                            next_ip = next_hop[1].ip
                        dfs(matrix, dest_mac, depth, path + [next_ip],
                            next_hop, next_in_port)
                    else:
                        # nexthop is a spine switch and has no API support
                        for hop in matrix.values():
                            if hop[0] != 'SWITCH' or hop[1].mip in path or \
                                    hop[1].rackid == switch.rackid:
                                continue
                            dfs(matrix, dest_mac, depth, path + [hop[1].mip],
                                hop, 'Spine?')

        elif curr_hop[0] == 'HOST':
            host = curr_hop[1]
            host_type = 'Unknown'
            if host.type == HOST_TYPE_VM:
                host_type = 'KVM'
            elif host.type == HOST_TYPE_NSP:
                host_type = 'NSP'
            rack_name = '%d' % host.rackid
            if host.rackid in matrix:
                rack_name = matrix[host.rackid][1].name

            trace_result = trace_in_host(host, dest_mac)
            for tr in trace_result:
                dom_name, br_name, br_type, in_iface, \
                    out_iface_type, out_port, out_iface, out_rate = tr
                if host_type == 'NSP':
                    # 通过id查询vgw/bwt的name
                    try:
                        vgw_id = 0
                        if in_iface[:3] != 'eth' and in_iface[0] != '?':
                            vgw_id = in_iface[:in_iface.index('-')]
                        elif out_iface[:3] != 'eth' and out_iface[0] != '?':
                            vgw_id = out_iface[:out_iface.index('-')]
                        if vgw_id:
                            vgw = VGateway.get(VGateway.id == vgw_id)
                            dom_name = vgw.name
                        else:
                            dom_name = '?'
                    except Exception:
                        print >> sys.stderr, \
                            'Error: get vgw %d from db failed', vgw_id
                print l_fmt % (
                    indent, host.rackid, display_long_string(rack_name, 1, 3),
                    host_type, display_long_string(host.name, 3, 5), host.ip,

                    # 当host上无法得知in_iface时，
                    # 使用dfs过程中在上一跳得知的lldp neighbour作为本跳的in_port
                    ('%s?' % display_long_string(in_port, 3, 5))
                    if in_iface == '?'
                    else display_long_string(in_iface, 3, 5),

                    display_long_string(out_iface, 3, 5), out_rate,
                    display_long_string(out_port, 3, 11)
                    if out_iface != out_port else '',  # 仅当iface!=port时显示
                    display_long_string(dom_name, 12, 0))

                if out_iface_type == 'PHYSICAL':
                    next_hop, next_in_port = get_host_neighbour(
                        matrix, host.name, out_iface)
                    if next_hop:
                        dfs(matrix, dest_mac, depth, path + [next_hop[1].mip],
                            next_hop, next_in_port)

        else:
            print '%-5s UNKNOWN HOP: %r' % (indent, curr_hop)

    print h_fmt % (
        'Hop#', 'Rack', 'Type', 'HopName', 'HopIP', 'InIF',
        'OutIF', 'TxBps/Err', 'OutPort', 'vDevice')
    matrix = get_physical_topo()
    curr_hop = matrix.get(args['first_hop'])
    dfs(matrix, args['dest_mac'], 0, [curr_hop[1].ip], curr_hop, '?')


@tracecmd.define(name='tcpdump.vgateway',
                 rqd_params='id, if_index, timeout, count, filter',
                 description='dump packets on vgateway\'s interface',
                 example='mt tcpdump.vgateway id=10 if_index=0 '
                         'timeout=10 count=100 filter="icmp"')
def tcpdump_vgateway(args):
    try:
        vgw = VGateway.get(VGateway.id == args['id'])
    except Exception:
        print >> sys.stderr, 'Error: get vgw from db failed'
        return

    try:
        host = HostDevice.get(HostDevice.ip == vgw.gw_launch_server)
    except Exception:
        print >> sys.stderr, \
            'Error: get gw_launch_server %s from db failed' % \
            vgw.gw_launch_server
        return

    try:
        vif = VInterface.get(
            VInterface.devicetype == VIF_DEVICE_TYPE_VGATEWAY,
            VInterface.deviceid == args['id'],
            VInterface.ifindex == args['if_index'])
    except Exception:
        print >> sys.stderr, 'Error: get vif if_index=%s from db failed' % \
            args['if_index']
        return

    if vif.iftype == VINTERFACE_TYPE_WAN:
        if_name = '%d-w-%d' % (vgw.id, vif.ifindex)
    else:
        if_name = '%d-l-%d' % (vgw.id, vif.ifindex)

    tcpdump_cmds = [
        'tcpdump', '-i', if_name,
        '-l', '-nn', '-e', '-v',
        '-c', args['count'], args['filter']]
    print ' '.join(tcpdump_cmds)
    for line in call_system_streaming(
            ['/usr/bin/timeout', args['timeout'],
             '/usr/bin/sshpass', '-p', host.user_passwd, 'ssh',
             ] + SSH_OPTION_LIST +
            ['%s@%s' % (host.user_name, host.ip)] + tcpdump_cmds,
            ignore_err=False):
        print line,  # do not print duplicate newline


@tracecmd.define(name='tcpdump.vm',
                 rqd_params='id, if_index, timeout, count, filter',
                 description='dump packets on vm\'s interface',
                 example='mt tcpdump.vm id=10 if_index=0 '
                         'timeout=10 count=100 filter="icmp"')
def tcpdump_vm(args):
    try:
        vm = VM.get(VM.id == args['id'])
    except Exception:
        print >> sys.stderr, 'Error: get vm from db failed'
        return

    try:
        host = HostDevice.get(HostDevice.ip == vm.launch_server)
    except Exception:
        print >> sys.stderr, \
            'Error: get launch_server %s from db failed' % vm.launch_server
        return

    try:
        vif = VInterface.get(
            VInterface.devicetype == VIF_DEVICE_TYPE_VM,
            VInterface.deviceid == args['id'],
            VInterface.ifindex == args['if_index'])
    except Exception:
        print >> sys.stderr, 'Error: get vif if_index=%s from db failed' % \
            args['if_index']
        return

    cmd = 'sshpass -p %s ssh %s %s@%s "ovs-vsctl --bare -- --columns=name ' \
        'find interface \'external_ids:attached-mac=\\\"%s\\\"\'"' % \
        (host.user_passwd, SSH_OPTION_STRING, host.user_name, host.ip, vif.mac)
    rc, output = commands.getstatusoutput(cmd)
    if rc:
        print >> sys.stderr, \
            'Error: cmd [%s] failed, rc=%d out=%s' % \
            (cmd.replace(host.user_passwd, '*'), rc, output)
        return
    if_name = output

    tcpdump_cmds = [
        'tcpdump', '-i', if_name,
        '-l', '-nn', '-e', '-v',
        '-c', args['count'], args['filter']]
    print ' '.join(tcpdump_cmds)
    for line in call_system_streaming(
            ['/usr/bin/timeout', args['timeout'],
             '/usr/bin/sshpass', '-p', host.user_passwd, 'ssh',
             ] + SSH_OPTION_LIST +
            ['%s@%s' % (host.user_name, host.ip)] + tcpdump_cmds,
            ignore_err=False):
        print line,  # do not print duplicate newline


def find_vif_by_id(vifid):
    try:
        vif = VInterface.get(VInterface.id == vifid)
    except Exception:
        return (None, None, None, 'Can not find vif id=%d in db' % vifid)

    try:
        if vif.devicetype == VIF_DEVICE_TYPE_VM:
            ins = VM.get(VM.id == vif.deviceid)
            host = HostDevice.get(HostDevice.ip == ins.launch_server)
        elif vif.deviceid == VIF_DEVICE_TYPE_VGATEWAY:
            ins = VGateway.get(VGateway.id == vif.deviceid)
            host = HostDevice.get(HostDevice.ip == ins.gw_launch_server)
        else:
            return (None, None, vif,
                    'Can not find device/host of %r' % vif.mac)
    except Exception:
        return (None, None, vif, 'Can not find device/host of %r' % vif.mac)

    return (host, ins, vif, '')


def find_vif_by_ip(epc_id, vl2_id, ip):
    """Must set epc_id or vl2_id """
    assert epc_id or vl2_id

    if epc_id:
        try:
            pub_ip = IpResource.get(IpResource.ip == ip)
            host, ins, vif, err = find_vif_by_id(pub_ip.vifid)
            if ins and ins.epc_id == epc_id:
                if vl2_id:
                    if vif and vif.subnetid == vl2_id:
                        return (host, ins, ins, err)
                else:
                    return (host, ins, vif, err)
        except Exception:
            pass

    if vl2_id:
        try:
            prv_ip = VInterfaceIP.get(
                VInterfaceIP.vl2id == vl2_id,
                VInterfaceIP.ip == ip)
            host, ins, vif, err = find_vif_by_id(prv_ip.vifid)
            if err:
                return (host, ins, vif, err)
        except Exception:
            return (None, None, None, 'Can not find LAN ip %r in db' % ip)

    return (None, None, None, 'Can not find LAN ip %r in db' % ip)


@tracecmd.define(name='probe.l2',
                 rqd_params='epc_id, src_ip, dst_ip',
                 description='使用租户(EPC内)的src_ip探测dst_ip',
                 example='mt probe.l2 epc_id=2 '
                         'src_ip=192.168.21.4 dst_ip=192.168.0.1')
def l2_probe(args):
    src_host, src_ins, src_vif, err = find_vif_by_ip(
        int(args['epc_id']), 0, args['src_ip'])
    if err:
        print sys.stderr, err
        return

    next_host, next_ins, next_vif, err = find_vif_by_ip(
        int(args['epc_id']), src_vif.subnetid, args['dst_ip'])

    if src_host.type == HOST_TYPE_VM:
        script = kvm_probe_script
    elif src_host.type == HOST_TYPE_NSP:
        script = nsp_probe_script
    else:
        print >> sys.stderr, "Error: unknown host type, %r" % src_host
        return

    for line in call_system_streaming(
            ['/usr/bin/sshpass', '-p', src_host.user_passwd, 'ssh',
             ] + SSH_OPTION_LIST +
            ['%s@%s' % (src_host.user_name, src_host.ip),
             script, 'ARP', '1', '5', str(src_vif.vlantag),
             args['src_ip'], src_vif.mac,
             args['dst_ip'], next_vif.mac if next_vif else '-'],
            ignore_err=False):
        print line,  # do not print duplicate newline


@tracecmd.define(name='probe.l3',
                 rqd_params='app, epc_id, src_ip, next_hop, dst_ip',
                 description='使用租户(EPC内)的src_ip探测dst_ip，网关使用next_hop'
                             '支持的app: ping, traceroute',
                 example='mt probe.l3 app=ping epc_id=2 src_ip=192.168.21.4 '
                         'next_hop=192.168.0.1 dst_ip=166.111.68.233')
def l3_probe(args):
    src_host, src_ins, src_vif, err = find_vif_by_ip(
        int(args['epc_id']), 0, args['src_ip'])
    if err:
        print sys.stderr, err
        return

    next_host, next_ins, next_vif, err = find_vif_by_ip(
        int(args['epc_id']), src_vif.subnetid, args['next_hop'])

    if src_host.type == HOST_TYPE_VM:
        script = kvm_probe_script
    elif src_host.type == HOST_TYPE_NSP:
        script = nsp_probe_script
    else:
        print >> sys.stderr, "Error: unknown host type, %r" % src_host
        return

    if args['app'] == 'ping':
        protocol = 'ICMP'
    elif args['app'] == 'traceroute':
        protocol = 'TRACEROUTE'
    else:
        print >> sys.stderr, "app must be one of: ping, traceroute"
        return

    for line in call_system_streaming(
            ['/usr/bin/sshpass', '-p', src_host.user_passwd, 'ssh',
             ] + SSH_OPTION_LIST +
            ['%s@%s' % (src_host.user_name, src_host.ip),
             script, protocol, '1',
             '30' if protocol == 'TRACEROUTE' else '5',
             str(src_vif.vlantag),
             args['src_ip'], src_vif.mac,
             args['dst_ip'], next_vif.mac if next_vif else '-',
             args['next_hop']],
            ignore_err=False):
        print line,  # do not print duplicate newline


@tracecmd.define(name='probe.l4',
                 rqd_params='app, epc_id, src_ip, next_hop, dst_ip, dst_port',
                 description='使用租户(EPC内)的src_ip TCP 探测dst_ip:dst_port，'
                             '网关使用next_hop, 支持的app: tcp',
                 example='mt probe.l4 app=tcp epc_id=2 src_ip=192.168.21.4 '
                         'next_hop=192.168.0.1 dst_ip=166.111.68.233 '
                         'dst_port=80')
def l4_probe(args):
    src_host, src_ins, src_vif, err = find_vif_by_ip(
        int(args['epc_id']), 0, args['src_ip'])
    if err:
        print sys.stderr, err
        return

    next_host, next_ins, next_vif, err = find_vif_by_ip(
        int(args['epc_id']), src_vif.subnetid, args['next_hop'])

    if src_host.type == HOST_TYPE_VM:
        script = kvm_probe_script
    elif src_host.type == HOST_TYPE_NSP:
        script = nsp_probe_script
    else:
        print >> sys.stderr, "Error: unknown host type, %r" % src_host
        return

    if args['app'] == 'tcp':
        protocol = 'TCP'
    else:
        print >> sys.stderr, "app must be one of: tcp"
        return

    for line in call_system_streaming(
            ['/usr/bin/sshpass', '-p', src_host.user_passwd, 'ssh',
             ] + SSH_OPTION_LIST +
            ['%s@%s' % (src_host.user_name, src_host.ip),
             script, protocol, '1', '5', str(src_vif.vlantag),
             args['src_ip'], src_vif.mac,
             args['dst_ip'], next_vif.mac if next_vif else '-',
             args['next_hop'], args['dst_port']],
            ignore_err=False):
        print line,  # do not print duplicate newline
