#!/bin/sh

EXEC=$1
BINPATH=$2
LCBINPATH=/usr/local/livecloud/bin

rm -rf $LCBINPATH/mntnct
mkdir -p $LCBINPATH/mntnct

if [ -d "$BINPATH" ] && [ -x "$EXEC" ]; then
    cp -f $EXEC $BINPATH
    cp -f bash-completion/mntnct /usr/share/bash-completion/
    ln -sf /usr/share/bash-completion/mntnct /etc/bash_completion.d/mntnct
    \cp -rf pymt mt.py $LCBINPATH/mntnct/
fi

if [ -d "$LCBINPATH" ]; then
    \cp -rf dialog mtps.py $LCBINPATH/mntnct/
    ln -sf $LCBINPATH/mntnct/mtps.py $BINPATH/mtps
fi
