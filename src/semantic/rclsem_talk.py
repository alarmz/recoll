#!/usr/bin/env python3

from getopt import getopt
import sys
import json

import cmdtalkplugin

from rclsem_common import common_init, deb
from rclsem_query import direct_query

# Func name to method mapper
dispatcher = cmdtalkplugin.Dispatch()

g_rcldb = None
g_collection = None
g_embedmodel = None

@dispatcher.record("query")
def query(a):
    global g_rcldb, g_collection, g_embedmodel
    # We get the recoll configuration in all questions, but it is only used once, at initialisation
    if not g_rcldb:
        g_rcldb, g_collection, g_embedmodel = common_init(a["confdir"])

    if "nres" in a:
        nres = int(a["nres"])
    else:
        nres = 5
    results = direct_query(g_rcldb, g_collection, g_embedmodel, a["question"], nres=nres)

    # Do something with the results and return a dict
    encoded = json.dumps(results)
    return {"results": encoded}

# Pipe message handler
msgproc = cmdtalkplugin.Processor(dispatcher)
msgproc.mainloop()
    
