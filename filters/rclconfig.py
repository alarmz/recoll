#!/usr/bin/env python3

import locale
import re
import os
import sys
import base64
import platform

import conftree


def msg(s):
    print(f"{s}", file=sys.stderr)


class RclDynConf:
    """Decoding the contents of the recoll config "history" file.
    This contains lists of values inside multiple submaps (e.g. "[docs]" for doc history,
    "[allExtDbs]" for external indexes etc.).
    In each submap, the values are encoded as base64 and the keys are string integers
    like 000001, 00002 etc., such that the contents of a submap are ordered.
    We provide the contents of a submap as a list of binary strings.
    """
    def __init__(self, fname):
        self.data = conftree.ConfSimple(fname)

    def getStringList(self, sk):
        """Return the contents of a history file submap as a list of binary strings"""
        nms = self.data.getNames(sk)
        out = []
        if nms is not None:
            for nm in nms:
                out.append(base64.b64decode(self.data.get(nm, sk)))
        return out


class RclConfig:
    """A Python class mimicking a subset of the C++ RclConfig interface"""
    def __init__(self, argcnf=None):
        """Peruse the environment and optional input config location to compute the list
        of stacked directories (self.cdirs) which will be used to build the configuration
        objects as ConfStacks.
        Also set:
         - self.datadir: (the installed Recoll datadir directory, e.g. /usr/share/recoll on Linux)
         - self.confdir: the personal config.
        Initialize self.config and self.mimemap to None for lazy building.

        Most of the interface related to fs locations is str-based, so if you want to
        use a non-decodable binary string as your configuration location, you are out of luck.
        However the getConfParam() method will accept a binary key and return a binary
        string in this case.
        """

        self.config = None
        self.mimemap = None
        self.keydir = ""
        platsys = platform.system()

        # We now always set casesens to False, even on Linux. ConfTree() sets the submaps names (fs
        # paths) case-sensitivity depending on the platform.
        self.casesens = False

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
        self.cdirs.append(baseconfdir)
        # msg("Config dirs: {self.cdirs}")


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
            self.config = conftree.ConfStack("recoll.conf", self.cdirs, tp="tree",
                                             casesensitive = self.casesens)
        return self.config.get(nm, self.keydir)


    def mimeType(self, path):
        """This is a simplified version of the c++ code, intended mostly for the
        test mode of rclexecm.py. We don't attempt to check the data, so this
        will not work on extension-less paths (e.g. mbox/mail/etc.)"""
        if not self.mimemap:
            self.mimemap = conftree.ConfStack("mimemap", self.cdirs, tp="tree",
                                              casesensitive = self.casesens)
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
    print(f"topdirs = {config.getConfParam('topdirs')}")
    extradbs = RclExtraDbs(config)
    print(f"{extradbs.getActDbs()}")
