#!/bin/sh

fatal()
{
    echo $*
    exit 1
}
Usage()
{
    fatal mkinstdir.sh targetdir
}

test $# -eq 1 || Usage

DESTDIR=$1

test -d $DESTDIR || mkdir $DESTDIR || fatal cant create $DESTDIR

# Script to make a prototype recoll install directory from locally compiled
# software. *** Needs msys or cygwin ***

################################
# Local values (to be adjusted)

#WEB=WEBKIT
WEB=WEBENGINE

# Recoll src tree
RCL=/c/users/bill/documents/recoll/src/
PYRECOLL=${RCL}/python/recoll/
# Recoll dependancies
RCLDEPS=/c/users/bill/documents/recolldeps/
LIBXML=${RCLDEPS}/msvc/libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc/libxml2.dll
LIBXSLT=${RCLDEPS}/msvc/libxslt/libxslt-1.1.29/win32/bin.msvc/libxslt.dll
ZLIB=${RCLDEPS}/msvc/zlib-1.2.11
LIBMAGIC=${RCLDEPS}/msvc/libmagic
# Qt
QTA=Desktop_Qt_6_7_3_MSVC2019_64bit-Release
qtsdir=release
QTBIN=C:/Qt/6.7.3/msvc2019_64/bin
# mingwbin has dlls for aux programs built with mingw (wpd, pff, aspell)
MINGWBIN=${RCLDEPS}/gcclibs

# We use the mingw-compiled aspell program for building the dict
ASPELL=${RCLDEPS}/mingw/aspell-0.60.7/aspell-installed
# We use an msvc-built aspell lib for supporting a python extension
# used by the suggestion script
LIBASPELL=${RCLDEPS}/msvc/aspell-0.60.7/


RCLW=$RCL/
GUIBIN=$RCL/qtgui/build/${QTA}/${qtsdir}/recoll.exe
RCLIDX=$RCLW/qmake/build/recollindex/${QTA}/${qtsdir}/recollindex.exe
RCLQ=$RCLW/qmake/build/recollq/${QTA}/${qtsdir}/recollq.exe
RCLS=$RCLW/qmake/build/rclstartw/${QTA}/${qtsdir}/rclstartw.exe
XAPC=$RCLW/qmake/build/xapian-check/${QTA}/${qtsdir}/xapian-check.exe
# Embedded Python version and tree
PYTHONMINOR=12
PYTHON=${RCLDEPS}python-3.12.4-embed-amd64
UNRTF=${RCLDEPS}unrtf
ANTIWORD=${RCLDEPS}antiword
PYEXIV2=${RCLDEPS}pyexiv2
EPUB=${RCLDEPS}epub-0.5.2
FUTURE=${RCLDEPS}python2-future
POPPLER=${RCLDEPS}poppler-22.04.0/
LIBWPD=${RCLDEPS}libwpd/libwpd-0.10.0/
LIBREVENGE=${RCLDEPS}libwpd/librevenge-0.0.1.jfd/
CHM=${RCLDEPS}pychm
MISC=${RCLDEPS}misc
LIBPFF=${RCLDEPS}pffinstall

################
# Script:
FILTERS=$DESTDIR/Share/filters
SHARE=$DESTDIR/Share

fatal()
{
    echo $*
    exit 1
}

# checkcopy. 
chkcp()
{
    echo "Copying $@"
    cp $@ || fatal cp $@ failed
}

copyqt()
{
    cd $DESTDIR
    PATH=$QTBIN:$PATH
    export PATH
    $QTBIN/windeployqt recoll.exe || exit 1

    if test $WEB = WEBKIT ; then
        addlibs="icudt65.dll icuin65.dll icuuc65.dll libxml2.dll libxslt.dll \
          Qt5WebKit.dll Qt5WebKitWidgets.dll"
        for i in $addlibs;do
            chkcp $QTBIN/$i $DESTDIR
        done
    fi
}

# Note that pychm and pyhwp are pre-copied into the python
# distribution directory. The latter also needs olefile and six (also
# copied to the python tree
copypython()
{
    set -x
    mkdir -p ${DESTDIR}/Share/filters/python
    rsync -av $PYTHON/ ${DESTDIR}/Share/filters/python || exit 1
    chkcp $PYTHON/python.exe $DESTDIR/Share/filters/python/python.exe
    chkcp $MISC/hwp5html $FILTERS
}

copyrecoll()
{
    
    chkcp $GUIBIN $DESTDIR
    chkcp $RCLIDX $DESTDIR
    chkcp $RCLQ $DESTDIR 
    chkcp $RCLS $DESTDIR 
    chkcp $ZLIB/zlib1.dll $DESTDIR
    chkcp $XAPC $DESTDIR
    chkcp $LIBXML $DESTDIR
    chkcp $LIBXSLT $DESTDIR

    chkcp $RCL/COPYING                  $DESTDIR/COPYING.txt
    chkcp $RCL/doc/user/usermanual.html $DESTDIR/Share/doc
    chkcp $RCL/doc/user/docbook-xsl.css $DESTDIR/Share/doc
    mkdir -p $DESTDIR/Share/doc/webhelp
    rsync -av $RCL/doc/user/webhelp/docs/* $DESTDIR/Share/doc/webhelp || exit 1
    chkcp $RCL/sampleconf/fields          $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/fragment-buttons.xml  $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimeconf        $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimeview        $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimemap         $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/backends        $DESTDIR/Share/examples
    chkcp $RCL/windows/mimeconf           $DESTDIR/Share/examples/windows
    chkcp $RCL/windows/recoll.conf        $DESTDIR/Share/examples/windows
    chkcp $RCL/windows/mimeview           $DESTDIR/Share/examples/windows
    chkcp $RCL/sampleconf/recoll.conf     $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll.qss      $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-common.qss $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-dark.qss $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-dark.css $DESTDIR/Share/examples

    chkcp $RCL/filters/*       $FILTERS
    # KEEP THESE TWO *AFTER* the above
    chkcp $RCL/python/recoll/recoll/rclconfig.py $FILTERS
    chkcp $RCL/python/recoll/recoll/conftree.py $FILTERS

    rm -f $FILTERS/rclimg*
    rm -f $FILTERS/rclimg $FILTERS/rclimg.py
    chkcp $RCLDEPS/rclimg/rclimg.exe $FILTERS

    chkcp $RCL/qtgui/mtpics/*  $DESTDIR/Share/images
    chkcp $RCL/qtgui/build/${QTA}/${qtsdir}/*.qm $DESTDIR/Share/translations

    chkcp $RCL/desktop/recoll.ico $DESTDIR/Share
}

copyantiword()
{
    bindir=$ANTIWORD/
    test -d $Filters/Resources || mkdir -p $FILTERS/Resources || exit 1
    chkcp  $bindir/antiword.exe            $FILTERS
    rsync -av  $ANTIWORD/Resources/*       $FILTERS/Resources || exit 1
}

copyunrtf()
{
    bindir=$UNRTF/Windows/

    test -d $FILTERS/Share || mkdir -p $FILTERS/Share || exit 1
    chkcp  $bindir/unrtf.exe               $FILTERS
    chkcp  $UNRTF/outputs/*.conf           $FILTERS/Share
    chkcp  $UNRTF/outputs/SYMBOL.charmap   $FILTERS/Share
    # libiconv-2 originally comes from mingw
    chkcp $MISC/libiconv-2.dll $FILTERS
}


# Not used any more, the epub python code is bundled with recoll
copyepub()
{
    cp -rp $EPUB/build/lib/epub $FILTERS
    # chkcp to check that epub is where we think it is
    chkcp $EPUB/build/lib/epub/opf.py $FILTERS/epub
}

# We used to copy the future module to the filters dir, but it is now
# part of the origin Python tree in recolldeps. (2 dirs:
# site-packages/builtins, site-packages/future)
copyfuture()
{
    cp -rp $FUTURE/future $FILTERS/
    cp -rp $FUTURE/builtins $FILTERS/
    # chkcp to check that things are where we think they are
    chkcp $FUTURE/future/builtins/newsuper.pyc $FILTERS/future/builtins
}

# Replaced by mutagen
copypyexiv2()
{
    cp -rp $PYEXIV2/pyexiv2 $FILTERS
    chkcp $PYEXIV2/libexiv2python.pyd $FILTERS/
}

copypoppler()
{
    # Note: the recent poppler build which we ship comes from conda builds, and it includes
    # poppler-data, which has additional fonts and encodings, for, e.g. Chinese. The utilities
    # (e.g. pdftotext) expect to find the data in a directory named shared/poppler, 2 levels above
    # the exec. There is no way that I could find to explicitely designate the data location, so we
    # have to keep the structure of the directories.
    test -d $FILTERS/poppler/Library/bin || mkdir -p $FILTERS/poppler/Library/bin || \
        fatal cant create poppler directory
    for f in pdftotext.exe pdfinfo.exe pdftoppm.exe $POPPLER/Library/bin/*.dll ; do
        chkcp $POPPLER/Library/bin/`basename $f` $FILTERS/poppler/Library/bin/
    done
    cp -rp $POPPLER/share $FILTERS/poppler
}

copywpd()
{
    DEST=$FILTERS/wpd
    test -d $DEST || mkdir $DEST || fatal cant create poppler dir $DEST

    for f in librevenge-0.0.dll librevenge-generators-0.0.dll \
             librevenge-stream-0.0.dll; do
        chkcp $LIBREVENGE/src/lib/.libs/$f $DEST
    done
    chkcp $LIBWPD/src/lib/.libs/libwpd-0.10.dll $DEST
    chkcp $LIBWPD/src/conv/html/.libs/wpd2html.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST
    chkcp $MINGWBIN/zlib1.dll $DEST
}

copychm()
{
    DEST=$FILTERS
    cp -rp $CHM/chm $DEST || fatal "can't copy pychm"
}

copypff()
{
    DEST=$FILTERS
    cp -rp $LIBPFF $DEST || fatal "can't copy $LIBPFF"
    DEST=$DEST/pffinstall/mingw32/bin
    chkcp $LIBPFF/mingw32/bin/pffexport.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST
}

copymagic()
{
    chkcp $LIBMAGIC/magic/magic.mgc $SHARE
}

copyaspell()
{
    DEST=$FILTERS
    cp -rp $ASPELL $DEST || fatal "can't copy $ASPELL"
    DEST=$DEST/aspell-installed/mingw32/bin
    # Check that we do have an aspell.exe.
    chkcp $ASPELL/mingw32/bin/aspell.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST

    # Build and install the Python aspell expansion. We use the normal
    # Python install as the embedded one does not have the build files
    # (.h etc.).
    pushd ${RCL}/python/pyaspell
    "/c/Program Files/Python3${PYTHONMINOR}/python" setup-win.py bdist_wheel || exit 1
    PYASPDIST=dist/aspell_python_py3-1.15-cp3${PYTHONMINOR}-cp3${PYTHONMINOR}-win_amd64.whl
    ${DESTDIR}/Share/filters/python/python -m pip install --no-user ${PYASPDIST} || exit 1
    chkcp $LIBASPELL/build/${QTA}/${qtsdir}/aspell.dll \
          ${DESTDIR}/Share/filters/python/lib/site-packages
    popd
}

# Recoll python package. Only when compiled with msvc as this is what
# the standard Python dist is built with
copypyrecoll()
{
    # e.g. build: "/c/Program Files (x86)/Python39-32/python" setup-win.py bdist_wheel

    # NOTE: the python 3.10 build outputs logging error messages
    # (ValueError: underlying buffer has been detached), but this does
    # not seem to affect the build
    DEST=${DESTDIR}/Share/dist
    test -d $DEST || mkdir $DEST || fatal cant create $DEST
    rm -f ${DEST}/Recoll*.egg ${DEST}/Recoll*.whl
    for v in 10 11 12;do
        PYRCLDIST=${PYRECOLL}/dist/Recoll-${VERSION}-cp3${v}-cp3${v}-win_amd64.whl
        if test ! -f ${PYRCLDIST}; then
            pushd ${PYRECOLL}
            # NOTE: with recent Python versions you need to install the wheel module for this
            # to work: xxx/python -m pip install setuptools wheel
            "/c/Program Files/Python3${v}/python" setup-win.py bdist_wheel
            popd
        fi
        chkcp ${PYRCLDIST} $DEST
        # If this is the right version for our embedded python, install the extension
        #(needed, e.g. for the Joplin indexer).
        if test "$v" = "$PYTHONMINOR";then
            ${DESTDIR}/Share/filters/python/python -m pip install --no-user ${PYRCLDIST} || exit 1
        fi
    done
}

# First check that the config is ok
diff -q $RCL/common/autoconfig.h $RCL/common/autoconfig-win.h || \
    fatal autoconfig.h and autoconfig-win.h differ
VERSION=`cat $RCL/RECOLL-VERSION.txt`
CFVERS=`grep PACKAGE_VERSION $RCL/common/autoconfig.h | \
cut -d ' ' -f 3 | sed -e 's/"//g'`
test "$VERSION" = "$CFVERS" ||
    fatal Versions in RECOLL-VERSION.txt and autoconfig.h differ


echo Packaging version $CFVERS

for d in doc examples examples/windows filters images translations; do
    test -d $DESTDIR/Share/$d || mkdir -p $DESTDIR/Share/$d || \
        fatal mkdir $d failed
done

# copyrecoll must stay before copyqt so that windeployqt can do its thing
copyrecoll
copyqt
copypython
copypoppler
copyantiword
copyunrtf
copywpd
copypff
copyaspell
copypyrecoll
copymagic

echo "MKINSTDIR OK"
