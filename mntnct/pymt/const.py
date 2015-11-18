TALKER_URL = 'http://localhost:20013/v1/'
STOREKEEPER_URL = 'http://localhost:20102/v1/'
PYAGEXEC_URL = 'http://%s:20016/v1/'
JSON_HEADER = {'content-type': 'application/json'}
CASHIER_URL = 'http://localhost:20017/'
SDNCTRL_URL = 'http://localhost:20103/v1/'
EXCHANGE_URL = 'http://localhost:20104/v1/'

CHARGE_DETAIL_VM_TYPE = 1
VINTERFACE_STATE_ATTACH = 1
VINTERFACE_TYPE_CTRL = 1
VINTERFACE_TYPE_SERV = 2
VINTERFACE_TYPE_WAN = 3
VINTERFACE_TYPE_LAN = 4
VINTERFACE_DEVICETYPE = {"VM": 1, "TPD": 3, "VGATEWAY": 5}
VINTERFACE_CONFIG_VLANTAG_LEVEL = 4
VINTERFACE_CONFIG_BROADC_BANDW_LEVEL = 6

VIF_DEVICE_TYPE_VM = 1
VIF_DEVICE_TYPE_VGW = 2
VIF_DEVICE_TYPE_THIRD = 3
VIF_DEVICE_TYPE_VMWAF = 4
VIF_DEVICE_TYPE_VGATEWAY = 5

ROLE_VGATEWAY = 7
ROLE_VALVE = 11

VGATEWAY_POLICY_ENABLE = 1
VGATEWAY_POLICY_SNAT = 4
VGATEWAY_POLICY_DNAT = 5
VGATEWAY_POLICY_ACL = 3

VM_FLAG_ISOLATE = 0x04

HOST_TYPE_VM = 1
HOST_TYPE_NSP = 3

HOST_HTYPE_XEN = 1
HOST_HTYPE_KVM = 3

SWITCH_TYPE_ETHERNET = 0
SWITCH_TYPE_TUNNEL = 2

MAX_CONNTRACKS_PER_SEC = 10000
MAX_CONNTRACKS = 4294967295

DEBUG = False
SCRIPT_SWITCH_CONF = "python /usr/local/livecloud/bin/sdncontroller/cli.py "
SSH_OPTION_LIST = [
    "-4", "-a",
    "-o", "ConnectTimeout=1",
    "-o", "VerifyHostKeyDNS=no",
    "-o", "GSSAPIAuthentication=no",
    "-o", "StrictHostKeyChecking=no",
    "-o", "UserKnownHostsFile=/dev/null",
    "-o", "LogLevel=ERROR",
]
SSH_OPTION_STRING = "-4 -a " \
    "-o ConnectTimeout=1 " \
    "-o VerifyHostKeyDNS=no " \
    "-o GSSAPIAuthentication=no " \
    "-o StrictHostKeyChecking=no " \
    "-o UserKnownHostsFile=/dev/null " \
    "-o LogLevel=ERROR "

MASK_MAX = 32
