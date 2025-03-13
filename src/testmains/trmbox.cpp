/* Copyright (C) 2017-2019 J.F.Dockes
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
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
using namespace std;

#include "rclconfig.h"
#include "rclinit.h"
#include "mh_mbox.h"
#include "pathut.h"

static char *thisprog;

static char usage [] = 
"Test driver for mbox walking function\n"
"mh_mbox [-m num] mboxfile\n"
"  \n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}
static RclConfig *config;
static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_m     0x2
//#define OPT_t     0x4

int main(int argc, char **argv)
{
    string msgnum;
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'm':   op_flags |= OPT_m; if (argc < 2)  Usage();
                msgnum = *(++argv);
                argc--; 
                goto b1;
//          case 't':   op_flags |= OPT_t;break;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    if (argc != 1)
        Usage();
    string filename = *argv++;argc--;
    string reason;
    config = recollinit(RclInitFlags(0), 0, 0, reason, 0);
    if (config == 0) {
        cerr << "init failed " << reason << endl;
        exit(1);
    }
    config->setKeyDir(path_getfather(filename));
    MimeHandlerMbox mh(config, "some_id");
    if (!mh.set_document_file("text/x-mail", filename)) {
        cerr << "set_document_file failed" << endl;
        exit(1);
    }
    if (!msgnum.empty()) {
        mh.skip_to_document(msgnum);
        if (!mh.next_document()) {
            cerr << "next_document failed after skipping to " << msgnum << endl;
            exit(1);
        }
        const auto it = mh.get_meta_data().find("content");
        if (it == mh.get_meta_data().end()) {
            cerr << "No content!!" << endl;
            exit(1);
        }
        cout << "Doc " << msgnum << ":" << endl;
        cout << it->second << endl;
        exit(0);
    }

    int docnt = 0;
    while (mh.has_documents()) {
        if (!mh.next_document()) {
            cerr << "next_document failed" << endl;
            exit(1);
        }
        docnt++;
        const auto it = mh.get_meta_data().find("content");
        int size;
        if (it == mh.get_meta_data().end()) {
            size = -1;
        } else {
            size = it->second.length();
        }
        cout << "Doc " << docnt << " size " << size << endl;
    }
    cout << docnt << " documents found in " << filename << endl;
    exit(0);
}
