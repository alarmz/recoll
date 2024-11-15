#!/usr/bin/env python3
# Copyright (C) 2021 J.F.Dockes
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

""" Recoll handler for iPython / Jupyter notebook files."""

# Rem 02-2023. This is not used and I'm not sure why it exists. We use jupyter nbconvert directly
# set in mimeconf instead. Kept around just in case.
import os
import sys
import json

import rclexecm
from rclbasehandler import RclBaseHandler


class IPYNBextractor(RclBaseHandler):

    def __init__(self, em):
        super(IPYNBextractor, self).__init__(em)

    def html_text(self, fn):
        text = open(fn, "rb").read()
        data = json.loads(text)
        mdtext = ""
        if "worksheets" in data:
            cells = data["worksheets"][0]["cells"]
        else:
            cells = data["cells"]
        for cell in cells:
            if cell["cell_type"] == "markdown":
                mdtext += "\n"
                for line in cell["source"]:
                    mdtext += "# " + line + "\n"
            elif cell["cell_type"] == "code":
                mdtext += "\n\n"
                key = "source" if "source" in cell else "input"
                for line in cell[key]:
                    mdtext += line
                mdtext += "\n"
        # print("%s"%mdtext, file=sys.stderr)
        self.outputmimetype = "text/plain"
        return mdtext


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = IPYNBextractor(proto)
rclexecm.main(proto, extract)
