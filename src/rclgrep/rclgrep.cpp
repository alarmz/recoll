/* Copyright (C) 2004 J.F.Dockes 
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

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fnmatch.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#else
#include <direct.h>
#endif
#include <fcntl.h>
#include "safeunistd.h"
#include <getopt.h>

#include <iostream>
#include <sstream>
#include <list>
#include <string>
#include <cstdlib>
#include <deque>
#include <memory>
#include <algorithm>

#include "log.h"
#include "rclinit.h"
#include "rclconfig.h"
#include "smallut.h"
#include "readfile.h"
#include "pathut.h"
#include "rclutil.h"
#include "cancelcheck.h"
#include "execmd.h"
#include "internfile.h"
#include "rcldoc.h"
#include "fstreewalk.h"

// Command line options
static int     op_flags;
#define OPT_A 0x1      
#define OPT_B 0x2      
#define OPT_C 0x4      
#define OPT_H 0x8      
#define OPT_L 0x10     
#define OPT_R 0x20     
#define OPT_T 0x40     
#define OPT_V 0x80     
#define OPT_Z 0x100    
#define OPT_a 0x200    
#define OPT_c 0x400    
#define OPT_e 0x800    
#define OPT_f 0x1000   
#define OPT_h 0x2000   
#define OPT_i 0x4000   
#define OPT_l 0x8000   
#define OPT_n 0x10000  
#define OPT_p 0x20000  
#define OPT_q 0x40000  
#define OPT_r 0x80000  
#define OPT_s 0x100000 
#define OPT_u 0x200000 
#define OPT_v 0x400000 
#define OPT_w 0x800000 
#define OPT_x 0x1000000


enum OptVal {OPTVAL_RECOLL_CONFIG=1000, OPTVAL_HELP, OPTVAL_INCLUDE, OPTVAL_EXCLUDE,
             OPTVAL_EXCLUDEFROM};

static struct option long_options[] = {
    {"regexp", required_argument, 0, 'e'},
    {"file", required_argument, 0, 'f'},
    {"invert-match", required_argument, 0, 'v'},
    {"word-regexp", 0, 0, 'w'}, 
    {"line-regexp", 0, 0, 'x'},
    {"config", required_argument, 0, OPTVAL_RECOLL_CONFIG},
    {"count", 0, 0, 'c'},
    {"files-without-match", 0, 0, 'L'},
    {"files-with-match", 0, 0, 'l'},
    {"with-filename", 0, 0, 'H'},
    {"no-filename", 0, 0, 'h'},
    {"line-number", 0, 0, 'n'},
    {"help", 0, 0, OPTVAL_HELP},
    {"after-context", required_argument, 0, 'A'},
    {"before-context", required_argument, 0, 'B'},
    {"context", required_argument, 0, 'C'},
    {"recurse", 0, 0, 'r'},
    {"include", required_argument, 0, OPTVAL_INCLUDE},
    {"version", 0, 0, 'V'},
    {"silent", 0, 0, 'q'},
    {"quiet", 0, 0, 'q'},
    {"no-messages", 0, 0, 's'},
    {"dereference-recursive", 0, 0, 'R'},
    {"text", 0, 0, 'a'}, //not implemented
    {"exclude", required_argument, 0, OPTVAL_EXCLUDE},
    {"exclude-from", required_argument, 0, OPTVAL_EXCLUDEFROM},
    {0, 0, 0, 0}
};

std::string thisprog;
static const char usage [] =
"\n"
"rclgrep [--help] \n"
"    Print help\n"
"rclgrep [pattern] [<path [path ...]>]\n"
"    Search files and (recursively) directories for a match of <pattern>.\n"
"  -e PATTERNS, --regexp=PATTERNS patterns to search for. Can be given multiple times.\n"
"    if -e is used, all non-option arguments will be used as paths (no [pattern] argument).\n"
"  -f FILE, --file=FILE get patterns from the file argument, one per line\n"
"  -r, --recurse: operate recursively on directories. This is the default except if no paths\n"
"    are given in which case rclgrep searches stdin. Use -r to cause recursing on . in this case\n"
"  --include=FN file name pattern for files to be searched. Can be given multiple times\n"
"  --exclude=FN file name pattern for files not to be searched. Can be given multiple times\n"
"  --exclude-from=FILE take file name exclusion patterns from FILE\n"
"  --config <configdir> : specify configuration directory (default ~/.config/rclgrep)\n"
    "  -v,-w,-x,-c,-L,-l,-H,-n,-A,-B,-C,-q,-s,-R : see grep manual.\n"
;

static void Usage(FILE* fp = stdout)
{
    fprintf(fp, "%s: Usage: %s", path_getsimple(thisprog).c_str(), usage);
    exit(1);
}

std::vector<SimpleRegexp *> g_expressions;
int g_reflags = SimpleRegexp::SRE_NOSUB;

static RclConfig *config;

namespace Rcl {
// This is needed to satisfy a reference from one of the recoll files.
std::string version_string()
{
    return std::string("rclgrep ") + std::string(PACKAGE_VERSION);
}
}
// Working directory before we change: it's simpler to change early
// but some options need the original for computing absolute paths.
static std::string orig_cwd;
static std::string current_topdir;

// Context management. We need to store the last seen lines in case we need to print them as
// context.
static int beforecontext; // Lines of context before match
static int aftercontext;  // Lines of context after match
// Store line for before-context
static void dequeshift(std::deque<std::string>& q, int n, const std::string& ln)
{
    if (n <= 0)
        return;
    if (q.size() >= unsigned(n)) {
        q.pop_front();
    }
    q.push_back(ln);
}
// Show context lines before the match
static void dequeprint(std::deque<std::string>& q)
{
    for (const auto& ln : q) {
        std::cout << ln;
    }
}

void grepit(std::istream& instream, const Rcl::Doc& doc)
{
    std::string ppath;
    std::string indic;
    std::string nindic;
    if (op_flags & OPT_H) {
        ppath = fileurltolocalpath(doc.url);
        if (path_isabsolute(ppath) && ppath.size() > current_topdir.size()) {
            ppath = ppath.substr(current_topdir.size());
        }
        ppath += ":";
        ppath += doc.ipath;
        indic = "::";
        nindic = "--";
    }

    int matchcount = 0;
    std::deque<std::string> beflines;
    int lnum = 0;
    std::string ln;
    bool inmatch{false};
    int aftercount = 0;
    for (;;) {
        std::string line;
        getline(instream, line, '\n');
        if (!instream.good()) {
            break;
        }

        ++lnum;
        if ((op_flags & OPT_n) && !(op_flags & OPT_c)) {
            ln = ulltodecstr(lnum) + ":";
        }

        bool ismatch = false;
        for (const auto e_p : g_expressions) {
            auto match = e_p->simpleMatch(line);
            if (((op_flags & OPT_v) ^ match)) {
                ismatch = true;
                break;
            }
        }

        if (ismatch) {
            if (op_flags&OPT_q) {
                exit(0);
            }
            // We have a winner line.
            if (op_flags & OPT_c) {
                matchcount++;
            } else {
                // Stop printing after-context
                aftercount = 0;
                // If this is the beginning/first matching line, possibly output the before context
                if (!inmatch && beforecontext) {
                    dequeprint(beflines);
                }
                inmatch=true;
                std::cout << ppath << indic << ln << line << "\n";
                if (beforecontext && !beflines.empty()) {
                    beflines.pop_front();
                }
            }
        } else {
            if (op_flags&OPT_q) {
                continue;
            }
            // Non-matching line.
            if (inmatch && aftercontext && !(op_flags&OPT_c)) {
                aftercount = aftercontext;
            }
            inmatch = false;
            if (aftercount) {
                std::cout << ppath << nindic << ln << line << "\n";
                aftercount--;
                if (aftercount == 0)
                    std::cout << "--\n";
            } else if (beforecontext) {
                dequeshift(beflines, beforecontext, ppath + nindic + ln + line + "\n");
            }
        } 
    }
    if (op_flags & OPT_L) {
        if (matchcount == 0) {
            std::cout << ppath << "\n";
        }
    } else if (op_flags & OPT_l) {
        if (matchcount) {
            std::cout << ppath << "\n";
        }
    } else if ((op_flags & OPT_c) && matchcount) {
        std::cout << ppath << "::" << matchcount << "\n";
    }
}

bool processpath(RclConfig *config, const std::string& path)
{
    LOGDEB1("processpath: [" << path << "]\n");
    if (path.empty()) {
        // stdin
        Rcl::Doc doc;
        doc.url = std::string("file://") + "(standard input)";
        grepit(std::cin, doc);
    } else {
        struct PathStat st;
        if (path_fileprops(path, &st, false) < 0) {
            std::cerr << path << " : ";
            perror("stat");
            return false;
        }
    
        config->setKeyDir(path_getfather(path));
    
        std::string mimetype;

        FileInterner interner(path, st, config, FileInterner::FIF_none);
        if (!interner.ok()) {
            return false;
        }
        mimetype = interner.getMimetype();

        FileInterner::Status fis = FileInterner::FIAgain;
        Rcl::Doc doc;
        while (fis == FileInterner::FIAgain) {
            doc.erase();
            try {
                fis = interner.internfile(doc);
            } catch (CancelExcept) {
                LOGERR("fsIndexer::processone: interrupted\n");
                return false;
            }
            if (fis == FileInterner::FIError) {
                return false;
            }
            
            if (doc.url.empty())
                doc.url = path_pathtofileurl(path);

            std::istringstream str(doc.text);
            grepit(str, doc);
        }
    }
    return true;
}


class WalkerCB : public FsTreeWalkerCB {
public:
    WalkerCB(const std::vector<std::string>& fnincl, RclConfig *config)
        : m_fnincludes(fnincl), m_config(config) {}
    virtual FsTreeWalker::Status processone(
        const std::string& fn, FsTreeWalker::CbFlag flg, const struct PathStat&) override {
        if (flg == FsTreeWalker::FtwRegular) {
            if (m_fnincludes.empty()) {
                processpath(m_config, fn);
            } else {
                for (const auto& pat : m_fnincludes) {
                    if (fnmatch(pat.c_str(), path_getsimple(fn).c_str(), 0) == 0) {
                        processpath(m_config, fn);
                        break;
                    }
                }
            }
        }
        return FsTreeWalker::FtwOk;
    }
    const std::vector<std::string>& m_fnincludes;
    RclConfig *m_config{nullptr};
};

bool recursive_grep(RclConfig *config, const std::string& top,
                    const std::vector<std::string>& fnincludes,
                    const std::vector<std::string>& fnexcludes)
{
//    std::cerr << "recursive_grep: top : [" << top << "]\n";
    WalkerCB cb(fnincludes, config);
    int opts = FsTreeWalker::FtwTravNatural;
    if (op_flags & OPT_R) {
        opts |= FsTreeWalker::FtwFollow;
    }
    FsTreeWalker walker(opts);
    walker.setSkippedNames(fnexcludes);
    current_topdir = top;
    if (path_isdir(top)) {
        path_catslash(current_topdir);
    }
    walker.walk(top, cb);
    return true;
}

bool processpaths(RclConfig *config, const std::vector<std::string> &_paths,
                  const std::vector<std::string>& fnincludes,
                  const std::vector<std::string>& fnexcludes)
{
    if (_paths.empty())
        return true;
    std::vector<std::string> paths;
    std::string origcwd = config->getOrigCwd();
    for (const auto& path : _paths) {
        paths.push_back(path_canon(path, &origcwd));
    }
    std::sort(paths.begin(), paths.end());
    auto uit = std::unique(paths.begin(), paths.end());
    paths.resize(uit - paths.begin());

    for (const auto& path : paths) {
        LOGDEB("processpaths: " << path << "\n");
        if (path_isdir(path)) {
            recursive_grep(config, path, fnincludes, fnexcludes);
        } else {
            if (!path_readable(path)) {
                if (!(op_flags & OPT_s)) {
                    std::cerr << "Can't read: " << path << "\n";
                }
                continue;
            }
            processpath(config, path);
        }
    }
    return true;
}

std::string make_config()
{
    std::string confdir;
    const char *cp = nullptr;
    cp = getenv("XDG_CONFIG_HOME");
    if (nullptr != cp) {
        confdir = cp;
    } else {
        confdir = path_cat(path_home(), ".config");
    }
    confdir = path_cat(confdir, "rclgrep");
    if (!path_makepath(confdir, 0755)) {
        std::cerr << "Could not create configuration directory " << confdir << " : ";
        perror("");
        exit(1);
    }
    std::string mainconf = path_cat(confdir, "recoll.conf");
    if (!path_exists(mainconf)) {
        FILE *fp = fopen(mainconf.c_str(), "w");
        if (nullptr == fp) {
            std::cerr << "Could not create configuration file " << mainconf << " : ";
            perror("");
            exit(1);
        }
        fprintf(fp, "# rclgrep default configuration. Will only be created by the program if it\n");
        fprintf(fp, "# does not exist, you can add your own customisations in here\n");
        fprintf(fp, "helperlogfilename = /dev/null\n");
        fprintf(fp, "textunknownasplain = 1\n");
        fprintf(fp, "loglevel = 1\n");
        fclose(fp);
    }
    return confdir;
}

static void add_expressions(const std::string& exps)
{
    std::vector<std::string> vexps;
    stringToTokens(exps, vexps, "\n");
    for (const auto& pattern : vexps) {
        if (op_flags & OPT_x) {
            auto newpat = std::string("^(") + pattern + ")$";
            g_expressions.push_back(new SimpleRegexp(newpat, g_reflags));
        } else if (op_flags & OPT_w) {
            auto newpat = std::string("(^|[^a-zA-Z0-9_])(") + pattern + ")([^a-zA-Z0-9_]|$)";
            g_expressions.push_back(new SimpleRegexp(newpat, g_reflags));
        } else {            
            g_expressions.push_back(new SimpleRegexp(pattern, g_reflags));
        }
    }
}

std::vector<std::string> g_expstrings;
static void buildexps()
{
    for (const auto& s : g_expstrings)
        add_expressions(s);
}

static void exps_from_file(const std::string& fn)
{
    std::string data;
    std::string reason;
    if (!file_to_string(fn, data, -1, -1, &reason)) {
        std::cerr << "Could not read " << fn << " : " << reason << "\n";
        exit(1);
    }
    g_expstrings.push_back(data);
}

std::string excludes_from_file(const std::string& fn)
{
    std::string data;
    std::string reason;
    if (!file_to_string(fn, data, -1, -1, &reason)) {
        std::cerr << "Could not read " << fn << " : " << reason << "\n";
        exit(1);
    }
    return data;
}

static std::vector<std::string> buildexcludes(const std::vector<std::string>& data)
{
    std::vector<std::string> ret;
    for (const auto& lst : data) {
        std::vector<std::string> v;
        stringToTokens(lst, v, "\n");
        for (const auto& s : v) {
            ret.push_back(s);
        }
    }
    return ret;
}
    
int main(int argc, char *argv[])
{
    int ret;
    std::string a_config;
    std::vector<std::string> fnincludes;
    std::vector<std::string> fnexcludedata;
    
    while ((ret = getopt_long(argc, argv, "A:B:C:HLRVce:f:hilnp:qrsvwx",
                              long_options, NULL)) != -1) {
        switch (ret) {
        case 'A': op_flags |= OPT_A; aftercontext = atoi(optarg); break;
        case 'B': op_flags |= OPT_B; beforecontext = atoi(optarg); break;
        case 'C': op_flags |= OPT_C; aftercontext = beforecontext = atoi(optarg); break;
        case 'H': op_flags |= OPT_H; break;
        case 'L': op_flags |= OPT_L|OPT_c; break;
        case 'R': op_flags |= OPT_R; break;
        case 'V': std::cout << Rcl::version_string() << "\n"; return 0;
        case 'c': op_flags |= OPT_c; break;
        case 'e': op_flags |= OPT_e; g_expstrings.push_back(optarg); break;
        case 'f': op_flags |= OPT_f; exps_from_file(optarg);break;
        case 'h': op_flags |= OPT_h; break;
        case 'i': op_flags |= OPT_i; g_reflags |= SimpleRegexp::SRE_ICASE; break;
        case 'l': op_flags |= OPT_l|OPT_c; break;
        case 'n': op_flags |= OPT_n; break;
        case 'p': op_flags |= OPT_p; fnincludes.push_back(optarg); break;
        case 'q': op_flags |= OPT_q; break;
        case 'r': op_flags |= OPT_r|OPT_H; break;
        case 's': op_flags |= OPT_s; break;
        case 'v': op_flags |= OPT_v; break;
        case 'w': op_flags |= OPT_w; break;
        case 'x': op_flags |= OPT_x; break;
        case OPTVAL_RECOLL_CONFIG: a_config = optarg; break;
        case OPTVAL_HELP: Usage(stdout); break;
        case OPTVAL_INCLUDE: fnincludes.push_back(optarg); break;
        case OPTVAL_EXCLUDE: fnexcludedata.push_back(optarg); break;
        case OPTVAL_EXCLUDEFROM: fnexcludedata.push_back(excludes_from_file(optarg)); break;
        default: Usage(); break;
        }
    }
    int aremain = argc - optind;

    if (!(op_flags & (OPT_e|OPT_f))) {
        if (aremain == 0)
            Usage();
        std::string patterns =  argv[optind++];
        aremain--;
        g_expstrings.push_back(patterns);
    }

    buildexps();

    // If there are more than 1 file args and -h was not used, we want to print file names.
    if ((aremain > 1 || (aremain == 1 && path_isdir(argv[optind]))) && !(op_flags & OPT_h)) {
        op_flags |= OPT_H;
    }
    
    if (a_config.empty()) {
        a_config = make_config();
    }
    std::string reason;
    int flags = 0;
    config = recollinit(flags, nullptr, nullptr, reason, &a_config);
    if (config == 0 || !config->ok()) {
        std::cerr << "Configuration problem: " << reason << "\n";
        exit(1);
    }

    // Get rid of log messages
    Logger::getTheLog()->setLogLevel(Logger::LLFAT);

    orig_cwd = path_cwd();
    std::string rundir;
    config->getConfParam("idxrundir", rundir);
    if (!rundir.empty()) {
        if (!rundir.compare("tmp")) {
            rundir = tmplocation();
        }
        LOGDEB2("rclgrep: changing current directory to [" << rundir << "]\n");
        if (!path_chdir(rundir)) {
            LOGSYSERR("main", "chdir", rundir);
        }
    }
    std::vector<std::string> paths;
    if (aremain == 0 && !(op_flags & OPT_r)) {
        // Read from stdin
        processpath(config, std::string());
    } else {
        if (aremain == 0) {
            paths.push_back(".");
        } else {
            while (aremain--) {
                paths.push_back(argv[optind++]);
            }
        }
    }

    auto fnexcludes = buildexcludes(fnexcludedata);
    bool status = processpaths(config, paths, fnincludes, fnexcludes);
    if (op_flags&OPT_q) {
        exit(1);
    }
    return status ? 0 : 1;
}
