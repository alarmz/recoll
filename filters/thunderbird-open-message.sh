#!/bin/sh
#
# Create a symbolic link with a .eml extension to convince thunderbird
# that it should open the target file as a message.

fatal()
{
    echo $*
    exit 1
}
usage()
{
    fatal Usage: `basename $0` '<path to message/rfc822 file>'
}

tempdir=/tmp/recoll-nonexistent
cleanup()
{
    rm -rf -- "$tempdir"
}

test $# -eq 1 || usage
message="$1"

tempdir="`mktemp -d`"
test -d "$tempdir" || exit 1

trap cleanup EXIT

lnk="$tempdir/message.eml"
ln -s "$message" "$lnk"

# set -x
thunderbird --profile "$tempdir" --no-remote --new-instance -file "$lnk"
