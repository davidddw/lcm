import requests
import simplejson as json
import sys

import mtcmd
import models
import data
from const import STOREKEEPER_URL
from const import JSON_HEADER

blockcmd = mtcmd.Command()


@blockcmd.define(name='block-device.import',
                 rqd_params='name, storage, user-email, product-specification',
                 opt_params='',
                 description='Import BlockDevice to database',
                 example='mt block-device.import name=block-1 '
                         'storage=capacity user-email=x@yunshan.net.cn '
                         'product-specification=CloudDisk')
def block_import(args):
    assert isinstance(args, dict)

    url = STOREKEEPER_URL + 'storages/'
    try:
        resp = requests.get(url)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to storekeeper'
        return -1
    if resp.status_code != 200:
        print >>sys.stderr, 'Error: unable to get storages'
        return -1
    r_data = resp.json()
    if 'DATA' not in r_data:
        print >>sys.stderr, 'Error: unable to get storages'
        return -1

    storage_lcuuid = None
    domain_uuid = None
    for it in r_data['DATA']:
        if args['storage'] == it.get('NAME', None):
            storage_lcuuid = it.get('UUID', None)
            domain_uuid = it.get('DOMAIN', None)
            break
    if storage_lcuuid is None:
        print >>sys.stderr, 'Error: storage %s not found' % args['storage']
        return -1

    try:
        user = data.User.get(data.User.email == args['user-email'])
        user_lcuuid = user.useruuid
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

    vol_info = models.VolImport()
    vol_info.name = args['name']
    vol_info.storage_lcuuid = storage_lcuuid
    vol_info.user_lcuuid = user_lcuuid
    vol_info.product_specification_lcuuid = ps_lcuuid

    url = STOREKEEPER_URL + 'blocks/import/'
    try:
        resp = requests.post(url, json.dumps(vol_info.to_primitive()),
                             headers=JSON_HEADER).json()
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to storekeeper'
        return -1

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: failed to import'
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error: failed to import'
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print >>sys.stdout, ''
    print >>sys.stdout, 'NAME: %s' % resp['DATA']['NAME']
    print >>sys.stdout, 'SIZE: %d M' % resp['DATA']['SIZE']
    print >>sys.stdout, 'LCUUID: %s' % resp['DATA']['LCUUID']

    return 0
