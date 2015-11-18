import sys
import json
import requests
import socket
from xmlrpclib import ServerProxy
import mtcmd
from const import EXCHANGE_URL
from const import JSON_HEADER

azurecmd = mtcmd.Command()


@azurecmd.define(name='azure.import-publishsetting',
                 rqd_params='user_name, publishsetting',
                 opt_params='',
                 description='Import azure publishsetting',
                 example='mt azure.import-publishsetting user_name=Yunshan '
                         'publishsetting=8-6-2015-credentials.publishsettings')
def azure_import_publishsetting(args):
    assert isinstance(args, dict)

    if args['user_name'] is None:
        print >>sys.stderr, 'No user specified'
        return -1

    if args['publishsetting'] is None:
        print >>sys.stderr,  'No publishsetting specified'
        return -1

    s = ServerProxy('http://%s:%d/' % ('172.16.20.248', 12346))
    try:
        socket.setdefaulttimeout(10)
        s.ping()
        socket.setdefaulttimeout(None)
    except Exception as e:
        print >>sys.stderr, 'Error: connect azure agent timeout'
        return -1

    try:
        socket.setdefaulttimeout(10)
        r, resp = s.import_publishsettings(args['publishsetting'])
        socket.setdefaulttimeout(None)
        if not r:
            raise
    except Exception as e:
        print >>sys.stderr, \
            'Error: connect azure to import publishsetting failed (%s)' % e
        return -1

    subscription = json.loads(resp)
    subscription['USER'] = args['user_name']
    subscription_req = json.dumps(subscription)

    r = requests.post(EXCHANGE_URL + 'azure-subscriptions/',
                      subscription_req, headers=JSON_HEADER)
    resp = r.json()
    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    print 'Import publishsetting SUCCESS'
