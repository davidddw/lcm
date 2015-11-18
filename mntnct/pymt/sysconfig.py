import requests
import simplejson as json
import sys

import mtcmd
from const import TALKER_URL, JSON_HEADER

syscmd = mtcmd.Command()


@syscmd.define(name='vcpu-over-allocation-ratio.config',
               rqd_params='ratio',
               description=('Change VCPU over allocation ratio, '
                            '0 for unlimited'),
               example='mt vcpu-over-allocation-ratio.config ratio=2.5')
def vcpu_over_allocation_ratio_config(args):
    assert isinstance(args, dict)

    url = TALKER_URL + 'sys/vcpu-over-allocation-ratio/'
    wrapped_data = json.dumps({'RATIO': args['ratio']})
    try:
        r = requests.post(url, wrapped_data, headers=JSON_HEADER)
    except requests.exceptions.ConnectionError:
        print >>sys.stderr, 'Error: unable to connet to talker'
        return -1
    if r.status_code != 200:
        print >>sys.stderr, ('Error: request failed (%s)' %
                             r.json()['DESCRIPTION'])
        return -1

    return 0
