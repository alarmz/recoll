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
from datetime import datetime

sys.path.append("/usr/share/recoll/filters")
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


# Retrieve named value from document data record.
# The record format is a sequence of nm=value lines
def get_attributes(xdb, docid, flds, decode=True):
    doc = xdb.get_document(docid)
    data = doc.get_data()
    res = []
    for fld in flds:
        s = data.find(fld.encode("utf-8") + b"=")
        if s == -1:
            res.append(None)
        else:
            e = data.find(b"\n", s)
            if decode:
                res.append(data[s + len(fld) + 1 : e].decode("UTF-8"))
            else:
                res.append(data[s + len(fld) + 1 : e])
    return res


def msg(s):
    print("%s" % s, file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: rcldocdata <path>", file=sys.stderr)
        sys.exit(1)

    # Assuming utf-8 locale here...
    upath = sys.argv[1]
    path = upath.encode("utf-8")

    config = rclconfig.RclConfig()
    dbdir = config.getDbDir()
    xdb = xapian.Database(dbdir)

    # Find document by looking up its UDI unique term
    udi = make_udi(path)
    term = b"Q" + udi
    postlist = get_postlist(xdb, term)

    if len(postlist) == 0:
        msg("No entry found for %s" % upath)
        sys.exit(1)
    elif len(postlist) > 1:
        msg("Multiple document entries for %s" % upath)
        sys.exit(1)

    docid = postlist[0]
    res = get_attributes(xdb, docid, ["sig", "fbytes"])
    msg("Document signature %s file size %s" % (res[0], res[1]))
    fbstr = res[1]
    tmstr = res[0][len(fbstr) :]
    ts = int(tmstr)
    msg("Doc ref uxtime: %s" % tmstr)
    msg("Doc ref date %s" % datetime.utcfromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S"))
