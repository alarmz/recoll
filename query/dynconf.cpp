/* Copyright (C) 2005 J.F.Dockes
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

#include "safeunistd.h"

#include "dynconf.h"
#include "base64.h"
#include "smallut.h"
#include "log.h"

using namespace std;

// Well known keys for history and external indexes.
const string docHistSubKey = "docs";
const string allEdbsSk = "allExtDbs";
const string actEdbsSk = "actExtDbs";
const string advSearchHistSk = "advSearchHist";

RclDynConf::RclDynConf(const std::string &fn)
    : m_data(fn.c_str())
{
    if (m_data.getStatus() != ConfSimple::STATUS_RW) {
        // Maybe the config dir is readonly, in which case we try to
        // open readonly, but we must also handle the case where the
        // history file does not exist
        if (access(fn.c_str(), 0) != 0) {
            m_data = ConfSimple(string(), 1);
        } else {
            m_data = ConfSimple(fn.c_str(), 1);
        }
    }
}

bool RclDynConf::insertNew(const string &sk, DynConfEntry &n, DynConfEntry &s, int maxlen)
{
    if (!rw()) {
        LOGDEB("RclDynConf::insertNew: not writable\n");
        return false;
    }
    // Is this doc already in list ? If it is we remove the old entry
    auto names = m_data.getNames(sk);
    bool changed = false;
    for (const auto& name: names) {
        string oval;
        if (!m_data.get(name, oval, sk)) {
            LOGDEB("No data for " << name << "\n");
            continue;
        }
        s.decode(oval);

        if (s.equal(n)) {
            LOGDEB("Erasing old entry\n");
            m_data.erase(name, sk);
            changed = true;
        }
    }

    // Maybe reget things
    if (changed)
        names = m_data.getNames(sk);

    // Need to prune ?
    if (maxlen > 0 && names.size() >= (unsigned int)maxlen) {
        // Need to erase entries until we're back to size. Note that
        // we don't ever reset numbers. Problems will arise when
        // history is 4 billion entries old
        for (unsigned int i = 0; i < names.size() - maxlen + 1; i++) {
            m_data.erase(names[i], sk);
        }
    }

    // Increment highest number
    unsigned int hi = names.empty() ? 0 : (unsigned int)atoi(names.back().c_str());
    hi++;
    char nname[20];
    snprintf(nname, sizeof(nname), "%010u", hi);

    string value;
    n.encode(value);
    LOGDEB1("Encoded value [" << value << "] (" << value.size() << ")\n");
    if (!m_data.set(string(nname), value, sk)) {
        LOGERR("RclDynConf::insertNew: set failed\n");
        return false;
    }
    return true;
}

bool RclDynConf::eraseAll(const string &sk)
{
    if (!rw()) {
        LOGDEB("RclDynConf::eraseAll: not writable\n");
        return false;
    }
    for (const auto& nm : m_data.getNames(sk)) {
        m_data.erase(nm, sk);
    }
    return true;
}

// Specialization for plain strings ///////////////////////////////////

bool RclDynConf::enterString(const string sk, const string value, int maxlen)
{
    if (!rw()) {
        LOGDEB("RclDynConf::enterString: not writable\n");
        return false;
    }
    RclSListEntry ne(value);
    RclSListEntry scratch;
    return insertNew(sk, ne, scratch, maxlen);
}

