/* Copyright (C) 2006-2022 J.F.Dockes
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

// Handle translation from rcl's SearchData structures to Xapian Queries

#include "autoconfig.h"

#include <stdio.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
using namespace std;

#include "xapian.h"

#include "cstr.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "searchdata.h"
#include "log.h"
#include "smallut.h"
#include "textsplit.h"
#include "unacpp.h"
#include "utf8iter.h"
#include "stoplist.h"
#include "rclconfig.h"
#include "termproc.h"
#include "synfamily.h"
#include "stemdb.h"
#include "expansiondbs.h"
#include "base64.h"
#include "daterange.h"

namespace Rcl {

SearchData::~SearchData() 
{
    LOGDEB0("SearchData::~SearchData\n");
    for (auto& clausep : m_query) 
        delete clausep;
}

// This is called by the GUI simple search if the option is set: add
// (OR) phrase to a query (if it is simple enough) so that results
// where the search terms are close and in order will come up on top.
// We remove very common terms from the query to avoid performance issues.
bool SearchData::maybeAddAutoPhrase(Rcl::Db& db, double freqThreshold)
{
    LOGDEB0("SearchData::maybeAddAutoPhrase()\n");

    simplify();

    if (m_query.empty()) {
        LOGDEB2("SearchData::maybeAddAutoPhrase: empty query\n");
        return false;
    }

    string field;
    auto clp0 = dynamic_cast<SearchDataClauseSimple*>(*m_query.begin());
    if (clp0)
        field = clp0->getfield();
    vector<string> words;
    // Walk the clause list. If this is not an AND list, we find any
    // non simple clause or different field names, bail out.
    for (auto& clausep : m_query) {
        SClType tp = clausep->m_tp;
        if (tp != SCLT_AND) {
            LOGDEB2("SearchData::maybeAddAutoPhrase: wrong tp " << tp << "\n");
            return false;
        }
        auto clp = dynamic_cast<SearchDataClauseSimple*>(clausep);
        if (clp == nullptr) {
            LOGDEB2("SearchData::maybeAddAutoPhrase: other than clauseSimple in query.\n");
            return false;
        }
        if (clp->getfield().compare(field)) {
            LOGDEB2("SearchData::maybeAddAutoPhrase: diff. fields\n");
            return false;
        }

        // If there are wildcards or quotes in there, bail out
        if (clp->gettext().find_first_of("\"*[?") != string::npos) { 
            LOGDEB2("SearchData::maybeAddAutoPhrase: wildcards\n");
            return false;
        }

        // Do a simple word-split here, not the full-blown
        // textsplit. Spans of stopwords should not be trimmed later
        // in this function, they will be properly split when the
        // phrase gets processed by toNativeQuery() later on.
        vector<string> wl;
        stringToStrings(clp->gettext(), wl);
        words.insert(words.end(), wl.begin(), wl.end());
    }


    // Trim the word list by eliminating very frequent terms
    // (increasing the slack as we do it):
    int slack = 0;
    int doccnt = db.docCnt();
    if (!doccnt)
        doccnt = 1;
    string swords;
    for (const auto& word : words) {
        double freq = double(db.termDocCnt(word)) / doccnt;
        if (freq < freqThreshold) {
            if (!swords.empty())
                swords.append(1, ' ');
            swords += word;
        } else {
            LOGDEB0("SearchData::Autophrase: [" << word << "] too frequent (" <<
                    100 * freq << " %" << ")\n");
            slack++;
        }
    }
    
    // We can't make a phrase with a single word :)
    int nwords = TextSplit::countWords(swords);
    if (nwords <= 1) {
        LOGDEB2("SearchData::maybeAddAutoPhrase: ended with 1 word\n");
        return false;
    }

    // Increase the slack: we want to be a little more laxist than for
    // an actual user-entered phrase
    slack += 1 + nwords / 3;
    
    m_autophrase = make_shared<SearchDataClauseDist>(SCLT_PHRASE, swords, slack, field);
    m_autophrase->setParent(this);
    return true;
}

// Add clause to current list. OR lists cant have EXCL clauses.
bool SearchData::addClause(SearchDataClause* cl)
{
    if (m_tp == SCLT_OR && cl->getexclude()) {
        LOGERR("SearchData::addClause: cant add EXCL to OR list\n");
        m_reason = "No Negative (AND_NOT) clauses allowed in OR queries";
        return false;
    }
    cl->setParent(this);
    m_haveWildCards = m_haveWildCards || cl->m_haveWildCards;
    m_query.push_back(cl);
    return true;
}

// Am I a file name only search ? This is to turn off term highlighting.
// There can't be a subclause in a filename search: no possible need to recurse
bool SearchData::fileNameOnly() 
{
    for (const auto& clausep : m_query) {
        if (!clausep->isFileName())
            return false;
    }
    return true;
}

// The query language creates a lot of subqueries. See if we can merge them.
void SearchData::simplify()
{
    LOGDEB0("SearchData::simplify()\n");
    //std::cerr << "SIMPLIFY BEFORE: "; dump(std::cerr);
    for (unsigned int i = 0; i < m_query.size(); i++) {
        if (m_query[i]->m_tp != SCLT_SUB)
            continue;

        auto clsubp = dynamic_cast<SearchDataClauseSub*>(m_query[i]);
        if (nullptr == clsubp) {
            // ??
            continue;
        }
        if (clsubp->getSub()->m_tp != m_tp) {
            LOGDEB0("Not simplifying because sub has differing m_tp\n");
            continue;
        }

        clsubp->getSub()->simplify();

        // If this subquery has special attributes, it's not a
        // candidate for collapsing, except if it has no clauses, because
        // then, we just pick the attributes.
        if (!clsubp->getSub()->m_filetypes.empty() ||
            !clsubp->getSub()->m_nfiletypes.empty() ||
            clsubp->getSub()->m_haveDates ||
#ifdef EXT4_BIRTH_TIME
            clsubp->getSub()->m_haveBrDates ||
#endif
            clsubp->getSub()->m_maxSize != -1 ||
            clsubp->getSub()->m_minSize != -1 ||
            clsubp->getSub()->m_subspec != SUBDOC_ANY ||
            clsubp->getSub()->m_haveWildCards) {
            if (!clsubp->getSub()->m_query.empty()) {
                LOGDEB0("Not simplifying because sub has special attributes and non-empty query\n");
                continue;
            }
            m_filetypes.insert(m_filetypes.end(),
                               clsubp->getSub()->m_filetypes.begin(),
                               clsubp->getSub()->m_filetypes.end());
            m_nfiletypes.insert(m_nfiletypes.end(),
                                clsubp->getSub()->m_nfiletypes.begin(),
                                clsubp->getSub()->m_nfiletypes.end());
            if (clsubp->getSub()->m_haveDates && !m_haveDates) {
                m_dates = clsubp->getSub()->m_dates;
                m_haveDates = 1;
            }
#ifdef EXT4_BIRTH_TIME
            if (clsubp->getSub()->m_haveBrDates && !m_haveBrDates) {
                m_brdates = clsubp->getSub()->m_brdates;
                m_haveBrDates = 1;
            }
#endif
            if (m_maxSize == -1)
                m_maxSize = clsubp->getSub()->m_maxSize;
            if (m_minSize == -1)
                m_minSize = clsubp->getSub()->m_minSize;
            if (m_subspec == SUBDOC_ANY)
                m_subspec = clsubp->getSub()->m_subspec;
            m_haveWildCards = m_haveWildCards || clsubp->getSub()->m_haveWildCards;
            // And then let the clauses processing go on, there are
            // none anyway, we will just delete the subquery.
        }
        
        // Delete the clause_sub, and insert the queries from its searchdata in its place
        m_query.erase(m_query.begin() + i);
        for (unsigned int j = 0; j < clsubp->getSub()->m_query.size(); j++) {
            m_query.insert(m_query.begin() + i + j, clsubp->getSub()->m_query[j]->clone());
            m_query[i+j]->setParent(this);
        }
        i += int(clsubp->getSub()->m_query.size()) - 1;
    }
    //std::cerr << "SIMPLIFY AFTER: "; dump(std::cerr);
}

// Extract terms and groups for highlighting
void SearchData::getTerms(HighlightData &hld) const
{
    for (const auto& clausep : m_query) {
        if (!(clausep->getModifiers() & SearchDataClause::SDCM_NOTERMS) && !clausep->getexclude()) {
            clausep->getTerms(hld);
        }
    }
    sort(hld.spellexpands.begin(), hld.spellexpands.end());
    hld.spellexpands.erase(
        unique(hld.spellexpands.begin(), hld.spellexpands.end()), hld.spellexpands.end());
    return;
}


bool sdataWalk(SearchData* top, SdataWalker& walker)
{
    if (!walker.sdata(top, true)) {
        return false;
    }
    for (auto& clausep : top->m_query) {
        if (!walker.clause(clausep))
            return false;
        auto clp = dynamic_cast<SearchDataClauseSub*>(clausep);
        if (clp) {
            if (!sdataWalk(clp->getSub().get(), walker))
                return false;
        }
    }
    if (!walker.sdata(top, false)) {
        return false;
    }
    return true;
}

} // Namespace Rcl

