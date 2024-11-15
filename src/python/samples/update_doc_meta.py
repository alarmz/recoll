#!/usr/bin/python3

import sys
from getopt import getopt

from recoll import recoll
from recoll import fsudi


def msg(s):
    print(f"{s}", file=sys.stderr)


def usage():
    msg("Usage: update_doc_meta.py [-c <configdir>] <filepath>")
    sys.exit(1)


confdir = ""

try:
    options, args = getopt(sys.argv[1:], "c:")
except Exception as ex:
    msg(ex)
    sys.exit(1)
for opt, val in options:
    if opt == "-c":
        confdir = val
    else:
        msg(f"{opt} ??")
        Usage()
if len(args) == 0:
    msg("No path found in command line")
    Usage()

path = args[0]

udi = fsudi.fs_udi(path)
db = recoll.connect(confdir, writable=1)
doc = db.getDoc(udi)
print(f"doc title: [{doc['title']}] author [{doc['author']}] sig [{doc['sig']}]")

doc["author"] = "somestrangeauthorname"

db.addOrUpdate(udi, doc, metaonly=1)
db.close()
print("Metadata updated")

db = recoll.connect(confdir, writable=0)
q = db.query()
q.execute("author:somestrangeauthorname")
for doc in q:
    print(f"{doc.url}")
    print(f"doc title: [{doc['title']}] author [{doc['author']}] sig [{doc['sig']}]")
