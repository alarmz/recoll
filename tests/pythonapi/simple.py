import sys
from recoll import recoll

db = recoll.connect()
query = db.query()

query.sortby("url")
nres = query.execute("huniique", stemlang="english")
print(f"Xapian query: [{query.getxquery()}]")

print(f"Result count: {nres} {query.rowcount}")

print("for i in range(nres):")
for i in range(nres):
    doc = query.fetchone()
    print(f"{doc.filename}")

query.scroll(0, 'absolute')
print("\nfor doc in query:")
for doc in query:
    print(f"{doc.filename}")

try:
    query.scroll(0, 'badmode')
except:
    print("\nCatched bad mode. (ok)")
