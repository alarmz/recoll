#!/usr/bin/python3
# Copyright (C) 2017-2022 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the
#  Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

""" Move pages created by the recoll-we extension from the download directory to the recoll web queue.

The files (content file and metadata side file) are then renamed and
moved to the Recoll web queue directory.

We recognize recoll files by their pattern:
recoll-we-[cm]-<md5>.rclwe, and the fact that both a d and an m file
must exist. While not absolutely foolproof, this should be quite robust.

The script is normally executed by recollindex at appropriate times,
but it can also be run by hand.
"""

import sys
import os
import re
import getopt

try:
    from hashlib import md5 as md5
except:
    import md5
import shutil

try:
    from recoll import rclconfig
except:
    import rclconfig

_mswindows = sys.platform == "win32"

verbosity = 0


def logdeb(s):
    if verbosity >= 4:
        print("%s" % s, file=sys.stderr)


# # wnloaded instances of the same page are suffixed with (nn) by the
# browser.  We are passed a list of (hash, instancenum, filename)
# triplets, sort it, and keep only the latest file.
def delete_previous_instances(l, downloadsdir):
    l.sort(key=lambda e: "%s-%05d" % (e[0], e[1]), reverse=True)
    ret = {}
    i = 0
    while i < len(l):
        hash, num, fn = l[i]
        logdeb("Found %s" % fn)
        ret[hash] = fn
        j = 1
        while i + j < len(l):
            if l[i + j][0] == hash:
                ofn = l[i + j][2]
                logdeb("Deleting %s" % ofn)
                os.unlink(os.path.join(downloadsdir, ofn))
                j += 1
            else:
                break
        i += j
    return ret


fn_re = re.compile(r"recoll-we-([mc])-([0-9a-f]+)(\([0-9]+\))?\.rclwe")


def list_all_files(dir):
    files = os.listdir(dir)
    mfiles = []
    cfiles = []
    for fn in files:
        mo = fn_re.match(fn)
        if mo:
            mc = mo.group(1)
            hash = mo.group(2)
            num = mo.group(3)
            if not num:
                num = "(0)"
            num = int(num.strip("()"))
            if mc == "m":
                mfiles.append([hash, num, fn])
            else:
                cfiles.append([hash, num, fn])
    return mfiles, cfiles


#######################
def msg(s):
    print(f"{s}", file=sys.stderr)


def usage():
    msg("Usage: recoll-we-move-files.py [-c <recollconfigdir>]")
    msg(
        " The script needs the recoll configuration directory. This can be set either through"
    )
    msg(
        " the RECOLL_CONFDIR environment variable or the '-c' command line option (which takes"
    )
    msg(
        " precedence). If none is set, the default configuration directory will be used."
    )
    sys.exit(1)


opts, args = getopt.getopt(sys.argv[1:], "c:")
if not len(args) == 0:
    usage()

configdir = None
for opt, val in opts:
    # logdeb(f"opt {opt} val {val}")
    if opt == "-c":
        configdir = val
        if not os.path.isdir(val):
            msg(f"{val} is not a directory")
            usage()
    else:
        usage()

config = rclconfig.RclConfig(argcnf=configdir)

# Get the directory where the browser extension creates the page files. Our user can set it as a
# subdirectory of the default Downloads directory, for tidiness
downloadsdir = config.getConfParam("webdownloadsdir")
if not downloadsdir:
    downloadsdir = "~/Downloads"
downloadsdir = os.path.expanduser(downloadsdir)
if not os.path.isdir(downloadsdir):
    msg(f"Downloads directory {downloadsdir} does not exist")
    sys.exit(1)

# Get the target recoll webqueue directory, into which we are going to move the downloaded files.
webqueuedir = config.getConfParam("webqueuedir")
if not webqueuedir:
    if _mswindows:
        webqueuedir = "~/AppData/Local/RecollWebQueue"
    else:
        webqueuedir = "~/.recollweb/ToIndex"
webqueuedir = os.path.expanduser(webqueuedir)
os.makedirs(webqueuedir, exist_ok=True)


# logdeb(f"recoll confdir [{configdir}] downloadsdir [{downloadsdir}] webqueuedir [{webqueuedir}]")

# Get the lists of all files created by the browser addon
mfiles, cfiles = list_all_files(downloadsdir)

# Only keep the last version
mfiles = delete_previous_instances(mfiles, downloadsdir)
cfiles = delete_previous_instances(cfiles, downloadsdir)

# logdeb("Mfiles: %s"% mfiles)
# logdeb("Cfiles: %s"% cfiles)

# Move files to webqueuedir target directory
# The webextensions plugin creates the metadata files first. So it may
# happen that a data file is missing, keep them for next pass.
# The old plugin created the data first, so we move data then meta
for hash in cfiles.keys():
    if hash in mfiles.keys():
        newname = "firefox-recoll-web-" + hash
        shutil.move(
            os.path.join(downloadsdir, cfiles[hash]), os.path.join(webqueuedir, newname)
        )
        shutil.move(
            os.path.join(downloadsdir, mfiles[hash]),
            os.path.join(webqueuedir, "_" + newname),
        )
