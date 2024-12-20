/* The dates-to-query routine is is lifted quasi-verbatim but
 *  modified from xapian-omega:date.cc. Copyright info:
 *
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2001 James Aylett
 * Copyright 2001,2002 Ananova Ltd
 * Copyright 2002 Intercede 1749 Ltd
 * Copyright 2002,2003,2006 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */
#include "autoconfig.h"

#include "daterange.h"

#include <stdio.h>

#include <vector>

#include <xapian.h>

#include "log.h"
#include "rclconfig.h"
#include "smallut.h"
#include "rclutil.h"

using namespace std;

namespace Rcl {

static inline void bufprefix(char *buf, string pre)
{
    const char* prechar = pre.c_str();
    if (o_index_stripchars) {
        for (size_t i= 0; i< pre.length(); i++) {
            buf[i] = prechar[i];
        }
    } else {
        buf[0] = ':';
        for (size_t i= 1; i< pre.length()+1; i++) {
            buf[i] = prechar[i-1];
        }
        buf[pre.length()+1] = ':';
    }
}

static inline size_t bpoffs(string pre) 
{
    return o_index_stripchars ? pre.length() : pre.length()+2;
}

static Xapian::Query anydate_range_filter(
    const std::string& daypref, const std::string& monpref, const std::string& yearpref,
    int y1, int m1, int d1, int y2, int m2, int d2)
{
    // We're doing at most 8 digits + prefix
    char buf[200];
    vector<Xapian::Query> v;

    // Deal with days till the end of the first month if any
    bufprefix(buf, daypref);
    auto offs = bpoffs(daypref);
    snprintf(buf + offs, sizeof(buf) - offs, "%04d%02d", y1, m1);
    int d_last = monthdays(m1, y1);
    int d_end = d_last;
    if (y1 == y2 && m1 == m2 && d2 < d_last) {
        d_end = d2;
    }
    if (d1 > 1 || d_end < d_last) {
        for ( ; d1 <= d_end ; d1++) {
            offs = 6 + bpoffs(daypref);
            snprintf(buf + offs, sizeof(buf) - offs, "%02d", d1);
            v.push_back(Xapian::Query(buf));
        }
    } else {
        bufprefix(buf, monpref);
        v.push_back(Xapian::Query(buf));
    }
    
    if (y1 == y2 && m1 == m2) {
        return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
    }

    // Months till the end of first year
    int m_last = (y1 < y2) ? 12 : m2 - 1;
    bufprefix(buf, monpref);
    while (++m1 <= m_last) {
        offs = 4 + bpoffs(monpref);
        snprintf(buf + offs, sizeof(buf) - offs, "%02d", m1);
        v.push_back(Xapian::Query(buf));
    }

    // Years inbetween and first months of the last year
    if (y1 < y2) {
        bufprefix(buf, yearpref);
        while (++y1 < y2) {
            offs = bpoffs(yearpref);
            snprintf(buf + offs, sizeof(buf) - offs, "%04d", y1);
            v.push_back(Xapian::Query(buf));
        }
        bufprefix(buf, monpref);
        snprintf(buf + bpoffs(monpref), sizeof(buf) - offs, "%04d", y2);
        for (m1 = 1; m1 < m2; m1++) {
            offs = 4 + bpoffs(monpref);
            snprintf(buf + offs, sizeof(buf) - offs, "%02d", m1);
            v.push_back(Xapian::Query(buf));
        }
    }

    // Last month
    offs = 4 + bpoffs(monpref);
    snprintf(buf + offs, sizeof(buf) - offs, "%02d", m2);

    // Deal with any final partial month
    if (d2 < monthdays(m2, y2)) {
        bufprefix(buf, daypref);
        for (d1 = 1 ; d1 <= d2; d1++) {
            offs = 6 + bpoffs(daypref);
            snprintf(buf + offs, sizeof(buf) - offs, "%02d", d1);
            v.push_back(Xapian::Query(buf));
        }
    } else {
        bufprefix(buf, monpref);
        v.push_back(Xapian::Query(buf));
    }

    return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
}

Xapian::Query date_range_filter(int y1, int m1, int d1, int y2, int m2, int d2)
{
    return anydate_range_filter(xapday_prefix, xapmonth_prefix, xapyear_prefix,
                                y1, m1, d1, y2, m2, d2);
}

#ifdef EXT4_BIRTH_TIME
Xapian::Query brdate_range_filter(int y1, int m1, int d1, int y2, int m2, int d2)
{
    return anydate_range_filter(xapbriday_prefix, xapbrimonth_prefix, xapbriyear_prefix,
                                y1, m1, d1, y2, m2, d2);
}
#endif

}
