import sys
import os
import Queue
import requests
import simplejson as json
import socket
import threading
import commands
import subprocess
import traceback

from const import JSON_HEADER, DEBUG


class CallbackServer(threading.Thread):
    def __init__(self, address, result_queue):
        self.address = address
        self.result_queue = result_queue
        super(CallbackServer, self).__init__()
        self.daemon = True

    def run(self):
        try:
            os.unlink(self.address)
        except OSError:
            if os.path.exists(self.address):
                raise
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.bind(self.address)
        sock.listen(1)

        connection, client_address = sock.accept()
        result = ''
        try:
            while True:
                data = connection.recv(1024)
                if data:
                    result += data
                else:
                    break
        finally:
            connection.close()

        sock.close()

        try:
            os.unlink(self.address)
        except:
            pass

        self.result_queue.put(result)


def call_post_with_callback(url, data=None):
    """Call POST and return response or callback response
    """
    callback_socket = '/tmp/mt-%d.sock' % os.getpid()

    # to receive data from thread
    q = Queue.Queue(1)

    cb = CallbackServer(callback_socket, q)
    cb.start()

    wrapped_data = json.dumps({'DATA': data,
                               'CALLBACK': callback_socket})
    r = requests.post(url, wrapped_data, headers=JSON_HEADER)

    resp = r.json()
    if ('OPT_STATUS' not in resp or
            'DESCRIPTION' not in resp or
            'WAIT_CALLBACK' not in resp):
        return resp

    if resp['OPT_STATUS'] != 'SUCCESS' or resp['WAIT_CALLBACK'] is False:
        return resp

    # wait for callback
    cb.join()
    return json.loads(q.get())


def call_patch_with_callback(url, data=None):
    """Call PATCH and return response or callback response
    """
    callback_socket = '/tmp/mt-%d.sock' % os.getpid()

    # to receive data from thread
    q = Queue.Queue(1)

    cb = CallbackServer(callback_socket, q)
    cb.start()

    wrapped_data = json.dumps({'DATA': data,
                               'CALLBACK': callback_socket})
    r = requests.patch(url, wrapped_data, headers=JSON_HEADER)

    resp = r.json()
    if ('OPT_STATUS' not in resp or
            'DESCRIPTION' not in resp or
            'WAIT_CALLBACK' not in resp):
        return resp

    if resp['OPT_STATUS'] != 'SUCCESS' or resp['WAIT_CALLBACK'] is False:
        return resp

    # wait for callback
    cb.join()
    return json.loads(q.get())


def call_system_sh(args):
    """call system
    """
    cmd = 'sh -x ' if DEBUG else 'sh '
    cmd += ' '.join(['"' + a + '"' for a in args]) + ' 2>&1'
    rc, output = commands.getstatusoutput(cmd)
    return (rc, output)


def call_system_streaming(args, verbose=False, shell=False, ignore_err=False):
    assert isinstance(args, list)
    try:
        p = subprocess.Popen(
            args if not shell else ' '.join(args),
            bufsize=1,  # line buffered
            shell=shell,
            stdout=subprocess.PIPE,
            stderr=None if ignore_err else subprocess.STDOUT)
        for line in iter(p.stdout.readline, ''):
            yield line
    except Exception as e:
        print >> sys.stderr, traceback.format_exc()
        print >> sys.stderr, '[%s] exception: %s' % (args, e)
