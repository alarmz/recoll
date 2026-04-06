#!/usr/bin/python3
# Copyright (C) 2022-2023 J.F.Dockes
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

import sys

from getopt import getopt

from recoll import recoll


def msg(s):
    print(f"{s}", file=sys.stderr)


def Usage():
    msg(
        "Usage: snippets.py [-c conf] [-i extra_index] [-w ctxwords] [-n] <recoll query>"
    )
    sys.exit(1)


if len(sys.argv) < 2:
    Usage()


confdir = ""
extra_dbs = []
ctxwords = 4
nohl = False
# Process options: [-c confdir] [-i extra_db [-i extra_db] ...]
try:
    options, args = getopt(sys.argv[1:], "c:i:w:n")
except Exception as ex:
    print(f"{ex}")
    sys.exit(1)
for opt, val in options:
    if opt == "-c":
        confdir = val
    elif opt == "-n":
        nohl = True
    elif opt == "-i":
        extra_dbs.append(val)
    elif opt == "-w":
        ctxwords = int(val)
    else:
        print(f"Bad opt: {opt}")
        Usage()

if len(args) == 0:
    msg("No query found in command line")
    Usage()
qs = " ".join(args)
# msg(f"QUERY: [{qs}]")

db = recoll.connect(confdir=confdir, extra_dbs=extra_dbs)
query = db.query()
query.execute(qs)


class HL:
    def startMatch(self, i):
        return "<span class='hit'>"

    def endMatch(self):
        return "</span>"


hlmeths = HL()

for doc in query:
    print("DOC %s" % doc.title)
    snippets = query.getsnippets(
        doc, maxoccs=-1, ctxwords=ctxwords, nohl=nohl, sortbypage=False
    )
    print("Got %d snippets" % len(snippets))
    for snip in snippets:
        print("Page %d term [%s] snippet [%s]" % (snip[0], snip[1], snip[2]))
