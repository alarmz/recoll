#!/bin/sh

thisdir=`dirname $0`
topdir="$thisdir"/..
. "$topdir"/shared.sh

initvariables $0


docdir=`mktemp -d --tmpdir rcltest.XXXX`
test -d "$docdir" || exit 1

cleanup()
{
    rm -f "$docdir"/docnonumbers.txt
    rm -rf "$docdir"/config
    rmdir "$docdir"
}
trap cleanup 0 2 15

cat > "$docdir"/docnonumbers.txt <<EOF
7569373329
+5858383546
-8499393438
88738.87243
EOF

RECOLL_CONFDIR="$docdir"/config/
mkdir "$RECOLL_CONFDIR" || exit 1
export RECOLL_CONFDIR

mkconf()
{
    nonumbers=$1
    egrep 'loglevel|logfilename|idxlogfilename' \
          ../config/recoll.conf > \
          "$RECOLL_CONFDIR"/recoll.conf

    cat >> "$RECOLL_CONFDIR"/recoll.conf <<EOF
noaspell = 1
indexstemminglanguages=
topdirs = $docdir
nonumbers = $nonumbers
EOF
}

idxandquery()
{
    nonumbers=$1
    recollindex -c "$RECOLL_CONFDIR" -z
    echo "Running queries with nonumbers=$nonumbers" >> $mystdout
    (
        for w in 7569373329 "+5858383546" "-8499393438" "88738.87243" ; do
            echo querying for $w
            recollq -a -S url -q " $w"
        done
    )  2> $mystderr | egrep -v '^Recoll query: ' >> $mystdout
}


cp /dev/null $mystdout

nonumbers=1
mkconf $nonumbers
idxandquery $nonumbers

nonumbers=0
mkconf $nonumbers
idxandquery $nonumbers

# Have to delete the tempdir name from the output for comparison
sed -i -e "s,$docdir,,g" $mystdout

checkresult
