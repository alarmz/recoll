/* Copyright (C) 2004-2022 J.F.Dockes
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

////////////////////////////////////////////////////////////////////
/** Things dealing with walking the terms lists and expansion dbs */

#include "autoconfig.h"

#include <string>
#include <algorithm>

#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "stemdb.h"
#include "expansiondbs.h"
#include "strmatcher.h"
#include "pathut.h"
#include "rcldoc.h"
#include "syngroups.h"
#include "fileudi.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

using namespace std;

namespace Rcl {

// File name wild card expansion. This is a specialisation ot termMatch
bool Db::filenameWildExp(const string& fnexp, vector<string>& names, int max)
{
    string pattern = fnexp;
    names.clear();

    // If pattern is not capitalized, not quoted (quoted pattern can't
    // get here currently anyway), and has no wildcards, we add * at
    // each end: match any substring
    if (pattern[0] == '"' && pattern[pattern.size()-1] == '"') {
        pattern = pattern.substr(1, pattern.size() -2);
    } else if (pattern.find_first_of(cstr_minwilds) == string::npos && 
               !unaciscapital(pattern)) {
        pattern = "*" + pattern + "*";
    } // else let it be

    LOGDEB("Rcl::Db::filenameWildExp: pattern: ["  << pattern << "]\n");

    // We inconditionnally lowercase and strip the pattern, as is done
    // during indexing. This seems to be the only sane possible
    // approach with file names and wild cards. termMatch does
    // stripping conditionally on indexstripchars.
    string pat1;
    if (unacmaybefold(pattern, pat1, UNACOP_UNACFOLD)) {
        pattern.swap(pat1);
    }

    TermMatchResult result;
    if (!idxTermMatch(ET_WILD, pattern, result, max, unsplitFilenameFieldName))
        return false;
    for (const auto& entry : result.entries) {
        names.push_back(entry.term);
    }
    if (names.empty()) {
        // Build an impossible query: we know its impossible because we
        // control the prefixes!
        names.push_back(wrap_prefix("XNONE") + "NoMatchingTerms");
    }
    return true;
}

// Walk the Y terms and return min/max
bool Db::maxYearSpan(int *minyear, int *maxyear)
{
    LOGDEB("Rcl::Db:maxYearSpan\n");
    *minyear = 1000000; 
    *maxyear = -1000000;
    TermMatchResult result;
    if (!idxTermMatch(ET_WILD, "*", result, -1, "xapyear")) {
        LOGINFO("Rcl::Db:maxYearSpan: termMatch failed\n");
        return false;
    }
    for (const auto& entry : result.entries) {
        if (!entry.term.empty()) {
            int year = atoi(strip_prefix(entry.term).c_str());
            if (year < *minyear)
                *minyear = year;
            if (year > *maxyear)
                *maxyear = year;
        }
    }
    return true;
}

bool Db::getAllDbMimeTypes(std::vector<std::string>& exp)
{
    Rcl::TermMatchResult res;
    if (!idxTermMatch(ET_WILD, "*", res, -1, "mtype")) {
        return false;
    }
    for (const auto& entry : res.entries) {
        // Eliminate some garbage types. Happens;
        auto tp = Rcl::strip_prefix(entry.term);
        if (tp.find('/') == std::string::npos)
            continue;
        exp.push_back(tp);
    }
    return true;
}

class TermMatchCmpByWcf {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return r.wcf - l.wcf < 0;
    }
};
class TermMatchCmpByTerm {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return l.term.compare(r.term) > 0;
    }
};
class TermMatchTermEqual {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
        return !l.term.compare(r.term);
    }
};

static const char *tmtptostr(int typ)
{
    switch (typ) {
    case Db::ET_WILD: return "wildcard";
    case Db::ET_REGEXP: return "regexp";
    case Db::ET_STEM: return "stem";
    case Db::ET_NONE:
    default: return "none";
    }
}

// Find all index terms that match an input along different expansion modes:
// wildcard, regular expression, or stemming. Depending on flags we perform
// case and/or diacritics expansion (this can be the only thing requested).
// If the "field" parameter is set, we return a list of appropriately
// prefixed terms (which are going to be used to build a Xapian
// query). 
// This routine performs case/diacritics/stemming expansion against
// the auxiliary tables, and possibly calls idxTermMatch() for work
// using the main index terms (filtering, retrieving stats, expansion
// in some cases).
bool Db::termMatch(int typ_sens, const string &lang, const string &_term,
                   TermMatchResult& res, int max,  const string& field,
                   vector<string>* multiwords)
{
    int matchtyp = matchTypeTp(typ_sens);
    if (!m_ndb || !m_ndb->m_isopen)
        return false;
    Xapian::Database xrdb = m_ndb->xrdb;

    bool diac_sensitive = (typ_sens & ET_DIACSENS) != 0;
    bool case_sensitive = (typ_sens & ET_CASESENS) != 0;
    // Path elements (used for dir: filtering) are special because
    // they are not unaccented or lowercased even if the index is
    // otherwise stripped.
    bool pathelt = (typ_sens & ET_PATHELT) != 0;
    
    LOGDEB0("Db::TermMatch: typ " << tmtptostr(matchtyp) << " diacsens " <<
            diac_sensitive << " casesens " << case_sensitive << " pathelt " <<
            pathelt << " lang ["  <<
            lang << "] term [" << _term << "] max " << max << " field [" <<
            field << "] stripped " << o_index_stripchars << " init res.size "
            << res.entries.size() << "\n");

    // If the index is stripped, no case or diac expansion can be needed:
    // for the processing inside this routine, everything looks like
    // we're all-sensitive: no use of expansion db.
    // Also, convert input to lowercase and strip its accents.
    string term = _term;
    if (o_index_stripchars) {
        diac_sensitive = case_sensitive = true;
        if (!pathelt && !unacmaybefold(_term, term, UNACOP_UNACFOLD)) {
            LOGERR("Db::termMatch: unac failed for [" << _term << "]\n");
            return false;
        }
    }

    // The case/diac expansion db
    SynTermTransUnac unacfoldtrans(UNACOP_UNACFOLD);
    XapComputableSynFamMember synac(xrdb, synFamDiCa, "all", &unacfoldtrans);

    if (matchtyp == ET_WILD || matchtyp == ET_REGEXP) {
        std::unique_ptr<StrMatcher> matcher;
        if (matchtyp == ET_REGEXP) {
            matcher = std::make_unique<StrRegexpMatcher>(term);
        } else {
            matcher = std::make_unique<StrWildMatcher>(term);
        }
        if (!diac_sensitive || !case_sensitive) {
            // Perform case/diac expansion on the exp as appropriate and
            // expand the result.
            vector<string> exp;
            if (diac_sensitive) {
                // Expand for diacritics and case, filtering for same diacritics
                SynTermTransUnac foldtrans(UNACOP_FOLD);
                synac.synKeyExpand(matcher.get(), exp, &foldtrans);
            } else if (case_sensitive) {
                // Expand for diacritics and case, filtering for same case
                SynTermTransUnac unactrans(UNACOP_UNAC);
                synac.synKeyExpand(matcher.get(), exp, &unactrans);
            } else {
                // Expand for diacritics and case, no filtering
                synac.synKeyExpand(matcher.get(), exp);
            }
            // Retrieve additional info and filter against the index itself
            for (const auto& term : exp) {
                idxTermMatch(ET_NONE, term, res, max, field);
            }
            // And also expand the original expression against the
            // main index: for the common case where the expression
            // had no case/diac expansion (no entry in the exp db if
            // the original term is lowercase and without accents).
            idxTermMatch(typ_sens, term, res, max, field);
        } else {
            idxTermMatch(typ_sens, term, res, max, field);
        }

    } else {
        // match_typ is either ET_STEM or ET_NONE (which may still need synonyms and case/diac exp)
        vector<string> lexp;
        if (diac_sensitive && case_sensitive) {
            // No case/diac expansion
            lexp.push_back(term);
        } else if (diac_sensitive) {
            // Expand for accents and case, filtering for same accents,
            SynTermTransUnac foldtrans(UNACOP_FOLD);
            synac.synExpand(term, lexp, &foldtrans);
        } else if (case_sensitive) {
            // Expand for accents and case, filtering for same case
            SynTermTransUnac unactrans(UNACOP_UNAC);
            synac.synExpand(term, lexp, &unactrans);
        } else {
            // We are neither accent- nor case- sensitive and may need stem
            // expansion or not. Expand for accents and case
            synac.synExpand(term, lexp);
        }

        if (matchtyp == ET_STEM || (typ_sens & ET_SYNEXP)) {
            // Note: if any of the above conds is true, we are insensitive to
            // diacs and case (enforced in searchdatatox:termexpand
            // Need stem expansion. Lowercase the result of accent and case
            // expansion for input to stemdb.
            for (auto& term : lexp) {
                string lower;
                unacmaybefold(term, lower, UNACOP_FOLD);
                term.swap(lower);
            }
            sort(lexp.begin(), lexp.end());
            lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());

            if (matchtyp == ET_STEM) {
                vector<string> exp1;
                if (m_usingSpellFuzz) {
                    // Apply spell expansion to the input term
                    spellExpand(term, field, exp1);
                    // Remember the generated terms. This is for informing the user.
                    res.fromspelling.insert(res.fromspelling.end(), exp1.begin(), exp1.end());
                    lexp.insert(lexp.end(), exp1.begin(), exp1.end());
                    sort(lexp.begin(), lexp.end());
                    lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
                    exp1.clear();
                }
                StemDb sdb(xrdb);
                for (const auto& term : lexp) {
                    sdb.stemExpand(lang, term, exp1);
                }
                exp1.swap(lexp);
                sort(lexp.begin(), lexp.end());
                lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
                LOGDEB("Db::TermMatch: stemexp: " << stringsToString(lexp) << "\n");
            }

            if (m_syngroups->ok() && (typ_sens & ET_SYNEXP)) {
                LOGDEB("Db::TermMatch: got syngroups\n");
                vector<string> exp1(lexp);
                for (const auto& term : lexp) {
                    vector<string> sg = m_syngroups->getgroup(term);
                    if (!sg.empty()) {
                        LOGDEB("Db::TermMatch: syngroups out: " <<
                               term << " -> " << stringsToString(sg) << "\n");
                        for (const auto& synonym : sg) {
                            if (synonym.find(' ') != string::npos) {
                                if (multiwords) {
                                    multiwords->push_back(synonym);
                                }
                            } else {
                                exp1.push_back(synonym);
                            }
                        }
                    }
                }
                lexp.swap(exp1);
                sort(lexp.begin(), lexp.end());
                lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
            }

            // Expand the resulting list for case and diacritics (all
            // stemdb content is case-folded)
            vector<string> exp1;
            for (const auto& term: lexp) {
                synac.synExpand(term, exp1);
            }
            exp1.swap(lexp);
            sort(lexp.begin(), lexp.end());
            lexp.erase(unique(lexp.begin(), lexp.end()), lexp.end());
        }

        // Filter the result against the index and get the stats, possibly add prefixes.
        LOGDEB0("Db::TermMatch: final lexp before idx filter: " << stringsToString(lexp) << "\n");
        for (const auto& term : lexp) {
            idxTermMatch(Rcl::Db::ET_NONE, term, res, max, field);
        }
    }

    TermMatchCmpByTerm tcmp;
    sort(res.entries.begin(), res.entries.end(), tcmp);
    TermMatchTermEqual teq;
    vector<TermMatchEntry>::iterator uit = unique(res.entries.begin(), res.entries.end(), teq);
    res.entries.resize(uit - res.entries.begin());
    TermMatchCmpByWcf wcmp;
    sort(res.entries.begin(), res.entries.end(), wcmp);
    if (max > 0) {
        // Would need a small max and big stem expansion...
        res.entries.resize(MIN(res.entries.size(), (unsigned int)max));
    }
    return true;
}

void Db::spellExpand(
    const std::string& term, const std::string& /*field*/, std::vector<std::string>& neighbours)
{
    TermMatchResult matchResult;
    idxTermMatch(Rcl::Db::ET_NONE, term, matchResult, 1);
    if (!matchResult.entries.empty()) {
        // Term exists. If it is very rare, try to expand query.
        auto totlen = m_ndb->xrdb.get_total_length();
        auto wcf = matchResult.entries[0].wcf;
        if (wcf == 0) // ??
            wcf += 1;
        auto rarity = int(totlen / wcf);
        LOGDEB1("Db::spellExpand: rarity for [" << term << "] " << rarity << "\n");
        if (rarity < m_autoSpellRarityThreshold) {
            LOGDEB0("Db::spellExpand: [" << term << "] is not rare: " << rarity << "\n");
            return;
        }
        vector<string> suggs;
        TermMatchResult locres1;
        if (getSpellingSuggestions(term, suggs) && !suggs.empty()) {
            LOGDEB0("Db::spellExpand: spelling suggestions for [" << term << "] : [" <<
                   stringsToString(suggs) << "]\n");
            // Only use spelling suggestions up to the chosen maximum distance
            for (int i = 0; i < int(suggs.size()) && i < 300;i++) {
                auto d = u8DLDistance(suggs[i], term);
                LOGDEB0("Db::spellExpand: spell: " << term << " -> " << suggs[i] << 
                        " distance " << d << " (max " << m_maxSpellDistance << ")\n");
                if (d <= m_maxSpellDistance) {
                  idxTermMatch(Rcl::Db::ET_NONE, suggs[i], locres1, 1);
                }
            }
            // Only use spelling suggestions which are more frequent than the term by a set ratio
            if (!locres1.entries.empty()) {
                TermMatchCmpByWcf wcmp;
                sort(locres1.entries.begin(), locres1.entries.end(), wcmp);
                for (int i = 0; i < int(locres1.entries.size()); i++) {
                    double freqratio = locres1.entries[i].wcf / wcf;
                    LOGDEB0("Db::spellExpand: freqratio for [" << locres1.entries[i].term <<
                            "] : " << freqratio <<"\n");
                    if (locres1.entries[i].wcf > m_autoSpellSelectionThreshold * wcf) {
                        LOGDEB0("Db::spellExpand: [" << locres1.entries[i].term <<
                                "] selected (frequent enough)\n");
                        neighbours.push_back(locres1.entries[i].term);
                    } else {
                        LOGDEB0("Db::spellExpand: [" << locres1.entries[i].term <<
                                "] rejected (not frequent enough)\n");
                        break;
                    }
                }
            }
        } else {
            LOGDEB("Db::spellExpand: no spelling suggestions for [" << term << "]\n");
        }

    } else {
        // If the expansion list is empty, the term is not in the index. Maybe try to use aspell
        // for a close one ?
        vector<string> suggs;
        if (getSpellingSuggestions(term, suggs) && !suggs.empty()) {
            LOGDEB0("Db::spellExpand: spelling suggestions for [" << term << "] : [" <<
                   stringsToString(suggs) << "]\n");
            for (int i = 0; i < int(suggs.size()) && i < 300;i++) {
                auto d = u8DLDistance(suggs[i], term);
                LOGDEB0("Db::spellExpand: spell: " << term << " -> " << suggs[i] << 
                       " distance " << d << " (max " << m_maxSpellDistance << ")\n");
                if (d <= m_maxSpellDistance) {
                    neighbours.push_back(suggs[i]);
                }
            }
        } else {
            LOGDEB0("Db::spellExpand: no spelling suggestions for [" << term << "]\n");
        }
    }
}


bool Db::Native::idxTermMatch_p(int typ, const string& expr, const std::string& prefix,
                                std::function<bool(const string& term, Xapian::termcount colfreq,
                                                   Xapian::doccount termfreq)> client)
{
    LOGDEB1("idxTermMatch_p: input expr: [" << expr << "] prefix [" << prefix << "]\n");

    Xapian::Database xdb = xrdb;

    std::unique_ptr<StrMatcher> matcher;
    if (typ == ET_REGEXP) {
        matcher = std::make_unique<StrRegexpMatcher>(expr);
        if (!matcher->ok()) {
            LOGERR("termMatch: regcomp failed: " << matcher->getreason());
            return false;
        }
    } else if (typ == ET_WILD) {
        matcher = std::make_unique<StrWildMatcher>(expr);
    }

    // Initial section: the part of the prefix+expr before the
    // first wildcard character. We only scan the part of the
    // index where this matches
    string is;
    if (matcher) {
        string::size_type es = matcher->baseprefixlen();
        is = prefix + expr.substr(0, es);
    } else {
        is = prefix + expr;
    }
    LOGDEB1("idxTermMatch_p: initial section: [" << is << "]\n");

    XAPTRY(
        Xapian::TermIterator it = xdb.allterms_begin(is);
        for (; it != xdb.allterms_end(); it++) {
            const string ixterm{*it};
            LOGDEB1("idxTermMatch_p: term at skip [" << ixterm << "]\n");
            // If we're beyond the terms matching the initial section, end.
            if (!is.empty() && ixterm.find(is) != 0) {
                LOGDEB1("idxTermMatch_p: initial section mismatch: stop\n");
                break;
            }
            // Else try to match the term. The matcher content is prefix-less.
            string term;
            if (!prefix.empty()) {
                term = ixterm.substr(prefix.length());
            } else {
                if (has_prefix(ixterm)) {
                    // This is possible with a left-side wildcard which would have an empty
                    // initial section so that we're scanning the whole term list.
                    continue;
                }
                term = ixterm;
            }

            // If matching an expanding expression, a mismatch does not stop us. Else we want
            // equality
            if (matcher) {
                if (!matcher->match(term)) {
                    continue;
                }
            } else if (term != expr) {
                break;
            }

            if (!client(ixterm, xdb.get_collection_freq(ixterm), it.get_termfreq()) || !matcher) {
                // If the client tells us or this is an exact search, stop.
                break;
            }
        }, xdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("termMatch: " << m_rcldb->m_reason << "\n");
        return false;
    }

    return true;
}


// Second phase of wildcard/regexp term expansion after case/diac
// expansion: expand against main index terms
bool Db::idxTermMatch(int typ_sens, const string &expr,
                      TermMatchResult& res, int max,  const string& field)
{
    int typ = matchTypeTp(typ_sens);
    LOGDEB1("Db::idxTermMatch: typ " << tmtptostr(typ) << "] expr [" << expr << "] max "  <<
            max << " field [" << field << "] init res.size " << res.entries.size() << "\n");

    if (typ == ET_STEM) {
        LOGFATAL("RCLDB: internal error: idxTermMatch called with ET_STEM\n");
        abort();
    }
    string prefix;
    if (!field.empty()) {
        const FieldTraits *ftp = nullptr;
        if (!fieldToTraits(field, &ftp, true) || ftp->pfx.empty()) {
            LOGDEB("Db::termMatch: field is not indexed (no prefix): [" << field << "]\n");
        } else {
            prefix = wrap_prefix(ftp->pfx);
        }
    }
    res.prefix = prefix;

    int rcnt = 0;
    bool dostrip = res.m_prefix_stripped;
    bool ret = m_ndb->idxTermMatch_p(
        typ, expr, prefix,
        [&res, &rcnt, max, dostrip](const string& term, Xapian::termcount cf, Xapian::doccount tf) {
            res.entries.push_back(TermMatchEntry((dostrip?strip_prefix(term):term), cf, tf));
            // The problem with truncating here is that this is done alphabetically and we may not
            // keep the most frequent terms. OTOH, not doing it may stall the program if we are
            // walking the whole term list. We compromise by cutting at 2*max
            if (max > 0 && ++rcnt >= 2*max)
                return false;
            return true;
        });

    return ret;
}

// Compute list of directories in index at given depth under the common root (which we also compute)
// This is used for the GUI directory side filter tree.
//
// This is more complicated than it seems because there could be extra_dbs so we can't use topdirs
// (which we did with an fstreewalk() initially), and also inode/directory might be excluded from
// the index (for example by an onlymimetypes parameter).
//
// We look at all the paths, compute a common prefix, and truncate at the given depth under the
// prefix and insert into an std::unordered_set for deduplication.
//
// unordered_set was the (slightly) fastest of "insert all then sort and truncate", std::set,
// std::unordered_set. Other approaches may exist, for example, by skipping same prefix in the list
// (which is sorted). Did not try, as the current approach is reasonably fast.
//
// This is admittedly horrible, and might be too slow on very big indexes, or actually fail if the
// requested depth is such that we reach the max term length and the terms are
// truncated-hashed. We'd need to look at the doc data for the full URLs, but this would be much
// slower.
//
// Still I have no other idea of how to do this, other than disable the side filter if directories
// are not indexed?
//
// We could use less memory by not computing a full list and walking the index twice instead (we
// need two passes in any case because of the common root computation).
//

bool Db::dirlist(int depth, std::string& root, std::vector<std::string>& dirs)
{
    // Build a full list of filesystem paths.
    Xapian::Database xdb = m_ndb->xrdb;
    auto prefix = wrap_prefix("Q");
    std::vector<std::string> listall;
    for (int tries = 0; tries < 2; tries++) { 
        try {
            Xapian::TermIterator it = xdb.allterms_begin(); 
            it.skip_to(prefix.c_str());
            for (; it != xdb.allterms_end(); it++) {
                string ixterm{*it};
                // If we're beyond the Q terms end
                if (ixterm.find(prefix) != 0)
                    break;
                ixterm = strip_prefix(ixterm);
                // Skip non-paths like Web entries etc.
                if (!path_isabsolute(ixterm))
                    continue;
                // Skip subdocs
                auto pos = ixterm.find_first_of('|');
                if (pos < ixterm.size() - 1)
                    continue;

                // Have to check for a hashed UDI, and fetch the data record and its URL then.
                if ((int)ixterm.size() == fileUdi::hashed_udi_size()) {
                    Rcl::Doc doc;
                    if (!getDoc(ixterm, -1, doc, false) || doc.pc == -1) {
                        continue;
                    }
                    ixterm = fileurltolocalpath(doc.url);
                }

                listall.push_back(ixterm);
            }
            break;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();
            xdb.reopen();
            continue;
        } XCATCHERROR(m_reason);
        break;
    }
    if (!m_reason.empty()) {
        LOGERR("Db::dirlist: exception while accessing index: " << m_reason << "\n");
        return false;
    }

    root = commonprefix(listall);
    std::unordered_set<std::string> unics;
    for (auto& entry : listall) {
        string::size_type pos = root.size();
        for (int i = 0; i < depth; i++) {
            auto npos = entry.find("/", pos+1);
            if (npos == std::string::npos) {
                break;
            }
            pos = npos;
        }
        entry.erase(pos);
        unics.insert(entry);
    }

    dirs.clear();
    dirs.insert(dirs.begin(), unics.begin(), unics.end());
    sort(dirs.begin(), dirs.end());
    return true;
}

/** Term list walking. */
class TermIter {
public:
    Xapian::TermIterator it;
    Xapian::Database db;
};
TermIter *Db::termWalkOpen()
{
    if (!m_ndb || !m_ndb->m_isopen)
        return nullptr;
    TermIter *tit = new TermIter;
    if (tit) {
        tit->db = m_ndb->xrdb;
        XAPTRY(tit->it = tit->db.allterms_begin(), tit->db, m_reason);
        if (!m_reason.empty()) {
            LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
            return nullptr;
        }
    }
    return tit;
}
bool Db::termWalkNext(TermIter *tit, string &term)
{
    XAPTRY(
        if (tit && tit->it != tit->db.allterms_end()) {
            term = *(tit->it)++;
            return true;
        }
        , tit->db, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
    }
    return false;
}
void Db::termWalkClose(TermIter *tit)
{
    try {
        delete tit;
    } catch (...) {}
}

bool Db::termExists(const string& word)
{
    if (!m_ndb || !m_ndb->m_isopen)
        return 0;

    XAPTRY(if (!m_ndb->xrdb.term_exists(word)) return false,
           m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termWalkOpen: xapian error: " << m_reason << "\n");
        return false;
    }
    return true;
}

bool Db::stemDiffers(const string& lang, const string& word,
                     const string& base)
{
    Xapian::Stem stemmer(lang);
    if (!stemmer(word).compare(stemmer(base))) {
        LOGDEB2("Rcl::Db::stemDiffers: same for " << word << " and " <<
                base << "\n");
        return false;
    }
    return true;
}

bool Db::isSpellingCandidate(const std::string& term, bool with_aspell)
{
    if (term.empty() || term.length() > 50 || has_prefix(term))
        return false;

    Utf8Iter u8i(term);
    if (with_aspell) {
        // If spelling with aspell, CJK and other scripts we process with ngrams are not
        // candidates. Same test as for stemming.
        if (TextSplit::noStemming(*u8i))
            return false;
    } else {
#ifdef TESTING_XAPIAN_SPELL
        // The Xapian speller (purely proximity-based) can be used for Katakana (when split as words
        // which is not always completely feasible because of separator-less compounds). Currently
        // we don't try to use the Xapian speller with other scripts with which it would be usable
        // in the absence of aspell (it would indeed be better than nothing with e.g. european
        // languages). This would require a few more config variables, maybe one day.
        if (!TextSplit::isKATAKANA(*u8i)) {
            return false;
        }
#else
        return false;
#endif
    }

    // Most punctuation chars inhibate stemming. We accept one dash. See o_nospell_chars init in
    // the rcldb constructor.
    int ccnt = 0;
    for (unsigned char c : term) {
        if (o_nospell_chars[(unsigned int)c] && (c != '-' || ++ccnt > 1))
            return false;
    }

    return true;
}

// At the moment, we only use aspell. The xapian speller code part is only for testing and normally
// not compiled in.
bool Db::getSpellingSuggestions(const string& word, vector<string>& suggs)
{
    LOGDEB("Db::getSpellingSuggestions:[" << word << "]\n");
    suggs.clear();
    if (nullptr == m_ndb) {
        return false;
    }

    string term = word;

    if (isSpellingCandidate(term, true)) {
        // Term is candidate for aspell processing
#ifdef RCL_USE_ASPELL
        bool noaspell = false;
        m_config->getConfParam("noaspell", &noaspell);
        if (noaspell) {
            return false;
        }
        if (nullptr == m_aspell) {
            m_aspell = new Aspell(m_config);
            if (m_aspell) {
                string reason;
                m_aspell->init(reason);
                if (!m_aspell->ok()) {
                    LOGDEB("Aspell speller init failed: " << reason << "\n");
                    delete m_aspell;
                    m_aspell = nullptr;
                }
            }
        }

        if (nullptr == m_aspell) {
            LOGERR("Db::getSpellingSuggestions: aspell not initialized\n");
            return false;
        }

        string reason;
        if (!m_aspell->suggest(*this, term, suggs, reason)) {
            LOGERR("Db::getSpellingSuggestions: aspell failed: " << reason << "\n");
            return false;
        }
#endif
    } else {
#ifdef TESTING_XAPIAN_SPELL
        // Was not aspell candidate (e.g.: katakana). Maybe use Xapian speller?
        if (isSpellingCandidate(term, false)) {
            if (!o_index_stripchars) {
                if (!unacmaybefold(word, term, UNACOP_UNACFOLD)) {
                    LOGINFO("Db::getSpelling: unac failed for [" << word << "]\n");
                    return false;
                }
            }
            string sugg = m_ndb->xrdb.get_spelling_suggestion(term);
            if (!sugg.empty()) {
                suggs.push_back(sugg);
            }
        }
#endif
    }
    return true;
}

} // End namespace Rcl
