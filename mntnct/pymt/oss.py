import requests
import sys, socket, json

import mtcmd

from data import BssDomain as Domain

osscmd = mtcmd.Command()

def set_oss_env(oss_ip):
    try:
        socket.inet_aton(oss_ip)
    except socket.error:
        return 'ip invalid'
    return {"APPSERVER": "https://"+oss_ip+"/appserver/v1/"}


def sync_products(oss):
    s=requests.Session()
    s.headers.update({"APPKEY": "5ee218a0-25ec-463a-81f5-6c5364707cda",
                    "MTKEY": "6e79a5b1-df16-4946-aeb8-cdd6c9ea43d0"})
    res=s.get(oss['APPSERVER']+"products", verify=False)
    if res.status_code == 200:
        webcall=requests.Session()
        webcall.headers.update({"Content-Type": "application/json"})
        ack=webcall.post(url="http://127.0.0.1/mt/addproduct",
                        data=json.dumps(res.json()['DATA']))
        return ack.text
    return "fetch oss products error: "+str(res.status_code)

def sync_user(oss):
    s=requests.Session()
    s.headers.update({"APPKEY": "5ee218a0-25ec-463a-81f5-6c5364707cda",
                    "MTKEY": "6e79a5b1-df16-4946-aeb8-cdd6c9ea43d0"})
    res=s.get(oss['APPSERVER']+"users", verify=False)
    if res.status_code == 200:
        webcall=requests.Session()
        webcall.headers.update({"Content-Type": "application/json"})
        ack=webcall.post(url="http://127.0.0.1/mt/adduser",
                        data=json.dumps({"user": res.json()['DATA']}))
        return ack.text
    return "fetch oss users error: "+str(res.status_code)


@osscmd.define(name='oss.sync',
            opt_params='ip',
            description='sync an oss',
            example='mt oss.sync ip=10.66.16.2')
def oss_sync(args):
    assert isinstance(args, dict)
    if 'ip' in args:
        oss=set_oss_env(args['ip'])
        if isinstance(oss, dict):
            domain=Domain()
            if domain.select().where(Domain.ip == args['ip']).count():
                print sync_products(oss)
                print sync_user(oss)
                return
            else:
                return "oss of ip "+args['ip']+" not found"
    return "ip invalid"

