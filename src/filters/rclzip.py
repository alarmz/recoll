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

# Zip file extractor for Recoll

import os
import posixpath
import datetime

from zipfile import ZipFile

import rclexecm
from archivextract import ArchiveExtractor

# Note about file names (updated for python 3)
#
# zipfile always returns the archive contents names as str (unicode).  There is a bit in zip
# entries to indicate if the filename is encoded as utf-8 or not.  If the bit is not set
# zipfile decodes from the binary value using CP437 because this is the default for zip
# non-ASCII paths, and no other encoding was really expected.
#
# When reading the file, the input file name is used by rclzip.py directly as an index into the
# catalog.
#
# When we send the file name data to the indexer, we have to serialize it as byte string, we
# can't pass unicode objects to and from. So, the value is encoded to UTF-8.
# For extracting, the ipath is decoded from UTF-8 into an str.
#
# This is simplified from Python2 where the old comments seem to imply that the catalog entry
# was sometimes binary and that there could be an ambiguity in some cases (so that both the
# binary and the decode value were tried in archivextract.py).
#
# Some zip files contain file names which are not encoded as CP437 (Ex: EUC-KR or BIG5) Python
# produces garbage paths in this case (this does not affect the ipath validity, just the
# display). Python3 zipfile has a parameter to force a non-CP437 encoding for the metadata
# (this does not affect a zip file with the UTF-8 flag).
# A configuration parameter was added in Recoll 1.43.3 to allow setting this value. 
# 
class ZipExtractor(ArchiveExtractor):
    def __init__(self, em):
        self.filename = None
        self.f = None
        self.zip = None
        self.config = em.config()
        super().__init__(em)

    def closefile(self):
        # self.em.rclog("Closing %s" % self.filename)
        if self.zip:
            self.zip.close()
        if self.f:
            self.f.close()
        self.f = None
        self.zip = None

    def extractone(self, ipath):
        # self.em.rclog(f"extractone: ipath [{ipath}]")
        docdata = ""
        try:
            info = self.zip.getinfo(ipath)
            # There could be a 4GB Iso in the zip. We have to set a limit
            if info.file_size > self.em.maxmembersize:
                self.em.rclog("extractone: entry %s size %d too big" % (ipath, info.file_size))
                docdata = ""
                # raise BadZipfile()
            else:
                docdata = self.zip.read(ipath)
            try:
                # We are assuming here that the zip uses forward slash separators, which is not
                # necessarily the case. At worse, we'll get a wrong or no file name, which is
                # no big deal (the ipath is the important data element).
                filename = posixpath.basename(ipath)
                self.em.setfield("filename", filename)
                dt = datetime.datetime(*info.date_time)
                self.em.setfield("modificationdate", str(int(dt.timestamp())))
            except:
                pass
            ok = True
        except Exception as err:
            #self.em.rclog(f"{err}")
            ok = False
        iseof = rclexecm.RclExecM.noteof
        if self.currentindex >= len(self.zip.namelist()) - 1:
            self.closefile()
            iseof = rclexecm.RclExecM.eofnext
        return (ok, docdata, rclexecm.makebytes(ipath), iseof)

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.closefile()
        self.filename = params["filename"]
        self.currentindex = -1
        self.namefilter.setforlocation(self.filename)
        # setforlocation calls config.setkeydir()
        metadataencoding = self.config.getConfParam("zipMetaEncoding")
        #self.em.rclog(f"metadataencoding: {metadataencoding}")
        try:
            # Note: py3 ZipFile wants an str file name, which is wrong: file names are
            # binary. But it accepts an open file, and open() has no such restriction. We might
            # be able to use the filesystemencoding and surrogateescape thingy, but let's leave
            # well enough alone...
            self.f = open(self.filename, "rb")
            self.zip = ZipFile(self.f, metadata_encoding = metadataencoding)
            return True
        except Exception as err:
            self.em.rclog("openfile: failed: [%s]" % err)
            return False

    def namelist(self):
        return self.zip.namelist()

    # getipath from ArchiveExtractor
    # getnext inherited from ArchiveExtractor


# Main program: create protocol handler and extractor and run them
proto = rclexecm.RclExecM()
extract = ZipExtractor(proto)
rclexecm.main(proto, extract)
