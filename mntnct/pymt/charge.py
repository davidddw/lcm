import sys
import requests
import mtcmd
import models
import simplejson as json
from const import TALKER_URL, CASHIER_URL, JSON_HEADER
from const import CHARGE_DETAIL_VM_TYPE
from peewee import MySQLDatabase
import copy
import data
import datetime

charge_cmd = mtcmd.Command()


@charge_cmd.define(name='vm-plan-charge.config',
                   rqd_params='product_specification_lcuuid',
                   description='Modify vm-plan charge details',
                   example='mt vm-plan-charge.config '
                   'product_specification_lcuuid='
                   '1a53f2bd-c874-41de-923b-c93a37df6b72')
def modify_vmplan_price(args):
    assert isinstance(args, dict)

    ps_info = data.ProductSpec.get(data.ProductSpec.lcuuid ==
                                   args['product_specification_lcuuid'])
    end_time = datetime.datetime.now()

    vm_plan = json.loads(ps_info.content)
    vm_plan_request = models.VmPlanRequest()
    vm_plan_request.cpu = vm_plan['compute_size']['vcpu_num']
    vm_plan_request.memory = vm_plan['compute_size']['mem_size']
    vm_plan_request.disk = vm_plan['user_disk_size']
    vm_plan_request.domain = ps_info.domain
    url = CASHIER_URL + 'plans/'
    payload = vm_plan_request.to_primitive()
    r = requests.post(url, data=json.dumps(payload),
                      headers=JSON_HEADER)
    resp = r.json()

    if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
        print >>sys.stderr, 'Error: response %s corrupted' % resp
        return -1

    if resp['OPT_STATUS'] != 'SUCCESS':
        print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                resp['DESCRIPTION'])
        return -1

    charge_details = data.ChargeDetail.select().\
        where((data.ChargeDetail.product_specification_lcuuid
              == args['product_specification_lcuuid']) &
              (data.ChargeDetail.end_time >> None))
    charge_details_new = []
    for detail in charge_details:
        detail_new = copy.deepcopy(detail)
        detail.end_time = end_time
        detail_new.price = resp["DATA"]["PRICE"]
        detail_new.cost = 0
        detail_new.start_time = end_time
        detail.save()
        del detail_new.__dict__['_data']['id']
        charge_details_new.append(detail_new.__dict__['_data'])

    data.ChargeDetail.insert_many(charge_details_new).execute()
    print 'SUCESS'
    return 0


@charge_cmd.define(name='vm-parts-charge.config',
                   rqd_params='domain',
                   description='Modify vm parts charge details',
                   example='mt vm-parts-charge.config '
                   'domain='
                   '1a53f2bd-c874-41de-923b-c93a37df6b72')
def modify_vm_parts_price(args):
    assert isinstance(args, dict)
    charge_details = data.ChargeDetail.select().\
        where((data.ChargeDetail.type == CHARGE_DETAIL_VM_TYPE) &
              (data.ChargeDetail.domain == args['domain']) &
              (data.ChargeDetail.end_time >> None))
    end_time = datetime.datetime.now()
    charge_details_new = []
    for detail in charge_details:
        vm = data.VM.get(data.VM.lcuuid == detail.obj_lcuuid)
        plan_request = models.VmPlanRequest()
        plan_request.cpu = vm.vcpu_num
        plan_request.memory = (vm.mem_size >> 10)
        plan_request.disk = vm.vdi_user_size
        plan_request.domain = args['domain']

        url = CASHIER_URL + 'plans/'
        payload = plan_request.to_primitive()
        r = requests.post(url, data=json.dumps(payload),
                          headers=JSON_HEADER)
        resp = r.json()
        if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
            print >>sys.stderr, 'Error: response %s corrupted' % resp
            return -1

        if resp['OPT_STATUS'] != 'SUCCESS':
            print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                    resp['DESCRIPTION'])
            return -1

        detail_new = copy.deepcopy(detail)
        detail.end_time = end_time
        detail_new.start_time = end_time
        detail_new.price = resp['DATA']['PRICE']
        detail_new.cost = 0
        detail.save()
        del detail_new.__dict__['_data']['id']
        charge_details_new.append(detail_new.__dict__['_data'])
    data.ChargeDetail.insert_many(charge_details_new).execute()
    print 'SUCESS'
    return 0


@charge_cmd.define(name='product-charge.config',
                   rqd_params='product_specification_lcuuid',
                   description='Modify product charge details',
                   example='mt product-charge.config '
                   'product_specification_lcuuid='
                   '1a53f2bd-c874-41de-923b-c93a37df6b72')
def modify_other_price(args):
    ps_info = data.ProductSpec.get(
        data.ProductSpec.lcuuid == args['product_specification_lcuuid'])
    charge_details = data.ChargeDetail.select().\
        where((data.ChargeDetail.product_specification_lcuuid
              == args['product_specification_lcuuid']) &
              (data.ChargeDetail.end_time >> None))

    charge_details_new = []
    end_time = datetime.datetime.now()
    for detail in charge_details:
        price_request = models.PriceRequest()
        price_request.name = ps_info.plan_name
        price_request.quantity = detail.size
        price_request.domain = ps_info.domain

        url = CASHIER_URL + 'prices/'
        payload = price_request.to_primitive()
        r = requests.post(url, data=json.dumps(payload),
                          headers=JSON_HEADER)
        resp = r.json()

        if 'OPT_STATUS' not in resp or 'DESCRIPTION' not in resp:
            print >>sys.stderr, 'Error: response %s corrupted' % resp
            return -1

        if resp['OPT_STATUS'] != 'SUCCESS':
            print >>sys.stderr, 'Error (%s): %s' % (resp['OPT_STATUS'],
                                                    resp['DESCRIPTION'])
            return -1

        detail.end_time = end_time
        detail_new = copy.deepcopy(detail)
        detail_new.price = resp['DATA']['PRICE']
        detail_new.cost = 0
        detail_new.start_time = end_time
        detail.save()
        del detail_new.__dict__['_data']['id']
        detail_new.end_time = None
        charge_details_new.append(detail_new.__dict__['_data'])

    data.ChargeDetail.insert_many(charge_details_new).execute()
    print 'SUCCESS'
    return 0
