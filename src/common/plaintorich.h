/* Copyright (C) 2004-2025 J.F.Dockes
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
#ifndef _PLAINTORICH_H_INCLUDED_
#define _PLAINTORICH_H_INCLUDED_

#include <string>
#include <list>

struct HighlightData;

/** 
 * Highlight match regions in search results or preview text.
 * This takes a plain text or HTML input and inserts highlighting tags around the match sections.
 * The output is always HTML.
 * Overridable methods allow for different styles. 
 * If the input is HTML, we may fail to highlight term groups if they are mixed with HTML tags
 * (e.g.: firstterm <b>2ndterm</b>).
 */
class PlainToRich {
public:
    PlainToRich() {}
    virtual ~PlainToRich() {}
    PlainToRich(const PlainToRich&) = delete;
    PlainToRich& operator=(const PlainToRich&) = delete;

    /** Choose the input type. The default is text/plain.
     *  This can be changed between calls to plaintorich() */
    void set_inputhtml(bool v) {
        m_inputhtml = v;
    }

    /** Turn strings which look like HTTP(s) URLs found in the input into clickable HTML hrefs.
     *  The default is off.*/
    void set_activatelinks(bool v) {
        m_activatelinks = v;
    }

    /**
     * Highlight match regions in input.
     *
     * The actual tags used for highlighting and anchoring can be changed by
     * overriding the startMatch() and endMatch() methods.
     * 
     * Finding the search terms is relatively complicated because of phrase/near searches, which
     * need group highlights. As a matter of simplification, we handle "phrase" as "near", not
     * filtering on word order.
     *
     * @param in raw text out of internfile.
     * @param[output] out rich text output, divided in chunks (to help our caller to
     *   avoid inserting half tags into textedit which doesnt like it)
     * @param in hdata terms and groups to be highlighted. See utils/hldata.h
     * @param chunksize max size of chunks in output list
     */
    virtual bool plaintorich(const std::string &in, std::list<std::string> &out,
                             const HighlightData& hdata, int chunksize = 50000);


    /** The following are overridable output methods for headers, highlighting and marking tags */

    /** Data for the <head> section */
    virtual std::string header() {
        return std::string();
    }

    /** Return match prefix (e.g.: <div class="match">). 
        @param groupidx the index into hdata.groups */
    virtual std::string startMatch(unsigned int) {
        return std::string();
    }

    /** Return data for end of match area (e.g.: </div>). */
    virtual std::string endMatch() {
        return std::string();
    }

    /** Data to prepend to each chunk. QTextEdit possibly wants a repeat of <pre>. */
    virtual std::string startChunk() {
        return std::string();
    }

protected:
    bool m_inputhtml{false};
    // Use <br> to break plain text lines (else caller has used a <pre> tag)
    bool m_eolbr{false}; 
    const HighlightData *m_hdata{0};
    bool m_activatelinks{false};
};

#endif /* _PLAINTORICH_H_INCLUDED_ */
