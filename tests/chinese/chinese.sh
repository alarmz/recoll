#!/bin/sh
# Testing Chinese tokenisation with Jieba
# See notes/chinese-jieba.txt, copy of message from chinese expert

LANG=en_US.UTF-8
export LANG

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

RECOLL_CONFDIR=$topdir/chinese
export RECOLL_CONFDIR
toptmp=${TMPDIR:-/tmp}/recolltsttmp

cat > $RECOLL_CONFDIR/recoll.conf <<EOF
loglevel = 6
logfilename = ${toptmp}/logchinese

topdirs = $tstdata/cjk/chinese-samples.html
chinesetagger = Jieba
noaspell = 1

EOF
cp /dev/null $RECOLL_CONFDIR/backends

recollindex -z 2> /dev/null

(
    # This is quite slow because of the repeated loading of the Jieba
    # model. It would be better to change this into a python program
    # running multiple requests
    
    echo expect 1 result    
    recollq 生命
    recollq 里面
    recollq 常常
    recollq 碰到
    recollq 愿意
    recollq 演员
    recollq 分享
    recollq 自己
    recollq 角色

    echo expect 0 result
    recollq 在生
    recollq 命里
    recollq 面你
    recollq 你常
    recollq 会碰
    recollq 到不

)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

checkresult
