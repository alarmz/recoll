#!/usr/bin/env python3
# Copyright (C) 2014 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
######################################

import sys
import os
import rclexecm
import rclxslt
import rclgenxslt

if __name__ == "__main__":
    sheetfile = os.path.join(os.path.dirname(__file__), "fb2.xsl")
    with open(sheetfile) as f:
        stylesheet_all = f.read()
    proto = rclexecm.RclExecM()
    extract = rclgenxslt.XSLTExtractor(proto, stylesheet_all)
    rclexecm.main(proto, extract)
