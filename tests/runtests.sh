#!/bin/sh

isLinux=0
isWindows=0
sys=`uname`
case $sys in
    Linux) isLinux=1;;
    MINGW*|MSYS*) isWindows=1;;
esac

iswindows()
{
    if test $isWindows -eq 1; then
        return 0
    fi
    return 1
}

       
fatal()
{
    echo Error: $*;exit 1
}

rerootResults()
{
    savedcd=`pwd`
    dirs=`ls -F | grep / | grep -v non-auto | grep -v config`
    for dir in $dirs ; do
    	cd $dir
	resfile=`basename $dir`.txt
	sed -i.bak \
	  -e "s!file:///.*/testrecoll/!file://$RECOLL_TESTDATA/!g" \
	  $resfile
    	cd ..
    done

    cd $RECOLL_CONFDIR
    sed -i.bak \
  -e "s!/.*/testrecoll/!$RECOLL_TESTDATA/!g" \
      mimemap

    cd $savedcd
}

testexe()
{
    _tx_cmd=$1
    if test -d "$_tx_cmd"; then
        return 1
    fi
    if test -x "$_tx_cmd"; then
        iscmdresult="$_tx_cmd"
        return 0
    else
        if iswindows; then
            if test -x "$_tx_cmd".exe; then
                iscmdresult="$_tx_cmd".exe
                return 0
            fi
        fi
    fi
    return 1
}

iscmd()
{
    _ic_cmd=$1
    case "$_ic_cmd" in
        */*)
	    if testexe "$_ic_cmd"; then
                return 0
            fi;;
        *)
            oldifs=$IFS; IFS=":"; set -- $PATH; IFS=$oldifs
            for d in "$@";do
                if testexe "$d/$_ic_cmd"; then
                    return 0
                fi
            done
    esac
    return 1
}

checkcmds()
{
    result=0
    for cmd in $*;do
      if iscmd $cmd 
      then 
        echo $cmd is $iscmdresult
      else 
        echo $cmd not found
        result=1
      fi
    done
    return $result
}

makeindex() {
    echo "Zeroing Index"
    for dir in "$RECOLL_TESTCACHEDIR" "$RECOLL_CONFDIR";do
        rm -rf "$dir"/xapiandb "$dir"/aspdict.*.rws "$dir"/ocrcache
    done
  echo "Indexing" 
  recollindex -c $RECOLL_CONFDIR -z
}

if test ! -f shared.sh ; then
    fatal must be run in the top test directory
fi

# We now want RECOLL_TESTDATA to be set always. No more relying on the default path
test -n "$RECOLL_TESTDATA" || fatal RECOLL_TESTDATA is not set
test -n "$TMPDIR" || fatal TMPDIR is not set

if iswindows; then
    checkcmds recollq || exit 1
    if test x$noindex = x ; then
        checkcmds recollindex || exit 1
    fi
    export MSYS2_ARG_CONV_EXCL='*'
    # badsuffs[1] notypes onlynames: tests need 'file' for mime id. onlynames is easy to fix.
    # casediac, cjk koi8r: needs utf8 on the command line
    # compressed: weird unix stuff
    # info kar kword lyx djvu dvi Maildir Maildir1 man purple scribus:
    #   need unix command (info) or code (rclmidi) missing or filter is shell-script etc.
    # empty: fails because of differing dir sizes (0 vs 4096), not worth fixing
    # nonumbers: fails because of paths and testing on unix is enough
    # pdf-annots: needs the poppler glib bindings
    # pdfattach: needs pdftk
    # postscript: needs ghostscript
    # program: issues with suffix-less files
    # pythonapi: would need a lot of porting. Design windows-specific one?
    # xattr: unix-specific
    # xml: several files are also compressed or have bad suffixes
    excluded="non-auto casediac cjk compressed info kar koi8r kword lyx \
              djvu dvi Maildir Maildir1 man empty nonumbers pdf-annots \
              pdf-ocr pdfattach postscript purple pythonapi scribus xattr xml"
else
    checkcmds recollq pxattr xadump pdftk || exit 1
    if test x$noindex = x ; then
        checkcmds recollindex || exit 1
    fi
    excluded="non-auto"
    iscmd pdftk
    pdftk=$iscmdresult
    tmpdir=${RECOLL_TMPDIR:-$TMPDIR}
    case "$pdftk" in
        /snap/*)
            if test X$tmpdir = X -o "$tmpdir" = /tmp;then
                fatal pdftk as snap need '$TMPDIR' to belong to you
            fi
            ;;
    esac
    if test ! x$reroot = x ; then
        rerootResults
    fi
    # Because of the snap thing
    export TMPDIR=$HOME/tmp
fi


# Temp directory for test results
# Make sure this is computed in the same way as in shared.sh
toptmp=${TMPDIR:-/tmp}/recolltsttmp

test X"$toptmp" = X && fatal "empty toptmp??"
test X"$toptmp" = X/ && fatal "toptmp == / ??"
if test -d "$toptmp" ; then
   rm -f $toptmp/*.diffs $toptmp/*.err  $toptmp/*.out
fi
mkdir -p $toptmp || fatal cant create temp dir $toptmp

# Unset DISPLAY because xdg-mime may be affected by the desktop
# environment on the X server
unset DISPLAY
export LC_ALL=en_US.UTF-8

RECOLL_TESTS=`pwd`
if iswindows; then
    RECOLL_TESTS=`echo $RECOLL_TESTS | sed -e 's,/c,c:,'`
fi
RECOLL_TESTDATA=${RECOLL_TESTDATA:-/home/dockes/projets/fulltext/testrecoll}
RECOLL_TESTDATA=`echo $RECOLL_TESTDATA | sed -e 's!/$!!'`
export RECOLL_CONFDIR=$RECOLL_TESTS/config/
# Some tests need to access RECOLL_TESTCACHEDIR
export RECOLL_TESTCACHEDIR=$toptmp

sed -e "s,@RECOLL_TESTS@,$RECOLL_TESTS,g" \
    -e "s,@RECOLL_TESTDATA@,$RECOLL_TESTDATA,g" \
    -e "s,@TMPDIR@,$TMPDIR,g" \
    -e "s,@RECOLL_TESTCACHEDIR@,$RECOLL_TESTCACHEDIR,g" \
    < $RECOLL_CONFDIR/recoll.conf.in \
    > $RECOLL_CONFDIR/recoll.conf || exit 1
sed -e "s,@RECOLL_TESTS@,$RECOLL_TESTS,g" \
    -e "s,@RECOLL_TESTDATA@,$RECOLL_TESTDATA,g" \
    -e "s,@RECOLL_TESTCACHEDIR@,$RECOLL_TESTCACHEDIR,g" \
    < $RECOLL_CONFDIR/mimemap.in \
    > $RECOLL_CONFDIR/mimemap || exit 1

if test x$noindex = x ; then
  makeindex
fi


dirs=`ls -F | grep / | grep -v config`

echo
echo "Running query tests:"

for dir in $dirs ; do
    skip=0
    for excl in $excluded;do
        if test "$dir" = "$excl"/; then
            skip=1
            break
        fi
    done
    if test "$skip" -eq 1;then
        continue
    fi
    cd $dir && echo -n "$dir "
    sh `basename $dir`.sh
    cd ..
done

echo

if iswindows ; then
    exit 0
fi

if test x$doraw = x ; then
    exit 0
fi

echo "Reindexing in raw index mode"
sed -i -e 's/indexStripChars = 1/indexStripChars = 0/' config/recoll.conf
makeindex
excluded="non-auto empty html langparser1 mail Maildir notypes ooff pythonapi rfc2231 skipped \
          unacex xls"
for dir in $dirs ; do
    skip=0
    for excl in $excluded;do
        if test "$dir" = "$excl"/; then
            skip=1
            break
        fi
    done
    if test "$skip" -eq 1;then
        continue
    fi
    cd $dir && echo -n "$dir "
    sh `basename $dir`.sh
    cd ..
done
echo
