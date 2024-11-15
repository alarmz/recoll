#!/usr/bin/env python3
# Copyright (C) 2022 J.F.Dockes
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

# Null handler returning an empty document. For perf testing, mainly.

import sys

import rclexecm
from rclbasehandler import RclBaseHandler


class NullDump(RclBaseHandler):
    def __init__(self, em):
        super(NullDump, self).__init__(em)

    def html_text(self, fn):
        return (
            b'<html><head><meta http-equiv="Content-Type" content="text/html;charset=UTF-8">'
            b"<title></title></head><body><pre></pre></body></html>"
        )


if __name__ == "__main__":
    proto = rclexecm.RclExecM()
    extract = NullDump(proto)
    rclexecm.main(proto, extract)
