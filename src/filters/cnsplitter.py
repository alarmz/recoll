#!/usr/bin/env python3
#################################
# Copyright (C) 2023 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
########################################################

#
# Interface to the Jieba Chinese text analyser: we receive text from our parent process and have it
# segmented by the analyser, then return the results. The analyser startup is very expensive
# (several seconds), which is why we can't just execute it from the main process.
#

import sys
import cmdtalk

# We can either use konlpy, which supports different analysers, or use
# the python-mecab-ko, a direct interface to mecab, with the same
# interface as konlpy https://pypi.org/project/python-mecab-ko/
import jieba


class Processor(object):
    def __init__(self, proto):
        self.proto = proto
        self.init = False

    def _init_tagger(self, taggername):
        # Nothing to do for now, really
        if taggername != "Jieba":
            raise Exception(f"Bad analyser name {taggername}")
        self.init = True

    def process(self, params):
        if "data" not in params:
            return {"error": "No data field in parameters"}
        # proto.log(f"DATA: {params['data']}")
        if not self.init:
            self._init_tagger(params["tagger"])

        tokens = jieba.tokenize(params["data"], mode="search")
        result = ""
        for e in tokens:
            # proto.log(f"term: {e}")
            result += (
                e[0].replace("\t", " ") + "\t" + str(e[1]) + "\t" + str(e[2]) + "\t"
            )
        return {"wordsandpos": result}


proto = cmdtalk.CmdTalk()
processor = Processor(proto)
cmdtalk.main(proto, processor)
