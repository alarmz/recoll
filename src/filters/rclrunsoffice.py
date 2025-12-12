#!/usr/bin/env python3

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

# This is used to encapsulate the soffice conversion function in something which will simply return
# or print out data. Soffice wants to write to a file inside a target directory, which is not
# convenient for what we do.

import rclexecm
import sys
import subprocess
import os

class SofficeRunner(object):
    def __init__(self, sofficecmd):
        self.tmpdir = rclexecm.SafeTmpDir("rclrsoff")
        self.cmdbase = [sofficecmd, "--norestore", "--safe-mode", "--headless",
                        "--convert-to", "html", "--outdir"]

    def runsoffice(self, inpath):
        if isinstance(inpath, str):
            inpath = inpath.encode("UTF-8")
        self.tmpdir.vacuumdir()
        cmd = self.cmdbase + [self.tmpdir.getpath(), inpath]
        try:
            subprocess.check_call(cmd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
            infn = os.path.basename(inpath)
            inbase = os.path.splitext(infn)[0]
            htmlfn = os.path.join(self.tmpdir.getpath().encode("UTF-8"), inbase) + b".html"
            if not os.path.exists(htmlfn):
                return ""
            return open(htmlfn).read()
        except Exception as ex:
            rclexecm.logmsg(f"soffice failed: {ex}")
            return ""

if __name__ == "__main__":
    sofficecmd = rclexecm.which("soffice")
    if not sofficecmd:
        print("RECFILTERROR HELPERNOTFOUND soffice")
        sys.exit(1)
    runner = SofficeRunner(sofficecmd)
    txt = runner.runsoffice(sys.argv[1])
    sys.stdout.buffer.write(txt.encode('utf-8'))
