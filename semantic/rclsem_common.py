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

from hashlib import md5

import chromadb
import ollama

from recoll import recoll
import rclconfig

def deb(s):
    print(f"{s}", file=sys.stderr)

def get_embedding(text, embedmodel):
    response = ollama.embed(model=embedmodel, input=text)
    return response['embeddings'][0]

_g_config = None

def get_rclconfig():
    return _g_config

def common_init(confdir=""):
    # if confdir is empty (default config), we get the actual chosen value with getConfDir()
    rclconf = rclconfig.RclConfig(argcnf=confdir)
    confdir = rclconf.getConfDir()

    chromadbdir = rclconf.getConfParam("sem_chromadbdir")
    if not chromadbdir:
        chromadbdir = os.path.join(confdir, "chromadb")
    elif not os.path.isabs(chromadbdir):
        chromadbdir = os.path.join(confdir, chromadbdir)
    deb(f"Confdir: {confdir} chromadbdir {chromadbdir}")

    embedmodel = rclconf.getConfParam("sem_embedmodel")
    if not embedmodel:
        embedmodel = 'nomic-embed-text'

    embedsegsize = rclconf.getConfParam("sem_embedsegsize")
    if embedsegsize:
        embedsegsize = int(embedsegsize)
    else:
        embedsegsize = 1000
        
    rcldb = recoll.connect(confdir)

    # We can't use the config directory path as collection name. It must be:
    #   3-512 characters from [a-zA-Z0-9._-], starting and ending with a character in [a-zA-Z0-9]
    # We use the md5 of the confdir, and store the actual path as metadata
    # Also: should we use different databases for different Recoll indexes ? Or just separate the
    # collections ?
    chroma_client = chromadb.PersistentClient(path=chromadbdir)
    collname = md5(confdir.encode("UTF-8")).hexdigest()
    collection = chroma_client.get_or_create_collection(name=collname, metadata={"confdir":confdir})

    global _g_config
    _g_config = rclconf

    return (rcldb, collection, embedmodel, embedsegsize)


if __name__ == "__main__":
    rcldb, collection, embedmodel, embedsegsize = common_init()
