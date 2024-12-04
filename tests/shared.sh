# @(#$Id: shared.sh,v 1.4 2009-01-06 18:47:33 dockes Exp $  (C) 2006 J.F.Dockes
# shared code and variables for all tests

isLinux=0
isWindows=0
sys=`uname`
case $sys in
    Linux) isLinux=1;;
    MSYS*|MINGW*) isWindows=1;;
esac

iswindows()
{
    if test $isWindows -eq 1; then
        return 0
    fi
    return 1
}

# The test data location which is recorded in the test results. New tests will need to be edited
# for comparison if the actual/current location differs.
RECOLL_TESTDATA_BASE=/home/dockes/projets/fulltext/testrecoll

# Current test data location for this run
RECOLL_TESTDATA=${RECOLL_TESTDATA:-/home/dockes/projets/fulltext/testrecoll}
RECOLL_TESTDATA=`echo $RECOLL_TESTDATA | sed -e 's!/$!!'`
TESTLOC_CHANGED=`expr "$RECOLL_TESTDATA" '!=' "$RECOLL_TESTDATA_BASE"`
#echo TESTLOC_CHANGED $TESTLOC_CHANGED

# All source'rs should set topdir as a relative path from their location to
# this directory. Computing RECOLL_CONFDIR this way allows to rerun an
# individual test from its directory.
topdir=${topdir:-.}

export RECOLL_CONFDIR=$topdir/config/

ECHON="/bin/echo -n"

# Call this with the script's $0 as argument
initvariables() {
    tstdata=${RECOLL_TESTDATA}
    tstdataindir=$tstdata
    if iswindows; then
        # tstdata path when used in dir clauses. Need to change c: to /c
        tstdataindir=`echo $tstdata | sed -e 's,c:,/c,'`
        export MSYS2_ARG_CONV_EXCL='*'
    fi
    toptmp=${TMPDIR:-/tmp}/recolltsttmp
    myname=`basename $1 .sh`
    mystderr=$toptmp/${myname}.err
    mystdout=$toptmp/${myname}.out
    mydiffs=$toptmp/${myname}.diffs
}

fatal () {
    set -f
    echo
    echo $*
    exit 1
}

checkresult() {
#    set -x
    if test -n "$WINDIR"; then
        # because file:///c:... : one more / after //
        ADDSLASH=/
    else
        ADDSLASH=""
    fi
    tocompare=$mystdout
    temp=""
    if test "$TESTLOC_CHANGED" -ne 0; then
        temp=`mktemp`
        if test -z "$temp";then
            exit 1
        fi
        sed -e "s!${ADDSLASH}${RECOLL_TESTDATA}!${RECOLL_TESTDATA_BASE}!g" "$mystdout" > "$temp"
        tocompare=$temp
    fi
       
    diff -u -w "${myname}.txt" "$tocompare" > $mydiffs 2>&1

    if test X"$temp" != X; then
        rm "$temp"
    fi
    
    if test -s "$mydiffs" ; then
        fatal '*** ' $myname FAILED
    else
        rm -f $mydiffs

        # for tests with a local index. Will need adjusting if some files need to be kept.
        rm -rf history idxstatus.txt index.pid missing recoll.conf xapiandb mimeview backends
        rm -rf aspdict.* ptrans
        exit 0
    fi
}

