#!/usr/bin/python3
# Copyright (C) 2020 J.F.Dockes
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import sys
import hashlib
import base64

import xapian
import rclconfig


# The following 2 methods replicate the file UDI computation in fileudi.cpp:
# process a file path so that it is usable (short enough) for use as a Xapian
# term. We truncate and replace the truncated part by its md5 hash.
def pathHash(path, maxlen):
    HASHLEN = 22
    if len(path) <= maxlen:
        return path
    digest = hashlib.md5(path[maxlen - HASHLEN :]).digest()
    adigest = base64.b64encode(digest)
    adigest = adigest[:-2]
    return path[: maxlen - HASHLEN] + adigest


def make_udi(path, ipath=b""):
    PATHHASHLEN = 150
    path += b"|"
    path += ipath
    return pathHash(path, PATHHASHLEN)


# Return xapian posting list contents as Python list
def get_postlist(xdb, term):
    ret = list()
    for posting in xdb.postlist(term):
        ret.append(posting.docid)
    return ret


if __name__ == "__main__":
    if len(sys.argv) != 1:
        print("Usage: rclnotindexed", file=sys.stderr)
        print(" reads paths on stdin", file=sys.stderr)
        sys.exit(1)

    config = rclconfig.RclConfig()
    dbdir = config.getDbDir()
    xdb = xapian.Database(dbdir)

    for line in sys.stdin.buffer:
        path = line.rstrip(b"\n")
        udi = make_udi(path)
        term = b"Q" + udi
        postlist = get_postlist(xdb, term)
        if len(postlist) == 0:
            sys.stdout.buffer.write(path + b"\n")
        elif len(postlist) > 1:
            sys.stdout.buffer.write("!! Multiple document entries for: " + path + b"\n")
            sys.exit(1)
