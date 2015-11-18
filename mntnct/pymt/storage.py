import requests
from schematics.exceptions import ModelConversionError, ModelValidationError
import simplejson as json
import sys

import mtcmd
storage_cmd = mtcmd.Command()

from const import TALKER_URL, STOREKEEPER_URL, PYAGEXEC_URL
from const import JSON_HEADER
import models


@storage_cmd.define(name='storage.add',
                    rqd_params='name, backend, domain, host, type',
                    description='Add storage to livecloud',
                    example='mt storage.add name=rbd backend=ceph-rbd '
                            'domain=xxx host=all type=performance')
def storage_add(args):
    assert isinstance(args, dict)

    if args['type'].lower() not in ['capacity', 'performance']:
        print >>sys.stderr, 'Error: type must be "capacity" or "performance"'
        return -1

    url = TALKER_URL + 'domains/'
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get domains'
        return -1
    data = resp.json()
    if 'DATA' not in data:
        print >>sys.stderr, 'Error: unable to get domains'
        return -1

    domain_uuid = None
    for it in data['DATA']:
        if args['domain'] == it.get('NAME', None):
            domain_uuid = it.get('LCUUID', None)
            break
    if domain_uuid is None:
        print >>sys.stderr, 'Error: domain %s not found' % args['domain']
        return -1

    post_data = models.StorageAdd()

    url = TALKER_URL + 'hosts/'
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get hosts'
        return -1
    data = resp.json()
    if 'DATA' not in data:
        print >>sys.stderr, 'Error: unable to get hosts'
        return -1

    all_host = [it['IP'] for it in data['DATA'] if 'IP' in it]
    if args['host'] == 'all':
        host_list = all_host
    else:
        hosts = args['host'].split(',')
        hosts = [it.strip() for it in hosts]
        host_list = []
        for host in hosts:
            if host in all_host:
                if host not in host_list:
                    host_list.append(host)
            else:
                print >>sys.stderr, 'Error: host %s not found' % host
                return -1

    url = (PYAGEXEC_URL + 'pools/%s/') % (host_list[0], args['name'])
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to pyagexec'
        return -1
    data = resp.json()
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get pool %s' % args['name']
        print >>sys.stderr, '%s: %s' % (data['OPT_STATUS'],
                                        data['DESCRIPTION'])
        return -1
    if 'DATA' not in data:
        print >>sys.stderr, 'Error: data corrupted'
        return -1

    try:
        if 'ID' in data['DATA']:
            del data['DATA']['ID']
        storage_data = models.PyagexecStorage(data['DATA'])
        storage_data.validate()
    except (ModelConversionError, ModelValidationError) as e:
        print e
        return -1

    if args['backend'].lower() == 'ceph-rbd':
        post_data.backend = post_data.backends.CEPH_RBD

    post_data.name = args['name']
    post_data.uuid = storage_data.uuid
    post_data.disk_total = storage_data.total_size
    post_data.disk_used = storage_data.used_size
    post_data.hosts = host_list
    post_data.type = args['type'].upper()
    post_data.domain = domain_uuid

    url = STOREKEEPER_URL + 'storages/'
    try:
        resp = requests.post(url, json.dumps(post_data.to_primitive()),
                             headers=JSON_HEADER)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to storekeeper'
        return -1

    if resp.status_code != 200:
        data = resp.json()
        print >>sys.stderr, 'Error: failed to add storage'
        print >>sys.stderr, '%s: %s' % (data['OPT_STATUS'],
                                        data['DESCRIPTION'])
        return -1

    return 0
