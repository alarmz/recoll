#!/usr/bin/env python3
# Copyright (C) 2016 J.F.Dockes
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
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

# Recoll DJVU extractor

import os
import sys
import rclexecm
import subprocess
import codecs

from rclbasehandler import RclBaseHandler


_BOMLEN = len(codecs.BOM_UTF8)


class DJVUExtractor(RclBaseHandler):

    def __init__(self, em):
        super(DJVUExtractor, self).__init__(em)
        self.djvutxt = rclexecm.which("djvutxt")
        if not self.djvutxt:
            print("RECFILTERROR HELPERNOTFOUND djvutxt")
            sys.exit(1)
        self.djvused = rclexecm.which("djvused")

    def html_text(self, fn):
        self.em.setmimetype("text/html")
        fn = rclexecm.subprocfile(fn)
        # Extract metadata
        metadata = b""
        if self.djvused:
            try:
                metadata = subprocess.check_output(
                    [
                        self.djvused,
                        fn,
                        "-u",
                        "-e",
                        "select;print-meta;select 1;print-meta",
                    ]
                )
            except Exception as e:
                self.em.rclog(f"djvused failed: {e}")
        fields = (
            b"author",
            b"title",
            b"booktitle",
            b"publisher",
            b"editor",
            b"year",
            b"creationdate",
        )
        metadic = {}
        for f in fields:
            metadic[f] = b""
        if metadata.startswith(codecs.BOM_UTF8):
            metadata = metadata[_BOMLEN:]

        for line in metadata.splitlines():
            ll = line.split(b"\t")
            if len(ll) >= 2:
                nm = ll[0].strip().lower()
                if nm in fields:
                    value = (b" ".join(ll[1:])).strip(b' \t"')
                    metadic[nm] += value + b" "
        # self.em.rclog(f"METADIC: {metadic}")

        # Main text
        txtdata = subprocess.check_output([self.djvutxt, fn])

        data = b"<html><head>\n"
        if metadic[b"title"]:
            data += (
                b"<title>"
                + rclexecm.htmlescape(metadic[b"title"].strip())
                + b"</title>\n"
            )
        data += b'<meta http-equiv="Content-Type" content="text/html;charset=UTF-8">\n'
        for nm in [nm for nm in fields if nm != b"title"]:
            if metadic[nm]:
                data += (
                    b'<meta name="'
                    + nm
                    + b'" content="'
                    + rclexecm.htmlescape(metadic[nm].strip())
                    + b'">\n'
                )
        data += b"</head><body><pre>"

        data += rclexecm.htmlescape(txtdata)
        data += b"</pre></body></html>"
        return data


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = DJVUExtractor(proto)
rclexecm.main(proto, extract)
