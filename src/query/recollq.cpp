/* Copyright (C) 2006-2022 J.F.Dockes
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
// Takes a query and run it, no gui, results to stdout
#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>

#include <iostream>
#include <sstream>
#include <list>
#include <string>

#include "rcldb.h"
#include "rclquery.h"
#include "rclconfig.h"
#include "pathut.h"
#include "rclinit.h"
#include "log.h"
#include "wasatorcl.h"
#include "internfile.h"
#include "wipedir.h"
#include "transcode.h"
#include "textsplit.h"
#include "smallut.h"
#include "chrono.h"
#include "base64.h"
#include "rclutil.h"
#include "internfile.h"
#include "plaintorich.h"
#include "hldata.h"
#include "rcldoc.h"
#include "searchdata.h"

static PlainToRich g_hiliter;
static const std::string cstr_ellipsis("...");

using namespace std;

bool dump_contents(RclConfig *rclconfig, Rcl::Doc& idoc)
{
    FileInterner interner(idoc, rclconfig, FileInterner::FIF_forPreview);
    Rcl::Doc fdoc;
    std::string ipath = idoc.ipath;
    if (interner.internfile(fdoc, ipath)) {
        cout << fdoc.text << "\n";
    } else {
        cout << "Cant turn to text:" << idoc.url << " | " << idoc.ipath << "\n";
    }
    return true;
}

std::string make_abstract(Rcl::Doc& doc, Rcl::Query& query, bool asSnippets,
                     int snipcount, bool showlines, HighlightData&)
{
    std::vector<Rcl::Snippet> snippets;
    std::ostringstream str;
    int cnt = 0;
    if (query.makeDocAbstract(doc, &g_hiliter, snippets, asSnippets?snipcount:0, -1, true)) {
        for (const auto& snippet : snippets) {
            if (snipcount > 0 && ++cnt > snipcount)
                break;
            if (asSnippets) {
                str << (showlines ? snippet.line : snippet.page) << " : " << snippet.snippet << "\n";
            } else {
                str << snippet.snippet << cstr_ellipsis;
            }
        }
    }
    if (!asSnippets) {
        str << "\n";
    }
    return str.str();
}

void output_fields(std::vector<std::string> fields, Rcl::Doc& doc,
                   Rcl::Query& query, Rcl::Db&, bool printnames,
                   bool asSnippets, int snipcnt, bool showlines, HighlightData& hldata)
{
    if (fields.empty()) {
        for (const auto& entry : doc.meta) {
            fields.push_back(entry.first);
        }
    }
    for (const auto& fld : fields) {
        std::string out;
        if (fld == "abstract") {
            base64_encode(make_abstract(doc, query, asSnippets, snipcnt, showlines, hldata), out);
        } else if (fld == "xdocid") {
            base64_encode(std::to_string(doc.xdocid), out);
        } else {
            base64_encode(doc.meta[fld], out);
        }
        // Before printnames existed, recollq printed a single blank for empty
        // fields. This is a problem when printing names and using strtok, but
        // have to keep the old behaviour when printnames is not set.
        if (!(out.empty() && printnames)) {
            if (printnames) {
                cout << fld << " ";
            }
            cout << out << " ";
        }
    }
    cout << "\n";
}

static char *thisprog;
static char usage [] =
    " Runs a recoll query and displays result lines. \n"
    "   By default, the argument(s) will be interpreted as a Recoll query language\n"
    "   string. The -q option was kept for compatibility with the GUI and is just\n"
    "   ignored: the query *must* be specified in the non-option arguments.\n"
    "  Query language elements:\n"
    "   * Implicit AND, exclusion, field spec:  t1 -t2 title:t3\n"
    "   * OR has priority: t1 OR t2 t3 OR t4 means (t1 OR t2) AND (t3 OR t4)\n"
    "   * Phrase: \"t1 t2\" (needs additional quoting on cmd line)\n"
    " Other query modes :\n"
    "  -o Emulate the GUI simple search in ANY TERM mode.\n"
    "  -a Emulate the GUI simple search in ALL TERMS mode.\n"
    "  -f Emulate the GUI simple search in filename mode.\n"
    " Query and results options:\n"
    "  -c <configdir> : specify configuration directory, overriding $RECOLL_CONFDIR.\n"
    "  -C : collapse duplicates.\n"
    "  -d also dump file contents.\n"
    "  -n [first-]<cnt> define the result slice. The default value for [first] is 0.\n"
    "     Without the option, the default max count is 2000. Use n=0 for no limit.\n"
    "  -b : basic. Just output urls, no mime types or titles.\n"
    "  -Q : no result lines, just the processed query and result count.\n"
    "  -m : dump the whole document meta[] array for each result.\n"
    "  -A : output the document abstracts.\n"
    "     -p <cnt> : show <cnt> snippets, with page numbers instead of the\n"
    "         concatenated abstract.\n"
    "     -g <cnt> : show <cnt> snippets, with line numbers instead of the\n"
    "         concatenated abstract.\n"
    "  -S fld : sort by field <fld>.\n"
    "    -D : sort descending.\n"
    "  -s stemlang : set stemming language to use (must exist in index...).\n"
    "     Use -s \"\" to turn off stem expansion.\n"
    "  -T <synonyms file>: use the parameter (Thesaurus) for word expansion.\n"
    "  -i <dbdir> : additional index, several can be given.\n"
    "  -e use url encoding (%xx) for urls.\n"
    "  -E use exact result count instead of lower bound estimate.\n"
    "  -F <field name list> : output exactly these fields for each result.\n"
    "     The field values are encoded in base64, output in one line and \n"
    "     separated by one space character. This is the recommended format \n"
    "     for use by other programs. Use a normal query with option -m to \n"
    "     see the field names. Use -F '' to output all fields, but you probably\n"
    "     also want option -N in this case.\n"
    "    -N : with -F, print the (plain text) field names before the field values.\n"
    "  --extract_to <filepath> : extract the first result to filepath, which must not\n"
    "     exist. Use a -n option with an offset to select the appropriate result.\n"
    "  --paths-only: only print results which would have a file:// scheme, and\n"
    "     exclude the scheme.\n"
    " Other non-query usages:\n"
    "  -P: Show the date span for all the documents present in the index.\n"
    ;

static void Usage(std::ostream &os = std::cerr)
{
    os << "Usage: " << thisprog <<  " [options] [query elements]" << "\n" << usage;
    exit(1);
}

// BEWARE COMPATIBILITY WITH recoll OPTIONS letters
static int     op_flags;

#define OPT_A 0x1   
#define OPT_a 0x2   
#define OPT_b 0x4   
#define OPT_C 0x8   
#define OPT_D 0x10  
#define OPT_d 0x20  
#define OPT_e 0x40  
#define OPT_F 0x80  
#define OPT_g 0x100 
#define OPT_f 0x200 
#define OPT_l 0x400 
#define OPT_m 0x800 
#define OPT_N 0x1000
#define OPT_o 0x2000
#define OPT_p 0x4000
#define OPT_P 0x8000
#define OPT_Q 0x10000
#define OPT_q 0x20000
#define OPT_S 0x40000
#define OPT_E 0x80000

static struct option long_options[] = {
#define OPTION_EXTRACT 1000
#define OPTION_AUTOSPELL 1001
#define OPTION_AUTOSPELLMAXDIST 1002
#define OPTION_PATHS_ONLY 1003
    {"extract-to", required_argument, 0, OPTION_EXTRACT},
    {"autospell", no_argument, nullptr, OPTION_AUTOSPELL},
    {"autospell-max-distance", required_argument, 0, OPTION_AUTOSPELLMAXDIST},
    {"help", no_argument, 0, 'h'},
    {"paths-only", no_argument, 0, OPTION_PATHS_ONLY},
    {nullptr, 0, nullptr, 0}
};

int recollq(RclConfig **cfp, int argc, char **argv)
{
    thisprog = argv[0];

    std::string a_config;
    std::string sortfield;
    std::string stemlang("english");
    list<std::string> extra_dbs;
    std::string sf;
    std::string syngroupsfn;
    int firstres = 0;
    int maxcount = 2000;
    int snipcnt = -1;
    std::string extractfile;
    bool autospell{false};
    int autospellmaxdist{1};
    bool paths_only{false};
    
    int ret;
    while ((ret = getopt_long(argc, argv, "+AabCc:DdEefF:hi:lmNn:oPp:g:QqS:s:tT:v",
                              long_options, NULL)) != -1) {
        switch (ret) {
        case 'A': op_flags |= OPT_A; break;
            // GUI: -a same
        case 'a': op_flags |= OPT_a; break;
        case 'b': op_flags |= OPT_b; break;
        case 'C': op_flags |= OPT_C; break;
            // GUI: -c same
        case 'c': a_config = optarg; break;
        case 'd': op_flags |= OPT_d; break;
        case 'D': op_flags |= OPT_D; break;
        case 'E': op_flags |= OPT_E; break;
        case 'e': op_flags |= OPT_e; break;
        case 'F': op_flags |= OPT_F; sf = optarg; break;
            // GUI: -f same
        case 'f': op_flags |= OPT_f; break;
            // GUI: -h same
        case 'h': Usage(std::cout); break; // Usage exits. break to avoid warnings
        case 'i': extra_dbs.push_back(optarg); break;
            // GUI uses -L to set language of messages
            // GUI: -l specifies query language, which is the default. Accept and ignore
        case 'l': op_flags |= OPT_l; break;
        case 'm': op_flags |= OPT_m; break;
        case 'N': op_flags |= OPT_N; break;
        case 'n':   
        {
            std::string rescnt = optarg;
            std::string::size_type dash = rescnt.find("-");
            if (dash != std::string::npos) {
                firstres = atoi(rescnt.substr(0, dash).c_str());
                if (dash < rescnt.size()-1) {
                    maxcount = atoi(rescnt.substr(dash+1).c_str());
                }
            } else {
                maxcount = atoi(rescnt.c_str());
            }
            if (maxcount <= 0) maxcount = INT_MAX;
        }
        break;
        // GUI: -o same
        case 'o':   op_flags |= OPT_o; break;
        case 'P':   op_flags |= OPT_P; break;
        case 'p':
            op_flags |= OPT_p;
            goto porg;
        case 'g':
            op_flags |= OPT_g;
            {
            porg:
                const char *cp = optarg;
                char *cpe;
                snipcnt = strtol(cp, &cpe, 10);
                if (*cpe != 0)
                    Usage();
            }
            break;
        case 'Q':   op_flags |= OPT_Q; break;
            // GUI: -q same
        case 'q':   op_flags |= OPT_q; break;
        case 'S':   op_flags |= OPT_S; sortfield = optarg; break;
        case 's':   stemlang = optarg; break;
            // GUI: -t use command line, us: ignored
        case 't': break;
        case 'T':   syngroupsfn = optarg; break;
            // GUI: -v same
        case 'v': std::cout << Rcl::version_string() << "\n"; return 0;
            // GUI uses -w : open minimized
        case OPTION_EXTRACT: extractfile = optarg;break;
        case OPTION_AUTOSPELL: autospell = true; break;
        case OPTION_AUTOSPELLMAXDIST: autospellmaxdist = atoi(optarg); break;
        case OPTION_PATHS_ONLY: paths_only = true; op_flags |= OPT_b; break;
        }
    }

    std::string reason;
    *cfp = recollinit(0, nullptr, nullptr, reason, &a_config);
    RclConfig *rclconfig = *cfp;
    if (!rclconfig || !rclconfig->ok()) {
        std::cerr << "Recoll init failed: " << reason << "\n";
        exit(1);
    }

    if (argc < 1 && !(op_flags & OPT_P)) {
        Usage();
    }
    std::vector<std::string> fields;
    if (op_flags & OPT_F) {
        if (op_flags & (OPT_b|OPT_d|OPT_b|OPT_Q|OPT_m|OPT_A))
            Usage();
        stringToStrings(sf, fields);
    }
    Rcl::Db rcldb(rclconfig);
    if (!extra_dbs.empty()) {
        for (const auto& db : extra_dbs) {
            if (!rcldb.addQueryDb(db)) {
                cerr << "Can't add index: " << db << "\n";
                exit(1);
            }
        }
    }
    if (!syngroupsfn.empty()) {
        if (!rcldb.setSynGroupsFile(syngroupsfn)) {
            cerr << "Can't use synonyms file: " << syngroupsfn << "\n";
            exit(1);
        }
    }
    rcldb.setUseSpellFuzz(autospell);
    rcldb.setMaxSpellDist(autospellmaxdist);
    
    if (!rcldb.open(Rcl::Db::DbRO)) {
        cerr << "Cant open database in " << rclconfig->getDbDir() << 
            " reason: " << rcldb.getReason() << "\n";
        exit(1);
    }

    if (op_flags & OPT_P) {
        int minyear, maxyear;
        if (!rcldb.maxYearSpan(&minyear, &maxyear)) {
            cerr << "maxYearSpan failed: " << rcldb.getReason() << "\n";
            return 1;
        } else {
            cout << "Min year " << minyear << " Max year " << maxyear << "\n";
            return 0;
        }
    }

    if (optind >= argc) {
        Usage();
    }
    std::string qs;
    while (optind < argc) {
        qs += std::string(argv[optind++]) + " ";
    }

    {
        std::string uq;
        std::string charset = rclconfig->getDefCharset(true);
        int ercnt;
        if (!transcode(qs, uq, charset, cstr_utf8, &ercnt)) {
            std::cerr << "Can't convert command line args to utf-8\n";
            return 1;
        } else if (ercnt) {
            std::cerr <<
                ercnt << " errors while converting arguments from " << charset << "to utf-8\n";
        }
        qs = uq;
    }

    std::shared_ptr<Rcl::SearchData> sd;

    if (op_flags & (OPT_a|OPT_o|OPT_f)) {
        sd = std::make_shared<Rcl::SearchData>(Rcl::SCLT_OR, stemlang);
        Rcl::SearchDataClause *clp;
        if (op_flags & OPT_f) {
            clp = new Rcl::SearchDataClauseFilename(qs);
        } else {
            clp = new Rcl::SearchDataClauseSimple(
                (op_flags & OPT_o) ? Rcl::SCLT_OR : Rcl::SCLT_AND, qs);
        }
        if (sd)
            sd->addClause(clp);
    } else {
        sd = wasaStringToRcl(rclconfig, stemlang, qs, reason);
    }

    if (!sd) {
        std::cerr << "Query string interpretation failed: " << reason << "\n";
        return 1;
    }

    std::shared_ptr<Rcl::SearchData> rq(sd);
    Rcl::Query query(&rcldb);
    if (op_flags & OPT_C) {
        query.setCollapseDuplicates(true);
    }
    if (op_flags & OPT_S) {
        query.setSortBy(sortfield, (op_flags & OPT_D) ? false : true);
    }
    Chrono chron;
    if (!query.setQuery(rq)) {
        std::cerr << "Query setup failed: " << query.getReason() << "\n";
        return 1;
    }
    HighlightData hldata;
    sd->getTerms(hldata);
    int cnt;
    if (op_flags & OPT_E) {
        cnt = query.getResCnt(-1, true);
    } else {
        cnt = query.getResCnt();
    }
    if (!(op_flags & OPT_b)) {
        std::cout << "Recoll query: " << rq->getDescription() << "\n";
        if (firstres == 0) {
            if (cnt <= maxcount)
                std::cout << cnt << " results" << "\n";
            else
                std::cout << cnt << " results (printing  " << maxcount << " max):" << "\n";
        } else {
            std::cout << "Printing at most " << cnt - (firstres+maxcount) <<
                " results from first " << firstres << "\n";
        }
    }
    if (op_flags & OPT_Q) {
        std::cout << "Query setup took " << chron.millis() << " mS" << "\n";
        return 0;
    }

    for (int i = firstres; i < firstres + maxcount; i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc))
            break;
        if (paths_only && doc.url.find("file://") != 0) {
            continue;
        }
        if (!extractfile.empty()) {
            if (path_exists(extractfile)) {
                std::cerr << "Output file must not exist.\n";
                return 1;
            }
            TempFile unused;
            auto ret = FileInterner::idocToFile(unused, extractfile, rclconfig, doc, false);
            return ret ? 0 : 1;
        }
        
        if (op_flags & OPT_F) {
            output_fields(fields, doc, query, rcldb, op_flags & OPT_N, op_flags & (OPT_p|OPT_g),
                          snipcnt, op_flags & OPT_g, hldata);
            continue;
        }

        if (op_flags & OPT_e) 
            doc.url = path_pcencode(doc.url);

        if (op_flags & OPT_b) {
            cout << (paths_only ? doc.url.substr(7) : doc.url) << "\n";
        } else {
            std::string titleorfn = doc.meta[Rcl::Doc::keytt];
            if (titleorfn.empty())
                titleorfn = doc.meta[Rcl::Doc::keyfn];
            if (titleorfn.empty()) {
                std::string url;
                printableUrl(rclconfig->getDefCharset(), doc.url, url);
                titleorfn = path_getsimple(url);
            }

            cout
                << doc.mimetype << "\t"
                << "[" << doc.url << "]" << "\t" 
                << "[" << titleorfn << "]" << "\t"
                << doc.fbytes << "\tbytes" << "\t"
                <<  "\n";
            if (op_flags & OPT_m) {
                for (const auto& ent : doc.meta) {
                    cout << ent.first << " = " << ent.second << "\n";
                }
            }
            if (op_flags & OPT_A) {
                bool asSnippets = (op_flags & (OPT_p|OPT_g)) != 0;
                bool showlines = (op_flags & OPT_g) != 0;
                std::string abstract =
                    make_abstract(doc, query, asSnippets, snipcnt, showlines, hldata);
                std::string marker = asSnippets ? "SNIPPETS" : "ABSTRACT";
                if (!abstract.empty()) {
                    cout << marker << "\n" << abstract << "/" << marker << "\n";
                }
            }
        }
        if (op_flags & OPT_d) {
            dump_contents(rclconfig, doc);
        }   
    }

    return 0;
}
