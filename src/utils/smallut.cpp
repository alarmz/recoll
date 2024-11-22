/* Copyright (C) 2006-2022 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "smallut.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

// std::regex makes the program text is almost 100kb bigger than the classical regex.h so we're
// keeping the latter wherever we can (also, older platforms have no or a buggy std::regex)
// Windows does not have the classical regex, no choice there.
// We define a class to solve the simple cases.
#if __has_include(<regex.h>)
#include <regex.h>
#else
#define USE_STD_REGEX
#include <regex>
#endif


#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stringapiset.h>

#endif // _WIN32

#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std::placeholders;

namespace MedocUtils {

#ifdef _WIN32

std::string wchartoutf8(const wchar_t *in, int len)
{
    std::string out;
    wchartoutf8(in, out, len);
    return out;
}

bool wchartoutf8(const wchar_t *in, std::string& out, int wlen)
{
    out.clear();
    if (nullptr == in) {
        return true;
    }
    if (wlen == 0) {
        wlen = static_cast<int>(wcslen(in));
    }
    if (wlen == 0) {
        return true;
    }
    int flags = WC_ERR_INVALID_CHARS;
    int bytes = ::WideCharToMultiByte(CP_UTF8, flags, in, wlen, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
#ifdef LOGERR
        LOGERR("wchartoutf8: conversion error1\n");
#endif
        fwprintf(stderr, L"wchartoutf8: conversion error1 for [%s]\n", in);
        return false;
    }
    DirtySmartBuf buffer(bytes+1);
    bytes = ::WideCharToMultiByte(CP_UTF8, flags, in, wlen, buffer.buf(), bytes, nullptr, nullptr);
    if (bytes <= 0) {
#ifdef LOGERR
        LOGERR("wchartoutf8: CONVERSION ERROR2\n");
#endif
        return false;
    }
    buffer.buf()[bytes] = 0;
    out = buffer.buf();
    //fwprintf(stderr, L"wchartoutf8: in: [%s]\n", in);
    //fprintf(stderr, "wchartoutf8: out:  [%s]\n", out.c_str());
    return true;
}

bool utf8towchar(const std::string& in, wchar_t *out, int obytescap)
{
    auto wcharsavail = obytescap / sizeof(wchar_t);
    if (nullptr == out || wcharsavail < 1) {
        return false;
    }
    out[0] = 0;
    int isize = static_cast<int>(in.size());
    auto wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), isize, nullptr, 0);
    if (wcharcnt <= 0) {
#ifdef LOGERR
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
#endif
        return false;
    }
    if (wcharcnt + 1 >  int(wcharsavail)) {
#ifdef LOGERR
        LOGERR("utf8towchar: not enough space\n");
#endif
        return false;
    }
    wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), isize, out, wcharsavail);
    if (wcharcnt <= 0) {
#ifdef LOGERR
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
#endif
        return false;
    }
    out[wcharcnt] = 0;
    return true;
}

std::unique_ptr<wchar_t[]> utf8towchar(const std::string& in)
{
    int isize = static_cast<int>(in.size());
    // Note that as we supply in.size(), mbtowch computes the size
    // without a terminating 0 (and won't write in the second call of
    // course). We take this into account by allocating one more and
    // terminating the output.
    auto wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), isize, nullptr, 0);
    if (wcharcnt <= 0) {
#ifdef LOGERR
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
#endif
        return std::unique_ptr<wchar_t[]>();
    }
    auto buf = std::make_unique<wchar_t[]>(wcharcnt+1);

    wcharcnt = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, in.c_str(), isize, buf.get(), wcharcnt);
    if (wcharcnt <= 0) {
#ifdef LOGERR
        LOGERR("utf8towchar: conversion error for [" << in << "]\n");
#endif
        return std::unique_ptr<wchar_t[]>();
    }
    buf[wcharcnt] = 0;
    return buf;
}
#endif // _WIN32

int stringicmp(const std::string& s1, const std::string& s2)
{
    return strcasecmp(s1.c_str(), s2.c_str());
}

void stringtolower(std::string& io)
{
    std::transform(io.begin(), io.end(), io.begin(), [](unsigned char c) {return std::tolower(c);});
}

std::string stringtolower(const std::string& i)
{
    std::string o = i;
    stringtolower(o);
    return o;
}

void stringtoupper(std::string& io)
{
    std::transform(io.begin(), io.end(), io.begin(), [](unsigned char c) {return std::toupper(c);});
}

std::string stringtoupper(const std::string& i)
{
    std::string o = i;
    stringtoupper(o);
    return o;
}

//  s1 is already lowercase
int stringlowercmp(const std::string& s1, const std::string& s2)
{
    std::string::const_iterator it1 = s1.begin();
    std::string::const_iterator it2 = s2.begin();
    std::string::size_type size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 < size2) {
        while (it1 != s1.end()) {
            c2 = ::tolower(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    }

    while (it2 != s2.end()) {
        c2 = ::tolower(*it2);
        if (*it1 != c2) {
            return *it1 > c2 ? 1 : -1;
        }
        ++it1;
        ++it2;
    }
    return size1 == size2 ? 0 : 1;
}

//  s1 is already uppercase
int stringuppercmp(const std::string& s1, const std::string& s2)
{
    std::string::const_iterator it1 = s1.begin();
    std::string::const_iterator it2 = s2.begin();
    std::string::size_type size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 < size2) {
        while (it1 != s1.end()) {
            c2 = ::toupper(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    }

    while (it2 != s2.end()) {
        c2 = ::toupper(*it2);
        if (*it1 != c2) {
            return *it1 > c2 ? 1 : -1;
        }
        ++it1;
        ++it2;
    }
    return size1 == size2 ? 0 : 1;
}

bool beginswith(const std::string& bg, const std::string& sml)
{
    return bg.compare(0, sml.size(), sml) == 0;
}
bool endswith(const std::string& bg, const std::string& sml)
{
    if (bg.size() >= sml.size()) {
        return (!bg.compare (bg.length() - sml.length(), sml.length(), sml));
    }
    return false;
}

template <class T> bool stringToStrings(const std::string& s, T& tokens, const std::string& addseps)
{
    std::string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (char i : s) {
        switch (i) {
        case '"':
            switch (state) {
            case SPACE:
                state = INQUOTE;
                continue;
            case TOKEN:
                current += '"';
                continue;
            case INQUOTE:
                tokens.insert(tokens.end(), current);
                current.clear();
                state = SPACE;
                continue;
            case ESCAPE:
                current += '"';
                state = INQUOTE;
                continue;
            }
            break;
        case '\\':
            switch (state) {
            case SPACE:
            case TOKEN:
                current += '\\';
                state = TOKEN;
                continue;
            case INQUOTE:
                state = ESCAPE;
                continue;
            case ESCAPE:
                current += '\\';
                state = INQUOTE;
                continue;
            }
            break;

        case ' ':
        case '\t':
        case '\n':
        case '\r':
            switch (state) {
            case SPACE:
                continue;
            case TOKEN:
                tokens.insert(tokens.end(), current);
                current.clear();
                state = SPACE;
                continue;
            case INQUOTE:
            case ESCAPE:
                current += i;
                continue;
            }
            break;

        default:
            if (!addseps.empty() && addseps.find(i) != std::string::npos) {
                switch (state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case INQUOTE:
                    break;
                case SPACE:
                    tokens.insert(tokens.end(), std::string(1, i));
                    continue;
                case TOKEN:
                    tokens.insert(tokens.end(), current);
                    current.erase();
                    tokens.insert(tokens.end(), std::string(1, i));
                    state = SPACE;
                    continue;
                }
            } else switch (state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case SPACE:
                    state = TOKEN;
                    break;
                case TOKEN:
                case INQUOTE:
                    break;
                }
            current += i;
        }
    }
    switch (state) {
    case SPACE:
        break;
    case TOKEN:
        tokens.insert(tokens.end(), current);
        break;
    case INQUOTE:
    case ESCAPE:
        return false;
    }
    return true;
}

template <class T> void stringsToString(const T& tokens, std::string& s)
{
    if (tokens.empty())
        return;
    for (const auto& tok : tokens) {
        if (tok.empty()) {
            s.append("\"\" ");
            continue;
        }
        bool hasblanks = tok.find_first_of(" \t\n") != std::string::npos;
        if (hasblanks) {
            s.append(1, '"');
        }
        for (auto car : tok) {
            if (car == '"') {
                s.append(1, '\\');
                s.append(1, car);
            } else {
                s.append(1, car);
            }
        }
        if (hasblanks) {
            s.append(1, '"');
        }
        s.append(1, ' ');
    }
    s.pop_back();
}

template <class T> std::string stringsToString(const T& tokens)
{
    std::string out;
    stringsToString<T>(tokens, out);
    return out;
}

template <class T> std::string stringsToCSV(const T& tokens, char sep)
{
    std::string s;
    for (const auto& tok : tokens) {
        bool needquotes = false;
        if (tok.empty() || tok.find_first_of(std::string(1, sep) + "\"\n") != std::string::npos) {
            needquotes = true;
        }
        if (needquotes) {
            s.append(1, '"');
        }
        for (auto&& car : tok) {
            if (car == '"') {
                s.append(2, '"');
            } else {
                s.append(1, car);
            }
        }
        if (needquotes) {
            s.append(1, '"');
        }
        s.append(1, sep);
    }
    // Remove last separator.
    if (!s.empty())
        s.pop_back();
    return s;
}

template <class T> std::string commonprefix(const T& values)
{
    if (values.empty())
        return {};
    if (values.size() == 1)
        return *values.begin();
    unsigned int i = 0;
    for (;;++i) {
        auto it = values.begin();
        if (it->size() <= i) {
            goto out;
        }
        auto val = (*it)[i];
        it++;
        for (;it < values.end(); it++) {
            if (it->size() <= i || (*it)[i] != val) {
                goto out;
            }
        }
    }
out:
    return values.begin()->substr(0, i);
}

#ifdef SMALLUT_EXTERNAL_INSTANTIATIONS
#include "smallut_instantiate.h"
#else
template bool stringToStrings<std::list<std::string>>(
    const std::string&, std::list<std::string>&, const std::string&);
template bool stringToStrings<std::vector<std::string>>(const std::string&,
                                               std::vector<std::string>&, const std::string&);
template bool stringToStrings<std::set<std::string>>(const std::string&,
                                                     std::set<std::string>&, const std::string&);
template bool stringToStrings<std::unordered_set<std::string>>
(const std::string&, std::unordered_set<std::string>&, const std::string&);
template void stringsToString<std::list<std::string>>(const std::list<std::string>&, std::string&);
template void stringsToString<std::vector<std::string>>(
    const std::vector<std::string>&, std::string&);
template void stringsToString<std::set<std::string>>(const std::set<std::string>&, std::string&);
template void stringsToString<std::unordered_set<std::string>>(
    const std::unordered_set<std::string>&, std::string&);
template std::string stringsToString<std::list<std::string>>(const std::list<std::string>&);
template std::string stringsToString<std::vector<std::string>>(const std::vector<std::string>&);
template std::string stringsToString<std::set<std::string>>(const std::set<std::string>&);
template std::string stringsToString<std::unordered_set<std::string>>(
    const std::unordered_set<std::string>&);
template std::string stringsToCSV<std::list<std::string>>(const std::list<std::string>&, char);
template std::string stringsToCSV<std::vector<std::string>>(const std::vector<std::string>&, char);
template std::string commonprefix<std::vector<std::string>>(const std::vector<std::string>&values);
#endif

void stringToTokens(const std::string& str, std::vector<std::string>& tokens,
                    const std::string& delims, bool skipinit, bool allowempty)
{
    std::string::size_type startPos = 0, pos;

    // Skip initial delims, return empty if this eats all.
    if (skipinit &&
        (startPos = str.find_first_not_of(delims, 0)) == std::string::npos) {
        return;
    }
    while (startPos < str.size()) {
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);

        // Add token to the vector and adjust start
        if (pos == std::string::npos) {
            tokens.push_back(str.substr(startPos));
            break;
        }
        if (pos == startPos) {
            // Dont' push empty tokens after first
            if (allowempty || tokens.empty()) {
                tokens.emplace_back();
            }
            startPos = ++pos;
        } else {
            tokens.push_back(str.substr(startPos, pos - startPos));
            startPos = ++pos;
        }
    }
}

void stringSplitString(const std::string& str, std::vector<std::string>& tokens,
                       const std::string& sep)
{
    if (str.empty() || sep.empty())
        return;

    std::string::size_type startPos = 0, pos;

    while (startPos < str.size()) {
        // Find next delimiter or end of string (end of token)
        pos = str.find(sep, startPos);
        // Add token to the vector and adjust start
        if (pos == std::string::npos) {
            tokens.push_back(str.substr(startPos));
            break;
        }
        if (pos == startPos) {
            // Initial or consecutive separators
            tokens.emplace_back();
        } else {
            tokens.push_back(str.substr(startPos, pos - startPos));
        }
        startPos = pos + sep.size();
    }
}

std::string tokensToString(const std::vector<std::string>& tokens, const std::string& sep)
{
    std::string out;
    for (const auto& token : tokens) {
        if (!out.empty()) {
            out += sep;
        }
        out += token;
    }
    return out;
}

bool stringToBool(const std::string& s)
{
    if (s.empty()) {
        return false;
    }
    if (isdigit(s[0])) {
        int val = atoi(s.c_str());
        return val != 0;
    }
    return s.find_first_of("yYtT") == 0;
}

std::string& trimstring(std::string& s, const char *ws)
{
    rtrimstring(s, ws);
    ltrimstring(s, ws);
    return s;
}

std::string& rtrimstring(std::string& s, const char *ws)
{
    std::string::size_type pos = s.find_last_not_of(ws);
    if (pos == std::string::npos) {
        s.clear();
    } else if (pos != s.length() - 1) {
        s.erase(pos + 1);
    }
    return s;
}

std::string& ltrimstring(std::string& s, const char *ws)
{
    std::string::size_type pos = s.find_first_not_of(ws);
    s.erase(0, pos);
    return s;
}

// Remove some chars and replace them with spaces
std::string neutchars(const std::string& str, const std::string& chars, char rep)
{
    std::string out;
    neutchars(str, out, chars, rep);
    return out;
}
void neutchars(const std::string& str, std::string& out, const std::string& chars, char rep)
{
    std::string::size_type startPos, pos;

    for (pos = 0;;) {
        // Skip initial chars, break if this eats all.
        if ((startPos = str.find_first_not_of(chars, pos)) == std::string::npos) {
            break;
        }
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(chars, startPos);
        // Add token to the output. Note: token cant be empty here
        if (pos == std::string::npos) {
            out += str.substr(startPos);
        } else {
            out += str.substr(startPos, pos - startPos) + rep;
        }
    }
}


/* Truncate a string to a given maxlength, avoiding cutting off midword
 * if reasonably possible. Note: we could also use textsplit, stopping when
 * we have enough, this would be cleanly utf8-aware but would remove
 * punctuation */
static const std::string cstr_SEPAR = " \t\n\r-:.;,/[]{}";
std::string truncate_to_word(const std::string& input, std::string::size_type maxlen)
{
    std::string output;
    if (input.length() <= maxlen) {
        output = input;
    } else {
        output = input.substr(0, maxlen);
        std::string::size_type space = output.find_last_of(cstr_SEPAR);
        // Original version only truncated at space if space was found after
        // maxlen/2. But we HAVE to truncate at space, else we'd need to do
        // utf8 stuff to avoid truncating at multibyte char. In any case,
        // not finding space means that the text probably has no value.
        // Except probably for Asian languages, so we may want to fix this
        // one day
        if (space == std::string::npos) {
            output.erase();
        } else {
            output.erase(space);
        }
    }
    return output;
}

// Escape things that would look like markup
std::string escapeHtml(const std::string& in)
{
    std::string out;
    for (char pos : in) {
        switch(pos) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        default: out += pos; break;
        }
    }
    return out;
}

std::string escapeShell(const std::string& in)
{
    std::string out;
    out += "\"";
    for (char pos : in) {
        switch (pos) {
        case '$':
            out += "\\$";
            break;
        case '`':
            out += "\\`";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\\n";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out += pos;
        }
    }
    out += "\"";
    return out;
}

// Escape value to be suitable as C++ source double-quoted string (for
// generating a c++ program
std::string makeCString(const std::string& in)
{
    std::string out;
    out += "\"";
    for (char pos : in) {
        switch (pos) {
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out += pos;
        }
    }
    out += "\"";
    return out;
}

// Substitute printf-like percent cmds inside a string
bool pcSubst(const std::string& in, std::string& out, const std::map<char, std::string>& subs)
{
    for (auto it = in.begin(); it != in.end(); ++it) {
        if (*it == '%') {
            if (++it == in.end()) {
                out += '%';
                break;
            }
            if (*it == '%') {
                out += '%';
                continue;
            }
            auto tr = subs.find(*it);
            if (tr != subs.end()) {
                out += tr->second;
            } else {
                out += std::string("%") + *it;
            }
        } else {
            out += *it;
        }
    }
    return true;
}

bool pcSubst(const std::string& in, std::string& out,
             const std::function<std::string(const std::string&)>& mapper)
{
    out.erase();

    for (std::string::size_type i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (++i == in.size()) {
                out += '%';
                break;
            }
            if (in[i] == '%') {
                out += '%';
                continue;
            }
            std::string key;
            if (in[i] == '(') {
                if (++i == in.size()) {
                    out += std::string("%(");
                    break;
                }
                auto j = in.find_first_of(')', i);
                if (j == std::string::npos) {
                    // ??concatenate remaining part and stop
                    out += in.substr(i - 2);
                    break;
                }
                key = in.substr(i, j - i);
                i = j;
            } else {
                key = in[i];
            }
            out += mapper(key);
        } else {
            out += in[i];
        }
    }
    return true;
}

class PcSubstMapMapper {
public:
    explicit PcSubstMapMapper(const std::map<std::string, std::string>& subs)
        : m_subs(subs) {}
    std::string domap(const std::string& key) {
        auto it = m_subs.find(key);
        if (it != m_subs.end())
            return it->second;
        return std::string("%") +(key.size() == 1 ? key : std::string("(") + key + std::string(")"));
    }
    const std::map<std::string, std::string>& m_subs;
};

bool pcSubst(const std::string& in, std::string& out,
             const std::map<std::string, std::string>& subs)
{
    PcSubstMapMapper mapper(subs);
    return pcSubst(in, out, std::bind(&PcSubstMapMapper::domap, &mapper, _1));
}

void ulltodecstr(uint64_t val, std::string& buf)
{
    buf.clear();
    if (val == 0) {
        buf = "0";
        return;
    }

    char rbuf[30];
    int idx=29;
    rbuf[idx--] = 0;
    do {
        rbuf[idx--] = '0' + val % 10;
        val /= 10;
    } while (val);

    buf.assign(&rbuf[idx+1]);
}

void lltodecstr(int64_t val, std::string& buf)
{
    buf.clear();
    if (val == 0) {
        buf = "0";
        return;
    }

    bool neg = val < 0;
    if (neg) {
        val = -val;
    }

    char rbuf[30];
    int idx=29;
    rbuf[idx--] = 0;
    do {
        rbuf[idx--] = '0' + val % 10;
        val /= 10;
    } while (val);
    if (neg) {
        rbuf[idx--] = '-';
    }
    buf.assign(&rbuf[idx+1]);
}

std::string lltodecstr(int64_t val)
{
    std::string buf;
    lltodecstr(val, buf);
    return buf;
}

std::string ulltodecstr(uint64_t val)
{
    std::string buf;
    ulltodecstr(val, buf);
    return buf;
}

// Convert byte count into unit (KB/MB...) appropriate for display
std::string displayableBytes(int64_t size)
{
    const char *unit;

    double roundable = 0;
    if (size < 1000) {
        unit = " B ";
        roundable = double(size);
    } else if (size < 1E6) {
        unit = " KB ";
        roundable = double(size) / 1E3;
    } else if (size < 1E9) {
        unit = " MB ";
        roundable = double(size) / 1E6;
    } else {
        unit = " GB ";
        roundable = double(size) / 1E9;
    }
    size = int64_t(std::round(roundable));
    return lltodecstr(size).append(unit);
}

std::string breakIntoLines(const std::string& in, unsigned int ll, unsigned int maxlines)
{
    std::string query = in;
    std::string oq;
    unsigned int nlines = 0;
    while (!query.empty()) {
        std::string ss = query.substr(0, ll);
        if (ss.length() == ll) {
            std::string::size_type pos = ss.find_last_of(' ');
            if (pos == std::string::npos) {
                pos = query.find_first_of(' ');
                if (pos != std::string::npos) {
                    ss = query.substr(0, pos + 1);
                } else {
                    ss = query;
                }
            } else {
                ss.resize(pos + 1);
            }
        }
        // This cant happen, but anyway. Be very sure to avoid an infinite loop
        if (ss.empty()) {
            oq = query;
            break;
        }
        oq += ss + "\n";
        if (nlines++ >= maxlines) {
            oq += " ... \n";
            break;
        }
        query = query.substr(ss.length());
    }
    return oq;
}

#ifdef _WIN32
static int setenv(const char* name, const char* value, int overwrite)
{
    if (!overwrite) {
        const char *cp = getenv(name);
        if (cp) {
            return -1;
        }
    }
    return _putenv_s(name, value);
}
static void unsetenv(const char* name)
{
    _putenv_s(name, "");
}
#ifdef _MSC_VER
time_t portable_timegm(struct tm *tm)
{
    return _mkgmtime(tm);
}
#else
time_t portable_timegm(struct tm *tm)
{
    time_t ret;
    const char *tz;

    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz) {
        setenv("TZ", tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return ret;
}
#endif // End Windows not msvc
#else // -> !Windows
time_t portable_timegm(struct tm *tm)
{
    return timegm(tm);
}
#endif


std::string hexprint(const std::string& in, char separ)
{
    std::string out;
    out.reserve(separ ? (3 *in.size()) : (2 * in.size()));
    static const char hex[]="0123456789abcdef";
    auto cp = reinterpret_cast<const unsigned char*>(in.c_str());
    for (unsigned int i = 0; i < in.size(); ++i) {
        out.append(1, hex[cp[i] >> 4]);
        out.append(1, hex[cp[i] & 0x0f]);
        if (separ && i != in.size() - 1)
            out.append(1, separ);
    }
    return out;
}

[[maybe_unused]] static char* _check_strerror_r(int, char* errbuf)
{
    return errbuf;
}
[[maybe_unused]] static char* _check_strerror_r(char* cp, char*)
{
    return cp;
}

void catstrerror(std::string *reason, const char *what, int _errno)
{
    if (!reason) {
        return;
    }
    if (what) {
        reason->append(what);
    }

    reason->append(": errno: ");
    reason->append(std::to_string(_errno));
    reason->append(" : ");

#if defined(sun) || defined(_WIN32)
    // Note: sun strerror is noted mt-safe ??
    reason->append(strerror(_errno));
#else
    // There are 2 versions of strerror_r.
    // - The GNU one returns a pointer to the message (maybe
    //   static storage or supplied buffer).
    // - The POSIX one always stores in supplied buffer and
    //   returns 0 on success. As the possibility of error and
    //   error code are not specified, we're basically doomed
    //   cause we can't use a test on the 0 value to know if we
    //   were returned a pointer...
    // Also couldn't find an easy way to disable the gnu version without
    // changing the cxxflags globally, so forget it. Recent gnu lib versions
    // normally default to the posix version. (not !)
    // The feature defines tests are too complicated and seem unreliable.
    // In short it's a mess, but thanks to c++ function overloading and smart 
    // people, we have a solution:
    // https://www.zverovich.net/2015/03/13/reliable-detection-of-strerror-variants.html
    char errbuf[200];
    errbuf[0] = 0;
    reason->append(_check_strerror_r(strerror_r(_errno, errbuf, sizeof(errbuf)), errbuf));
#endif
}


#ifndef SMALLUT_NO_REGEX
#ifdef USE_STD_REGEX

class SimpleRegexp::Internal {
public:
    Internal(const std::string& exp, int flags, int nm)
        : expr(exp,
               std::basic_regex<char>::flag_type(
                   std::regex_constants::extended |
                   ((flags&SRE_ICASE) ? int(std::regex_constants::icase) : 0) |
                   ((flags&SRE_NOSUB) ? int(std::regex_constants::nosubs) : 0)
                   )), ok(true), nmatch(nm) {
    }
    std::regex expr;
    std::smatch res;
    bool ok;
    int nmatch;
};

bool SimpleRegexp::simpleMatch(const std::string& val) const
{
    if (!ok())
        return false;
    return regex_search(val, m->res, m->expr);
}

// Substitute one instance of regular expression
std::string SimpleRegexp::simpleSub(
    const std::string& in, const std::string& repl)
{
    if (!ok()) {
        return std::string();
    }
    return regex_replace(in, m->expr, repl, std::regex_constants::format_first_only);
}

std::string SimpleRegexp::getMatch(const std::string&, int i) const
{
    return m->res.str(i);
}

#else // -> !USE_STD_REGEX, use classic regex.h

class SimpleRegexp::Internal {
public:
    Internal(const std::string& exp, int flags, int nm) : nmatch(nm) {
        ok = regcomp(&expr, exp.c_str(), REG_EXTENDED |

                     ((flags & SRE_ICASE) ? REG_ICASE : 0) |

                     ((flags & SRE_NOSUB) ? REG_NOSUB : 0)) == 0;
        matches.resize(nmatch+1);
    }
    ~Internal() {
        regfree(&expr);
    }
    bool ok;
    regex_t expr;
    int nmatch;
    std::vector<regmatch_t> matches;
};

// Substitute one instance of regular expression
std::string SimpleRegexp::simpleSub(const std::string& in, const std::string& repl)
{
    if (!ok()) {
        return {};
    }
    if (int err = regexec(&m->expr, in.c_str(), m->nmatch + 1, &m->matches[0], 0)) {
        PRETEND_USE(err);
#if SIMPLESUB_DBG
        const int ERRSIZE = 200;
        char errbuf[ERRSIZE + 1];
        regerror(err, &expr, errbuf, ERRSIZE);
        std::cerr << "simpleSub: regexec(" << sexp << ") failed: " <<  errbuf << "\n";
#endif
        return in;
    }
    if (m->matches[0].rm_so == -1) {
        // No match
        return in;
    }
    std::string out = in.substr(0, m->matches[0].rm_so);
    out += repl;
    out += in.substr(m->matches[0].rm_eo);
    return out;
}

bool SimpleRegexp::simpleMatch(const std::string& val) const
{
    if (!ok())
        return false;
    return regexec(&m->expr, val.c_str(), m->nmatch + 1, &m->matches[0], 0) == 0;
}

std::string SimpleRegexp::getMatch(const std::string& val, int i) const
{
    if (i > m->nmatch) {
        return {};
    }
    return val.substr(m->matches[i].rm_so,
                      m->matches[i].rm_eo - m->matches[i].rm_so);
}

#endif // !windows, using C regexps

SimpleRegexp::SimpleRegexp(const std::string& exp, int flags, int nmatch)
    : m(std::make_unique<Internal>(exp, flags, nmatch))
{
}

SimpleRegexp::~SimpleRegexp() = default;

bool SimpleRegexp::ok() const
{
    return m->ok;
}

bool SimpleRegexp::operator() (const std::string& val) const
{
    return simpleMatch(val);
}
#endif // SMALLUT_NO_REGEX

std::string flagsToString(const std::vector<CharFlags>& flags, unsigned int val)
{
    const char *s;
    std::string out;
    for (const auto& flag : flags) {
        if ((val & flag.value) == flag.value) {
            s = flag.yesname;
        } else {
            s = flag.noname;
        }
        if (s && *s) {
            /* We have something to write */
            if (!out.empty()) {
                // If not first, add '|' separator
                out.append("|");
            }
            out.append(s);
        }
    }
    return out;
}

std::string valToString(const std::vector<CharFlags>& flags, unsigned int val)
{
    for (const auto& flag : flags)
        if (flag.value == val)
            return flag.yesname;

    char mybuf[100];
    snprintf(mybuf, sizeof(mybuf), "Unknown Value 0x%x", val);
    return mybuf;
}

// Decode %-encoded string. We leave along anything which does not look like %xy where x,y are
// hexadecimal digits.
static inline int h2d(int c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return 10 + c - 'A';
    if ('a' <= c && c <= 'f')
        return 10 + c - 'a';
    return -1;
}

std::string pc_decode(const std::string &in)
{
    if (in.size() <= 2)
        return in;
    std::string out;
    out.reserve(in.size());
    const char *cp = in.c_str();
    std::string::size_type i = 0;
    for (; i < in.size() - 2; ++i) {
        if (cp[i] == '%') {
            int d1 = h2d(cp[i+1]);
            int d2 = h2d(cp[i+2]);
            if (d1 != -1 && d2 != -1) {
                out += (d1 << 4) + d2;
            } else {
                out += '%';
                out += cp[i+1];
                out += cp[i+2];
            }
            i += 2;
        } else {
            out += cp[i];
        }
    }
    while (i < in.size()) {
        out += cp[i++];
    }
    return out;
}

bool parseHTTPRanges(const std::string& ranges, std::vector<std::pair<int64_t, int64_t>>& oranges)
{
    oranges.clear();
    std::string::size_type pos = ranges.find("bytes=");
    if (pos == std::string::npos) {
        return false;
    }
    pos += 6;
    bool done = false;
    while(!done) {
        std::string::size_type dash = ranges.find('-', pos);
        if (dash == std::string::npos) {
            return false;
        }
        std::string::size_type comma = ranges.find(',', pos);
        std::string firstPart = ranges.substr(pos, dash-pos);
        trimstring(firstPart);
        int64_t start = firstPart.empty() ? -1 : atoll(firstPart.c_str());
        std::string secondPart = ranges.substr(
            dash+1, comma != std::string::npos ?
            comma-dash-1 : std::string::npos);
        trimstring(secondPart);
        int64_t fin = secondPart.empty() ? -1 : atoll(secondPart.c_str());
        if (start == -1 && fin == -1) {
            return false;
        }
        oranges.emplace_back(start, fin);
        if (comma != std::string::npos) {
            pos = comma + 1;
        }
        done = comma == std::string::npos;
    }
    return true;
}

void millisleep(int millis)
{
#ifdef _WIN32
    Sleep(millis);
#else
    struct timespec spec;
    spec.tv_sec = millis / 1000;
    spec.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&spec, nullptr);
#endif
}

// Initialization for static stuff to be called from main thread before going
// multiple
void smallut_init_mt()
{
}

} // End namespace MedocUtils
