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

import rclexecm
import rclexec1
import re
import sys
import os


# Processing the output from unrtf
class RTFProcessData:
    def __init__(self, em):
        self.em = em
        self.out = []
        self.gothead = 0
        self.patendhead = re.compile(rb"</head>")
        self.patcharset = re.compile(rb"^<meta http-equiv=")

    # Some versions of unrtf put out a garbled charset line.
    # Apart from this, we pass the data untouched.
    def takeLine(self, line):
        if not self.gothead:
            if self.patendhead.search(line):
                self.out.append(
                    b'<meta http-equiv="Content-Type" '
                    + b'content="text/html;charset=UTF-8">'
                )
                self.out.append(line)
                self.gothead = 1
            elif not self.patcharset.search(line):
                self.out.append(line)
        else:
            self.out.append(line)

    def wrapData(self):
        return b"\n".join(self.out)


class RTFFilter:
    def __init__(self, em):
        self.em = em
        self.ntry = 0

    def reset(self):
        self.ntry = 0

    def getCmd(self, fn):
        if self.ntry:
            return ([], None)
        self.ntry = 1
        cmd = rclexecm.which("unrtf")
        if cmd:
            return ([cmd, "--nopict", "--html"], RTFProcessData(self.em))
        else:
            return ([], None)


if __name__ == "__main__":
    if not rclexecm.which("unrtf"):
        print("RECFILTERROR HELPERNOTFOUND unrtf")
        sys.exit(1)
    proto = rclexecm.RclExecM()
    filter = RTFFilter(proto)
    extract = rclexec1.Executor(proto, filter)
    rclexecm.main(proto, extract)
