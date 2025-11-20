#!/bin/sh

# Note: this supposes that all used python version have been built
# from source and installed with make altinstall, and that a
# py3versions program is found in the PATH before the system one, and
# outputs the desired versions. E.g.:
# #!/bin/sh
# echo 3.8 3.9 3.10 3.11 3.12 3.13


DOCKHOME=/home/dockes/tmp/distrohomes/deb11

FROMGIT=0
RECOLL_VERSION=1.43.7
if test $FROMGIT -ne 0; then
    RECOLL=/home/dockes/projets/fulltext/recoll/src
else
    RECOLL=$DOCKHOME/recoll-${RECOLL_VERSION}
fi
UNRTF=$DOCKHOME/unrtf-0.21.11
ANTIWORD=$DOCKHOME/antiword

APPDIR=$DOCKHOME/AppDir/
DEPLOYBINDIR=$DOCKHOME/.local/bin
BUILDDIR=$DOCKHOME/build

auxprogs()
{
    # This works because:
    # - We change the apps to find their resources relative to the exec. Ok for antiword and unrtf
    # - We make sure that the PATH is ok for the handlers to find them in the mounted bin

    echo "Building UNRTF"
    cd $UNRTF
    ./configure --prefix=/usr
    make -j 4 || exit 1
    make install DESTDIR=$APPDIR || exit 1

    echo "Building ANTIWORD"
    cd $ANTIWORD
    make -f Makefile.Linux
    cp antiword $APPDIR/usr/bin/ || exit 1
    mkdir -p $APPDIR/usr/share/antiword || exit 1
    cp -rp Resources/*.txt Resources/fontnames $APPDIR/usr/share/antiword/ || exit 1
}

rm -rf ${APPDIR}/*

cd $RECOLL/

if test $FROMGIT -ne 0; then
    hash=`git log -n 1 | head -1 | awk '{print $2}' | cut -b 1-8`
fi
RECOLL_VERSION=`cat RECOLL-VERSION.txt`

# The -Dappimage=true is not used after 2025-11-20, but needed for recoll-1.43.7
meson setup --prefix=/usr -Dappimage=true ${BUILDDIR}
ninja -C ${BUILDDIR}

echo;echo INSTALLING TO $APPDIR
DESTDIR=$APPDIR ninja -C ${BUILDDIR} install || exit 1

echo;echo ALSO INSTALLING TO /usr FOR PYTHON BUILDS
sudo ninja -C ${BUILDDIR} install || exit 1

echo;Copying Python binary extension shared objects
cp ${BUILDDIR}/python/recoll/*.so $APPDIR/usr/lib/python3/dist-packages/recoll || exit 1
cp ${BUILDDIR}/python/pychm/*.so $APPDIR/usr/lib/python3/dist-packages/recollchm || exit 1
cp ${BUILDDIR}/python/pyaspell/*.so $APPDIR/usr/lib/python3/dist-packages/ || exit 1

auxprogs

cd
$DEPLOYBINDIR/linuxdeploy-x86_64.AppImage --appdir $APPDIR --output appimage --plugin qt
dte=`date +%Y%m%d`
mv Recoll-x86_64.AppImage Recoll-${RECOLL_VERSION}-${dte}-${hash}-x86_64.AppImage
