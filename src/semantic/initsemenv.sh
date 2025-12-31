#!/bin/sh
# Copyright (C) 2025 J.F.Dockes
#
# License: GPL 2.1
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Initialize a venv for the semantic recoll python part.
#
# Temporary: We assume that we are living in a recoll source tree (tar
# or git), and that the recoll python module is installed on the
# system


fatal()
{
    echo $* 1>&2
    exit 1
}
usage()
{
    fatal Usage: initsemenv.sh venvdir
}

test $# = 1 || usage
venvdir=$1


mkdir -p "$venvdir" || exit 1
python3 -m venv "$venvdir" || exit 1
. "$venvdir"/bin/activate
python3 -m pip install chromadb ollama
deactivate

ol=`which ollama`
if test -z "$ol"; then 
    curl -fsSL https://ollama.com/install.sh | sh
    ollama pull nomic-embed-text
fi

cp rclsem_common.py  rclsem_embed.py  rclsem_query.py  rclsem_segment.py  rclsem_talk.py  \
   slicelist.py cmdtalkplugin.py "$venvdir"
(cd "$venvdir";chmod a+x rclsem_embed.py  rclsem_query.py  rclsem_talk.py)

toprecoll=`dirname $0`/..
rclpydir="$toprecoll/python/recoll/recoll"
(cd "$rclpydir";cp conftree.py rclconfig.py "$venvdir")
cp "$toprecoll"/filters/cmdtalk.py "$venvdir"

recollmod=`echo 'from recoll import recoll; print(recoll.__file__)' | python3`
echo recollmod $recollmod
test -z "$recollmod" && fatal recoll module not found
rclmoddir=`dirname $recollmod`
cp -rp "$rclmoddir" "$venvdir"/lib/python*/site-packages
