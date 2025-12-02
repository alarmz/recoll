#!/bin/sh
# Case and diacritics sensitivity testing
# We use a separate configuration for this.
# This file is encoded in UTF-8: accentué

LANG=en_US.UTF-8
export LANG

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

RECOLL_CONFDIR=$topdir/casediac
export RECOLL_CONFDIR
toptmp=${TMPDIR:-/tmp}/recolltsttmp

cat > $RECOLL_CONFDIR/recoll.conf <<EOF
loglevel = 6
logfilename = ${toptmp}/logcasediac

daemloglevel = 6
daemlogfilename = ${toptmp}/rclmoncasediac

indexStripChars = 0

topdirs = $tstdata/casediac
indexstemminglanguages = french

EOF

recollindex -z

(
set -x
# Diac-sensitive search for existing term
recollq -S url -s french éviter
# Succeeds because of default autodiacsens = false
recollq -S url -s french èviter
# Diac-sensitive searches for non-existing term
recollq -S url -s french '"èviter"D'
recollq -S url -s french '"eviter"D'
# Diac-unsensitive search
recollq -S url -s french eviter
# Search for stem-existing term (has to be unsensitive)
recollq -S url -s french evite
# Search for stem-existing term with disabled stemming
recollq -S url -s french Evite
# Case-sensitive, existing and non existing
recollq -S url -s french MAJUSCULESXX
recollq -S url -s french MAjUSCULESXX
# Case-unsensitive
recollq -S url -s french majusculesxx
# Case-unsensitive
recollq -S url -s french Majusculesxx
# Case sensitive, term does not exist
recollq -S url -s french '"Majusculesxx"C'

)  2>&1 | egrep -v '^Recoll query: ' > $mystdout

checkresult

