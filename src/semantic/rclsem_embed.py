#!/usr/bin/env python3
# Copyright (C) 2025 J.F.Dockes
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
import os
import getopt
from hashlib import md5

import chromadb
import ollama

from recoll import recoll
import rclconfig

from rclsem_segment import dotbreak
from slicelist import slicelist
from rclsem_common import deb, common_init, get_embedding, get_rclconfig

def update_embeddings(rcldb, collection, embedmodel, embedsegsize):

    # It is possible to configure a restricting recoll query instead of processing the whole index
    rclconf = get_rclconfig()
    rclquery = rclconf.getConfParam("sem_rclquery")
    if not rclquery:
        rclquery = "mime:*"
        
    query = rcldb.query()
    query.execute(rclquery, fetchtext=True)

    ids=[]
    for doc in query:
        # Each segment sent for embedding gets the doc file name and title as prefix to maintain
        # global context.
        prefix = os.path.splitext(doc.filename)[0] + ": " + doc.title
        # Break up the document text in segments for embedding.
        sentences = dotbreak(doc.text, embedsegsize, prefix=prefix)
        # Doc identifier
        rcludi = doc.rcludi
        deb(f"{rcludi}: Got {len(sentences)} phrases")

        if len(sentences) == 0:
            continue
        
        # We both slice and batch the chroma updates, breaking up the list of phrases by slices of
        # 100
        for sentidx, sentslice in slicelist(sentences, 100):
            # Process one slice of segments. Sentidx is the base index for the slice in the whole
            # segments list.
            deb(f"  sentidx {sentidx} len(sentslice) {len(sentslice)}")
            ids=[]
            embeddings=[]
            for i in range(len(sentslice)-1):
                # We store a segment index (slicebase+offsetinslice), which forces re-splitting at
                # query time. We could store byte offsets instead for faster query time access ?
                idx = sentidx + i
                segid = rcludi + "+" + str(idx)
                # Check if already there
                results = collection.get(ids=[segid])
                if results['ids']:
                    #deb("Already there")
                    continue
                deb(f"Adding SEGID [{segid}]\n   TEXT[{sentslice[i]}]")
                ids.append(segid)
                embeddings.append(get_embedding(sentslice[i], embedmodel))
            # Store the processed slice in chromadb
            if len(ids):
                collection.add(
                    ids=ids,
                    embeddings=embeddings,
                )


########## main

def Usage(s=""):
    print(f"{s}", file=sys.stderr)
    print(f"Usage: {os.path.basename(sys.argv[0])} [-c confdir]", file=sys.stderr)
    sys.exit(1)
    
confdir=""
try:
    options, args = getopt.getopt(sys.argv[1:], "c:")
except Exception as ex:
    Usage(str(ex))
for opt,val in options:
    if opt == "-c":
        confdir = val
    else:
        Usage(f"bad option {opt}")


rcldb, collection, embedmodel, embedsegsize = common_init(confdir)

update_embeddings(rcldb, collection, embedmodel, embedsegsize)
