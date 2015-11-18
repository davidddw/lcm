#!/usr/bin/python

import os
from subprocess import Popen, PIPE

from dialog.dialog import main_dialog

if __name__ == '__main__':
    main_dialog()
    p1 = Popen(['/bin/ps', '-ef'], stdout=PIPE)
    p2 = Popen(['/bin/grep', 'cashier'], stdin=p1.stdout, stdout=PIPE)
    p3 = Popen(['/bin/grep', '-v', 'grep'], stdin=p2.stdout, stdout=PIPE)
    p4 = Popen(['/bin/awk', '{print $2}'], stdin=p3.stdout, stdout=PIPE)
    with open(os.devnull, 'w') as devnull:
        p5 = Popen(['/usr/bin/xargs', 'kill', '-1'],
                   stdin=p4.stdout, stdout=devnull, stderr=devnull)
        p5.communicate()
