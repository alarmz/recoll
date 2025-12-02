#!/bin/sh

files='
/home/dockes/projets/fulltext/testrecoll/mail/attach
/home/dockes/projets/fulltext/testrecoll/mail/fred
/home/dockes/projets/fulltext/testrecoll/mail/fred1
/home/dockes/projets/fulltext/testrecoll/mail/outmail
/home/dockes/projets/fulltext/testrecoll/mail/recursive
/home/dockes/projets/fulltext/testrecoll/mail/strangedate1
/home/dockes/mail/amis
/home/dockes/mail/caughtspam.1
/home/dockes/projets/fulltext/testrecoll/mail/tbird/Sent
'

#files=/home/dockes/projets/fulltext/testrecoll/cjk/mbox

fatal()
{
    echo $*; exit 1
}
test $# = 1 || fatal Need topdir arg
topdir=$1
echo $topdir
test -d $topdir || fatal $topdir does not exist

mh_mbox=${mh_mbox:-mh_mbox}
echo mh_mbox $mh_mbox

for mbox in $files;do 
    dir=`basename $mbox`
    #echo $topdir/$dir
    rm -rf $topdir/$dir
    mkdir $topdir/$dir
    echo "Processing $mbox"
    nmsg=`${mh_mbox} $mbox | tail -1 | awk '{print $1}'`
    echo nmsg $nmsg
    for i in `jot $nmsg`;do
        ${mh_mbox} -m $i $mbox > $topdir/$dir/$i
    done
done


