#!/usr/bin/env python3
# Copyright (C) 2023 J.F.Dockes
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

"""This is a small sample script to show how a handler can return a data file pointer instead of
returning the data itself (needs recoll source after 2023-02-16).

This handler processes .link files which just hold a file path (like a symlink, but they are regular
files). We give them a bogus text/x-link MIME type.

mimemap has:
   .link = text/x-link

mimeconf:
    [index]
    text/x-link = execm rcllink.py

mimeview:
    xallexcepts+ = text/x-link
    [view]
    text/x-link = rcllink.py --open %f

Note that we need a special script to "open" the documents, because the top level doc seen by
recoll has the text/x-link MIME, not the actual content's

In this example the code for the opening script is also in this file, but this would probably not
be a good idea in a real case because of the possibilities of conflict with the rclexecm main
program. Use a separate script.

This script has no real world utility, but it allows trying things out. The only really reusable
part is how to signal the content as link by setting the rclisdatapath field
"""

import rclexecm
import sys
from rclbasehandler import RclBaseHandler


class LinkDump(RclBaseHandler):
    def html_text(self, fn):
        self.em.setmimetype("application/pdf")
        self.outputmimetype = "application/pdf"
        self.em.setfield("rclisdatapath", "1")
        with open(fn, "rb") as f:
            path = f.read()
        path = path.decode("UTF-8")
        path = path.rstrip("\n\r ")
        return path


if __name__ == "__main__":
    # The --open option is a hack because it could conflict with rclexecm wanting to use
    # --open in the future. The "Open" code is in this sample script, just for the sake of
    # simplicity, as the GUI also looks in the filters directory for "Open" commands so there is no
    # need to deal with the PATH
    if len(sys.argv) > 2 and sys.argv[1] == "--open":
        import subprocess

        with open(sys.argv[2], "rb") as f:
            path = f.read()
        path = path.decode("UTF-8")
        path = path.rstrip("\n\r ")
        subprocess.run(["xdg-open", path], check=True)
        sys.exit(0)
    # Regular handler boilerplate:
    proto = rclexecm.RclExecM()
    extract = LinkDump(proto)
    rclexecm.main(proto, extract)
