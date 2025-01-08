import sys
from recoll import recoll

# Test the doc.getbinurl() method.
# Select file with a binary name (actually iso8859-1), open it and
# convert/print the contents (also iso8859-1)

db = recoll.connect()
query = db.query()

# This should select a file with an iso8859-1 file name
nres = query.execute("LATIN1NAME_UNIQUEXXX dir:iso8859name", stemming=0)

print(f"Xapian query: [{query.getxquery()}]")

print(f"Result count: {nres} {query.rowcount}")

for doc in query:
    print(f"{doc.filename}")
    burl = doc.getbinurl()
    bytesname = burl[7:]
    f = open(bytesname, 'rb')
    s = f.read()
    f.close()
    content = str(s, "iso8859-1")
    print(f"Contents: [{content}]")
