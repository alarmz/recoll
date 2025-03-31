#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
recollq -S url '"Lorem ipsum odor amet"' 
)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

checkresult
