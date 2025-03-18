/* Copyright (C) 2003-2022 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#define _FILE_OFFSET_BITS 64
#include "conftree.h"

#include <ctype.h>
#if defined(BUILDING_RECOLL) || !defined(_WIN32)
#include <fnmatch.h>
#endif /* BUILDING_RECOLL */
#include <stdlib.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>
#include <map>

#include "pathut.h"
#include "smallut.h"
#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

#undef DEBUG_CONFTREE
#ifdef DEBUG_CONFTREE
#define CONFDEB LOGERR
#else
#define CONFDEB LOGDEB2
#endif

// type/data lookups in the lines array (m_order). Case-sensitive or not depending on what we are
// searching and the ConfSimple case-sensitivity flags
class OrderComp {
public:
    OrderComp(ConfLine &ln, const CaseComparator& comp)
        : m_cfl(ln), m_comp(comp) {}
    bool operator()(const ConfLine& cfl) {
        return cfl.m_kind == m_cfl.m_kind &&
            !m_comp(m_cfl.m_data, cfl.m_data) && !m_comp(cfl.m_data, m_cfl.m_data);
    }
    const ConfLine& m_cfl;
    const CaseComparator& m_comp;
};

long long ConfNull::getInt(const std::string& name, long long dflt,
                           const std::string& sk)
{
    std::string val;
    if (!get(name, val, sk)) {
        return dflt;
    }
    char *endptr;
    long long ret = strtoll(val.c_str(), &endptr, 0);
    if (endptr == val.c_str()) {
        return dflt;
    }
    return ret;
}

double ConfNull::getFloat(const std::string& name, double dflt,
                          const std::string& sk)
{
    std::string val;
    if (!get(name, val, sk)) {
        return dflt;
    }
    char *endptr;
    double ret = strtod(val.c_str(), &endptr);
    if (endptr == val.c_str()) {
        return dflt;
    }
    return ret;
}

bool ConfNull::getBool(const std::string& name, bool dflt,
                       const std::string& sk)
{
    std::string val;
    if (!get(name, val, sk)) {
        return dflt;
    }
    return stringToBool(val);
}

static const SimpleRegexp varcomment_rx("[ \t]*#[ \t]*([a-zA-Z0-9]+)[ \t]*=", 0, 1);

void ConfSimple::parseinput(std::istream& input)
{
    std::string submapkey;
    std::string cline;
    bool appending = false;
    std::string line;
    bool eof = false;

    for (;;) {
        cline.clear();
        std::getline(input, cline);
        CONFDEB("Parse:line: ["  << cline << "] status "  << status << "\n");
        if (!input.good()) {
            if (input.bad()) {
                CONFDEB("Parse: input.bad()\n");
                status = STATUS_ERROR;
                return;
            }
            CONFDEB("Parse: eof\n");
            // Must be eof ? But maybe we have a partial line which
            // must be processed. This happens if the last line before
            // eof ends with a backslash, or there is no final \n
            eof = true;
        }

        {
            std::string::size_type pos = cline.find_last_not_of("\n\r");
            if (pos == std::string::npos) {
                cline.clear();
            } else if (pos != cline.length() - 1) {
                cline.erase(pos + 1);
            }
        }

        if (appending) {
            line += cline;
        } else {
            line = cline;
        }

        // Note that we trim whitespace before checking for backslash-eol
        // This avoids invisible whitespace problems.
        if (trimvalues) {
            trimstring(line);
        } else {
            ltrimstring(line);
        }
        if (line.empty() || line.at(0) == '#') {
            if (eof) {
                break;
            }
            if (varcomment_rx.simpleMatch(line)) {
                m_order.push_back(ConfLine(ConfLine::CFL_VARCOMMENT, line,
                                           varcomment_rx.getMatch(line, 1)));
            } else {
                m_order.push_back(ConfLine(ConfLine::CFL_COMMENT, line));
            }
            continue;
        }
        if (line[line.length() - 1] == '\\') {
            line.erase(line.length() - 1);
            appending = true;
            continue;
        }
        appending = false;

        if (line[0] == '[') {
            trimstring(line, "[] \t");
            if (dotildexpand) {
                submapkey = path_tildexpand(line);
            } else {
                submapkey = line;
            }
            m_subkeys_unsorted.push_back(submapkey);
            m_order.push_back(ConfLine(ConfLine::CFL_SK, submapkey));
            continue;
        }

        // Look for first equal sign
        std::string::size_type eqpos = line.find("=");
        if (eqpos == std::string::npos) {
            m_order.push_back(ConfLine(ConfLine::CFL_COMMENT, line));
            continue;
        }

        // Compute name and value, trim white space
        std::string nm, val;
        nm = line.substr(0, eqpos);
        trimstring(nm);
        val = line.substr(eqpos + 1, std::string::npos);
        if (trimvalues) {
            trimstring(val);
        }

        if (nm.length() == 0) {
            m_order.push_back(ConfLine(ConfLine::CFL_COMMENT, line));
            continue;
        }
        i_set(nm, val, submapkey, true);
        if (eof) {
            break;
        }
    }
}


static int varsToFlags(int readonly, bool tildexp, bool trimv)
{
    int flags = 0;
    if (readonly)
        flags |= ConfSimple::CFSF_RO;
    if (tildexp)
        flags |= ConfSimple::CFSF_TILDEXP;
    if (!trimv)
        flags |= ConfSimple::CFSF_NOTRIMVALUES;
    return flags;
}


ConfSimple::ConfSimple(int readonly, bool tildexp, bool trimv)
    : ConfSimple(varsToFlags(readonly, tildexp, trimv) | CFSF_FROMSTRING, std::string())
{
}

void ConfSimple::reparse(const std::string& d)
{
    clear();
    std::stringstream input(d, std::ios::in);
    parseinput(input);
}

ConfSimple::ConfSimple(const std::string& d, int readonly, bool tildexp, bool trimv)
    : ConfSimple(varsToFlags(readonly, tildexp, trimv) | CFSF_FROMSTRING, d)
{
}

ConfSimple::ConfSimple(const char *fname, int readonly, bool tildexp, bool trimv)
    : ConfSimple(varsToFlags(readonly, tildexp, trimv), std::string(fname))
{
}

ConfSimple::ConfSimple(int flags, const std::string& dataorfn)
{
    m_flags = flags;
    status = (flags & CFSF_RO) ? STATUS_RO : STATUS_RW;
    dotildexpand = (flags & CFSF_TILDEXP) != 0;
    trimvalues = (flags & CFSF_NOTRIMVALUES) == 0;
    if (flags & CFSF_SUBMAPNOCASE) {
        m_submaps =  std::map<std::string, std::map<std::string, std::string, CaseComparator>,
                              CaseComparator>(m_nocasecomp);
    }
    CONFDEB("ConfSimple::ConfSimple: RO: " << (status==STATUS_RO) << " tildexp " << dotildexpand <<
           " trimvalues " << trimvalues << " from string? " << bool(flags & CFSF_FROMSTRING) <<
           " file name: " << ((flags & CFSF_FROMSTRING)?" data input " : dataorfn.c_str()) << "\n");
    if (flags & CFSF_FROMSTRING) {
        if (!dataorfn.empty()) {
            std::stringstream input(dataorfn, std::ios::in);
            parseinput(input);
        }
    } else {
        m_filename = dataorfn;
        std::fstream input;
        openfile((flags & CFSF_RO), input);
        if (status == STATUS_ERROR)
            return;
        parseinput(input);
        i_changed(true);
    }
}

ConfSimple::StatusCode ConfSimple::getStatus() const
{
    switch (status) {
    case STATUS_RO:
        return STATUS_RO;
    case STATUS_RW:
        return STATUS_RW;
    default:
        return STATUS_ERROR;
    }
}

void ConfSimple::openfile(int readonly, std::fstream& input)
{
    int mode = readonly ? std::ios::in : std::ios::in | std::ios::out;
    if (!readonly && !path_exists(m_filename)) {
        mode |= std::ios::trunc;
    }
    path_streamopen(m_filename, mode, input);
    if (!input.is_open()) {
        LOGDEB0("ConfSimple::ConfSimple: fstream(w)(" << m_filename << ", " << mode <<
                ") errno " << errno << "\n");
    }

    if (!readonly && !input.is_open()) {
        // reset errors
        input.clear();
        status = STATUS_RO;
        // open readonly
        path_streamopen(m_filename, std::ios::in, input);
    }

    if (!input.is_open()) {
        // Don't log ENOENT, this is common with some recoll config files
        std::string reason;
        catstrerror(&reason, nullptr, errno);
        if (errno != 2) {
            LOGERR("ConfSimple::ConfSimple: fstream(" << m_filename << ", " <<
                   std::ios::in << ") " << reason << "\n");
        }
        status = STATUS_ERROR;
        return;
    }
}

bool ConfSimple::sourceChanged() const
{
    if (!m_filename.empty()) {
        PathStat st;
        if (path_fileprops(m_filename, &st) == 0) {
            if (m_fmtime != st.pst_mtime) {
                return true;
            }
        }
    }
    return false;
}

bool ConfSimple::i_changed(bool upd)
{
    if (!m_filename.empty()) {
        PathStat st;
        if (path_fileprops(m_filename, &st) == 0) {
            if (m_fmtime != st.pst_mtime) {
                if (upd) {
                    m_fmtime = st.pst_mtime;
                }
                return true;
            }
        }
    }
    return false;
}

int ConfSimple::get(const std::string& nm, std::string& value, const std::string& sk) const
{
    if (!ok()) {
        return 0;
    }

    // Find submap
    const auto ss = m_submaps.find(sk);
    if (ss == m_submaps.end()) {
        return 0;
    }

    // Find named value
    const auto s = ss->second.find(nm);
    if (s == ss->second.end()) {
        return 0;
    }
    value = s->second;
    return 1;
}

// Appropriately output a subkey (nm=="") or variable line.
// We can't make any assumption about the data except that it does not
// contain line breaks.
// Avoid long lines if possible (for hand-editing)
// We used to break at arbitrary places, but this was ennoying for
// files with pure UTF-8 encoding (some files can be binary anyway),
// because it made later editing difficult, as the file would no
// longer have a valid encoding.
// Any ASCII byte would be a safe break point for utf-8, but could
// break some other encoding with, e.g. escape sequences? So break at
// whitespace (is this safe with all encodings?).
// Note that the choice of break point does not affect the validity of
// the file data (when read back by conftree), only its ease of
// editing with a normal editor.
static ConfSimple::WalkerCode varprinter(void *f, const std::string& nm,
                                         const std::string& value)
{
    std::ostream& output = *((std::ostream *)f);
    if (nm.empty()) {
        output << "\n[" << value << "]\n";
    } else {
        output << nm << " = ";
        if (nm.length() + value.length() < 75) {
            output << value;
        } else {
            std::string::size_type ll = 0;
            for (std::string::size_type pos = 0; pos < value.length(); pos++) {
                std::string::value_type c = value[pos];
                output << c;
                ll++;
                // Break at whitespace if line too long and "a lot" of
                // remaining data
                if (ll > 50 && (value.length() - pos) > 10 &&
                    (c == ' ' || c == '\t')) {
                    ll = 0;
                    output << "\\\n";
                }
            }
        }
        output << "\n";
    }
    return ConfSimple::WALK_CONTINUE;
}

// Set variable and rewrite data
int ConfSimple::set(const std::string& nm, const std::string& value, const std::string& sk)
{
    if (status  != STATUS_RW) {
        return 0;
    }
    CONFDEB("ConfSimple::set ["<<sk<< "]:[" << nm << "] -> [" << value << "]\n");
    if (!i_set(nm, value, sk)) {
        return 0;
    }
    return write();
}

int ConfSimple::set(const std::string& nm, long long val, const std::string& sk)
{
    return this->set(nm, lltodecstr(val), sk);
}

// Internal set variable: no rw checking or file rewriting. If init is
// set, we're doing initial parsing, else we are changing a parsed
// tree (changes the way we update the order data)
int ConfSimple::i_set(const std::string& nm, const std::string& value,
                      const std::string& sk, bool init)
{
    CONFDEB("ConfSimple::i_set: nm[" << nm << "] val[" << value <<
            "] key[" << sk << "], init " << init << "\n");
    // Values must not have embedded newlines
    if (value.find_first_of("\n\r") != std::string::npos) {
        CONFDEB("ConfSimple::i_set: LF in value\n");
        return 0;
    }
    bool existing = false;
    auto ss = m_submaps.find(sk);
    // Test if submap already exists, else create it, and insert variable:
    if (ss == m_submaps.end()) {
        CONFDEB("ConfSimple::i_set: new submap\n");
        std::map<std::string, std::string, CaseComparator> submap(
            (m_flags & CFSF_KEYNOCASE) ? m_nocasecomp : m_casecomp);
        submap[nm] = value;
        m_submaps[sk] = submap;

        // Maybe add sk entry to m_order data, if not already there (see comment below).
        if (!sk.empty()) {
            ConfLine nl(ConfLine::CFL_SK, sk);
            // Append SK entry only if it's not already there (erase
            // does not remove entries from the order data, and it may
            // be being recreated after deletion)
            OrderComp cmp(nl, (m_flags & CFSF_SUBMAPNOCASE) ? m_nocasecomp : m_casecomp);
            if (find_if(m_order.begin(), m_order.end(), cmp) == m_order.end()) {
                m_order.push_back(nl);
            }
        }
    } else {
        // Insert or update variable in existing map.
        auto it = ss->second.find(nm);
        if (it == ss->second.end()) {
            ss->second.insert(std::pair<std::string, std::string>(nm, value));
        } else {
            it->second = value;
            existing = true;
        }
    }

    // If the variable already existed, no need to change the m_order data
    if (existing) {
        CONFDEB("ConfSimple::i_set: existing var: no order update\n");
        return 1;
    }

    // Add the new variable at the end of its submap in the order data.

    if (init) {
        // During the initial construction, just append:
        CONFDEB("ConfSimple::i_set: init true: append\n");
        m_order.push_back(ConfLine(ConfLine::CFL_VAR, nm));
        m_order.back().m_value = value;
        return 1;
    }

    // Look for the start and end of the subkey zone. Start is either
    // at begin() for a null subkey, or just behind the subkey
    // entry. End is either the next subkey entry, or the end of
    // list. We insert the new entry just before end.
    std::vector<ConfLine>::iterator start, fin;
    if (sk.empty()) {
        start = m_order.begin();
        CONFDEB("ConfSimple::i_set: null sk, start at top of order\n");
    } else {
        ConfLine cfl(ConfLine::CFL_SK, sk);
        OrderComp cmp(cfl, (m_flags & CFSF_SUBMAPNOCASE) ? m_nocasecomp : m_casecomp);
        start = find_if(m_order.begin(), m_order.end(), cmp);
        if (start == m_order.end()) {
            // This is not logically possible. The subkey must exist. We're doomed
            std::cerr << "Logical failure during configuration variable insertion" << "\n";
            abort();
        }
    }

    fin = m_order.end();
    if (start != m_order.end()) {
        // The null subkey has no entry (maybe it should)
        if (!sk.empty()) {
            start++;
        }
        for (std::vector<ConfLine>::iterator it = start; it != m_order.end(); it++) {
            if (it->m_kind == ConfLine::CFL_SK) {
                fin = it;
                break;
            }
        }
    }

    // It may happen that the order entry already exists because erase doesnt
    // update m_order
    ConfLine cfl(ConfLine::CFL_VAR, nm);
    OrderComp cmp(cfl, (m_flags & CFSF_KEYNOCASE) ? m_nocasecomp : m_casecomp);
    if (find_if(start, fin, cmp) == fin) {
        // Look for a varcomment line, insert the value right after if
        // it's there.
        bool inserted(false);
        std::vector<ConfLine>::iterator it;
        for (it = start; it != fin; it++) {
            if (it->m_kind == ConfLine::CFL_VARCOMMENT && it->m_aux == nm) {
                it++;
                m_order.insert(it, ConfLine(ConfLine::CFL_VAR, nm));
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            m_order.insert(fin, ConfLine(ConfLine::CFL_VAR, nm));
        }
    }

    return 1;
}

int ConfSimple::erase(const std::string& nm, const std::string& sk)
{
    if (status  != STATUS_RW) {
        return 0;
    }

    auto ss = m_submaps.find(sk);
    if (ss == m_submaps.end()) {
        return 0;
    }

    ss->second.erase(nm);
    if (ss->second.empty()) {
        m_submaps.erase(ss);
    }
    return write();
}

int ConfSimple::eraseKey(const std::string& sk)
{
    std::vector<std::string> nms = getNames(sk);
    for (const auto& nm : nms) {
        erase(nm, sk);
    }
    return write();
}

int ConfSimple::clear()
{
    m_submaps.clear();
    m_order.clear();
    return write();
}

// Walk the tree, calling user function at each node
ConfSimple::WalkerCode
ConfSimple::sortwalk(WalkerCode(*walker)(void *, const std::string&, const std::string&),
                     void *clidata) const
{
    if (!ok()) {
        return WALK_STOP;
    }
    // For all submaps:
    for (const auto& submap : m_submaps) {
        // Possibly emit submap name:
        if (!submap.first.empty() &&
            walker(clidata, std::string(), submap.first.c_str()) == WALK_STOP) {
            return WALK_STOP;
        }

        // Walk submap
        for (const auto& item : submap.second) {
            if (walker(clidata, item.first, item.second) == WALK_STOP) {
                return WALK_STOP;
            }
        }
    }
    return WALK_CONTINUE;
}

// Write to default output. This currently only does something if output is
// a file
bool ConfSimple::write()
{
    if (!ok()) {
        return false;
    }
    if (m_holdWrites) {
        return true;
    }
    if (!m_filename.empty()) {
        std::fstream output;
        path_streamopen(m_filename, std::ios::out | std::ios::trunc, output);
        if (!output.is_open()) {
            return 0;
        }
        return this->write(output);
    } else {
        // No backing store, no writing. Maybe one day we'll need it with
        // some kind of output string. This can't be the original string which
        // is currently readonly.
        //ostringstream output(m_ostring, ios::out | std::ios::trunc);
        return 1;
    }
}


bool ConfSimple::content_write(std::ostream& out) const
{
    return sortwalk(varprinter, &out) == WALK_CONTINUE ? true : false;
}


// Write out the tree in configuration file format:
// This does not check holdWrites, this is done by write(void), which
// lets this work even when holdWrites is set
bool ConfSimple::write(std::ostream& out) const
{
    if (!ok()) {
        return false;
    }
    if (m_order.empty()) {
        // Then we have no presentation data. Just output the values and subkeys
        content_write(out);
    }
    std::string sk;
    for (const auto& confline : m_order) {
        switch (confline.m_kind) {
        case ConfLine::CFL_COMMENT:
        case ConfLine::CFL_VARCOMMENT:
            out << confline.m_data << "\n";
            if (!out.good()) {
                return false;
            }
            break;
        case ConfLine::CFL_SK:
            sk = confline.m_data;
            CONFDEB("ConfSimple::write: SK ["  << sk << "]\n");
            // Check that the submap still exists, and only output it if it
            // does
            if (m_submaps.find(sk) != m_submaps.end()) {
                out << "[" << confline.m_data << "]" << "\n";
                if (!out.good()) {
                    return false;
                }
            }
            break;
        case ConfLine::CFL_VAR:
            std::string nm = confline.m_data;
            CONFDEB("ConfSimple::write: VAR [" << nm << "], sk [" <<sk<< "]\n");
            // As erase() doesnt update m_order we can find unexisting
            // variables, and must not output anything for them. Have
            // to use a ConfSimple::get() to check here, because
            // ConfTree's could retrieve from an ancestor even if the
            // local var is gone.
            std::string value;
            if (ConfSimple::get(nm, value, sk)) {
                varprinter(&out, nm, value);
                if (!out.good()) {
                    return false;
                }
                break;
            }
            CONFDEB("ConfSimple::write: no value: nm["<<nm<<"] sk["<<sk<<"]\n");
            break;
        }
    }
    return true;
}

std::vector<std::string> ConfSimple::getNames(const std::string& sk, const char *pattern) const
{
    std::vector<std::string> mylist;
    if (!ok()) {
        return mylist;
    }
    const auto ss = m_submaps.find(sk);
    if (ss == m_submaps.end()) {
        return mylist;
    }
    mylist.reserve(ss->second.size());
    for (const auto& item : ss->second) {
        if (pattern &&
#if defined(BUILDING_RECOLL) || !defined(_WIN32)
            0 != fnmatch(pattern, item.first.c_str(), 0)
#else
            /* Default to no match: yields easier to spot errors */
            1
#endif
        ) {
            continue;
        }
        mylist.push_back(item.first);
    }
    return mylist;
}

std::vector<std::string> ConfSimple::getSubKeys() const
{
    std::vector<std::string> mylist;
    if (!ok()) {
        return mylist;
    }
    mylist.reserve(m_submaps.size());
    for (const auto& submap : m_submaps) {
        mylist.push_back(submap.first);
    }
    return mylist;
}

bool ConfSimple::hasNameAnywhere(const std::string& nm) const
{
    std::vector<std::string>keys = getSubKeys();
    for (const auto& key : keys) {
        std::string val;
        if (get(nm, val, key)) {
            return true;
        }
    }
    return false;
}

bool ConfSimple::commentsAsXML(std::ostream& out)
{
    const std::vector<ConfLine>& lines = getlines();

    out << "<confcomments>\n";
    
    for (const auto& line : lines) {
        switch (line.m_kind) {
        case ConfLine::CFL_COMMENT:
        case ConfLine::CFL_VARCOMMENT:
        {
            std::string::size_type pos = line.m_data.find_first_not_of("# ");
            if (pos != std::string::npos) {
                out << line.m_data.substr(pos) << "\n";
            } else {
                out << "\n";
            }
            break;
        }
        case ConfLine::CFL_SK:
            out << "<subkey>" << line.m_data << "</subkey>" << "\n";
            break;
        case ConfLine::CFL_VAR:
            out << "<varsetting>" << line.m_data << " = " << line.m_value << "</varsetting>" << "\n";
            break;
        default:
            break;
        }
    }
    out << "</confcomments>\n";
    
    return true;
}


// //////////////////////////////////////////////////////////////////////////
// ConfTree Methods: conftree interpret keys like a hierarchical file tree
// //////////////////////////////////////////////////////////////////////////

int ConfTree::get(const std::string& name, std::string& value, const std::string& sk)
    const
{
    if (sk.empty() || !path_isabsolute(sk)) {
        CONFDEB("ConfTree::get: looking in global space for ["  << sk << "]\n");
        return ConfSimple::get(name, value, sk);
    }

    // Get writable copy of subkey path
    std::string msk = sk;

    // Handle the case where the config file path has an ending / and not
    // the input sk
    path_catslash(msk);

    // Look in subkey and up its parents until root ('')
    for (;;) {
        CONFDEB("ConfTree::get: looking for ["  << name << "] in ["  <<
                msk << "]\n");
        if (ConfSimple::get(name, value, msk)) {
            return 1;
        }
        std::string::size_type pos = msk.rfind("/");
        if (pos != std::string::npos) {
            msk.replace(pos, std::string::npos, std::string());
        } else {
#ifdef _WIN32
            if (msk.size() == 2 && isalpha(msk[0]) && msk[1] == ':') {
                msk.clear();
            } else
#endif
                break;
        }
    }
    return 0;
}
