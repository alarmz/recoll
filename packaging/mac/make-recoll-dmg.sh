#!/bin/sh

fatal()
{
    echo $*
    exit 1
}
usage()
{
    fatal make-recoll-dmg.sh
}

#### Adjustable things
top=~/Recoll
# The possibly bogus version we have in paths (may be harcoded in the .pro)
qcbuildloc=Qt_6_7_3_for_macOS
deploy=~/Qt/6.7.3/macos/bin/macdeployqt

#### Locations depending on the above, should not change much
toprecoll=$top/recoll/src
appdir=$toprecoll/qtgui/build/${qcbuildloc}-Release/recoll.app
rclindexdir=$toprecoll/qmake/build/recollindex/${qcbuildloc}-Release
rclqdir=$toprecoll/qmake/build/recollq/${qcbuildloc}-Release
bindir=$appdir/Contents/MacOS
datadir=$appdir/Contents/Resources
# Target
dmg=$appdir/../recoll.dmg

### Do it
version=`cat $toprecoll/RECOLL-VERSION.txt`
VERSION=`cat $toprecoll/RECOLL-VERSION.txt`
CFVERS=`grep PACKAGE_VERSION $toprecoll/common/autoconfig.h | \
cut -d ' ' -f 3 | sed -e 's/"//g'`
test "$VERSION" = "$CFVERS" ||
    fatal Versions in RECOLL-VERSION.txt and autoconfig.h differ

test -d $appdir || fatal Must first have built recoll in $appdir

cp $rclindexdir/recollindex $bindir || exit 1
cp $rclqdir/recollq $bindir || exit 1

# Antiword
cp $top/antiword/antiword $bindir || exit 1
mkdir -p $datadir/antiword || exit 1
cp $top/antiword/Resources/* $datadir/antiword || exit 1

# Libmagic data
cp $top/libmagic/magic/magic.mgc $datadir

# Poppler: slight modification and special build of poppler 24.04.0 with static linking of
# pdfinfo and pdftotext, see the 00-JF-README.txt in the relocatable-for-recoll branch in the
# local source.  poppler-data is a copy of ubuntu 22.04 /usr/share/poppler
cp $top/poppler/build/utils/pdfinfo $top/poppler/build/utils/pdftotext $bindir || exit 1
mkdir -p $datadir/poppler || exit 1
cp -rp $top/poppler-data/* $datadir/poppler || exit 1

### Create the dmg
rm -f $dmg ~/Documents/recoll-$VERSION-*.dmg

$deploy $appdir -dmg || exit 1

### Rename the dmg according to date and version
hash=`(cd $top/recoll;git log -n 1  | head -1  | awk '{print $2}' |cut -b 1-8)`
dte=`date +%Y%m%d`
mv $dmg ~/Documents/recoll-$VERSION-$dte-$hash.dmg || exit 1
ls -l ~/Documents/recoll-$VERSION-*.dmg

