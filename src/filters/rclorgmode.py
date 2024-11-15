#!/usr/bin/env python3
# Copyright (C) 2020-2022 J.F.Dockes
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

"""Read an org-mode file, optionally break it into subdocs" along level 1 headings"""

import sys
import re

import rclexecm
import rclconfig
import conftree


class OrgModeExtractor:
    def __init__(self, em):
        self.file = ""
        self.em = em
        self.selftext = ""
        self.docs = []
        config = rclconfig.RclConfig()
        self.createsubdocs = conftree.valToBool(config.getConfParam("orgmodesubdocs"))

    def extractone(self, index):
        if index >= len(self.docs):
            return (False, "", "", True)
        docdata = self.docs[index]
        # self.em.rclog(docdata)

        iseof = rclexecm.RclExecM.noteof
        if self.currentindex >= len(self.docs) - 1:
            iseof = rclexecm.RclExecM.eofnext
        self.em.setmimetype("text/x-orgmode-sub")
        try:
            self.em.setfield("title", docdata.splitlines()[0])
        except:
            pass
        return (True, docdata, str(index), iseof)

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.file = params["filename"]
        try:
            data = open(self.file, "rb").read()
        except Exception as e:
            self.em.rclog("Openfile: open: %s" % str(e))
            return False

        self.currentindex = -1
        if not self.createsubdocs:
            self.selftext = data
            return True

        res = rb"^\* "
        self.docs = re.compile(res, flags=re.MULTILINE).split(data)
        # Note that there can be text before the first heading. This goes into the self doc,
        # because it's not a proper entry.
        self.selftext = self.docs[0]
        self.docs = self.docs[1:]
        # self.em.rclog("openfile: Entry count: %d" % len(self.docs))
        return True

    def getipath(self, params):
        try:
            if params["ipath"] == b"":
                index = 0
            else:
                index = int(params["ipath"])
        except:
            return (False, "", "", True)
        return self.extractone(index)

    def getnext(self, params):
        if not self.createsubdocs:
            return (True, self.selftext, "", rclexecm.RclExecM.eofnext)

        if self.currentindex == -1:
            # Return "self" doc
            self.currentindex = 0
            self.em.setmimetype(b"text/plain")
            if len(self.docs) == 0:
                eof = rclexecm.RclExecM.eofnext
            else:
                eof = rclexecm.RclExecM.noteof
            return (True, self.selftext, "", eof)

        if self.currentindex >= len(self.docs):
            self.em.rclog("getnext: EOF hit")
            return (False, "", "", rclexecm.RclExecM.eofnow)
        else:
            ret = self.extractone(self.currentindex)
            self.currentindex += 1
            return ret


proto = rclexecm.RclExecM()
extract = OrgModeExtractor(proto)
rclexecm.main(proto, extract)
