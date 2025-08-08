/*
 * Copyright (C) 2000, 2001 Loic Dachary
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "unac.h"

static int usage(const char* message)
{
    fprintf(stderr,
            "%s"
            "usage: unaccent [--debug_low] [--debug_high] [--help|-h]\n"
            "\t\t<charset> [<string>] [<expected>]\n"
            "\tVersion "UNAC_VERSION"\n"
            "\tReads input from stdin and remove accents from each line,\n"
            "\tassuming it is encoded in <charset> (iconv -l shows a list of\n"
            "\tall available charsets).\n"
            "\tIf <string> is provided, remove accents from <string>.\n"
            "\tIf <expected> is provided, compare the result and exit on error\n"
            "\tif not equal. \n"
            "\tThe unaccented result is always output on stdout.\n",
            message);
    exit(1);
}

int main(int argc, char** argv) {
    int c;
    static int debug_level = UNAC_DEBUG_NONE;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"debug_low", no_argument, &debug_level, UNAC_DEBUG_LOW},
            {"debug_high", no_argument, &debug_level, UNAC_DEBUG_HIGH},
            {"help", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "h",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            break;

        case '?':
            break;

        default:
            usage("");
            break;
        }
    }

#ifdef UNAC_DEBUG_AVAILABLE
    unac_debug(debug_level);
#else /* UNAC_DEBUG_AVAILABLE */
    if(debug_level > UNAC_DEBUG_NONE)
        fprintf(stderr, "debug is not available, means vsnprintf was not found by configure\n");
#endif /* UNAC_DEBUG_AVAILABLE */

    if(optind >= argc)
        usage("missing <charset>\n");
    else {
        const char* charset = argv[optind++];

        char* unaccented = 0;
        size_t unaccented_length = 0;

        if(optind >= argc) {
#define BUFFER_SIZE 10240
            char buffer[BUFFER_SIZE];
            while(fgets(buffer, BUFFER_SIZE, stdin)) {
                if(unac_string(charset, buffer, strlen(buffer), &unaccented, &unaccented_length) < 0) {
                    fprintf(stderr, "unac_string attempt to unaccent %s with charset %s failed: ", buffer, charset);
                    perror("");
                    exit(1);
                }
                printf("%.*s", (int)unaccented_length, unaccented);
            }
        } else {
            const char* string = argv[optind++];

            if(unac_string(charset, string, strlen(string), &unaccented, &unaccented_length) < 0) {
                fprintf(stderr, "unac_string attempt to unaccent %s with charset %s failed: ", string, charset);
                perror("");
                exit(1);
            }

            if(debug_level > UNAC_DEBUG_NONE)
                fprintf(stderr, "unaccented version is");
            printf("%.*s\n", (int)unaccented_length, unaccented);

            if(optind < argc) {
                const char* expected = argv[optind++];

                if(unaccented_length != strlen(expected) ||
                   !memcpy(unaccented, expected, unaccented_length)) {
                    if(debug_level > UNAC_DEBUG_NONE)
                        fprintf(stderr, "attempt to unaccent %s with charset %s does not match expected string %s\n", string, charset, expected);
      
                    free(unaccented);
                    return 1;
                }
            }
        }

        free(unaccented);
    }

    return 0;
}
