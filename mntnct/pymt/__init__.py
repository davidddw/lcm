# -*- coding: utf-8 -*-
import mtcmd

allcmd = mtcmd.Command()

groups = [
    (u'部署 - 域', ['domain\..*']),
    (u'部署 - 物理集群', ['cluster\..*']),
    (u'部署 - 逻辑机架', ['rack\..*']),
    (u'部署 - 交换机', ['torswitch\..*']),
    (u'部署 - 虚拟化服务器', ['host\..*']),
    (u'部署 - 产品规格',
        ['product-specification\..*', 'product-charge\..*',
         'live-x\..*', 'vm-parts-charge\..*', 'service-charge\..*',
         'vm-plan-charge\..*']),
    (u'部署 - 资源池',
        ['pool\..*', 'vgateway-connection-limit\..*',
         'nsp-vgateway-upper-limit\..*', 'vcpu-over-allocation-ratio\..*']),
    (u'部署 - 存储资源', ['storage\..*']),
    (u'部署 - 服务', ['service-vendor\..*', 'vul-scanner\..*']),
    (u'部署 - 网络配置',
        ['ip-ranges\..*', '-plane-.*\..*', '^vlan\..*',
         'vlan-range\..*', 'tunnel-protocol\..*', 'vlantag-ranges\..*']),
    (u'部署 - ISP', ['ip\..*', 'isp\..*']),
    (u'部署 - 镜像', ['template\..*']),
    ('', []),  # dummy node
    (u'运维 - 部署检查、故障排查', ['debug\..*', 'trace\..*']),
    (u'运维 - 租户虚拟设备', ['vm\..*', 'vgateway\..*', 'vm-agent\..*',
                              'block-device\..*']),
    (u'运维 - 租户物理设备', ['hwdev\..*', 'hwdev-connect\..*']),
    (u'运维 - 网络接口', ['vif\..*', 'switch-port-mirror\..*']),
    (u'运维 - 业务', ['invitation\..*', 'user\..*', 'oss\..*']),
    ('', []),  # dummy node
    (u'第三方云 - VMware', ['vcenter\..*', 'vcdatacenter\..*', 'vcserver\..*']),
    (u'第三方云 - Azure', ['azure\..*']),
    (u'第三方云 - OpenStack', ['openstack\..*']),
    ('', []),  # dummy node
]

from charge import charge_cmd
allcmd.add_commands(charge_cmd)

from host import hostcmd
allcmd.add_commands(hostcmd)

from service import service_cmd
allcmd.add_commands(service_cmd)

from storage import storage_cmd
allcmd.add_commands(storage_cmd)

from sysconfig import syscmd
allcmd.add_commands(syscmd)

from vm import vmcmd
allcmd.add_commands(vmcmd)

from block import blockcmd
allcmd.add_commands(blockcmd)

from oss import osscmd
allcmd.add_commands(osscmd)

from vif import vifcmd
allcmd.add_commands(vifcmd)

from debug import debugcmd
allcmd.add_commands(debugcmd)

from trace import tracecmd
allcmd.add_commands(tracecmd)

from vgateway import vgatewaycmd
allcmd.add_commands(vgatewaycmd)

from switch import switchcmd
allcmd.add_commands(switchcmd)

from azure import azurecmd
allcmd.add_commands(azurecmd)

from openstack import openstackcmd
allcmd.add_commands(openstackcmd)
