#!/usr/bin/python
# -*- coding: utf-8 -*-

import copy
import os
import subprocess
import sys
import re

from pymt import allcmd, groups

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 1)

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print >>sys.stderr, '''Usage: mt <command> [command specific arguments]

    A full list of commands can be obtained by running
            mt help'''
    else:
        local_args = copy.deepcopy(sys.argv)
        f_minimal = False
        if '--minimal' in local_args:
            local_args.remove('--minimal')
            f_minimal = True

        args = {}
        op = local_args[1]
        if op == 'help':
            call_args = []
            cmds = []
            if len(local_args) == 2:
                if os.path.isfile('/usr/local/bin/mntnct'):
                    call_args.append('mntnct')
                else:
                    call_args.append(os.path.dirname(
                        os.path.realpath(__file__)) + '/mntnct')
                call_args.extend(['help', '--minimal'])
                out, _ = subprocess.Popen(
                    call_args, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE).communicate()
                if out:
                    cmds = [it.strip() for it in out.split(',')]
                for op in allcmd:
                    if op not in cmds:
                        cmds.append(op)
                if f_minimal:
                    print ','.join(cmds)
                else:
                    print ('Usage: mt <command> '
                           '[command specific arguments]\n\n')
                    print('To get help on a specific command: '
                          'mt help <command>\n\n')
                    print 'Common command list\n-------------------\n'

                    other_cmds = set()
                    for c in cmds:
                        other_cmds.add(c)
                    for group, cmd_res in groups:
                        if not group:
                            print
                            continue
                        group_cmds = []
                        for re_c in cmd_res:
                            for c in cmds:
                                if re.search(re_c, c):
                                    group_cmds.append(c)
                                    other_cmds.remove(c)
                        group_cmds.sort()
                        print '%s:\n    %s' % (group, ', '.join(group_cmds))
                    other_cmds = list(other_cmds)
                    other_cmds.sort()
                    print '%s:\n    %s' % (u'其它', ', '.join(other_cmds))

            elif local_args[2] in allcmd:
                print allcmd.help(local_args[2])
            else:
                call_args = copy.deepcopy(sys.argv)
                if os.path.isfile('/usr/local/bin/mntnct'):
                    call_args[0] = 'mntnct'
                else:
                    call_args[0] = os.path.dirname(
                        os.path.realpath(__file__)) + '/mntnct'
                subprocess.call(call_args)
            sys.exit(0)
        if op in allcmd:
            for arg in local_args[2:]:
                key, value = arg.split('=', 1)
                args[key] = value

            # temporarily use mt.py host.add for KVM VM host only
            if ((op != 'host.add' and op != 'host.rescan') or
                    'type' in args and
                    'htype' in args and
                    args['type'].lower() in ['vm', 'server', '1'] and
                    args['htype'].lower() == 'kvm'):
                sys.exit(allcmd.call(op, args))
        # fall into origin mntnct command in C
        call_args = copy.deepcopy(sys.argv)
        if os.path.isfile('/usr/local/bin/mntnct'):
            call_args[0] = 'mntnct'
        else:
            call_args[0] = (os.path.dirname(os.path.realpath(__file__)) +
                            '/mntnct')
        subprocess.call(call_args)
