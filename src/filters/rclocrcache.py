#!/usr/bin/env python3
##########################################################################
# Copyright (C) 2020-2023 J.F.Dockes
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
###########################################################################
"""Caching the results of expensive processing of file documents

Overview:
=========

This module was initially created for caching the result of running OCR on image
files, hence its name, but it was then reused for the output of speech
recognition (STT, Speech To Text) on audio files. More generally, it could be
used for caching the results of any expensive transformation performed on a
document file.

The storage is organised so that retrieving the cached data for an unchanged
file is very fast, and retrieving it for a file which has been moved while
retaining the same contents is much faster than the OCR or STT treatment, only
needing a, possibly partial, contents hash computation.

The cache is made of two separate directory trees: the paths tree holds files
named by a hash of the document file path, and contain its path, size, recorded
modification time and data hash. The data tree holds files named by a hash of
the document file data, and containing the results of the OCR or STT operation.

Interface:
==========

API:
----

The main programming interface is provided by the methods in the OCRCache class:
store(), erase(), get(), along with some auxiliary methods for purging stale data.

Script interface:
-----------------

This file can be run as a script with for performing operations on the cache,
with different options. In all cases the cache affected is the one designated by
the Recoll configuration determined by the RECOLL_CONFDIR environment variable
(the -c option is not supported at the moment).

rclocrcache.py --purge [--purgedata]
++++++++++++++++++++++++++++++++++++

Purge the cache of obsolete data.

* --purge: purge the paths tree.  Walk the tree, read each path file, and
  check the existence of the recorded path. If the recorded path does not exist
  any more, delete the path file.
* --purgedata: purge the data tree. Make a list of all data files referenced by
  at least one path file, then walk the data tree, deleting unreferenced
  files. This means that data files from temporary document copies (see further
  below) will be deleted, which is quite unsatisfying. This would be difficult
  to change:
  - There is no way to detect the affected files because the data files store
    no origin information.
  - Even if we wanted to store an indication that the data file comes from a
    temporary document, we'd have no way to access the original document
    because the full ipath is not available. Changing this would be close to
    impossible for Recoll internals reasons (internfile...).

In consequence the --purgedata option must be explicitly added for a data purge
to be performed. Only set it if re-OCRing all embedded documents is reasonable.

rclocrcache.py --store <imgdatapath> <ocrdatapath>
++++++++++++++++++++++++++++++++++++++++++++++++++
Store data: create a cache entry for for the source file <imgdatapath>, and the
processed data <ocrdatapath>.

rclocrcache.py --get <imgdatapath>
++++++++++++++++++++++++++++++++++
Retrieve data stored for <imgdatapath>. The data is written to stdout.

rclocrcache.py --erase <imgdatapath>
++++++++++++++++++++++++++++++++++++
Erase the cache contents for <imgdatapath>.


Cache organisation details:
============================

The cache stores 2 kinds of objects:

- Path files are named from the hash of the document file path and contain the
  document data hash, the modification time and size of the document file at the
  time it was processed, and the document file path itself (the last is for
  purging only).
- Data files are named with the hash of the document data and contain the
  processed data (z-lib compressed).
- The cache path and data files are stored under top subdirectories: objects/
  and paths/
- In both cases, the the first two characters of the path or data sha1 hash are
  used as a subdirectory name, the rest as a file name, so a typical path would
  look like *objects/xx/xxxxxxxxxx* or *paths/xx/xxxxxxxxxxx*.

When retrieving data from the cache:

- We first use the document file path, size, and modification time: if an entry
  exists for the imagepath/mtime/size triplet, and is up to date, the
  data file is obtained from the hash contained in the path file, and its
  contents are returned.
- Else we then use the document data: if an entry exists for the computed hashed
  value of the data, it is returned. This allows moving files around without
  needing to process them again, but of course, it is more expensive than the
  first step.

If we need to use the second step, as a side effect, a path file is created or
updated so that the data will be found with the first step next time around.

When processing embedded documents like email attachments, recoll uses
temporary copies in TMPDIR (which defaults to /tmp) or RECOLL_TMPDIR. Of
course the paths for the temporary files changes when re-processing a given
document. We do not store the Path file for data stored in TMPDIR or
RECOLL_TMPDIR, because doing so would cause an indefinite accumulation of
unusable Path files. This means that access to the OCR data for these
documents always causes the computation of the data hash, and is slower. With
recent Recoll versions which cache the text content in the index, this only
occurs when reindexing (with older versions, this could also occur for
Preview).
"""

import sys
import os
import hashlib
import urllib.parse
import zlib
import glob

from rclexecm import logmsg as _deb


def _catslash(p):
    if p and p[-1] != "/":
        p += "/"
    return p


_tmpdir = os.environ["TMPDIR"] if "TMPDIR" in os.environ else "/tmp"
_tmpdir = _catslash(_tmpdir)
_recoll_tmpdir = os.environ["RECOLL_TMPDIR"] if "RECOLL_TMPDIR" in os.environ else None
_recoll_tmpdir = _catslash(_recoll_tmpdir)


class OCRCache(object):
    def __init__(self, conf, hashmb=0):
        """Initialize the cache object using parameters from conf, a RclConfig object.

        Keyword arguments:
        hashmb -- if non zero we only hash the amount of data specified by the value (mb).
          Data slices will be taken from the start, middle, and end of the file.
        """
        self.config = conf
        self.hashmb = hashmb
        self.cachedir = conf.getConfParam("ocrcachedir")
        if not self.cachedir:
            self.cachedir = os.path.join(self.config.getConfDir(), "ocrcache")
        self.objdir = os.path.join(self.cachedir, "objects")
        self.pathdir = os.path.join(self.cachedir, "paths")
        for dir in (self.objdir, self.pathdir):
            os.makedirs(dir, exist_ok=True)

    # Compute sha1 of path, as two parts of 2 and 38 chars
    def _hashpath(self, path):
        if type(path) != type(b""):
            path = path.encode("utf-8")
        m = hashlib.sha1()
        m.update(path)
        h = m.hexdigest()
        return h[0:2], h[2:]

    def _hashslice(self, hobj, f, offs, slicesz):
        """Update hash with data slice from file"""
        # _deb(f"Hashing {slicesz} at {offs}")
        f.seek(int(offs))
        cnt = 0
        while cnt < slicesz:
            rcnt = min(slicesz - cnt, 8192)
            d = f.read(int(rcnt))
            if not d:
                break
            hobj.update(d)
            cnt += len(d)

    def _hashdata(self, path):
        """Compute sha1 of path data contents, as two parts of 2 and 38 chars.

        If hashmb was set on init, we compute a partial hash in three slices at the start,
        middle and end of the file, with a total size of hashmb * MBs.
        """
        # _deb("Hashing DATA")
        sz = os.path.getsize(path)
        hobj = hashlib.sha1()
        hashbytes = self.hashmb * 1000000
        with open(path, "rb", buffering=8192) as f:
            if not self.hashmb or sz <= hashbytes:
                self._hashslice(hobj, f, 0, sz)
            else:
                slicesz = hashbytes / 3
                for offs in (0, sz / 2 - slicesz / 2, sz - slicesz):
                    self._hashslice(hobj, f, offs, slicesz)
        h = hobj.hexdigest()
        # _deb(f"DATA hash {h[0:2]} {h[2:]}")
        return h[0:2], h[2:]

    def _readpathfile(self, ppf):
        """Read path file and return values. We do not decode the image path
        as this is only used for purging"""
        with open(ppf, "r") as f:
            line = f.read()
        dd, df, tm, sz, pth = line.split()
        tm = int(tm)
        sz = int(sz)
        return dd, df, tm, sz, pth

    # Try to read the stored attributes for a given path: data hash,
    # modification time and size. If this fails, the path itself is
    # not cached (but the data still might be, maybe the file was moved)
    def _cachedpathattrs(self, path):
        pd, pf = self._hashpath(path)
        pathfilepath = os.path.join(self.pathdir, pd, pf)
        if not os.path.exists(pathfilepath):
            return False, None, None, None, None
        try:
            dd, df, tm, sz, pth = self._readpathfile(pathfilepath)
            return True, dd, df, tm, sz
        except Exception as ex:
            _deb(f"Error while trying to access pathfile {pathfilepath}: {ex}")
            return False, None, None, None, None

    # Compute the path hash, and get the mtime and size for given
    # path, for updating the cache path file
    def _newpathattrs(self, path):
        pd, pf = self._hashpath(path)
        tm = int(os.path.getmtime(path))
        sz = int(os.path.getsize(path))
        return pd, pf, tm, sz

    # Check if the cache appears up to date for a given path, only
    # using the modification time and size. Return the data file path
    # elements if we get a hit.
    def _pathincache(self, path):
        ret, od, of, otm, osz = self._cachedpathattrs(path)
        if not ret:
            return False, None, None
        pd, pf, ntm, nsz = self._newpathattrs(path)
        # _deb(" tm %d  sz %d" % (ntm, nsz))
        # _deb("otm %d osz %d" % (otm, osz))
        if otm != ntm or osz != nsz:
            return False, None, None
        return True, od, of

    # Compute the data file name for path. Expensive: we compute the data hash.
    # Return both the data file path and path elements (for storage in path file)
    def _datafilename(self, path):
        d, f = self._hashdata(path)
        return os.path.join(self.objdir, d, f), d, f

    # Create path file with given elements.
    def _updatepathfile(self, pd, pf, dd, df, tm, sz, path):
        global _tmpdir, _recoll_tmpdir
        if (_tmpdir and path.startswith(_tmpdir)) or (
            _recoll_tmpdir and path.startswith(_recoll_tmpdir)
        ):
            _deb(f"ocrcache: not storing path data for temporary file {path}")
            return
        dir = os.path.join(self.pathdir, pd)
        if not os.path.exists(dir):
            os.makedirs(dir)
        pfile = os.path.join(dir, pf)
        codedpath = urllib.parse.quote(path)
        with open(pfile, "w") as f:
            f.write("%s %s %d %d %s\n" % (dd, df, tm, sz, codedpath))

    # Store data for path. Only rewrite an existing data file if told
    # to do so: this is only useful if we are forcing an OCR re-run.
    def store(self, path, datatostore, force=False):
        dd, df = self._hashdata(path)
        pd, pf, tm, sz = self._newpathattrs(path)
        self._updatepathfile(pd, pf, dd, df, tm, sz, path)
        dir = os.path.join(self.objdir, dd)
        if not os.path.exists(dir):
            os.makedirs(dir)
        dfile = os.path.join(dir, df)
        if force or not os.path.exists(dfile):
            # _deb("Storing data")
            cpressed = zlib.compress(datatostore)
            with open(dfile, "wb") as f:
                f.write(cpressed)
        return True

    def erase(self, path):
        """Unconditionally erase cached data for input data file path"""
        ok = True
        pd, pf = self._hashpath(path)
        pfn = os.path.join(self.pathdir, pd, pf)
        try:
            dd, df, tm, sz, orgpath = self._readpathfile(pfn)
        except Exception as ex:
            _deb(f"Ocrcache: failed reading path file: {ex}")
            return False
        dfn = os.path.join(self.objdir, dd, df)
        try:
            os.unlink(pfn)
        except Exception as ex:
            ok = False
            _deb(f"Ocrcache: failed deleting {pfn}: {ex}")
        try:
            os.unlink(dfn)
        except Exception as ex:
            ok = False
            _deb(f"Ocrcache: failed deleting {dpfn}: {ex}")
        return ok

    # Retrieve cached OCR'd data for image path. Possibly update the
    # path file as a side effect (case where the image has moved, but
    # the data has not changed).
    def get(self, path):
        pincache, dd, df = self._pathincache(path)
        if pincache:
            dfn = os.path.join(self.objdir, dd, df)
        else:
            dfn, dd, df = self._datafilename(path)

        if not os.path.exists(dfn):
            _deb(f"ocrcache: no cached data for {path}")
            return False, b""

        if not pincache:
            # File may have moved. Create/Update path file for next time
            _deb(
                f"ocrcache::get: data ok but path file for {path} does not exist: creating it"
            )
            pd, pf, tm, sz = self._newpathattrs(path)
            self._updatepathfile(pd, pf, dd, df, tm, sz, path)

        with open(dfn, "rb") as f:
            cpressed = f.read()
            data = zlib.decompress(cpressed)
            return True, data

    def _pathstale(self, origpath, otm, osz):
        """Return True if the input path has been removed or modified"""
        if not os.path.exists(origpath):
            return True
        ntm = int(os.path.getmtime(origpath))
        nsz = int(os.path.getsize(origpath))
        if ntm != otm or nsz != osz:
            # _deb("Purgepaths otm %d ntm %d osz %d nsz %d"%(otm, ntm, osz, nsz))
            return True
        return False

    def purgepaths(self):
        """Remove all stale pathfiles: source image does not exist or has
        been changed. Mostly useful for removed files, modified ones would be
        processed by recollindex."""
        allpathfiles = glob.glob(os.path.join(self.pathdir, "*", "*"))
        for pathfile in allpathfiles:
            dd, df, tm, sz, orgpath = self._readpathfile(pathfile)
            needpurge = self._pathstale(orgpath, tm, sz)
            if needpurge:
                _deb("purgepaths: removing %s (%s)" % (pathfile, orgpath))
                os.remove(pathfile)

    def _walk(self, topdir, cb):
        """Specific fs walk: we know that our tree has 2 levels. Call cb with
        the file path as parameter for each file"""
        dlist = glob.glob(os.path.join(topdir, "*"))
        for dir in dlist:
            files = glob.glob(os.path.join(dir, "*"))
            for f in files:
                cb(f)

    def _pgdt_pathcb(self, f):
        """Get a pathfile name, read it, and record datafile identifier
        (concatenate data file subdir and file name)"""
        # _deb("_pgdt_pathcb: %s" % f)
        dd, df, tm, sz, orgpath = self._readpathfile(f)
        self._pgdt_alldatafns.add(dd + df)

    def _pgdt_datacb(self, datafn):
        """Get a datafile name and check that it is referenced by a previously
        seen pathfile"""
        p1, fn = os.path.split(datafn)
        p2, dn = os.path.split(p1)
        tst = dn + fn
        if tst in self._pgdt_alldatafns:
            _deb("purgedata: ok         : %s" % datafn)
            pass
        else:
            _deb("purgedata: removing   : %s" % datafn)
            os.remove(datafn)

    def purgedata(self):
        """Remove all data files which do not match any from the input list,
        based on data contents hash. We make a list of all data files
        referenced by the path files, then walk the data tree,
        removing all unreferenced files. This should only be run after
        an indexing pass, so that the path files are up to date. It's
        a relatively onerous operation as we have to read all the path
        files, and walk both sets of files."""

        self._pgdt_alldatafns = set()
        self._walk(self.pathdir, self._pgdt_pathcb)
        self._walk(self.objdir, self._pgdt_datacb)


if __name__ == "__main__":
    import rclconfig
    import getopt

    def Usage(f=sys.stderr):
        print("Usage: rclocrcache.py --purge [--purgedata]", file=f)
        print("Usage: rclocrcache.py --store <imgdatapath> <ocrdatapath>", file=f)
        print("Usage: rclocrcache.py --get <imgdatapath>", file=f)
        print("Usage: rclocrcache.py --erase <imgdatapath>", file=f)
        sys.exit(1)

    opts, args = getopt.getopt(
        sys.argv[1:],
        "h",
        ["help", "purge", "purgedata", "store", "get", "erase", "hashmb="],
    )
    purgedata = False
    action = ""
    hashmb = 0

    for opt, arg in opts:
        if opt in ["-h", "--help"]:
            Usage(sys.stdout)
        elif opt in ["--purgedata"]:
            purgedata = True
        elif opt in ["--hashmb"]:
            hashmb = int(arg)
        elif opt in ["--purge"]:
            if len(args) != 0:
                Usage()
            action = "purge"
        elif opt in ["--store"]:
            if len(args) != 2:
                Usage()
            imgdatapath = args[0]
            ocrdatapath = args[1]
            ocrdata = open(ocrdatapath, "rb").read()
            action = "store"
        elif opt in ["--get"]:
            if len(args) != 1:
                Usage()
            imgdatapath = args[0]
            action = "get"
        elif opt in ["--erase"]:
            if len(args) != 1:
                Usage()
            imgdatapath = args[0]
            action = "erase"
        else:
            print(f"Unknown option {opt}", file=sys.stderr)
            Usage()

    conf = rclconfig.RclConfig()
    cache = OCRCache(conf, hashmb=hashmb)
    if action == "purge":
        cache.purgepaths()
        if purgedata:
            cache.purgedata()
    elif action == "store":
        cache.store(imgdatapath, ocrdata, force=False)
    elif action == "get":
        incache, data = cache.get(imgdatapath)
        if incache:
            print(f"OCR data from cache {data}")
            sys.exit(0)
        else:
            print("OCR Data was not found in cache", file=sys.stderr)
            sys.exit(1)
    elif action == "erase":
        if cache.erase(imgdatapath):
            print(f"Erased data for {imgdatapath}")
            sys.exit(0)
        else:
            print(f"Failed erasing data for {imgdatapath}")
            sys.exit(1)
    else:
        Usage()
