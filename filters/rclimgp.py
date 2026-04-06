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


# This exists because the only way I have to properly extract image tags on Windows is the Perl
# ExifTool library (py3exiv2 appears very difficult to port).
#
# As I'm a Perl noob, I don't want to code the OCR part in Perl, so this Python program executes the
# Perl one to extract the tags, then possibly does the OCR dance. The data is transferred as JSON,
# and the part of the Perl program which outputs the HTML is duplicated here.

import sys
import os
import re
import subprocess
import platform
import json

import rclexecm
from rclbasehandler import RclBaseHandler


# maps image file tags to recoll fields
_fieldMap = {
    'subject' : 'subject',
    'comment' : 'description',
    'title' : 'title',
    'headline' : 'title',
    'caption' : 'caption',
    'caption-abstract' : 'caption',
    'author' : 'author',
    'creator' : 'creator',
    'from' : 'from',
    'keywords' : 'keywords',
    'keyword' : 'keywords',
    'tag' : 'tag',
    'date' : 'date',
}


# recollField: returns a recoll field to be used for this tag
def recollField(imgtag):
    for tagre, field in _fieldMap.items():
        if re.match("^" + tagre + "$", imgtag, flags = re.IGNORECASE):
            return field
    return None


class ImgTagExtractor(RclBaseHandler):
    def __init__(self, em):
        super(ImgTagExtractor, self).__init__(em)
        self.config = self.em.config()
        self.iswin = False
        platsys = platform.system()
        # On Windows, the rclimg perl program is a standalone binary.
        if platsys == "Windows":
            progname = "rclimg.exe"
        else:
            progname = "rclimg"
        self.cmd = [os.path.join(_execdir, progname), "-j"]
            
    def html_text(self, filename):
        ok = False
        cmd = self.cmd
        cmd.append(os.fsdecode(filename))
        #self.em.rclog(f"cmd: {cmd}")
        try:
            global perlproc
            perlproc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
            data, stderr = perlproc.communicate()
            perlproc = None
        except Exception as e:
            self.em.rclog(f"{cmd} failed: {e}")
            return ""
        
        metadata = json.loads(data);
        fields = {}
        others = {}
        titleHtmlTag = "";
        for key in metadata.keys():
            if type(metadata[key]) != type(""):
                metadata[key] = str(metadata[key])
            others[key] = metadata[key]
            field = recollField(key);
            if field:
                if field == "title":
                    if titleHtmlTag:
                        titleHtmlTag = titleHtmlTag + f" - {metadata[key]}"
                    else:
                        titleHtmlTag = metadata[key]
                else:
                    fields[field] = metadata[key]

        docdata = "<html><head>\n"
        if titleHtmlTag:
            docdata += "<title>" + titleHtmlTag + "</title>\n"
            docdata += "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">\n"
        for key,val in fields.items():
            docdata += f"<meta name=\"{key}\" content=\"{rclexecm.htmlescape(val)}\">\n";

        docdata += "</head><body>\n"

        #### Body stuff 
        docdata += "</body>\n</html>\n"

        docdata = docdata.encode("UTF-8")
        ## Do we need OCR ?
        self.config.setKeyDir(os.path.dirname(filename))
        s = self.config.getConfParam("imgocr")
        if rclexecm.configparamtrue(s):
            # Run image OCR
            htmlprefix = b"<H3>O_C_R T_E_X_T</H3>\n<PRE>"
            htmlsuffix = b"</PRE>"
            cmd = [
                sys.executable,
                os.path.join(_execdir, "rclocr.py"),
                filename,
            ]
            try:
                global ocrproc
                ocrproc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
                data, stderr = ocrproc.communicate()
                ocrproc = None
                if len(data) > 1:
                    docdata += htmlprefix + rclexecm.htmlescape(data) + htmlsuffix
            except Exception as e:
                self.em.rclog(f"{cmd} failed: {e}")
                pass

        # Use all extracted fields as main text. Not that useful but ensure that
        # everything is indexed and allows previewing them
        flddata = ""
        for key,val in others.items():
            flddata += f"{key:30s} : {rclexecm.htmlescape(val)}\n"
        if len(flddata) > 1:
            flddata = flddata.encode("UTF-8")
            docdata += b"<H3>F_I_E_L_D_S:</H3>\n<PRE>\n"
            docdata += flddata
        docdata += b"</PRE></body></html>"

        # self.em.rclog(f"DOCDATA: {docdata.decode('UTF-8', errors='ignore')}")
        return docdata


if __name__ == "__main__":
    _execdir = os.path.dirname(sys.argv[0])
    proto = rclexecm.RclExecM()
    extract = ImgTagExtractor(proto)
    rclexecm.main(proto, extract)
