/* Copyright (C) 2012-2025 J.F.Dockes
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
#include "strmatcher.h"

#include <stdio.h>
#include <sys/types.h>
#include <fnmatch.h>

#include <string>

#include "cstr.h"
#include "log.h"
#include "rclutil.h"

// Characters that can -begin- a wildcard expression. 
// Note that backslash is in the looked-for characters. This is used to compute a term
// prefix for starting a match search. In the rare case of a backslash which is not escaping a
// wildcard, we'll start with a shorter prefix, which is no big deal for such a rare case and makes
// things simpler.
const std::string_view cstr_wildSpecStChars("*?[\\");
// Characters that can -begin- regexp expression. 
const std::string_view cstr_regSpecStChars("(.[{");

bool StrWildMatcher::match(const std::string& val) const
{
    LOGDEB2("StrWildMatcher::match: in: ["<< val << "] expr: [" << m_sexp << "]\n");
    // We used to set FNM_NOESCAPE here, but I really have no idea why. Escaping can be useful in
    // file name mode (other modes would clobber the backslashes during split).
    int ret = fnmatch(m_sexp.c_str(), val.c_str(), 0);
    switch (ret) {
    case 0: return true;
    case FNM_NOMATCH: return false;
    default:
        LOGINFO("StrWildMatcher::match:err: e [" << m_sexp << "] s [" << val
                << "] (" << path_pcencode(val) << ") ret " << ret << "\n");
        return false;
    }
}

std::string::size_type StrWildMatcher::baseprefixlen() const
{
    return m_sexp.find_first_of(cstr_wildSpecStChars);
}

StrRegexpMatcher::StrRegexpMatcher(const std::string& exp)
    : StrMatcher(exp),
      m_re(new SimpleRegexp(exp, SimpleRegexp::SRE_NOSUB))
{
}

bool StrRegexpMatcher::setExp(const std::string& exp)
{
    m_re = std::unique_ptr<SimpleRegexp>(new SimpleRegexp(exp, SimpleRegexp::SRE_NOSUB));
    return ok();
}

bool StrRegexpMatcher::match(const std::string& val) const
{
    if (ok()) 
        return false;
    return (*m_re)(val);
}

std::string::size_type StrRegexpMatcher::baseprefixlen() const
{
    return m_sexp.find_first_of(cstr_regSpecStChars);
}

bool StrRegexpMatcher::ok() const
{
    return m_re && m_re->ok();
}
