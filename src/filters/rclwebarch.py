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

# Handler for Mac OS Safari .webarchive format.
# On MacOS, we use the native "textutil" program. Else plistlib and bs4

import sys
import platform

if sys.platform == "darwin":
    onmacos = True
else:
    onmacos = False

if onmacos:
    import subprocess
    import os
else:
    import plistlib
    from bs4 import BeautifulSoup

import rclexecm
from rclbasehandler import RclBaseHandler

class WebarchiveHandler(RclBaseHandler):
    def __init__(self, em):
        self.em = em
        self.output_html = True
        if onmacos:
            self.tmpdir = rclexecm.SafeTmpDir("rclwebarch")
            self.cmdbase = ["textutil", "-convert", "html", "-noload", "-nostore", "-output"]

    def html_text_macos(self, inpath):
        self.tmpdir.vacuumdir()
        htmlfile = os.path.join(self.tmpdir.getpath(), "index.html")
        cmd = self.cmdbase + [htmlfile, inpath]
        subprocess.check_call(cmd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        return open(htmlfile).read()
        
    def html_text_other(self, inpath):
        with open(inpath, 'rb') as f:
            plist = plistlib.load(f)

        main_resource = plist.get('WebMainResource', {})
        data = main_resource.get('WebResourceData')

        if not data:
            raise RuntimeError("No main HTML content found in the .webarchive file.")

        # Decode HTML content
        try:
            html_data = data.decode('utf-8')
        except UnicodeDecodeError:
            html_data = data.decode('latin-1', errors='ignore')

        if self.output_html:
            # Parse HTML and remove all <svg> blocks
            soup = BeautifulSoup(html_data, 'html.parser')
            for svg in soup.find_all('svg'):
                svg.decompose()
            # Convert to string
            clean_html = str(soup)
            self.em.setmimetype("text/html")
            return clean_html
        else:
            self.em.setmimetype("text/plain")
            text = soup.get_text(separator='\n', strip=True)
            return rclexecm.htmlescape(text.encode("utf-8"))
            
        
    def html_text(self, inpath):
        if onmacos:
            return self.html_text_macos(inpath)
        else:
            return self.html_text_other(inpath)

if __name__ == "__main__":
    proto = rclexecm.RclExecM()
    extract = WebarchiveHandler(proto)
    rclexecm.main(proto, extract)
