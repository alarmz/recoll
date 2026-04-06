#!/bin/sh
# This is used with meson for the recoll custom_target().
# This just runs qmake then make
# I could find no way to do this in 2 steps inside meson
set -e
pro_file=$1
shift

dir=`dirname $pro_file`
fn=`basename $pro_file`

QMAKE=${QMAKE:-qmake}
MAKE=${MAKE:-make}

ncpus=2
if which nproc > /dev/null ; then
    ncpus=`nproc`
elif which sysctl > /dev/null ; then
    ncpus=`sysctl hw.ncpu | awk '{print $2}'`
elif test -f /proc/cpuinfo ; then
    ncpus=`grep -E '^processor[        ]*:' /proc/cpuinfo | wc -l`
fi
if test -z "$ncpus" ; then
    ncpus=1
fi

cd $dir
${QMAKE} $fn $*
${MAKE} -j $ncpus

if test `uname -s` = Darwin ; then
    # qmake will create a recoll.app and put the bin in there. The alternative to an installation
    # to $bindir would be to create the full bundle in place but this would make things extremely
    # different on the mac
    cp -p recoll.app/Contents/MacOS/recoll recoll
fi
