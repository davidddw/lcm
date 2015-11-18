import requests
import sys

import mtcmd
import models
import json
from const import TALKER_URL, PYAGEXEC_URL, JSON_HEADER

vgatewaycmd = mtcmd.Command()


@vgatewaycmd.define(
    name='vgateway-connection-limit.add',
    rqd_params='lcuuid, conn_max',
    opt_params='new_conn_per_sec',
    description='Add connection limit for vgateway',
    example='mt vgateway-connection-limit.add '
    'lcuuid=cd10be82-a146-4707-b670-19eb23aef379 '
    'conn_max=4294967295 new_conn_per_sec=10000')
def vgateway_connection_limit_add(args):
    assert isinstance(args, dict)

    conn_info = models.Conntrack()
    conn_info.conn_max = int(args['conn_max'])
    conn_info.new_conn_per_sec = 0
    if 'new_conn_per_sec' in args:
        conn_info.new_conn_per_sec = int(args['new_conn_per_sec'])

    url = TALKER_URL + 'vgateways'
    if 'lcuuid' in args:
        url += '/%s/conntracks/' % args['lcuuid']
    payload = conn_info.to_primitive()

    try:
        resp = requests.post(url=url, data=json.dumps(payload),
                             headers=JSON_HEADER)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to add connection limit to VGW'
        return -1
    return 0


@vgatewaycmd.define(
    name='vgateway-connection-limit.update',
    rqd_params='lcuuid, conn_max',
    opt_params='new_conn_per_sec',
    description='Add connection limit for vgateway',
    example='mt vgateway-connection-limit.update '
    'lcuuid=cd10be82-a146-4707-b670-19eb23aef379 '
    'conn_max=4294967295 new_conn_per_sec=10000')
def vgateway_connection_limit_update(args):
    assert isinstance(args, dict)

    conn_info = models.Conntrack()
    conn_info.conn_max = args['conn_max']
    conn_info.new_conn_per_sec = 0
    if 'new_conn_per_sec' in args:
        conn_info.new_conn_per_sec = int(args['new_conn_per_sec'])

    url = TALKER_URL + 'vgateways'
    if 'lcuuid' in args:
        url += '/%s/conntracks/' % args['lcuuid']
    payload = conn_info.to_primitive()

    try:
        resp = requests.patch(url=url, data=json.dumps(payload),
                              headers=JSON_HEADER)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to update connection limit to VGW'
        return -1
    return 0


@vgatewaycmd.define(
    name='vgateway-connection-limit.get',
    rqd_params='lcuuid',
    description='Get connection limit of vgateway',
    example='mt vgateway-connection-limit.get '
    'lcuuid=cd10be82-a146-4707-b670-19eb23aef379 ')
def vgateway_connection_limit_get(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'vgateways'
    if 'lcuuid' in args:
        url += '/%s/conntracks/' % args['lcuuid']

    try:
        resp = requests.get(url=url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get connection limit of VGW'
        return -1
    data = resp.json()
    print >>sys.stderr, data["DATA"]
    return 0


@vgatewaycmd.define(
    name='vgateway-connection-limit.remove',
    rqd_params='lcuuid',
    description='Add connection limit for vgateway',
    example='mt vgateway-connection-limit.remove '
    'lcuuid=cd10be82-a146-4707-b670-19eb23aef379 ')
def vgateway_connection_limit_remove(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'vgateways'
    if 'lcuuid' in args:
        url += '/%s/conntracks/' % args['lcuuid']

    try:
        resp = requests.delete(url=url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to delete connection limit to VGW'
        return -1
    return 0
