#!/usr/bin/python3
"""This sample uses the Recoll Python API to index a directory
containing mbox files. This is not particularly useful as Recoll
itself can do this better (e.g. this script does not process
attachments), but it shows the use of most of the Recoll interface
features, except 'parent_udi' (we do not create a 'self' document to
act as the parent)."""

import sys
import glob
import os
import stat
import signal
import mailbox
import email.header
import email.utils

from recoll import recoll


def logmsg(s):
    print(f"rclmbox.py: {s}", file=sys.stderr)


# EDIT
# Change this for some directory with mbox files, such as a
# Thunderbird/Icedove mail storage directory.
mbdir = os.path.expanduser("~/mail")
# mbdir = os.path.expanduser("~/.icedove/n8n19644.default/Mail/Local Folders/")


# Utility: extract text for named header
def header_value(msg, nm, to_utf=False):
    value = msg.get(nm)
    if value == None:
        return ""
    # value = value.replace("\n", "")
    # value = value.replace("\r", "")
    parts = email.header.decode_header(value)
    univalue = ""
    for part in parts:
        try:
            if part[1] != None:
                univalue += part[0].decode(part[1]) + " "
            else:
                if isinstance(part[0], bytes):
                    univalue += part[0].decode("cp1252") + " "
                else:
                    univalue += part[0] + " "
        except Exception as err:
            logmsg("Failed decoding header: %s" % err)
            pass
    if to_utf:
        return univalue.encode("utf-8")
    else:
        return univalue


# Utility: extract text parts from body
def extract_text(msg):
    """Extract and decode all text/plain parts from the message"""
    text = ""
    # We only output the headers for previewing, else they're already
    # output/indexed as fields.
    if (
        "RECOLL_FILTER_FORPREVIEW" in os.environ
        and os.environ["RECOLL_FILTER_FORPREVIEW"] == "yes"
    ):
        text += "From: " + header_value(msg, "From") + "\n"
        text += "To: " + header_value(msg, "To") + "\n"
        text += "Subject: " + header_value(msg, "Subject") + "\n"
        text += "\n"
    for part in msg.walk():
        if part.is_multipart():
            pass
        else:
            ct = part.get_content_type()
            if ct.lower() == "text/plain":
                charset = part.get_content_charset("cp1252")
                try:
                    ntxt = part.get_payload(None, True).decode(charset)
                    text += ntxt
                except Exception as err:
                    logmsg("Failed decoding payload: %s" % err)
                    pass
    return text


class mbox_indexer:
    """The indexer classs. An object is created for indexing one mbox folder"""

    def __init__(self, db, mbfile):
        """Initialize for writable db recoll.Db object and mbfile mbox
        file. We retrieve the the file size and mtime."""
        self.db = db
        self.mbfile = mbfile
        stdata = os.stat(mbfile)
        self.fmtime = stdata[stat.ST_MTIME]
        self.fbytes = stdata[stat.ST_SIZE]
        self.msgnum = 1
        self.parent_udi = self.udi(0)

    def sig(self):
        """Create an update verification value for an mbox file: we use
        the modification time concatenated with the size"""
        return str(self.fmtime) + ":" + str(self.fbytes)

    def udi(self, msgnum):
        """Create a unique document identifier for a given message. This should
        be shorter than 150 bytes, which we optimistically don't check
        here, as we just concatenate the mbox file name and message
        number"""
        return self.mbfile + ":" + str(msgnum)

    def index(self):
        if not self.db.needUpdate(self.udi(0), self.sig()):
            logmsg("Index is up to date for %s" % self.mbfile)
            return None
        mb = mailbox.mbox(self.mbfile)
        for msg in mb.values():
            logmsg("Indexing message %d" % self.msgnum)
            self.index_message(msg)
            self.msgnum += 1
        # Finally create and add the top doc (must be done after indexing the subdocs because, else,
        # the up to date tests would succeed...
        doc = self.db.doc()
        doc.mimetype = "text/x-mail"
        doc.rclbes = "MBOX"
        doc.url = "file://" + self.mbfile
        doc.sig = self.sig()
        self.db.addOrUpdate(self.parent_udi, doc)

    def getdata(self, ipath):
        """Implements the 'fetch' data access interface (called at
        query time from the command line)."""
        # logmsg("mbox::getdata: ipath: %s" % ipath)
        imsgnum = int(ipath)
        mb = mailbox.mbox(self.mbfile)
        msgnum = 0
        for msg in mb.values():
            msgnum += 1
            if msgnum == imsgnum:
                return extract_text(msg)
        return ""

    def index_message(self, msg):
        doc = self.db.doc()

        # Misc standard recoll fields
        doc.author = header_value(msg, "From")
        doc.recipient = header_value(msg, "To") + " " + header_value(msg, "Cc")
        dte = header_value(msg, "Date")
        tm = email.utils.parsedate_tz(dte)
        if tm == None:
            doc.mtime = str(self.fmtime)
        else:
            doc.mtime = str(email.utils.mktime_tz(tm))
        doc.title = header_value(msg, "Subject")
        doc.fbytes = str(self.fbytes)

        # Custom field
        doc.myfield = "some value"

        # Main document text and MIME type
        doc.text = extract_text(msg)
        doc.dbytes = str(len(doc.text.encode("UTF-8")))
        doc.mimetype = "text/plain"

        # Store data for later "up to date" checks
        doc.sig = self.sig()

        # The rclbes field is the link between the index data and this
        # script when used at query time
        doc.rclbes = "MBOX"

        # These get stored inside the index, and returned at query
        # time, but the main identifier is the condensed 'udi'
        doc.url = "file://" + self.mbfile
        doc.ipath = str(self.msgnum)
        # The udi is the unique document identifier, later used if we
        # want to e.g. delete the document index data (and other ops).
        udi = self.udi(self.msgnum)

        self.db.addOrUpdate(udi, doc, parent_udi=self.parent_udi)


db = None


def handler(signum, frame):
    logmsg(f"Got signal")
    if db:
        db.close()
    logmsg(f"Exiting")
    sys.exit(1)


signal.signal(signal.SIGINT, handler)
signal.signal(signal.SIGTERM, handler)


# Index a directory containing mbox files
def index_mboxdir(dir):
    global db
    db = recoll.connect(writable=1)
    db.preparePurge("MBOX")
    entries = glob.glob(dir + "/*")
    for dirent in entries:
        if os.path.basename(dirent)[0] == "." or not os.path.isfile(dirent):
            continue
        logmsg(f"Processing {dirent}")
        mbidx = mbox_indexer(db, dirent)
        mbidx.index()
    db.purge()


usage_string = """Usage:
rclmbox.py index
    Index the directory (the path is hard-coded inside the script)
rclmbox.py <fetch|makesig> udi url ipath
    fetch subdoc data or make signature (query time)
"""


def usage():
    print("%s" % usage_string, file=sys.stderr)
    sys.exit(1)


if len(sys.argv) < 2:
    usage()
elif len(sys.argv) == 2 and sys.argv[1] == "index":
    index_mboxdir(mbdir)
elif len(sys.argv) == 5:
    cmd = sys.argv[1]
    udi = sys.argv[2]
    url = sys.argv[3]
    ipath = sys.argv[4]

    mbfile = url.replace("file://", "")
    # No need for a db for getdata or makesig.
    mbidx = mbox_indexer(None, mbfile)

    if cmd == "fetch":
        print(f"{mbidx.getdata(ipath)}", end="")
    elif cmd == "makesig":
        print(mbidx.sig(), end="")
    else:
        usage()
else:
    usage()
sys.exit(0)
