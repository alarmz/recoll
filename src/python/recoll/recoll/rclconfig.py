#!/usr/bin/env python3
from __future__ import print_function

import locale
import re
import os
import sys
import base64
import platform

try:
    from . import conftree
except:
    import conftree


def msg(s):
    print("%s" % s, file=sys.stderr)


class RclDynConf:
    def __init__(self, fname):
        self.data = conftree.ConfSimple(fname)

    def getStringList(self, sk):
        nms = self.data.getNames(sk)
        out = []
        if nms is not None:
            for nm in nms:
                out.append(base64.b64decode(self.data.get(nm, sk)))
        return out


class RclConfig:
    def __init__(self, argcnf=None):
        self.config = None
        self.mimemap = None
        platsys = platform.system()
        # Find configuration directory
        if argcnf:
            self.confdir = os.path.abspath(argcnf)
        elif "RECOLL_CONFDIR" in os.environ:
            self.confdir = os.environ["RECOLL_CONFDIR"]
        else:
            if platsys == "Windows":
                if "LOCALAPPDATA" in os.environ:
                    dir = os.environ["LOCALAPPDATA"]
                else:
                    dir = os.path.expanduser("~")
                self.confdir = os.path.join(dir, "Recoll")
            else:
                self.confdir = os.path.expanduser("~/.recoll")
        # msg(f"Confdir: [{self.confdir}]")

        # Find datadir to get at the base configuration files. This is tricky because this module
        # could be loaded either from the recoll filters/ directory, or from the Recoll Python
        # extension. In the first case, the base configuration should be in __file__/../examples.
        # In the second case, if we are loaded from a recoll filter, we can use sys.argv[0],
        # but else, we have to guess.
        self.datadir = None
        if "RECOLL_DATADIR" in os.environ:
            self.datadir = os.environ["RECOLL_DATADIR"]
        else:
            if platsys == "Windows":
                # Note that this is not guaranteed to work if we come from the Python extension and
                # not started from a recoll filter. Then we try a guess, which will fail if recoll
                # is installed in a non-default (or multiple) place.
                dirs = (
                    os.path.join(os.path.dirname(sys.argv[0]), "..", ".."),
                    "C:/Program Files (X86)/Recoll/",
                    "C:/Program Files/Recoll/",
                    "C:/install/recoll/",
                )
                for dir in dirs:
                    if os.path.exists(os.path.join(dir, "Share")):
                        self.datadir = os.path.join(dir, "Share")
                        break
            elif platsys == "Darwin":
                # On Darwin things are simpler because we don't supply a Python package. We are
                # certainly loaded from filters/
                self.datadir = os.path.join(os.path.dirname(__file__), "..")
            else:
                # Try to use __file__ and sys.argv[0]. This may fail if we're not a filter
                for filtersdir in (
                    os.path.dirname(__file__),
                    os.path.dirname(sys.argv[0]),
                ):
                    if filtersdir.find("recoll/filters") != -1:
                        # msg("Using __file__ to compute datadir")
                        self.datadir = os.path.dirname(filtersdir)
                        break
                if self.datadir is None:
                    # On ux platforms, the datadir value is set by "configure" in the C code.
                    # Try to guess
                    # msg("Trying to guess datadir")
                    dirs = ("/opt/local", "/opt", "/usr", "/usr/local", "/usr/pkg")
                    for dir in dirs:
                        dd = os.path.join(dir, "share/recoll")
                        if os.path.exists(dd):
                            self.datadir = dd
        if self.datadir is None:
            # msg("DATADIR could not be computed. Trying /usr/share/recoll as last resort")
            self.datadir = "/usr/share/recoll"
        # msg(f"DATADIR: [{self.datadir}]")
        baseconfdir = os.path.join(self.datadir, "examples")
        f = None
        try:
            f = open(os.path.join(baseconfdir, "recoll.conf"), "r")
        except:
            pass
        if f is None:
            raise (
                Exception(
                    "Can't open default/system recoll.conf in [%s]. " % baseconfdir
                    + "Please set RECOLL_DATADIR in the environment to point "
                    + "to the installed recoll data files."
                )
            )
        else:
            f.close()

        self.cdirs = []

        # Additional config directory, values override user ones
        if "RECOLL_CONFTOP" in os.environ:
            self.cdirs.append(os.environ["RECOLL_CONFTOP"])
        self.cdirs.append(self.confdir)
        # Additional config directory, overrides system's, overridden by user's
        if "RECOLL_CONFMID" in os.environ:
            self.cdirs.append(os.environ["RECOLL_CONFMID"])
        self.cdirs.append(os.path.join(self.datadir, "examples"))
        # msg("Config dirs: {self.cdirs}")
        self.keydir = ""

    def getConfDir(self):
        return self.confdir

    def getDataDir(self):
        return self.datadir

    def getDbDir(self):
        dir = self.getConfParam("dbdir")
        if os.path.isabs(dir):
            return dir
        cachedir = self.getConfParam("cachedir")
        if not cachedir:
            cachedir = self.confdir
        return os.path.join(cachedir, dir)

    def setKeyDir(self, dir):
        self.keydir = dir

    def getConfParam(self, nm):
        if not self.config:
            self.config = conftree.ConfStack("recoll.conf", self.cdirs, "tree")
        return self.config.get(nm, self.keydir)

    # This is a simplified version of the c++ code, intended mostly for the
    # test mode of rclexecm.py. We don't attempt to check the data, so this
    # will not work on extension-less paths (e.g. mbox/mail/etc.)
    def mimeType(self, path):
        if not self.mimemap:
            self.mimemap = conftree.ConfStack("mimemap", self.cdirs, "tree")
        if os.path.exists(path):
            if os.path.isdir(path):
                return "inode/directory"
            if os.path.islink(path):
                return "inode/symlink"
            if not os.path.isfile(path):
                return "inode/x-fsspecial"
            try:
                size = os.path.getsize(path)
                if size == 0:
                    return "inode/x-empty"
            except:
                pass
        ext = os.path.splitext(path)[1]
        return self.mimemap.get(ext, self.keydir)


class RclExtraDbs:
    def __init__(self, config):
        self.config = config

    def getActDbs(self):
        dyncfile = os.path.join(self.config.getConfDir(), "history")
        dync = RclDynConf(dyncfile)
        return dync.getStringList("actExtDbs")


if __name__ == "__main__":
    config = RclConfig()
    print("topdirs = %s" % config.getConfParam("topdirs"))
    extradbs = RclExtraDbs(config)
    print("%s" % extradbs.getActDbs())
