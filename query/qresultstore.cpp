/* Copyright (C) 2017-2020 J.F.Dockes
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

#include "qresultstore.h"

#include <string>
#include <iostream>
#include <map>
#include <vector>

#include <string.h>

#include "rcldoc.h"
#include "rclquery.h"
#include "log.h"
#include "smallut.h"

namespace Rcl {

class QResultStore::Internal {
public:
    static bool fieldneeded(const std::set<std::string>& fieldspec, bool isinc, 
                          const std::pair<std::string, std::string>& entry) {
        return !entry.second.empty() &&
            (isinc ? fieldspec.find(entry.first) != fieldspec.end() :
             fieldspec.find(entry.first) == fieldspec.end());
    }

    // Store correspondance between field name and index in offsets array.
    std::map<std::string, int> keyidx;

    // Storage for one doc. Uses a char array (base) for the data and an std::vector for the field
    // offsets. Notes:
    // - offsets[0] is always 0, not really useful, simpler this way.
    // - This could be made more efficient by going C: make the storage more linear: one big char
    //   array for all the docs data and one big vector of Nxm offsets with a bit of index
    //   computations. Or, not:
    //     *** Actually tried it, but not significantly better. See resultstore_bigarrays branch for
    //         more details. ***
    struct docoffs {
        ~docoffs() {
            free(base);
        }
        char *base{nullptr};
        std::vector<int> offsets;
    };

    // One entry with data and offsets for each result doc.
    std::vector<struct docoffs> docs;
};

QResultStore::QResultStore()
{
    m = new Internal;
}
QResultStore::~QResultStore()
{
    delete m;
}

bool QResultStore::storeQuery(Rcl::Query& query, const std::set<std::string>& fldspec, bool isinc)
{
    LOGDEB1("QResultStore::storeQuery: fldspec " << stringsToString(fldspec)  << " isinc " <<
           isinc << '\n');
    /////////////
    // Enumerate all existing keys and assign array indexes for them. Count documents while we are
    // at it.

    // The fields we always include
    m->keyidx = {{"url", 0},
                 {"mimetype", 1},
                 {"fmtime", 2},
                 {"dmtime", 3},
                 {"fbytes", 4},
                 {"dbytes", 5}
    };

    // Walk the docs and assign a keyidx slot to any field which both is included by fldspec and
    // exists in at least one doc.
    int count = 0;
    for (;;count++) {
        Rcl::Doc doc;
        if (!query.getDoc(count, doc, false)) {
            break;
        }
        for (const auto& entry : doc.meta) {
            if (Internal::fieldneeded(fldspec, isinc, entry)) {
                m->keyidx.try_emplace(entry.first, (int)m->keyidx.size());
            }
        }
    }
    
    ///////
    // Populate the main array with doc-equivalent structures.
    
    m->docs.resize(count);
    
    for (int i = 0; i < count; i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = m->docs[i];
        vdoc.offsets.resize(m->keyidx.size());
        auto nbytes =
            doc.url.size() + 1 +
            doc.mimetype.size() + 1 +
            doc.fmtime.size() + 1 +
            doc.dmtime.size() + 1 +
            doc.fbytes.size() + 1 +
            doc.dbytes.size() + 1;
        for (const auto& entry : doc.meta) {
            if (Internal::fieldneeded(fldspec, isinc, entry)) {
                nbytes += entry.second.size() + 1;
            }
        }

        char *cp = (char*)malloc(nbytes);
        if (nullptr == cp) {
            abort();
        }

#define STRINGCPCOPY(CHARP, S) do { \
            memcpy(CHARP, S.c_str(), S.size()+1); \
            CHARP += S.size()+1; \
        } while (false);

        // Copy to storage and take note of offsets for all static (5 first) fields.
        vdoc.base = cp;
        vdoc.offsets[0] = static_cast<int>(cp - vdoc.base);
        STRINGCPCOPY(cp, doc.url);
        vdoc.offsets[1] = static_cast<int>(cp - vdoc.base);
        auto firstzero = vdoc.offsets[1] - 1;
        STRINGCPCOPY(cp, doc.mimetype);
        vdoc.offsets[2] = static_cast<int>(cp - vdoc.base);
        STRINGCPCOPY(cp, doc.fmtime);
        vdoc.offsets[3] = static_cast<int>(cp - vdoc.base);
        STRINGCPCOPY(cp, doc.dmtime);
        vdoc.offsets[4] = static_cast<int>(cp - vdoc.base);
        STRINGCPCOPY(cp, doc.fbytes);
        vdoc.offsets[5] = static_cast<int>(cp - vdoc.base);
        STRINGCPCOPY(cp, doc.dbytes);
        // Walk our variable field list. If doc.meta[fld] is absent or empty, store a pointer to a
        // zero byte, else copy the data and store a pointer to it. Walking the field list and not
        // the doc meta allows setting all the needed empty pointers while we are at it.
        for (auto& entry : m->keyidx) {
            if (entry.second <= 5)
                continue;
            auto it = doc.meta.find(entry.first);
            if (it == doc.meta.end() || it->second.empty()) {
                vdoc.offsets[entry.second] = firstzero;
            } else {
                vdoc.offsets[entry.second] = static_cast<int>(cp - vdoc.base);
                STRINGCPCOPY(cp, it->second);
            }
        }
    }
    return true;
}

int QResultStore::getCount()
{
    return int(m->docs.size());
}

const char *QResultStore::fieldValue(int docindex, const std::string& fldname)
{
    if (docindex < 0 || docindex >= int(m->docs.size())) {
        return nullptr;
    }
    auto& vdoc = m->docs[docindex];

    auto it = m->keyidx.find(fldname);
    if (it == m->keyidx.end() || it->second < 0 || it->second >= (int)vdoc.offsets.size()) {
        return nullptr;
    }
    return vdoc.base + vdoc.offsets[it->second];
}

} // namespace Rcl
