#!/usr/bin/env python3
# Copyright (C) 2022 J.F.Dockes
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

# Indexer for "SingleFileZ" files. These are single-file web archives stored in a file with a
# .zip.html extension, which manages to be both sort of an html and sort of a zip
# file. Unfortunately the Python zip interface does not like the hack so that we have to use an
# unzip or 7z command to extract the index.html member.

import rclexecm
from rclbasehandler import RclBaseHandler
import sys
import subprocess

unzip = None
sevenz = None


class SFZExtractor(RclBaseHandler):
    def __init__(self, em):
        super(SFZExtractor, self).__init__(em)

    def html_text(self, fn):
        if unzip:
            cmd = [unzip, "-p", rclexecm.subprocfile(fn), "index.html"]
        else:
            cmd = [sevenz, "e", "-so", rclexecm.subprocfile(fn), "index.html"]

        data = subprocess.check_output(cmd)
        return data


if __name__ == "__main__":
    unzip = rclexecm.which("unzip")
    if not unzip:
        sevenz = rclexecm.which("7z")
        if not sevenz:
            print("RECFILTERROR HELPERNOTFOUND unzip|7z")
            sys.exit(1)
    proto = rclexecm.RclExecM()
    extract = SFZExtractor(proto)
    rclexecm.main(proto, extract)
