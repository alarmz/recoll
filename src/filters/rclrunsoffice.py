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

import sys
import subprocess
import os

import rclexecm
from rclexecm import logmsg as _deb
import conftree

def findsoffice():
    import rclconfig

    config = rclconfig.RclConfig()
    sofficecmd = config.getConfParam("sofficecmd")
    if sofficecmd:
        if sofficecmd.find('"') != -1:
            sofficecmd = conftree.stringToStrings(sofficecmd)
        else:
            sofficecmd = [sofficecmd,]
    else:
        sofficecmd = [rclexecm.which("soffice"),]
    #_deb(f"sofficecmd {sofficecmd}")
    if not sofficecmd or not sofficecmd[0]:
        _deb("sofficecmd not found")
        return None
    if not os.path.isfile(sofficecmd[0]):
        _deb(f"sofficecmd {sofficecmd[0]} is not a file")
        return None
    #_deb(f"sofficecmd {sofficecmd}")
    return sofficecmd

class SofficeRunner(object):
    def __init__(self, sofficecmd):
        self.tmpdir = rclexecm.SafeTmpDir("rclrsoff")
        self.cmdbase = sofficecmd + ["--norestore", "--safe-mode", "--headless",
                                     "--convert-to", "html", "--outdir"]

    def runsoffice(self, inpath):
        if isinstance(inpath, bytes):
            inpath = os.fsdecode(inpath)
        self.tmpdir.vacuumdir()
        cmd = self.cmdbase + [self.tmpdir.getpath(), inpath]
        try:
            subprocess.check_call(cmd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
            infn = os.path.basename(inpath)
            inbase = os.path.splitext(infn)[0]
            htmlfn = os.path.join(self.tmpdir.getpath(), inbase) + ".html"
            if not os.path.exists(htmlfn):
                _deb("rclrunsoffice: HTML file not found after running command")
                return ""
            return open(htmlfn).read()
        except Exception as ex:
            rclexecm.logmsg(f"soffice failed: {ex}")
            return ""


if __name__ == "__main__":
    sofficecmd = findsoffice()
    if not sofficecmd:
        print("RECFILTERROR HELPERNOTFOUND soffice")
        sys.exit(1)
    runner = SofficeRunner(sofficecmd)
    txt = runner.runsoffice(sys.argv[1])
    sys.stdout.buffer.write(txt.encode('utf-8'))
