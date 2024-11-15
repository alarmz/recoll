#!/usr/bin/env python3

# 7-Zip file filter for Recoll

# Thanks to Recoll user Martin Ziegler
# This is a modified version of rclzip.py, with some help from rcltar.py
#
# Normally using py7zr https://github.com/miurahr/py7zr
#
# Else, but it does not work on all archives, may use:
#   Python pylzma library required. See http://www.joachim-bauch.de/projects/pylzma/

import sys
import os

import rclexecm
from archivextract import ArchiveExtractor


usingpy7zr = False
try:
    from py7zr import SevenZipFile as Archive7z

    usingpy7zr = True
except:
    try:
        from py7zlib import Archive7z
    except:
        print("RECFILTERROR HELPERNOTFOUND python3:py7zr or python3:pylzma")
        sys.exit(1)

import rclconfig


class SevenZipExtractor(ArchiveExtractor):
    def __init__(self, em):
        self.fp = None
        super().__init__(em)

    def extractone(self, ipath):
        # self.em.rclog("extractone: [%s]" % ipath)
        docdata = b""
        ok = False
        try:
            if usingpy7zr:
                docdata = self.sevenzdic[ipath].read()
            else:
                docdata = self.sevenzip.getmember(ipath).read()
            ok = True
        except Exception as err:
            self.em.rclog("extractone: failed: [%s]" % err)

        iseof = rclexecm.RclExecM.noteof
        if self.currentindex >= len(self.names) - 1:
            iseof = rclexecm.RclExecM.eofnext
        return (ok, docdata, rclexecm.makebytes(ipath), iseof)

    def closefile(self):
        if self.fp:
            self.fp.close()

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        filename = params["filename"]
        self.currentindex = -1
        self.namefilter.setforlocation(filename)

        try:
            self.fp = open(filename, "rb")
            self.sevenzip = Archive7z(self.fp)
            if usingpy7zr:
                self.sevenzdic = self.sevenzip.readall()
                self.names = [k[0] for k in self.sevenzdic.items()]
            else:
                self.names = self.sevenzip.getnames()
            return True
        except Exception as err:
            self.em.rclog("openfile: failed: [%s]" % err)
            return False

    def namelist(self):
        return self.names

    # getipath from ArchiveExtractor
    # getnext from ArchiveExtractor


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = SevenZipExtractor(proto)
rclexecm.main(proto, extract)
