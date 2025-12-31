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

"""Break text in phrase based on fullstop characters"""

# Notes about breaking up text for embedding.
# - pdftohtml does not try to detect paragraphs and just generates line breaks with  <br/>
# - Many epubs have explicit <p> paragraphs, but not all. In any case, this is lost in the html to
#   text translation as <br/> and </p> yield the same text (this could be fixed).
# Other formats present similar issues.
# 
# It seems that the most reliable to detect a minimal unit of meaning is to use the sentences as
# delimited by periods.
#
# Single sentences are probably too short, so we define a target segment length and we fill this up
# as much as possible with complete sentences.
# If no periods are found at a given size limit, we break up the text at a word. If this is not
# possible, the data is deemed to not be natural text and discarded.
#
# TBD: Maybe we should overlap consecutive segments ? By 15% approximately ?


import re
import sys

def _deb(s):
    print(f"{s}", file=sys.stderr)
    
restr = r"\s+"
expr = re.compile(restr)

def dotbreak(data, segsize, prefix=""):
    sentences = []
    if isinstance(data, bytes):
        data = data.decode("utf-8", errors="replace")
    if prefix:
        prefix += ": "
        
    # Split into phrases, using the period characters.
    count=0
    segment = ""
    segments = []
    for phrase in data.split("."):
        # Replace all white space with single space chars
        phrase = expr.sub(' ', phrase).strip()
        if not phrase:
            continue
        
        #_deb(f"phrase: {phrase}")
        if len(phrase) >= segsize:
            # If phrase is very long, split it in words
            splitphr = phrase.split()
            avgwordlen = len(phrase) / len(splitphr)
            if avgwordlen > 10 or avgwordlen < 3:
                # Very long phrase and strange average word length?? Does not look like text
                continue
            # Else use the text.
            for word in splitphr:
                if len(segment) + len(word) < segsize:
                    segment += word + " "
                else:
                    segments.append(prefix + segment)
                    segment = word + " "
            segment = segment.strip() + ". "
        else:
            # Phrase needs no splitting (smaller than segment). Either add it to current or begin new
            # segment
            if len(segment) + len(phrase) < segsize:
                segment += phrase + ". "
            else:
                segments.append(prefix + segment)
                segment = phrase + ". "

    # Residual ?
    if len(segment):
        segments.append(prefix + segment)
        
    return segments


if __name__ == "__main__":
    r = dotbreak("""Edmund was fond of speaking to her of Miss Crawford, but he seemed
    to think it enough that the admiral had since been spared; and she scrupled
    to point out her own remarks to him, lest it should appear like ill-nature.
    The first actual pain which Miss Crawford occasioned her, was the consequence
    of an inclination to learn to ride, which the former caught soon after
    her being settled at Mansfield from the example of the young ladies
    at the park, and which, when Edmund's acquaintance with her increased,
    led to his encouraging the wish, and the offer of his own quiet mare
    for the purpose of her first attempts, as the best fitted for a beginner
    that either stable could furnish. No pain, no injury, however, was designed
    by him to his cousin in this offer: she was not to lose a day's exercise.
    Thisverylongwordinaphrase madeofverylongwords shoulddisappear mostcertainly dontyouthinkorwhat
    Thisverylongwordinaphrase madeofverylongwords shoulddisappear mostcertainly dontyouthinkorwhat.
    """, 100)
    for s in r:
        print(f"{s}")
