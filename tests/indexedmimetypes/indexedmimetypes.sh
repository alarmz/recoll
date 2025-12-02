#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

RECOLL_CONFDIR=$topdir/$myname
export RECOLL_CONFDIR
logfilename=$toptmp/$myname.log

sharedconf() {
cat > recoll.conf <<EOF
topdirs = $tstdata/html $tstdata/txt
logfilename=$logfilename
loglevel = 6
noaspell = 1
EOF
}
###########
# End boilerplate


sharedconf
cat >> recoll.conf <<EOF
indexedmimetypes = text/plain
indexallfilenames = 0
EOF

recollindex -z

(
# text files are indexed 
recollq shorty
# html ones are not (not even the names)
recollq qtextedit
) 2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

sharedconf
cat >> recoll.conf <<EOF
indexedmimetypes = text/plain
indexallfilenames = 1
EOF

recollindex -z

(
# text files are indexed 
recollq shorty
# html ones are not (but the names are)
recollq qtextedit
recollq template
) 2> $mystderr | egrep -v '^Recoll query: ' >> $mystdout

checkresult
