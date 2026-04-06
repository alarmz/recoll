Python samples:

rclmbox.py
backends
A sample external indexer and its backends link file (see the user manual
programming section). The sample indexes a directory containing mbox files.

rcldlkp.py
Another sample indexer for a simple %-separated record format.

recollq.py
recollqsd.py
Sample query programs based on the Python query interface.

recollgui/
A sample GUI based on the python query interface.

docdups.py
A script based on the Xapian Python interface which explores a Recoll index
and prints out sets of duplicate documents (based on the md5 hashes).

mutt-recoll.py
Interface between recoll and mutt (based on mutt-notmuch). Not related to
the Recoll Python API, this executes recollq.

trconfig.py
Not useful at all: internal exercises for the python rclconfig interface.
