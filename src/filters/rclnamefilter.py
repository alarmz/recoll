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

# Utility class to manage name-based filtering for archive handlers (zip, 7z)

import os
import fnmatch

import rclconfig
import conftree


class NameFilter(object):
    """Utility class to process name-based filtering for archive handlers (zip, 7z). Note that
    specific parameters have names beginning with "zip". There are no parameters by archive-type
    """

    def __init__(self, em):
        """Initialize by reading the configuration. em is the RclExecM object and used for logging"""
        self.config = rclconfig.RclConfig()
        self.em = em

    def setforlocation(self, filename):
        """filename is the archive file name and is used to query location-dependant
        parameters from the configuration."""

        self.config.setKeyDir(os.path.dirname(filename))
        self.skipnames = []
        self.onlynames = []

        # OnlyNames: test if we should use the general skip list
        usebaseonlynames = conftree.valToBool(
            self.config.getConfParam("zipUseOnlyNames")
        )
        if usebaseonlynames:
            onlynames = self.config.getConfParam("onlyNames")
            self.onlynames += conftree.stringToStrings(onlynames)
        # And add specific archive skip list if it exists
        onlynames = self.config.getConfParam("zipOnlyNames")
        if onlynames is not None:
            self.onlynames += conftree.stringToStrings(onlynames)

        if self.onlynames:
            # self.em.rclog(f"self.onlynames {self.onlynames}")
            # No need for a skip list
            self.dofilter = True
            return True

        # SkippedNames: test if we should use the general skip list. Note that the Python rclconfig
        # does not use the skippedNames+ and skippedNames- variables at the moment (TBD).
        usebaseskipped = conftree.valToBool(
            self.config.getConfParam("zipUseSkippedNames")
        )
        if usebaseskipped:
            skipped = self.config.getConfParam("skippedNames")
            self.skipnames += conftree.stringToStrings(skipped)
        # And add specific archive skip list if it exists
        skipped = self.config.getConfParam("zipSkippedNames")
        if skipped is not None:
            self.skipnames += conftree.stringToStrings(skipped)
        # self.em.rclog(f"self.skipnames: {self.skipnames}")
        self.dofilter = bool(self.skipnames)
        return True

    def shouldprocess(self, path):
        """Test path against filters, return True if it should be processed, else False"""
        if not self.dofilter:
            return True
        fn = os.path.basename(path)
        if self.onlynames:
            for pat in self.onlynames:
                if fnmatch.fnmatch(fn, pat):
                    return True
            return False
        for pat in self.skipnames:
            if fnmatch.fnmatch(fn, pat):
                return False
        return True
