#!/bin/sh

# Note: this supposes that all used python version have been built
# from source and installed with make altinstall

DOCKHOME=/home/dockes/tmp/distrohomes/deb10
RECOLL=$DOCKHOME/recoll
UNRTF=$DOCKHOME/unrtf
ANTIWORD=$DOCKHOME/antiword
APPDIR=$DOCKHOME/AppDir/
DEPLOYBINDIR=$DOCKHOME


auxprogs()
{
    # This would need that:
    # - We change the apps to find their resources relative to the exec (probably almost done,
    #   check on Windows ?
    # - We make sure that the PATH is ok for the handlers to find them in the mounted bin

    echo "Building UNRTF"
    cd $UNRTF
    make -j 4 || exit 1
    make install DESTDIR=$APPDIR || exit 1

    echo "Building ANTIWORD"
    cd $ANTIWORD
    make -f Makefile.Linux
    cp antiword $APPDIR/usr/bin/ || exit 1
    mkdir -p $APPDIR/usr/share/antiword || exit 1
    cp -rp Resources/*.txt Resources/fontnames $APPDIR/usr/share/antiword/ || exit 1
}

cd $RECOLL/src
hash=`git log -n 1 | head -1 | awk '{print $2}' | cut -b 1-8`
vers=`cat RECOLL-VERSION.txt`
meson setup --prefix=/usr build
ninja -C build
DESTDIR=$APPDIR ninja -C build install || exit 1
sudo ninja -C build install || exit 1
for pyversion in 3.8 3.9 3.10 3.11 3.12;do
    for nm in recoll chm aspell; do
        case $nm in
            recoll) srcdir=recoll;modir=recoll;;
            chm) srcdir=pychm;modir=recollchm;;
            aspell) srcdir=pyaspell;modir='';;
        esac
        cd $RECOLL/src/python/$srcdir
        #rm -rf build/
        python$pyversion setup.py build || exit 1
        if test $nm = aspell;then
            cp build/lib*/*.so $APPDIR/usr/lib/python3/dist-packages/ || exit 1
        else
            cp build/lib*/*/*.so $APPDIR/usr/lib/python3/dist-packages/$modir || exit 1
        fi
    done
done

auxprogs

cd
$DEPLOYBINDIR/linuxdeploy-x86_64.AppImage --appdir $APPDIR --output appimage --plugin qt
dte=`date +%Y%m%d`
mv Recoll-x86_64.AppImage Recoll-${vers}-${dte}-${hash}-x86_64.AppImage
