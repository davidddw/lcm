import sys
import mtcmd
import models
import simplejson as json
from const import TALKER_URL, CASHIER_URL, JSON_HEADER
from peewee import MySQLDatabase
import copy
import data
import datetime

service_cmd = mtcmd.Command()


@service_cmd.define(name='nas-storage.config',
                    rqd_params='lcuuid, path, service_vendor_lcuuid',
                    description='Config nas storage info for '
                    'user to LiveCloud',
                    example='mt nas-storage.config lcuuid=xxx-ddd '
                    'path=/mnt/nas '
                    'service_vendor_lcuuid=xxx-ddd-aaa')
def nas_storage_config(args):
    assert isinstance(args, dict)

    try:
        nas_storages = data.NasStorage.get(data.NasStorage.lcuuid
                                           == args['lcuuid'])
    except Exception:
        print >>sys.stderr, 'Error: Nas Storage %s not found' % args['lcuuid']
        return -1

    try:
        data.NasStorage.update(
            service_vendor_lcuuid=args['service_vendor_lcuuid'],
            path=args['path']).where(data.NasStorage.lcuuid
                                     == args['lcuuid']).execute()
    except Exception:
        print >>sys.stderr, 'DB error'
        return -1

    print 'SUCCESS'
    return 0
