/* Copyright (C) 2025 J.F.Dockes
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

#include "firstmatchline.h"

#include <string>

#include "log.h"
#include "rclconfig.h"
#include "rcldoc.h"
#include "textsplit.h"
#include "unacpp.h"

class TermLineSplitter : public TextSplit {
public:
    TermLineSplitter(const std::string& term)
        : TextSplit(), m_term(term) {
        LOGDEB1("TermLineSplitter: m_term " << m_term << "\n");
    }
    bool takeword(const std::string& _term, size_t, size_t, size_t) override {
        std::string term;
        if (o_index_stripchars) {
            if (!unacmaybefold(_term, term, UNACOP_UNACFOLD)) {
                LOGINFO("PlainToRich::takeword: unac failed for [" << term << "]\n");
                return true;
            }
        }
        LOGDEB1("TermLineSplitter: checking term " << term << "\n");
        if (term == m_term) {
            return false;
        }
        return true;
    }
    void newline(size_t) override {
        m_line++;
    }
    int getline() {
        return m_line;
    }
private:
    int m_line{1};
    std::string m_term;
};

int getFirstMatchLine(const Rcl::Doc &doc, const std::string& term)
{
    int line = 1;
    TermLineSplitter splitter(term);
    bool ret = splitter.text_to_words(doc.text);
    // The splitter takeword() breaks by returning false as soon as the term is found
    if (ret == false) {
        line = splitter.getline();
    }
    return line;
}
