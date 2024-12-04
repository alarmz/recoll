/* Copyright (C) 2004-2024 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "autoconfig.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <unordered_set>

#include "textsplit.h"
// #define LOGGER_LOCAL_LOGINC 3
#include "log.h"
//#define UTF8ITER_CHECK
#include "utf8iter.h"
#include "uproplist.h"
#include "smallut.h"
#include "rclconfig.h"
#include "cjksplitter.h"
#include "kosplitter.h"
#include "cnsplitter.h"

using namespace std;

// Splitting a text into words. The code in this file works with utf-8 in a semi-clean way (see
// uproplist.h). Ascii still gets special treatment in the sense that many special characters can
// only be ascii (e.g. @, _,...). However, this compromise works quite well while being much more
// light-weight than a full-blown Unicode approach (ICU...)


// Decide if we treat katakana as western scripts, splitting into words instead of n-grams. This is
// not absurd (katakana is a kind of alphabet, albeit phonetic and syllabic and is mostly used to
// transcribe western words), but it does not work well because japanese uses separator-less
// compound katakana words, and because the plural terminaisons are irregular and would need a
// specialized stemmer. So we for now process katakana as the rest of cjk, using ngrams. This would
// change if we used an external "intelligent" segmenter like is possible for Hangul. Then Katakana
// processing would depend on the configuration and not a hard ifdef.
#undef KATAKANA_AS_WORDS

// Same for Korean syllabic, and same problem. However we have a runtime option to use an external
// text analyser for Hangul, so this is defined at compile time.
#define HANGUL_AS_WORDS

// Same for Chinese, we may use an external Jieba text segmenter.
// https://github.com/fxsjy/jieba
#define CHINESE_AS_WORDS

// ASCII character classes: we have three main groups, and then some chars are their own class
// because they want special handling.
// 
// We have an array with 256 slots where we keep the character types. 
// The array could be fully static, but we use a small function to fill it once.
// The array is actually a remnant of the original version which did no utf8.
// Only the lower 127 slots are now used, but keep it at 256 because it makes some tests in the code
// simpler.
const unsigned int charclasses_size = 256;
static int charclasses[charclasses_size];


bool TextSplit::o_processCJK{true};
int  TextSplit::o_CJKNgramLen{2};
bool TextSplit::o_noNumbers{false};
bool TextSplit::o_deHyphenate{false};
// Max word size in Unicode characters
int  TextSplit::o_maxWordLength{40};
int  TextSplit::o_maxWordsInSpan{6};

static std::unordered_set<unsigned int> o_idxpunct;
static bool o_haveidxpunct;

static bool o_exthangultagger{false};
static bool o_extchinesetagger{false};

// This is changed to 0 if _ is processed as a letter
static char underscoreatend = '_';

void TextSplit::staticConfInit(RclConfig *config)
{
    config->getConfParam("maxtermlength", &o_maxWordLength);
    config->getConfParam("maxwordsinspan", &o_maxWordsInSpan);

    std::string sidxpunct;
    config->getConfParam("indexedpunctuation", sidxpunct);
    if (!sidxpunct.empty()) {
        Utf8Iter it(sidxpunct);
        for (; !it.eof() && !it.error(); it++) {
            o_idxpunct.insert(*it);
        }
        o_haveidxpunct = true;
    }
    
    bool bvalue{false};
    if (config->getConfParam("nocjk", &bvalue) && bvalue == true) {
        o_processCJK = false;
    } else {
        o_processCJK = true;
        int ngramlen;
        if (config->getConfParam("cjkngramlen", &ngramlen)) {
            o_CJKNgramLen = ngramlen <= CJKSplitter::max_ngramlen() ?
                ngramlen : CJKSplitter::max_ngramlen();
        }
    }

    bvalue = false;
    if (config->getConfParam("nonumbers", &bvalue)) {
        o_noNumbers = bvalue;
    }

    bvalue = false;
    if (config->getConfParam("dehyphenate", &bvalue)) {
        o_deHyphenate = bvalue;
    }

    bvalue = false;
    if (config->getConfParam("backslashasletter", &bvalue)) {
        if (bvalue) {
        } else {
            charclasses[int('\\')] = SPACE;
        }
    }

    bvalue = false;
    if (config->getConfParam("underscoreasletter", &bvalue)) {
        if (bvalue) {
            charclasses[int('_')] = A_LLETTER;
            underscoreatend = 0;
        }
    }

    string kotagger;
    config->getConfParam("hangultagger", kotagger);
    if (!kotagger.empty()) {
        o_exthangultagger = true;
        koStaticConfInit(config, kotagger);
    }

    string chinesetagger;
    config->getConfParam("chinesetagger", chinesetagger);
    if (!chinesetagger.empty()) {
        o_extchinesetagger = true;
        cnStaticConfInit(config, chinesetagger);
    }
}


// Non-ASCII UTF-8 characters are handled with sets holding all characters with interesting
// properties. This is far from full-blown management of Unicode properties, but seems to do the job
// well enough in most common cases
static vector<unsigned int> vpuncblocks;
static std::unordered_set<unsigned int> spunc;
static std::unordered_set<unsigned int> visiblewhite;
static std::unordered_set<unsigned int> sskip;

class CharClassInit {
public:
    CharClassInit() {
        unsigned int i;

        // Set default value for all: SPACE
        for (i = 0 ; i < 256 ; i ++)
            charclasses[i] = TextSplit::SPACE;

        char digits[] = "0123456789";
        for (i = 0; i  < strlen(digits); i++)
            charclasses[int(digits[i])] = TextSplit::DIGIT;

        char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (i = 0; i  < strlen(upper); i++)
            charclasses[int(upper[i])] = TextSplit::A_ULETTER;

        char lower[] = "abcdefghijklmnopqrstuvwxyz";
        for (i = 0; i  < strlen(lower); i++)
            charclasses[int(lower[i])] = TextSplit::A_LLETTER;

        char wild[] = "*?[]";
        for (i = 0; i  < strlen(wild); i++)
            charclasses[int(wild[i])] = TextSplit::WILD;

        // Characters with special treatment:
        //
        // The first ones are mostly span-constructing "glue"
        // characters, for example those typically allowing us to
        // search for an email address as a whole (bob@isp.org instead
        // of as a phrase "bob isp org"
        //
        // The case of the minus sign is a complicated one. It went
        // from glue to non-glue to glue along Recoll versions. 
        // See minus-hyphen-dash.txt in doc/notes
        char special[] = ".@+-#'_\n\r\f";
        for (i = 0; i  < strlen(special); i++)
            charclasses[int(special[i])] = special[i];

        for (i = 0; i < sizeof(unipunc) / sizeof(int); i++) {
            spunc.insert(unipunc[i]);
        }
        spunc.insert((unsigned int)-1);

        for (i = 0; i < sizeof(unipuncblocks) / sizeof(int); i++) {
            vpuncblocks.push_back(unipuncblocks[i]);
        }
        assert((vpuncblocks.size() % 2) == 0);

        for (i = 0; i < sizeof(avsbwht) / sizeof(int); i++) {
            visiblewhite.insert(avsbwht[i]);
        }
        for (i = 0; i < sizeof(uniskip) / sizeof(int); i++) {
            sskip.insert(uniskip[i]);
        }
    }
};
static const CharClassInit charClassInitInstance;

static inline bool isvisiblewhite(int c)
{
    return visiblewhite.find(c) != visiblewhite.end();
}

// Character class: if we do not find it to be special, process as letter.
int TextSplit::whatcc(unsigned int c)
{
    if (c <= 127) {
        return charclasses[c]; 
    } else {
        if (c == 0x2010 || c == 0x2019 || c == 0x275c || c == 0x02bc) {
            return c;
        } else if (sskip.find(c) != sskip.end()) {
            return SKIP;
        } else if (spunc.find(c) != spunc.end()) {
            return SPACE;
        } else {
            auto it = lower_bound(vpuncblocks.begin(), vpuncblocks.end(), c);
            if (it == vpuncblocks.end())
                return LETTER;
            if (c == *it)
                return SPACE;
            if ((it - vpuncblocks.begin()) % 2 == 1) {
                return SPACE;
            } else {
                return LETTER;
            }
        } 
    }
}

// Chinese, Japanese, Korean Unicode character ranges. CJK text is generally indexed using an n-gram
// method.
// Exceptions:
//  Hangul: we can use an external text linguistic-aware segmenter.
//  Chinese: same. Using Jieba
//  Katakana: not successful for now. n-grams.
//
// 1100..11FF; Hangul Jamo (optional: see UNICODE_IS_HANGUL)
// 2E80..2EFF; CJK Radicals Supplement
// 3000..303F; CJK Symbols and Punctuation
// 3040..309F; Hiragana
// 30A0..30FF; Katakana
// 3100..312F; Bopomofo
// 3130..318F; Hangul Compatibility Jamo (optional: see UNICODE_IS_HANGUL)
// 3190..319F; Kanbun
// 31A0..31BF; Bopomofo Extended
// 31C0..31EF; CJK Strokes
// 31F0..31FF; Katakana Phonetic Extensions
// 3200..32FF; Enclosed CJK Letters and Months
// 3300..33FF; CJK Compatibility
// 3400..4DBF; CJK Unified Ideographs Extension A
// 4DC0..4DFF; Yijing Hexagram Symbols
// 4E00..9FFF; CJK Unified Ideographs 
// A700..A71F; Modifier Tone Letters
// AC00..D7AF; Hangul Syllables (optional: see UNICODE_IS_HANGUL)
// F900..FAFF; CJK Compatibility Ideographs
// FE30..FE4F; CJK Compatibility Forms
// FF00..FFEF; Halfwidth and Fullwidth Forms
// 20000..2A6DF; CJK Unified Ideographs Extension B
// 2F800..2FA1F; CJK Compatibility Ideographs Supplement
#define UNICODE_IS_CJK(p)                       \
    (((p) >= 0x1100 && (p) <= 0x11FF) ||        \
     ((p) >= 0x2E80 && (p) <= 0x2EFF) ||        \
     ((p) >= 0x3000 && (p) <= 0x9FFF) ||        \
     ((p) >= 0xA700 && (p) <= 0xA71F) ||        \
     ((p) >= 0xAC00 && (p) <= 0xD7AF) ||        \
     ((p) >= 0xF900 && (p) <= 0xFAFF) ||        \
     ((p) >= 0xFE30 && (p) <= 0xFE4F) ||        \
     ((p) >= 0xFF00 && (p) <= 0xFFEF) ||        \
     ((p) >= 0x20000 && (p) <= 0x2A6DF) ||      \
     ((p) >= 0x2F800 && (p) <= 0x2FA1F))

// We should probably map 'fullwidth ascii variants' and 'halfwidth
// katakana variants' to something else.  Look up "Kuromoji" Lucene
// filter, KuromojiNormalizeFilter.java
// 309F is Hiragana.
#define UNICODE_IN_KATAKANA_RANGE(p)            \
    ((p) != 0x309F &&                           \
     (((p) >= 0x3099 && (p) <= 0x30FF) ||       \
      ((p) >= 0x31F0 && (p) <= 0x31FF)))
#ifdef KATAKANA_AS_WORDS
#define UNICODE_IS_KATAKANA(p)  UNICODE_IN_KATAKANA_RANGE(p)
#else
#define UNICODE_IS_KATAKANA(p) false
#endif

// Hangul ranges and options. If no external tagger is configured, we process HANGUL as generic CJK
// (n-grams): IS_HANGUL will be false and IS_CJK true.
#define UNICODE_IN_HANGUL_RANGE(p)             \
        (((p) >= 0x1100 && (p) <= 0x11FF) ||   \
         ((p) >= 0x3130 && (p) <= 0x318F) ||   \
         ((p) >= 0x3200 && (p) <= 0x321e) ||   \
         ((p) >= 0x3248 && (p) <= 0x327F) ||   \
         ((p) >= 0x3281 && (p) <= 0x32BF) ||   \
         ((p) >= 0xAC00 && (p) <= 0xD7AF))
#ifdef HANGUL_AS_WORDS
#define UNICODE_IS_HANGUL(p) (o_exthangultagger &&  UNICODE_IN_HANGUL_RANGE(p))
#else
#define UNICODE_IS_HANGUL(p) false
#endif

// Chinese ranges: everything in CJK range defined above which is not Katakana or Hangul
#define UNICODE_IN_CHINESE_RANGE(p) \
    (UNICODE_IS_CJK(p) && !(UNICODE_IN_KATAKANA_RANGE(p) || UNICODE_IN_HANGUL_RANGE(p)))
#ifdef CHINESE_AS_WORDS
// If we are not using an external text splitter, IS_CJK will be true and IS_CHINESE false
#define UNICODE_IS_CHINESE(p) (o_extchinesetagger && UNICODE_IN_CHINESE_RANGE(p))
#else
#define UNICODE_IS_CHINESE(p) false
#endif

// Tibetan. For now, uses n-grams. Not CJK though so handled a bit differently (because not in the
// big CJK range)
#define UNICODE_IN_TIBETAN_RANGE(p) \
    ((p) > 0x0f00 && (p) <= 0x0fff)
#define UNICODE_IS_TIBETAN(p) UNICODE_IN_TIBETAN_RANGE(p)

// Thai and Lao range are contiguous
#define UNICODE_IN_THAI_LAO_RANGE(p) \
    ((p) > 0x0e00 && (p) <= 0x0eff)
#define UNICODE_IS_THAI_OR_LAO(p) UNICODE_IN_THAI_LAO_RANGE(p)

// Burmese
#define UNICODE_IN_BURMESE_RANGE(p) \
    ((p) > 0x1000 && (p) <= 0x109f)
#define UNICODE_IS_BURMESE(p) UNICODE_IN_BURMESE_RANGE(p)

// Non CJK scripts processed as ngrams
#define UNICODE_IS_OTHER_NGRAM(p) (UNICODE_IS_TIBETAN(p) || UNICODE_IS_THAI_OR_LAO(p) || \
                                   UNICODE_IS_BURMESE(p))

// Just for testing the ngram splitter: this European reader finds it easier to look at results when
// using a simple A-Z range.
#define UNICODE_IN_TESTNGRAM_RANGE(p) \
    ((p) >= 'A' && (p) <= 'B')
#ifdef TESTING_NGRAMS
#define UNICODE_IS_TESTNGRAM(p) UNICODE_IN_TESTNGRAM_RANGE(p)
#else
#define UNICODE_IS_TESTNGRAM(p) false
#endif

bool TextSplit::isSpace(int c)
{
    return whatcc(c) == SPACE;
}
bool TextSplit::isCJK(int c)
{
    return UNICODE_IS_CJK(c);
}
bool TextSplit::noStemming(int c)
{
    return UNICODE_IS_CJK(c) || UNICODE_IS_OTHER_NGRAM(c);
}
bool TextSplit::isKATAKANA(int c)
{
    PRETEND_USE(c);
    return UNICODE_IS_KATAKANA(c);
}
bool TextSplit::isHANGUL(int c)
{
    PRETEND_USE(c);
    return UNICODE_IS_HANGUL(c);
}
bool TextSplit::isCHINESE(int c)
{
    PRETEND_USE(c);
    return UNICODE_IS_CHINESE(c);
}
bool TextSplit::isNGRAMMED(int c)
{
    return UNICODE_IS_OTHER_NGRAM(c) || UNICODE_IS_TESTNGRAM(c) || 
        (UNICODE_IS_CJK(c) && !UNICODE_IS_KATAKANA(c) && !UNICODE_IS_HANGUL(c) &&
         !UNICODE_IS_CHINESE(c));
}


// This is used to detect katakana/other transitions, which must trigger a word split (there is not
// always a separator, and katakana is otherwise treated like other, in the same routine, unless CJK
// which has its span reader causing a word break)
enum CharSpanClass {CSC_OTHER, CSC_HANGUL, CSC_CHINESE, CSC_CJK, CSC_KATAKANA, CSC_OTHERNGRAM,
                    CSC_TESTNGRAM};
std::vector<CharFlags> csc_names {
    CHARFLAGENTRY(CSC_HANGUL), CHARFLAGENTRY(CSC_CHINESE), CHARFLAGENTRY(CSC_CJK),
    CHARFLAGENTRY(CSC_KATAKANA), CHARFLAGENTRY(CSC_OTHER), CHARFLAGENTRY(CSC_OTHERNGRAM),
    CHARFLAGENTRY(CSC_TESTNGRAM)};

// Final term checkpoint: do some checking (the kind which is simpler to do here than in the main
// loop), then send term to our client.
inline bool TextSplit::emitterm(bool isspan, const string& w, int pos, size_t btstart, size_t btend)
{
    LOGDEB2("TextSplit::emitterm: [" << w << "] pos " << pos << "\n");

#ifdef TEXTSPLIT_STATS
    // Update word length statistics. Do this before we filter out
    // long words because stats are used to detect bad text
    if (!isspan || m_wordLen == m_span.length())
        m_stats.newsamp(m_wordChars);
#else
    PRETEND_USE(isspan);
#endif

    // If the size in bytes is > max size in unicode chars, compute the Unicode length.
    // The bytes test saves a bit of CPU as most words will skip the unicode test.
    // In theory we know the character size, but in practise, it was lost somewhere in the maze.
    if (w.size() == 0 || w.size() >= (size_t)o_maxWordLength) {
        int l = utf8len(w);
        if (l == 0 || l > o_maxWordLength) {
            discarded(w, pos, btstart, btend, WORD_TOO_LONG);
            return true;
        }
    }
    if (w.size() == 1) {
        // 1 byte word: we index single ASCII letters and digits, but nothing else. Why?
        unsigned int c = ((unsigned int)w[0]) & 0xff;
        if (charclasses[c] != A_ULETTER && charclasses[c] != A_LLETTER && charclasses[c] != DIGIT &&
            (!(m_flags & TXTS_KEEPWILD) || charclasses[c] != WILD)  ) {
            LOGDEB2("TextSplit::emitterm: erasing single letter term " << c << "\n");
            return true;
        }
    }
    // Detect doublons. Happens...?
    if (pos != m_prevpos || w.size() != m_prevlen) {
        bool ret = takeword(w, pos, btstart, btend);
        m_prevpos = pos;
        m_prevlen = w.size();
        return ret;
    }
    LOGDEB2("TextSplit::emitterm:dup: [" << w << "] pos " << pos << "\n");
    return true;
}

// Check for an acronym/abbreviation spelled like I.B.M. This only works with ASCII.
bool TextSplit::span_is_initials(string& initials)
{
    LOGDEB1("span_is_initials: wordlen " << m_wordLen << " spanlen " << m_span.length() <<
            " span ["<< m_span << "]\n");
    if (m_wordLen == m_span.length() || (m_span.length() & 1) ||
        m_span.length() <= 2 || m_span.length() > 20) {
        return false;
    }

    // Check odd chars are '.'
    for (unsigned int i = 1 ; i < m_span.length(); i += 2) {
        if (m_span[i] != '.') {
            return false;
        }
    }
    // Check that even chars are letters
    for (unsigned int i = 0 ; i < m_span.length(); i += 2) {
        int c = m_span[i];
        if (!((c >= 'a' && c <= 'z')||(c >= 'A' && c <= 'Z'))) {
            return false;
        }
    }
    initials.reserve(m_span.length()/2 + 1);
    for (unsigned int i = 0; i < m_span.length(); i += 2) {
        initials += m_span[i];
    }
    return true;
}


// Generate terms from span. Have to take into account the flags: ONLYSPANS, NOSPANS, noNumbers
bool TextSplit::words_from_span(size_t bp)
{
#if 0
    std::cerr<<"Span: ["<<m_span<<"] "<<" bp "<<bp<<" w_i_s size: "<<m_words_in_span.size()<<" :";
    for (const auto& [beg, nd] : m_words_in_span)
        std::cerr << " [" << beg << " " << nd << "] ";
    cerr << "\n";
#endif
    int spanwords = int(m_words_in_span.size());
    // It seems that something like: tv_combo-sample_util.Po@am_quote can get the splitter to call
    // doemit with a span of '@' and words_in_span==0, which then causes a crash when accessing
    // words_in_span[0] if the stl assertions are active (e.g. Fedora RPM build). Not too sure what
    // the right fix would be, but for now, just defend against it
    if (spanwords == 0) {
        return true;
    }
    int pos = m_spanpos;
    // Byte position of the span start
    size_t spboffs = bp - m_span.size();

    if (o_deHyphenate && spanwords == 2 && m_span[m_words_in_span[0].second] == '-') {
        unsigned int s0 = m_words_in_span[0].first;
        unsigned int l0 = m_words_in_span[0].second - m_words_in_span[0].first;
        unsigned int s1 = m_words_in_span[1].first;
        unsigned int l1 = m_words_in_span[1].second - m_words_in_span[1].first;
        string word = m_span.substr(s0, l0) + m_span.substr(s1, l1);
        if (l0 && l1) 
            emitterm(false, word, m_spanpos, spboffs, spboffs + m_words_in_span[1].second);
    }

    for (int i = 0; i < ((m_flags&TXTS_ONLYSPANS) ? 1 : spanwords); i++) {
        int deb = m_words_in_span[i].first;
        bool noposinc = m_words_in_span[i].second == deb;
        for (int j = ((m_flags&TXTS_ONLYSPANS) ? spanwords-1 : i);
             j < ((m_flags&TXTS_NOSPANS) ? i+1 : spanwords);
             j++) {
            int fin = m_words_in_span[j].second;
            if (fin - deb > int(m_span.size()))
                break;
            if (!emitterm(j != i+1, m_span.substr(deb, fin-deb), pos, spboffs+deb, spboffs+fin))
                return false;
        }
        if (!noposinc)
            ++pos;
    }
    return true;
}

// A method called at word boundaries (different places in text_to_words()), to adjust the current
// state of the parser, and possibly generate term(s).
//
// While inside a span (words linked by glue characters), we just keep track of the word boundaries.
//
// Once actual white-space is reached, we get called with spanerase set to true, and we process the
// span, calling words_from_span() to actually compute and emit the terms.
// 
// The object flags can modify our behaviour, deciding if we only emit single words (bill, recoll,
// org), only spans (bill@recoll.org), or words and spans (bill@recoll.org, recoll.org, jf,
// recoll...)
// 
// @return true if ok, false for error. Splitting should stop in this case.
// @param spanerase Set if the current span is at its end. Process it.
// @param bp        The current BYTE position in the stream (it's beyond the current span data).
inline bool TextSplit::doemit(bool spanerase, size_t _bp)
{
    int bp = int(_bp);
    LOGDEB2("TextSplit::doemit: sper " << spanerase << " bp " << bp <<
            " spp " << m_spanpos << " spanwords " << m_words_in_span.size() <<
            " wS " << m_wordStart << " wL " << m_wordLen << " inn " <<
            m_inNumber << " span [" << m_span << "]\n");

    if (m_wordLen) {
        // We have a current word. Remember it
        if (!(o_noNumbers && m_inNumber)) {
            m_words_in_span.push_back({m_wordStart, m_wordStart + m_wordLen});
            m_wordpos++;
        }
        if (int(m_words_in_span.size()) >= o_maxWordsInSpan) {
            // Limit max span word count
            spanerase = true;
            discarded(m_span, m_spanpos, m_words_in_span[0].first, m_words_in_span.back().second,
                      LONG_SPAN_TRUNCATED);
        }
        m_wordLen = m_wordChars = 0;
    }

    if (!spanerase) {
        // Not done with this span. Just update relative word start offset.
        m_wordStart = int(m_span.length());
        return true;
    }
    
    // Span is done (too long or span-terminating character). Produce terms and reset it.
    string initials;
    if (span_is_initials(initials)) {
        if (!emitterm(false, initials, m_spanpos, bp - m_span.length(), bp))
            return false;
    }

    if (!words_from_span(bp)) {
        return false;
    }
    discardspan();
    return true;
}

void TextSplit::discardspan()
{
    m_span.clear();
    m_words_in_span.clear();
    m_spanpos = m_wordpos;
    m_wordStart = 0;
    m_wordLen = m_wordChars = 0;
}

static inline bool isalphanum(int what, unsigned int flgs)
{
    return what == TextSplit::A_LLETTER || what == TextSplit::A_ULETTER ||
        what == TextSplit::DIGIT || what == TextSplit::LETTER ||
        ((flgs & TextSplit::TXTS_KEEPWILD) && what == TextSplit::WILD);
}
static inline bool isdigit(int what, unsigned int flgs)
{
    return what == TextSplit::DIGIT || ((flgs & TextSplit::TXTS_KEEPWILD)&& what == TextSplit::WILD);
}

#ifdef TEXTSPLIT_STATS
#define STATS_INC_WORDCHARS ++m_wordChars
#else
#define STATS_INC_WORDCHARS
#endif

vector<CharFlags> splitFlags{
    {TextSplit::TXTS_NOSPANS, "nospans"},
    {TextSplit::TXTS_ONLYSPANS, "onlyspans"},
    {TextSplit::TXTS_KEEPWILD, "keepwild"}
};

TextSplit::TextSplit(int flags)
    : m_flags(flags)
{
}
TextSplit::~TextSplit()
{
}

// Splitting a text into terms to be indexed.
//
// We mostly emit a word every time we see a separator, but some chars are handled specially so that
// special cases, ie, c++, jfd@recoll.org etc, are handled properly.
//
// Esp. we generate "spans". e.g. jfd@recoll.org, in addition to the individual words. This was
// quite useful with old Xapian versions, which were slow with frequent word phrase searches
// (e.g. the com in .com). This is not so useful now because Xapian performance has much improved,
// but, it does not hurt much either, as these "spans" are infrequent in real text.
bool TextSplit::text_to_words(const string &in)
{
    LOGDEB1("TextSplit::text_to_words: docjk " << o_processCJK << "(" <<
            o_CJKNgramLen <<  ") " << flagsToString(splitFlags, m_flags) <<
            " [" << in.substr(0,50) << "]\n");

    if (in.empty())
        return true;

    // Reset the data members relative to splitting state
    clearsplitstate();
    
    bool pagepending = false;
    bool nlpending = false;
    bool softhyphenpending = false;

    Utf8Iter it(in);
#if defined(KATAKANA_AS_WORDS) || defined(HANGUL_AS_WORDS) || defined(CHINESE_AS_WORDS)
    int prev_csc = -1;
#endif
    for (; !it.eof() && !it.error(); it++) {
        unsigned int c = *it;

        if (c == (unsigned int)-1) {
            LOGERR("Textsplit: error occurred while scanning UTF-8 string\n");
            return false;
        }

        CharSpanClass csc;
        // General logic here: UNICODE_IS_CJK is always true for CJK characters and must be tested
        // last.  UNICODE_IS_whatever will be true if the character belongs to the appropriate
        // segment AND a specific processor (e.g. external Hangul word segmenter) has been
        // configured. In other words, CJK characters are processed by the generic ngram term
        // generator, except if a language-specific processor has been implemented and configured.
        bool gospecial = true;
        if (UNICODE_IS_KATAKANA(c)) {
            csc = CSC_KATAKANA;
        } else if (UNICODE_IS_HANGUL(c)) {
            csc = CSC_HANGUL;
        } else if (UNICODE_IS_CHINESE(c)) {
            csc = CSC_CHINESE;
        } else if (UNICODE_IS_OTHER_NGRAM(c)) {
            csc = CSC_OTHERNGRAM;
        } else if (UNICODE_IS_TESTNGRAM(c)) {
            csc = CSC_TESTNGRAM;
        } else if (UNICODE_IS_CJK(c)) {
            csc = CSC_CJK;
        } else {
            gospecial = false;
            csc = CSC_OTHER;
        }
        if (o_processCJK && gospecial) {
            // Using ngrams with possible exceptions for Hangul and Chinese if so configured.

            // Do like at EOF with the current non-cjk data.
            if (m_wordLen || m_span.length()) {
                if (!doemit(true, it.getBpos()))
                    return false;
            }
            // Hand off situation to the appropriate routine.
            if (csc == CSC_HANGUL) {
                // Note: at the moment, the real ko splitter is a lock-protected singleton which is
                // why we just recreate it here: no real initialisation.
                KOSplitter splt(*this);
                if (!splt.text_to_words(it, &c, m_wordpos)) {
                    LOGERR("Textsplit: scan error in korean handler\n");
                    return false;
                }
            } else if (csc == CSC_CHINESE) {
                if (!m_cnsplitter) {
                    m_cnsplitter = std::make_unique<CNSplitter>(*this);
                }
                if (!m_cnsplitter->text_to_words(it, &c, m_wordpos)) {
                    LOGERR("Textsplit: scan error in chinese handler\n");
                    return false;
                }
            } else {
                CJKSplitter splt(*this, o_CJKNgramLen);
                if (!splt.text_to_words(it, &c, m_wordpos)) {
                    LOGERR("Textsplit: scan error in cjk handler\n");
                    return false;
                }
            }
            // Reset state, saving term position, and return the found non-cjk
            // unicode character value. The current input byte offset is kept
            // in the utf8Iter
            int pos = m_wordpos;
            clearsplitstate();
            m_spanpos = m_wordpos = pos;
            // Check for eof, else c contains the first non-cjk
            // character after the cjk sequence, just go on.
            if (it.eof() || it.error())
                break;
        }

#if defined(KATAKANA_AS_WORDS) || defined(HANGUL_AS_WORDS) || defined(CHINESE_AS_WORDS)
        // Only needed if we have script transitions inside this
        // routine, else the call to cjk_to_words does the job (so do
        // nothing right after a CJK section). Because
        // katakana-western transitions sometimes have no whitespace
        // (and maybe hangul too, but probably not).
        if (prev_csc != CSC_CJK && prev_csc != CSC_HANGUL && prev_csc != CSC_CHINESE &&
            csc != prev_csc && (m_wordLen || m_span.length())) {
            LOGDEB2("csc " << valToString(csc_names, csc) << " prev_csc " <<
                    valToString(csc_names, prev_csc) << " wl " <<
                    m_wordLen << " spl " << m_span.length() << endl);
            if (!doemit(true, it.getBpos())) {
                return false;
            }
        }
        prev_csc = csc;
#endif

        int cc = whatcc(c);

        switch (cc) {
        case SKIP:
            // Special-case soft-hyphen. To work, this depends on the
            // fact that only SKIP calls "continue" inside the
            // switch. All the others will do the softhyphenpending
            // reset after the switch
            if (c == 0xad) {
                softhyphenpending = true;
            } else {
                softhyphenpending = false;
            }
            // Skips the softhyphenpending reset
            continue;

        case DIGIT:
            if (m_wordLen == 0)
                m_inNumber = true;
            m_wordLen += it.appendchartostring(m_span);
            STATS_INC_WORDCHARS;
            break;

        case SPACE:
        SPACE:
            if (m_wordLen || m_span.length()) {
                if (!doemit(true, it.getBpos()))
                    return false;
                m_inNumber = false;
            }
            if (pagepending) {
                pagepending = false;
                newpage(m_wordpos);
            }
            if (nlpending) {
                nlpending = false;
                newline(m_wordpos);
            }

            if (o_haveidxpunct && (o_idxpunct.find(c) != o_idxpunct.end())) {
                std::string tmp;
                auto l = it.appendchartostring(tmp);
                // std::cerr << "IDXSPACE: [" << c << "] [" << tmp << "]\n";
                takeword(tmp, m_wordpos, it.getBpos(), it.getBpos()+l);
                if (m_spanpos == m_wordpos)
                    m_spanpos++;
                m_wordpos++;
            }
            break;

        case WILD:
            if (m_flags & TXTS_KEEPWILD)
                goto NORMALCHAR;
            else
                goto SPACE;
            break;

        case '-':
        case '+':
            if (m_wordLen == 0) {
                // + or - don't start a term except if this looks like
                // it's going to be to be a number
                if (isdigit(whatcc(it[it.getCpos()+1]), m_flags)) {
                    // -10
                    m_inNumber = true;
                    m_wordLen += it.appendchartostring(m_span);
                    STATS_INC_WORDCHARS;
                    break;
                } 
            } else if (m_inNumber) {
                if ((m_span[m_span.length() - 1] == 'e' ||
                     m_span[m_span.length() - 1] == 'E')) {
                    if (isdigit(whatcc(it[it.getCpos()+1]), m_flags)) {
                        m_wordLen += it.appendchartostring(m_span);
                        STATS_INC_WORDCHARS;
                        break;
                    }
                }
            } else {
                int nextc = it[it.getCpos()+1];
                if (cc == '+') {
                    if (nextc == '+' || nextc == -1 || isvisiblewhite(nextc)) {
                        // someword++[+...] !
                        m_wordLen += it.appendchartostring(m_span);
                        STATS_INC_WORDCHARS;
                        break;
                    }
                } else {
                    // Note about dangling hyphens: we always strip '-' found before whitespace,
                    // even before a newline, then generate two terms, before and after the line
                    // break. We have no way to know if '-' is there because a word was broken by
                    // justification or if it was part of an actual compound word (would need a
                    // dictionary to check). As soft-hyphen *should* be used if the '-' is not part
                    // of the text.
                    if (nextc == -1 || isvisiblewhite(nextc)) {
                        goto SPACE;
                    }
                    if (!doemit(false, it.getBpos()))
                        return false;
                    m_inNumber = false;
                    m_wordStart += it.appendchartostring(m_span);
                    break;
                }
            }
            goto SPACE;

        case '.':
        {
            // Need a little lookahead here. At worse this gets the end null
            int nextc = it[it.getCpos()+1];
            int nextwhat = whatcc(nextc);
            if (m_inNumber) {
                if (!isdigit(nextwhat, m_flags))
                    goto SPACE;
                m_wordLen += it.appendchartostring(m_span);
                STATS_INC_WORDCHARS;
                break;
            } else {
                // Found '.' while not in number

                // Only letters and digits make sense after
                if (!isalphanum(nextwhat, m_flags)) {
                    // Do append the dot to the span: for abbrev recognition.
                    it.appendchartostring(m_span);
                    // Set the iterator to point to the look-ahead char. Need to
                    // test for nl/newpage here (there should be a better way?)
                    it++;
                    if (*it == '\n')
                        nlpending = true;
                    else if (*it == '\f')
                        pagepending = true;
                    goto SPACE;
                }

                // Keep an initial '.' for catching .net, and .34 (aka
                // 0.34) but this adds quite a few spurious terms !
                if (m_span.length() == 0) {
                    // Check for number like .1
                    if (isdigit(nextwhat, m_flags)) {
                        m_inNumber = true;
                        m_wordLen += it.appendchartostring(m_span);
                    } else {
                        m_words_in_span.push_back(pair<int,int>(m_wordStart, m_wordStart));
                        m_wordStart += it.appendchartostring(m_span);
                    }
                    STATS_INC_WORDCHARS;
                    break;
                }

                // '.' between words: span glue
                if (m_wordLen) {
                    if (!doemit(false, it.getBpos()))
                        return false;
                    m_wordStart += it.appendchartostring(m_span);
                }
            }
        }
        break;

        case 0x2010: // hyphen
        case 0x2019: // variations on single quote
        case 0x275c:
        case 0x02bc:
        case '@':
        case '_': // If underscoreasletter is set, we'll never get this
        case '\'':
        {
            // If in word, potential span: o'brien, jf@dockes.org,
            // else just ignore
            int nextc = it[it.getCpos()+1];
            if (nextc == -1 || isvisiblewhite(nextc)) {
                goto SPACE;
            }
            if (m_wordLen) {
                if (!doemit(false, it.getBpos()))
                    return false;
                m_inNumber = false;
                m_wordStart += it.appendchartostring(m_span);
            }
        }
        break;

        case '#':  {
            int w = whatcc(it[it.getCpos()+1]);
            // Keep it only at the beginning of a word (hashtag), 
            if (m_wordLen == 0 && isalphanum(w, m_flags)) {
                m_wordLen += it.appendchartostring(m_span);
                STATS_INC_WORDCHARS;
                break;
            }
            // or at the end (special case for c# ...)
            if (m_wordLen > 0) {
                if (w == SPACE || w == '\n' || w == '\r') {
                    m_wordLen += it.appendchartostring(m_span);
                    STATS_INC_WORDCHARS;
                    break;
                }
            }
            goto SPACE;
        }
            break;

        case '\n':
            nlpending = true;
            /* FALLTHROUGH */
        case '\r':
            if (softhyphenpending) {
                // Don't reset soft-hyphen
                continue;
            } else {
                // Normal case: EOL is white space
                goto SPACE;
            }
            break;

        case '\f':
            pagepending = true;
            goto SPACE;
            break;

#ifdef RCL_SPLIT_CAMELCASE
            // Camelcase handling. 
            // If we get uppercase ascii after lowercase ascii, emit word.
            // This emits "camel" when hitting the 'C' of camelCase
            // Not enabled by defaults as this makes phrase searches quite 
            // confusing. 
            // ie "MySQL manual" is matched by "MySQL manual" and 
            // "my sql manual" but not "mysql manual"

            // A possibility would be to emit both my and sql at the
            // same position. All non-phrase searches would work, and
            // both "MySQL manual" and "mysql manual" phrases would
            // match too. "my sql manual" would not match, but this is
            // not an issue.
        case A_ULETTER:
            if (m_span.length() && 
                charclasses[(unsigned char)m_span[m_span.length() - 1]] == 
                A_LLETTER) {
                if (m_wordLen) {
                    if (!doemit(false, it.getBpos()))
                        return false;
                }
            }
            goto NORMALCHAR;

            // CamelCase handling.
            // If we get lowercase after uppercase and the current
            // word length is bigger than one, it means we had a
            // string of several upper-case letters:  an
            // acronym (readHTML) or a single letter article (ALittleHelp).
            // Emit the uppercase word before proceeding
        case A_LLETTER:
            if (m_span.length() && 
                charclasses[(unsigned char)m_span[m_span.length() - 1]] == 
                A_ULETTER && m_wordLen > 1) {
                // Multiple upper-case letters. Single letter word
                // or acronym which we want to emit now
                m_wordLen--;
                if (!doemit(false, it.getBpos()))
                    return false;
                // m_wordstart could be 0 here if the span was reset
                // for excessive length
                if (m_wordStart)
                    m_wordStart--;
                m_wordLen++;
            }
            goto NORMALCHAR;
#endif /* CAMELCASE */

        default:
        NORMALCHAR:
            if (m_inNumber && c != 'e' && c != 'E') {
                m_inNumber = false;
            }
            m_wordLen += it.appendchartostring(m_span);
            STATS_INC_WORDCHARS;
            break;
        }
        softhyphenpending = false;
    }
    if (m_wordLen || m_span.length()) {
        if (!doemit(true, it.getBpos()))
            return false;
    }
    return true;
}

// Specialization for countWords 
class TextSplitCW : public TextSplit {
public:
    int wcnt;
    TextSplitCW(int flags) : TextSplit(flags), wcnt(0) {}
    bool takeword(const string &, size_t, size_t, size_t) override {
        wcnt++;
        return true;
    }
};

int TextSplit::countWords(const string& s, int flgs)
{
    TextSplitCW splitter(flgs);
    splitter.text_to_words(s);
    return splitter.wcnt;
}

bool TextSplit::hasVisibleWhite(const string &in)
{
    Utf8Iter it(in);
    for (; !it.eof() && !it.error(); it++) {
        unsigned int c = (unsigned char)*it;
        if (c == (unsigned int)-1) {
            LOGERR("hasVisibleWhite: error while scanning UTF-8 string\n");
            return false;
        }
        if (isvisiblewhite(c))
            return true;
    }
    return false;
}

template <class T> bool u8stringToStrings(const string &s, T &tokens)
{
    Utf8Iter it(s);

    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (; !it.eof() && !it.error(); it++) {
        unsigned int c = *it;
        if (visiblewhite.find(c) != visiblewhite.end()) 
            c = ' ';
        if (c == (unsigned int)-1) {
            LOGERR("TextSplit::stringToStrings: error while scanning UTF-8 "
                   "string\n");
            return false;
        }

        switch (c) {
        case '"': 
            switch(state) {
            case SPACE: state = INQUOTE; continue;
            case TOKEN: goto push_char;
            case ESCAPE: state = INQUOTE; goto push_char;
            case INQUOTE: tokens.push_back(current);current.clear();
                state = SPACE; continue;
            }
            break;
        case '\\': 
            switch(state) {
            case SPACE: 
            case TOKEN: state=TOKEN; goto push_char;
            case INQUOTE: state = ESCAPE; continue;
            case ESCAPE: state = INQUOTE; goto push_char;
            }
            break;

        case ' ': 
        case '\t': 
        case '\n': 
        case '\r': 
            switch(state) {
            case SPACE: continue;
            case TOKEN: tokens.push_back(current); current.clear();
                state = SPACE; continue; 
            case INQUOTE: 
            case ESCAPE: goto push_char;
            }
            break;

        default:
            switch(state) {
            case ESCAPE: state = INQUOTE; break;
            case SPACE:  state = TOKEN;  break;
            case TOKEN: 
            case INQUOTE: break;
            }
        push_char:
            it.appendchartostring(current);
        }
    }

    // End of string. Process residue, and possible error (unfinished quote)
    switch(state) {
    case SPACE: break;
    case TOKEN: tokens.push_back(current); break;
    case INQUOTE: 
    case ESCAPE: return false;
    }
    return true;
}

bool TextSplit::stringToStrings(const string &s, vector<string> &tokens)
{
    return u8stringToStrings<vector<string> >(s, tokens);
}
