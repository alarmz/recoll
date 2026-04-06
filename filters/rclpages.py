#!/usr/bin/env python3
# Copyright (C) 2024 J.F.Dockes
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

import sys
import subprocess
import os

import rclexecm
from rclbasehandler import RclBaseHandler
import rclrunsoffice

_sofficecmd = None

class PagesHandler(RclBaseHandler):
    def __init__(self, em):
        self.em = em
        self.runner = rclrunsoffice.SofficeRunner(_sofficecmd)

    def html_text(self, inpath):
        return self.runner.runsoffice(inpath)


if __name__ == "__main__":
    _sofficecmd = rclrunsoffice.findsoffice()
    if not _sofficecmd:
        print("RECFILTERROR HELPERNOTFOUND soffice")
        sys.exit(1)
    proto = rclexecm.RclExecM()
    extract = PagesHandler(proto)
    rclexecm.main(proto, extract)
