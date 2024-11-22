#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
    echo Expecting tibetan.html
    recollq -q 'རྒྱ་གར་སྐད་དུ༔'
    echo Expecting tibetanfalsehits.html
    recollq -q 'སོང་'
    echo Expecting tibetan.html and tibetanconfound.html
    recollq -q 'སྙིང་'
    
)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout


checkresult
