#!/bin/sh
# Packages needed
# sudo apt-get install g++ gnupg dput lintian mini-dinstall yaclc bzr devscripts
# For the kio: (and kdesdk?)
# sudo apt-get install pkg-kde-tools  cdbs

# Active series:
# 20.04LTS focal    2025-06
# 22.04LTS jammy    2027-06
# 24.04LTS noble    2029-06
# 24.10    oracular 2025-07
SERIES="focal jammy noble oracular"

PPA_KEYID=7808CE96D38B9201

RCLVERS=1.41.1
GSSPVERS=1.1.3
PPAVERS=1

PPANAME=recoll15-ppa
#PPANAME=recollexp-ppa

# recoll 
series_rcl="focal jammy noble oracular"
# KIO
series_kio=
# krunner: does not build on focal and bionic because of the 5.90 min version requested in the
# CMakeLists.txt. Did not try to work around.
series_krunner=
# gssp
series_gssp=

txt=""
test -n "$series_rcl" && txt="$txt Recoll $RCLVERS on $series_rcl "
test -n "$series_kio" && txt="$txt KIO $RCLVERS on $series_kio "
test -n "$series_krunner" && txt="$txt Krunner $RCLVERS on $series_krunner "
test -n "$series_gssp" && txt="$txt GSSP $GSSPVERS on $series_gssp "
echo Building $txt
echo PPA: $PPANAME. PPAVERS $PPAVERS. Type CR if Ok, else ^C
read rep

#
#Y=/y
Y=
RCLSRC=${Y}/home/dockes/projets/fulltext/recoll/src
GSSPSRC=${Y}/home/dockes/projets/fulltext/gssp-recoll
RCLDOWNLOAD=${Y}/home/dockes/projets/fulltext/web-recoll

fatal()
{
    echo $*; exit 1
}

check_recoll_orig()
{
    if test ! -f recoll_${RCLVERS}.orig.tar.gz ; then 
        cp -p $RCLDOWNLOAD/recoll-${RCLVERS}.tar.gz recoll_${RCLVERS}.orig.tar.gz || \
            fatal "Can find neither recoll_${RCLVERS}.orig.tar.gz nor recoll-${RCLVERS}.tar.gz"
    fi
}

####### recoll 

debdir=debian
topdir=recoll-${RCLVERS}
if test "X$series_rcl" != X ; then
    check_recoll_orig
    test -d $topdir || tar xzf recoll_${RCLVERS}.orig.tar.gz
fi

for svers in ${series_rcl} ; do

  rm -rf $topdir/debian
  cp -rp ${debdir}/ $topdir/debian || exit 1

  if test -f $debdir/control-$svers ; then
      cp -f -p $debdir/control-$svers $topdir/debian/control
  fi
  if test -f $debdir/rules-$svers ; then
      cp -f -p $debdir/rules-$svers $topdir/debian/rules
  fi
  if test -f $debdir/python3-recoll.install-$svers ; then
     cp -f -p $debdir/python3-recoll.install-$svers $topdir/debian/python3-recoll.install
  fi
  if test -f $debdir/patches/series-$svers ; then
      cp -f -p $debdir/patches/series-$svers $topdir/debian/patches/series
  fi

  sed -e s/SERIES/${svers}/g -e s/PPAVERS/${PPAVERS}/g \
      < ${debdir}/changelog > $topdir/debian/changelog

  (cd $topdir;debuild -d -k$PPA_KEYID -S -sa)  || break

  dput $PPANAME recoll_${RCLVERS}-1~ppa${PPAVERS}~${svers}1_source.changes
done



### KIO.

debdir=debiankio
topdir=kio-recoll-${RCLVERS}
if test "X$series_kio" != X ; then
    check_recoll_orig
    if test ! -f kio-recoll_${RCLVERS}.orig.tar.gz ; then
        cp -p recoll_${RCLVERS}.orig.tar.gz kio-recoll_${RCLVERS}.orig.tar.gz || exit 1
    fi
    if test ! -d $topdir ; then 
        mkdir -p temp
        cd temp
        tar xzf ../recoll_${RCLVERS}.orig.tar.gz || exit 1
        mv recoll-${RCLVERS} ../$topdir || exit 1
        cd ..
    fi
fi
for svers in ${series_kio} ; do
    rm -rf $topdir/debian
    cp -rp ${debdir}/ $topdir/debian || exit 1
    if test -f $debdir/control-$svers ; then
        cp -f -p $debdir/control-$svers $topdir/debian/control
    else 
        cp -f -p $debdir/control $topdir/debian/control
    fi
    if test -f $debdir/patches/series-$svers ; then
        cp -f -p $debdir/patches/series-$svers $topdir/debian/patches/series
    fi
    sed -e s/SERIES/$svers/g -e s/PPAVERS/${PPAVERS}/g \
        < ${debdir}/changelog > $topdir/debian/changelog ;
    (cd $topdir;debuild -k$PPA_KEYID -S -sa) || exit 1
    dput $PPANAME kio-recoll_${RCLVERS}-1~ppa${PPAVERS}~${svers}1_source.changes
done

### Krunner plugin.

debdir=debiankrunner
topdir=krunner-recoll-${RCLVERS}
if test "X$series_krunner" != X ; then
    check_recoll_orig
    if test ! -f krunner-recoll_${RCLVERS}.orig.tar.gz ; then
        cp -p recoll_${RCLVERS}.orig.tar.gz krunner-recoll_${RCLVERS}.orig.tar.gz || exit 1
    fi
    if test ! -d $topdir ; then 
        mkdir -p temp
        cd temp
        tar xzf ../recoll_${RCLVERS}.orig.tar.gz || exit 1
        mv recoll-${RCLVERS} ../$topdir || exit 1
        cd ..
    fi
fi
for svers in ${series_krunner} ; do
    rm -rf $topdir/debian
    cp -rp ${debdir}/ $topdir/debian || exit 1
    if test -f $debdir/control-$svers ; then
        cp -f -p $debdir/control-$svers $topdir/debian/control
    else 
        cp -f -p $debdir/control $topdir/debian/control
    fi
    sed -e s/SERIES/$svers/g -e s/PPAVERS/${PPAVERS}/g \
        < ${debdir}/changelog > $topdir/debian/changelog ;
    (cd $topdir;debuild -k$PPA_KEYID -S -sa) || exit 1
    dput $PPANAME krunner-recoll_${RCLVERS}-1~ppa${PPAVERS}~${svers}1_source.changes
done

### GSSP

debdir=debiangssp
if test ! -d ${debdir}/ ; then
    rm -f ${debdir}
    ln -s ${GSSPSRC}/debian $debdir
fi
topdir=gssp-recoll-${GSSPVERS}
dload=$RCLDOWNLOAD/downloads
if test "X$series_gssp" != X ; then
    if test ! -f gssp-recoll_${GSSPVERS}.orig.tar.gz ; then 
        if test -f gssp-recoll-${GSSPVERS}.tar.gz ; then
          mv gssp-recoll-${GSSPVERS}.tar.gz gssp-recoll_${GSSPVERS}.orig.tar.gz
        else
          if test -f $dload/gssp-recoll-${GSSPVERS}.tar.gz;then
                cp -p $dload/gssp-recoll-${GSSPVERS}.tar.gz \
                   gssp-recoll_${GSSPVERS}.orig.tar.gz || fatal copy
            else
                fatal "Can find neither gssp-recoll_${GSSPVERS}.orig.tar.gz nor " \
                      "$dload/gssp-recoll-${GSSPVERS}.tar.gz"
            fi
        fi
    fi
    test -d $topdir || tar xzf gssp-recoll_${GSSPVERS}.orig.tar.gz || exit 1
fi
for svers in ${series_gssp} ; do

    rm -rf $topdir/debian
    cp -rp ${debdir}/ $topdir/debian || exit 1

    sed -e s/SERIES/$svers/g  -e s/PPAVERS/${PPAVERS}/g \
        < ${debdir}/changelog > $topdir/debian/changelog ;

    (cd $topdir;debuild -k$PPA_KEYID -S -sa) || break

    dput $PPANAME gssp-recoll_${GSSPVERS}-1~ppa${PPAVERS}~${svers}1_source.changes
done
