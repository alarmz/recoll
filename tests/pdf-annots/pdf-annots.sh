#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

# Note the grep options to output binary. One of the URLs has an 8 byte (iso encoding) char in it
(
    recollq -q -S url '"new test JF annotation using Adobe Acrobat X"'

    # This supposes that the fields file is customized, which is not the case by default
    echo 
    echo "Extracting the value for an annotation field:"
    recollq -F annotation pdfannot:'"DAVID: Test of a highlight"'  |  tail -1 | base64 -d

)  2> $mystderr | grep -E -a -v '^Recoll query: ' > $mystdout

checkresult
