import sys
import json
import requests
import mtcmd
from const import EXCHANGE_URL
from const import JSON_HEADER

openstackcmd = mtcmd.Command()


@openstackcmd.define(name='openstack.import-tenant',
                 rqd_params='user_name, openstack_tenant',
                 opt_params='',
                 description='Import openstack tenant',
                 example='mt openstack.import-tenant user_name=admin '
                         'openstack_tenant=admin')
def openstack_import_tenant(args):
    assert isinstance(args, dict)

    if args['user_name'] is None:
        print >>sys.stderr, 'No user specified'
        return -1

    if args['openstack_tenant'] is None:
        print >>sys.stderr,  'No openstack_tenant specified'
        return -1

    import_tenant_req = {}
    import_tenant_req['USER'] = args['user_name']
    import_tenant_req['OPENSTACK_TENANT'] = args['openstack_tenant']

    r = requests.post(EXCHANGE_URL + 'openstack-tenants/',
                      json.dumps(import_tenant_req), headers=JSON_HEADER)
    if r.status_code != 200:
        print >>sys.stderr, 'Error: call URL failed'
        return -1

    resp = r.json()
    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print 'Import openstack tenant SUCCESS'


@openstackcmd.define(name='openstack.remove-tenant',
                 rqd_params='openstack_tenant',
                 opt_params='',
                 description='Remove openstack tenant',
                 example='mt openstack.remove-tenant '
                         'openstack_tenant=admin')
def openstack_remove_tenant(args):
    assert isinstance(args, dict)

    if args['openstack_tenant'] is None:
        print >>sys.stderr,  'No openstack_tenant specified'
        return -1

    r = requests.delete(EXCHANGE_URL + 'openstack-tenants/%s' %
                        args['openstack_tenant'])
    if r.status_code != 200:
        print >>sys.stderr, 'Error: call URL failed'
        return -1

    resp = r.json()
    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print 'Remove openstack tenant SUCCESS'
