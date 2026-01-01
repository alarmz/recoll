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
from getopt import getopt
import readline

import chromadb
import ollama

from recoll import recoll
import rclconfig

from rclsem_segment import dotbreak
from rclsem_common import deb, common_init, get_embedding


# Reranking: impossible on a cpu
RERANKING_MODEL = "dengcao/Qwen3-Reranker-4B:Q5_K_M"
def rerank_results(question, candidates):
    output = {}
    for doc in candidates:
        # Simplified logic: use the chat endpoint to ask for relevance
        prompt = f"Query: {question}\nDocument: {doc}\n" + \
            f"Is this relevant? Answer only with a score 0-1."
        deb("Chatting")
        response = ollama.chat(model=RERANKING_MODEL,
                               messages=[{'role': 'user',
                                          'content': prompt}])
        deb(f"Chatdone: {response}")
        answer = response['message']['content'].strip().lower()
        deb(f"Answer: {answer}")
        output[f"{int(float(answer)*100):03d}"] = doc

    for k in output.keys():
        print(output[k])
        break


def direct_query(rcldb, collection, embedmodel, embedsegsize, question, nres=5):
    deb(f"direct_query: question:[{question}]")
    question_embedding = get_embedding(question, embedmodel)
    #deb(f"question_embedding: {question_embedding}")
    results = collection.query(
        query_embeddings=[question_embedding],
        n_results=nres,
    )
    #deb(f"Got ids: {results['ids']}")

    # We got the ids, which are made up of a recoll rcludi and a phrase offset. Fetch the texts
    myresults = []
    for id in results["ids"][0]:
        plus = id.rfind("+")
        rcludi = id[0:plus]
        rcludi = rcludi.rstrip("|")
        # Phrase index
        phridx = int(id[plus+1:])

        # Fetch the doc from recoll
        if rcludi.find("|") == -1:
            rcludi += "|"
        doc = rcldb.getDoc(rcludi)

        # We would store the texts if we were doing reranking ? Not even sure, maybe be use the
        # enlarged segment instead.
        #texts.append(doc.text)

        phrases = dotbreak(doc.text, embedsegsize)
        #deb(f"phridx {phridx} Got {len(phrases)} phrases")

        # The results are (rcludi, phraseindex, ctxbefore, segment, ctxafter)
        ctxbefore = ""
        segment = ""
        ctxafter = ""
        lines = []
        for step in range(-1, 2):
            idx = phridx + step
            if idx >= 0 and idx < len(phrases):
                if idx == phridx:
                    segment = phrases[idx]
                elif idx < phridx:
                    ctxbefore += phrases[idx]
                elif idx > phridx:
                    ctxafter += phrases[idx]
        myresults.append((rcludi, phridx, ctxbefore, segment, ctxafter))
    return myresults

   
########## main

if __name__ == "__main__":
    def printseg(begofline, seg):
        line = begofline
        lines = []
        for word in seg.split():
            line = line + " " + word
            if len(line) > 70:
                lines.append(line)
                line = begofline
        if line:
            lines.append(line)
        if lines:
            lines[-1] = lines[-1] + "."
        for line in lines:
            print(f"{line}")
            
    def print_result(rcludi, ctxbefore, segment, ctxafter):
        print(f"{rcludi}")
        printseg("   ", ctxbefore)
        printseg(" * ", segment)
        printseg("   ", ctxafter)
        
    def Usage():
        me = os.path.basename(sys.argv[0])
        print(f"Usage: {me} [-c confdir] [-n nres] [-t] [word [word ...]]", file=sys.stderr)
        print(f" -t: start a cmdtalk server", file=sys.stderr)
        print(f" -n nres: Number of results to print", file=sys.stderr)
        sys.exit(1)
        
    nres=5
    confdir=""
    try:
        options, args = getopt(sys.argv[1:], "c:n:")
    except:
        Usage()
        
    for opt,val in options:
        if opt == "-c":
            confdir = val
        elif opt == "-n":
            nres = int(val)
        else:
            Usage()
            
    # Remaining args if any are a single question to run
    question=""
    for word in args:
        question = question + " " + word
        
    rcldb, collection, embedmodel, embedsegsize = common_init(confdir)
    
    if question:
        results = direct_query(rcldb, collection, embedmodel, embedsegsize, question, nres=nres)
        for rcludi, phridx, ctxbefore, segment, ctxafter in results:
            print_result(rcludi, ctxbefore, segment, ctxafter)
    else:
        while True:
            line = input("--> ")
            if not line:
                break
            results = direct_query(rcldb, collection, embedmodel, embedsegsize, line, nres=nres)
            for rcludi, phridx, ctxbefore, segment, ctxafter in results:
                print_result(rcludi, ctxbefore, segment, ctxafter)
