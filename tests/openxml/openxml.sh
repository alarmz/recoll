#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(

recollq author:ben '"Consideration of the high correlation"'
recollq '"The Circassian Education Foundation"' date:2008-01-20
recollq author:"Johnny Walker" '"Thin Lizzy"'
recollq '"Objekt steht im Akkusativ"'
echo Checking numbers found in xlsx spreadsheet
recollq 2005 2000 1983 1962 1990 ext:xlsx
) 2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

checkresult
