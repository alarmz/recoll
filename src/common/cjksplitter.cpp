/* Copyright (C) 2022 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "cjksplitter.h"

#include <cassert>

#include "smallut.h"
#include "log.h"
#include "utf8iter.h"

static const int o_CJKMaxNgramLen{5};
int CJKSplitter::max_ngramlen()
{
    return o_CJKMaxNgramLen;
}

// Note that this should probably be renamed NGRAMSplitter as this is also used for other scripts.
//
// We output ngrams.
// For example: for input "abc" and ngramlen==2, we generate: a ab b bc c as words.
//
// This is very different from the normal behaviour, so we don't use doemit() or emitterm().
//
// The routine is sort of a mess and goes to show that we'd probably be better off converting the
// whole buffer to utf32 on entry...
bool CJKSplitter::text_to_words(Utf8Iter& it, unsigned int *cp, int& wordpos)
{
    LOGDEB1("cjk_to_words: wordpos " << wordpos << "\n");
    int flags = m_sink.flags();
    
    // We use an offset buffer to remember the starts of the utf-8
    // characters which we still need to use.
    assert(m_ngramlen < o_CJKMaxNgramLen);
    std::string::size_type boffs[o_CJKMaxNgramLen+1];
    std::string mybuf;
    std::string::size_type myboffs[o_CJKMaxNgramLen+1];
    
    // Current number of valid offsets;
    int nchars = 0;
    unsigned int c = 0;
    bool spacebefore{false};
    for (; !it.eof() && !it.error(); it++) {
        c = *it;
        LOGDEB1("Processing [" << (std::string)it << "]\n");
        // We had a version which ignored whitespace for some time,
        // but this was a bad idea. Only break on a non-cjk
        // *alphabetic* character, except if following punctuation, in
        // which case we return for any non-cjk. This allows compound
        // cjk+numeric spans, or punctuated cjk spans to be
        // continually indexed as cjk. The best approach is a matter
        // of appreciation...
        if ((spacebefore || (c > 255
#ifdef TESTING_NGRAMS
                             || islower(c)
#else
                             || isalpha(c)
#endif
                 )) && !TextSplit::isNGRAMMED(c)) {
            // Return to normal handler
            break;
        }
        if (TextSplit::isSpace(c)) {
            // Flush the ngram buffer and go on
            nchars = 0;
            mybuf.clear();
            spacebefore = true;
            continue;
        } else {
            spacebefore = false;
        }
        if (nchars == m_ngramlen) {
            // Offset buffer full, shift it. Might be more efficient
            // to have a circular one, but things are complicated
            // enough already...
            for (int i = 0; i < nchars-1; i++) {
                boffs[i] = boffs[i+1];
            }
            for (int i = 0; i < nchars-1; i++) {
                myboffs[i] = myboffs[i+1];
            }
        }  else {
            nchars++;
        }

        // Copy to local buffer, and note local offset
        myboffs[nchars-1] = mybuf.size();
        it.appendchartostring(mybuf);
        // Take note of document byte offset for this character.
        boffs[nchars-1] = it.getBpos();

        // Output all new ngrams: they begin at each existing position
        // and end after the new character. onlyspans->only output
        // maximum words, nospans=> single chars
        if (!(flags & TextSplit::TXTS_ONLYSPANS) || nchars == m_ngramlen) {
            auto btend = it.getBpos() + it.getBlen();
            int loopbeg = (flags & TextSplit::TXTS_NOSPANS) ? nchars-1 : 0;
            int loopend = (flags & TextSplit::TXTS_ONLYSPANS) ? 1 : nchars;
            for (int i = loopbeg; i < loopend; i++) {
                // Because of the whitespace handling above there may be whitespace in the
                // buffer. Strip it from the output words. This means that the offs/size will be
                // slightly off (->highlights), to be fixed one day.
                auto word = mybuf.substr(myboffs[i], mybuf.size() - myboffs[i]);
                LOGDEB1("EMITTING [" << trimstring(word, "\r\n\f \t") << "] at pos " <<
                        wordpos - (nchars - i - 1) << '\n');
                if (!m_sink.takeword(trimstring(word, "\r\n\f \t"), 
                                     wordpos - (nchars - i - 1), boffs[i], btend)) {
                    return false;
                }
            }

            if ((flags & TextSplit::TXTS_ONLYSPANS)) {
                // Only spans: don't overlap: flush buffer
                nchars = 0;
                mybuf.clear();
            }
        }
        // Increase word position by one, other words are at an
        // existing position. This could be subject to discussion...
        wordpos++;
    }

    // If onlyspans is set, there may be things to flush in the buffer
    // first
    if ((flags & TextSplit::TXTS_ONLYSPANS) && nchars > 0 && nchars != m_ngramlen)  {
        int btend = int(it.getBpos()); // Current char is out
        // See comment before takeword above.
        auto word = mybuf.substr(myboffs[0], mybuf.size() - myboffs[0]);
        LOGDEB1("EMITTING [" << trimstring(word, "\r\n\f \t") << "] at pos " <<
                wordpos - nchars << '\n');
        if (!m_sink.takeword(trimstring(word, "\r\n\f \t"), wordpos - nchars, boffs[0], btend)) {
            return false;
        }
    }

    *cp = c;
    return true;
}
