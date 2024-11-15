#!/usr/bin/env python3
# Copyright (C) 2014-2024 J.F.Dockes
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
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

import rclexecm
import rclnamefilter


class ArchiveExtractor:
    """Common code for archive processors (zip,7z,rar,tar...)"""

    def __init__(self, em):
        self.em = em
        self.currentindex = 0
        self.namefilter = rclnamefilter.NameFilter(em)

    def getipath(self, params):
        ipath = params["ipath"]
        ok, data, ipath, eof = self.extractone(ipath)
        if ok:
            return (ok, data, ipath, eof)
        # Not found. Maybe we need to decode the path?
        try:
            ipath = ipath.decode("utf-8")
            return self.extractone(ipath)
        except Exception as err:
            self.em.rclog("extractone: failed: [%s]" % err)
            return (ok, data, ipath, eof)

    def getnext(self, params):
        if self.currentindex == -1:
            # Return "self" doc
            self.currentindex = 0
            self.em.setmimetype("text/plain")
            if len(self.namelist()) == 0:
                self.closefile()
                eof = rclexecm.RclExecM.eofnext
            else:
                eof = rclexecm.RclExecM.noteof
            return (True, "", "", eof)

        while self.currentindex < len(self.namelist()):
            entryname = self.namelist()[self.currentindex]
            if self.namefilter.shouldprocess(entryname):
                break
            entryname = None
            self.currentindex += 1

        if entryname is None:
            self.closefile()
            return (False, "", "", rclexecm.RclExecM.eofnow)

        ret = self.extractone(entryname)
        self.currentindex += 1
        if ret[3] != rclexecm.RclExecM.noteof:
            self.closefile()
        return ret
