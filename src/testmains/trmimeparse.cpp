/* Copyright (C) 2023 J.F.Dockes
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include "mimeparse.h"
#include "readfile.h"
#include "base64.h"

using namespace std;
extern bool rfc2231_decode(const string& in, string& out, string& charset); 
extern time_t rfc2822DateToUxTime(const string& date);
static const char *thisprog;

static char usage [] =
    "-p: header value and parameter test\n"
    "-q: qp decoding\n"
    "-b: base64\n"
    "-7: rfc2047\n"
    "-1: rfc2331\n"
    "-t: date time\n"
    "  \n\n"
    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_p     0x2 
#define OPT_q     0x4 
#define OPT_b     0x8
#define OPT_7     0x10
#define OPT_1     0x20
#define OPT_t     0x40
int
main(int argc, const char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'p':   op_flags |= OPT_p; break;
            case 'q':   op_flags |= OPT_q; break;
            case 'b':   op_flags |= OPT_b; break;
            case '1':   op_flags |= OPT_1; break;
            case '7':   op_flags |= OPT_7; break;
            case 't':   op_flags |= OPT_t; break;
            default: Usage();   break;
            }
        argc--; argv++;
    }

    if (argc != 0)
        Usage();

    if (op_flags & OPT_p) {
        // Mime header value and parameters extraction
        const char *tr[] = {
            "text/html;charset = UTF-8 ; otherparam=garb; \n"
            "QUOTEDPARAM=\"quoted value\"",

            "text/plain; charset=ASCII\r\n name=\"809D3016_5691DPS_5.2.LIC\"",

            "application/x-stuff;"
            "title*0*=us-ascii'en'This%20is%20even%20more%20;"
            "title*1*=%2A%2A%2Afun%2A%2A%2A%20;"
            "title*2=\"isn't it!\"",

            // The following are all invalid, trying to crash the parser...
            "",
            // This does not parse because of whitespace in the value.
            " complete garbage;",
            // This parses, but only the first word gets into the value
            " some value",
            " word ;",  ";",  "=",  "; = ",  "a;=\"toto tutu\"=", ";;;;a=b",
        };
      
        for (unsigned int i = 0; i < sizeof(tr) / sizeof(char *); i++) {
            MimeHeaderValue parsed;
            if (!parseMimeHeaderValue(tr[i], parsed)) {
                fprintf(stderr, "PARSE ERROR for [%s]\n", tr[i]);
                continue;
            }
            printf("Field value: [%s]\n", parsed.value.c_str());
            map<string, string>::iterator it;
            for (it = parsed.params.begin();it != parsed.params.end();it++) {
                if (it == parsed.params.begin())
                    printf("Parameters:\n");
                printf("  [%s] = [%s]\n", it->first.c_str(), it->second.c_str());
            }
        }

    } else if (op_flags & OPT_q) {
        // Quoted printable stuff
        const char *qp = 
            "=41=68 =e0 boire=\r\n continue 1ere\ndeuxieme\n\r3eme "
            "agrave is: '=E0' probable skipped decode error: =\n"
            "Actual decode error =xx this wont show";

        string out;
        if (!qp_decode(string(qp), out)) {
            fprintf(stderr, "qp_decode returned error\n");
        }
        printf("Decoded: '%s'\n", out.c_str());
    } else if (op_flags & OPT_b) {
        // Base64
        //'C'est à boire qu'il nous faut éviter l'excès.'
        //'Deuxième ligne'
        //'Troisième ligne'
        //'Et la fin (pas de nl). '
        const char *b64 = 
            "Qydlc3Qg4CBib2lyZSBxdSdpbCBub3VzIGZhdXQg6XZpdGVyIGwnZXhj6HMuCkRldXhp6G1l\r\n"
            "IGxpZ25lClRyb2lzaehtZSBsaWduZQpFdCBsYSBmaW4gKHBhcyBkZSBubCkuIA==\r\n";

        string out;
        if (!base64_decode(string(b64), out)) {
            fprintf(stderr, "base64_decode returned error\n");
            exit(1);
        }
        printf("Decoded: [%s]\n", out.c_str());
#if 0
        string coded, decoded;
        const char *fname = "/tmp/recoll_decodefail";
        if (!file_to_string(fname, coded)) {
            fprintf(stderr, "Cant read %s\n", fname);
            exit(1);
        }
    
        if (!base64_decode(coded, decoded)) {
            fprintf(stderr, "base64_decode returned error\n");
            exit(1);
        }
        printf("Decoded: [%s]\n", decoded.c_str());
#endif

    } else if (op_flags & (OPT_7|OPT_1)) {
        // rfc2047 or 2231
        char line [1024];
        string out;
        bool res;
        while (fgets(line, 1023, stdin)) {
            int l = strlen(line);
            if (l == 0)
                continue;
            line[l-1] = 0;
            fprintf(stderr, "Line: [%s]\n", line);
            string charset;
            if (op_flags & OPT_7) {
                res = rfc2047_decode(line, out);
            } else {
                res = rfc2231_decode(line, out, charset);
            }
            if (res)
                fprintf(stderr, "Out:  [%s] cs %s\n", out.c_str(), charset.c_str());
            else
                fprintf(stderr, "Decoding failed, out [%s]\n", out.c_str());
        }
    } else if (op_flags & OPT_t) {
        time_t t;
        
        const char *dates[] = {
            " Wed, 13 Sep 2006 11:40:26 -0700 (PDT)",
            " Mon, 3 Jul 2006 09:51:58 +0200",
            " Wed, 13 Sep 2006 08:19:48 GMT-07:00",
            " Wed, 13 Sep 2006 11:40:26 -0700 (PDT)",
            " Sat, 23 Dec 89 19:27:12 EST",
            "   13 Jan 90 08:23:29 GMT"};

        for (unsigned int i = 0; i <sizeof(dates) / sizeof(char *); i++) {
            t = rfc2822DateToUxTime(dates[i]);
            struct tm *tm = localtime(&t);
            char datebuf[100];
            strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S %z", tm);
            printf("[%s] -> [%s]\n", dates[i], datebuf);
        }
        printf("Enter date:\n");
        char line [1024];
        while (fgets(line, 1023, stdin)) {
            int l = strlen(line);
            if (l == 0) continue;
            line[l-1] = 0;
            t = rfc2822DateToUxTime(line);
            struct tm *tm = localtime(&t);
            char datebuf[100];
            strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S %z", tm);
            printf("[%s] -> [%s]\n", line, datebuf);
        }


    }
    exit(0);
}
