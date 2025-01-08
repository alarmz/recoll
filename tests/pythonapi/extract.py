import sys
import hashlib
from recoll import recoll
from recoll import rclextract

db = recoll.connect()
query = db.query()

# This normally has only one result, a well-known html file
nres = query.execute("HtmlAttachment_uniqueTerm", stemming=0)
print(f"Result count: {nres} {query.rowcount}")
doc = query.fetchone()
xtrac = rclextract.Extractor(doc)
doc = xtrac.textextract(doc.ipath)
print(f"Text length: {len(doc.text)}")

refdigest = 'bfbb63f7a245c31767585b45014dbd07'

# This normally has 2 results, one of which is a pdf attachment.
nres = query.execute("population_size_cultural_transmission", stemming=0)
for doc in query:
    if doc.mimetype == 'application/pdf':
        xtrac = rclextract.Extractor(doc)
        filename = xtrac.idoctofile(doc.ipath, doc.mimetype)
        f = open(filename, 'rb')
        data = f.read()
        f.close()
        m = hashlib.md5()
        m.update(data)
        digest = m.hexdigest()
        print(f"{digest}")
        if digest != refdigest:
            print("extract.py: wrong digest for extracted file!")
            
