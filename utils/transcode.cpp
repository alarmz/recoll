/* Copyright (C) 2004-2024 J.F.Dockes
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

#include <string>
#include <iostream>
#include <mutex>

#include <errno.h>
#include <iconv.h>

#include "transcode.h"
#include "log.h"
#include "rclutil.h"
#include "simdutf.h"

using namespace std;

// iconv_open caching
// We used to? gain approximately 25% exec time for word at a time conversions by caching the
// iconv_open result (except if this was all an error). On modern Linux (2024) there is not much
// gain any more probably because the translation data is pre-cached in .so gconv modules.
//
// We may also lose some concurrency on multiproc because of the necessary locking, but
// all in all it's still very marginally better in all cases.
#define ICONV_CACHE_OPEN

// Using simdutf for the common utf-8 to utf-8 case (just check and copy) yields a very marginal
// improvement, but why not, it it's configured in anyway.
#ifdef USE_SIMDUTF
#define UTF8_UTF8_USE_SIMDUTF 1
#endif

bool transcode(const string &in, string &out, const string &icode, const string &ocode, int *ecnt)
{
    LOGDEB2("Transcode: " << icode << " -> " << ocode << "\n");

#if UTF8_UTF8_USE_SIMDUTF
    // For the very common utf-8 -> utf-8 case (because the doc handlers usually produce utf-8 and
    // we just check it here): tried to use simdutf: if the encoding is valid, just copy and return,
    // else do the full thing.
    if (ecnt)
        *ecnt = 0;
    if (icode == cstr_utf8 && ocode == cstr_utf8 && simdutf::validate_utf8(in.c_str(), in.size())) {
        out = in;
        return true;
    }
#endif

#ifdef ICONV_CACHE_OPEN
    static iconv_t ic = (iconv_t)-1;
    static string cachedicode;
    static string cachedocode;
    static std::mutex o_cachediconv_mutex;
    std::unique_lock<std::mutex> lock(o_cachediconv_mutex);
#else 
    iconv_t ic;
#endif
    bool ret = false;
    const int OBSIZ = 8192;
    char obuf[OBSIZ], *op;
    bool icopen = false;
    int mecnt = 0;
    out.erase();
    size_t isiz = in.length();
    out.reserve(isiz);
    const char *ip = in.c_str();

#ifdef ICONV_CACHE_OPEN
    if (cachedicode.compare(icode) || cachedocode.compare(ocode)) {
        if (ic != (iconv_t)-1) {
            iconv_close(ic);
            ic = (iconv_t)-1;
        }
#endif
        if((ic = iconv_open(ocode.c_str(), icode.c_str())) == (iconv_t)-1) {
            out = string("iconv_open failed for ") + icode
                + " -> " + ocode;
#ifdef ICONV_CACHE_OPEN
            cachedicode.erase();
            cachedocode.erase();
#endif
            goto error;
        }

#ifdef ICONV_CACHE_OPEN
        cachedicode.assign(icode);
        cachedocode.assign(ocode);
    }
#endif

    icopen = true;

    while (isiz > 0) {
        size_t osiz;
        op = obuf;
        osiz = OBSIZ;

        if(iconv(ic, (ICONV_CONST char **)&ip, &isiz, &op, &osiz) == (size_t)-1
           && errno != E2BIG) {
#if 0
            out.erase();
            out = string("iconv failed for ") + icode + " -> " + ocode +
                " : " + strerror(errno);
#endif
            if (errno == EILSEQ) {
                LOGDEB1("transcode:iconv: bad input seq.: shift, retry\n");
                LOGDEB1(" Input consumed " << ip - in.c_str() <<
                        " output produced " << out.length()+OBSIZ-osiz << "\n");
                out.append(obuf, OBSIZ - osiz);
                out += "?";
                mecnt++;
                ip++;isiz--;
                continue;
            }
            // Normally only EINVAL is possible here: incomplete
            // multibyte sequence at the end. This is not fatal. Any
            // other is supposedly impossible, we return an error
            if (errno == EINVAL)
                goto out;
            else
                goto error;
        }

        out.append(obuf, OBSIZ - osiz);
    }

#ifndef ICONV_CACHE_OPEN
    icopen = false;
    if(iconv_close(ic) == -1) {
        out.erase();
        out = string("iconv_close failed for ") + icode + " -> " + ocode;
        goto error;
    }
#endif

out:
    ret = true;

error:

    if (icopen) {
#ifndef ICONV_CACHE_OPEN
        iconv_close(ic);
#else
        // Just reset conversion
        iconv(ic, nullptr, nullptr, nullptr, nullptr);
#endif
    }

    if (mecnt)
        LOGDEB("transcode: [" << icode << "]->[" << ocode << "] " <<
               mecnt << " errors\n");
    if (ecnt)
        *ecnt = mecnt;
    return ret;
}

