#!/usr/bin/python3
"""Show how to extract the document texts from an index which stores them,
which is the default for Recoll versions with Xapian 1.4 support, after 1.24.
Would not work with 1.23 and earlier. This also depends on the
indexStoreDocText configuration variable. The usual RECOLL_CONFDIR can be used
to determine the index we operate on.
Use pyloglevel/pylogfilename or redirect stderr to get rid of the log messages.
"""

import sys
from recoll import recoll


def deb(s):
    print("%s" % s, file=sys.stderr)


def usage():
    deb("Usage doctexts.py")
    sys.exit(1)


if len(sys.argv) != 1:
    usage()

db = recoll.connect()
q = db.query()
q.execute("mime:*", fetchtext=True)

ndocs = 0
for doc in q:
    ndocs += 1
    print("TITLE: %s" % doc.title)
    print("TEXT: %s" % doc.get("text"))

print("Got %d documents" % ndocs)
