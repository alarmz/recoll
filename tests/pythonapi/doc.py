import sys
from recoll import recoll

db = recoll.connect()
query = db.query()

nres = query.execute("testfield:testfieldvalue1", stemming=0)
print(f"Xapian query: [{query.getxquery()}]")

print(f"Result count: {nres} {query.rowcount}")

for doc in query:
    print(f"doc.title: [{doc.title}]")
    print(f"doc.testfield: [{doc.testfield}]")
    for fld in ('title', 'testfield', 'filename'):
        print(f"getattr(doc, {fld}) -> [{getattr(doc,fld)}]")
        print(f"doc.get({fld}) -> [{doc.get(fld)}]")
    print("\nfor fld in sorted(doc.keys()):")
    # Note: 2025-01-08: udi now not retrieved by dflt. Do it to avoid changing test output
    udi = doc['rcludi']
    for fld in sorted(doc.keys()):
        # Sig keeps changing and makes it impossible to compare test results
        if fld != 'sig':
            print(f"[{fld}] -> [{getattr(doc, fld)}]")
    print("\nfor k,v in sorted(doc.items().items()):")
    for k,v in sorted(doc.items().items(), key=lambda itm: itm[0]):
        # Sig keeps changing and makes it impossible to compare test results
        if k != 'sig':
            print(f"[{k}] -> [{v}]")

print("\nAccented query:")
uqs = 'title:"\u00e9t\u00e9 \u00e0 no\u00ebl"'
print(f"User query [{uqs}]")
nres = query.execute(uqs, stemming=0)
#nres = query.execute('title:"ete a noel"', stemming=0)
qs = "Xapian query: [%s]" % query.getxquery()
print(qs)
print(f"nres {nres}")
doc = query.fetchone()
print(f"doc.title: [{doc.title}]")

query.execute("onlynametestkeyword")
doc = query.fetchone()
print("HIGHLIGHT DEFAULT===============")
s = query.makedocabstract(doc)
print(f"{s}")

print("HIGHLIGHT METHODS SET TO NONE ===============")
s = query.makedocabstract(doc, methods=None)
print(f"{s}")

print("HIGHLIGHT NOHL TRUE ===============")
s = query.makedocabstract(doc, nohl=True)
print(f"{s}")

print("HIGHLIGHT CUSTOM METHODS===============")
class HlMeths:
    def startMatch(self, idx):
        return '<span class="CUSTOM search-result-highlight">'
    def endMatch(self):
        return '</span> CUSTOM'

s = query.makedocabstract(doc, methods = HlMeths())
print(f"{s}")
    

