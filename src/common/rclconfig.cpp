/* Copyright (C) 2004-2022 J.F.Dockes 
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
#include <errno.h>
#ifndef _WIN32
#include <langinfo.h>
#include <sys/param.h>
#else
#include <direct.h>
#include "wincodepages.h"
#include "safeunistd.h"
#endif
#include <limits.h>
#ifdef __FreeBSD__
#include <osreldate.h>
#endif

#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <iterator>
#include <array>

#include "cstr.h"
#include "pathut.h"
#include "copyfile.h"
#include "rclutil.h"
#include "rclconfig.h"
#include "conftree.h"
#include "log.h"
#include "smallut.h"
#include "readfile.h"
#include "fstreewalk.h"
#include "cpuconf.h"
#include "execmd.h"
#include "md5.h"
#include "idxdiags.h"
#include "unacpp.h"

using namespace std;

// Naming the directory for platform-specific default config files, overriding the top-level ones
// E.g. /usr/share/recoll/examples/windows
#ifdef _WIN32
static const string confsysdir{"windows"};
#elif defined(__APPLE__)
static const string confsysdir{"macos"};
#else
static const string confsysdir;
#endif

// This is only used for testing the lower-casing code on linux
//#define NOCASE_PTRANS
#ifdef NOCASE_PTRANS
#warning NOCASE_PTRANS IS SET
#endif

#if defined(_WIN32) || defined(NOCASE_PTRANS)
// The confsimple copies can't print themselves out: no conflines. Need to work around this one day.
// For now: use a ConfSimple::sortwalk() callback. While we're at it, lowercase the values.
ConfSimple::WalkerCode varprintlower(void *vstr, const std::string& nm, const std::string& val)
{
    std::string *str = (std::string *)vstr;
    if (nm.empty()) {
        *str += std::string("[") + unactolower(val) + "]\n";
    } else {
        *str += unactolower(nm) + " = " + val + "\n";
    }
    return ConfSimple::WALK_CONTINUE;
}
#endif // _WIN32

// Things for suffix comparison. We define a string class and string 
// comparison with suffix-only sensitivity
class SfString {
public:
    SfString(const string& s) : m_str(s) {}
    bool operator==(const SfString& s2) const {
        string::const_reverse_iterator r1 = m_str.rbegin(), re1 = m_str.rend(),
            r2 = s2.m_str.rbegin(), re2 = s2.m_str.rend();
        while (r1 != re1 && r2 != re2) {
            if (*r1 != *r2) {
                return 0;
            }
            ++r1; ++r2;
        }
        return 1;
    }
    string m_str;
};

class SuffCmp {
public:
    int operator()(const SfString& s1, const SfString& s2) const {
        //cout << "Comparing " << s1.m_str << " and " << s2.m_str << "\n";
        string::const_reverse_iterator 
            r1 = s1.m_str.rbegin(), re1 = s1.m_str.rend(),
            r2 = s2.m_str.rbegin(), re2 = s2.m_str.rend();
        while (r1 != re1 && r2 != re2) {
            if (*r1 != *r2) {
                return *r1 < *r2 ? 1 : 0;
            }
            ++r1; ++r2;
        }
        return 0;
    }
};

typedef multiset<SfString, SuffCmp> SuffixStore;

// Static, logically const, RclConfig members or module static
// variables are initialized once from the first object build during
// process initialization.

// We default to a case- and diacritics-less index for now
bool o_index_stripchars = true;
// Default to storing the text contents for generating snippets. This
// is only an approximate 10% bigger index and produces nicer
// snippets.
bool o_index_storedoctext = true;

bool o_no_term_positions = false;

bool o_uptodate_test_use_mtime = false;

bool o_expand_phrases = false;
// We build this once. Used to ensure that the suffix used for a temp
// file of a given MIME type is the FIRST one from the mimemap config
// file. Previously it was the first in alphabetic (map) order, with
// sometimes strange results.
static unordered_map<string, string> mime_suffixes;

// Cache parameter string values for params which need computation and which can change with the
// keydir. Minimize work by using the keydirgen and a saved string to avoid unneeded recomputations:
// keydirgen is incremented in RclConfig with each setKeyDir(). We compare our saved value with the
// current one. If it did not change no get() is needed. If it did change, but the resulting param
// get() string value is identical, no recomputation is needed.
class ParamStale {
public:
    ParamStale() {}
    ParamStale(RclConfig *rconf, const std::string& nm)
        : parent(rconf), paramnames(std::vector<std::string>(1, nm)), savedvalues(1) {
    }
    ParamStale(RclConfig *rconf, const std::vector<std::string>& nms)
        : parent(rconf), paramnames(nms), savedvalues(nms.size()) {
    }

    void init(ConfNull *cnf) {
        conffile = cnf;
        active = false;
        if (conffile) {
            for (auto& nm : paramnames) {
                if (conffile->hasNameAnywhere(nm)) {
                    active = true;
                    break;
                }
            }
        }
        savedkeydirgen = -1;
    }

    const string& getvalue(unsigned int i = 0) const {
        if (i < savedvalues.size()) {
            return savedvalues[i];
        } else {
            static std::string nll;
            return nll;
        }
    }
    // definition needs Internal class: later
    bool needrecompute();

private:
    // The config we belong to. 
    RclConfig *parent{0};
    // The configuration file we search for values. This is a borrowed
    // pointer belonging to the parent, we do not manage it.
    ConfNull  *conffile{0};
    std::vector<std::string>    paramnames;
    std::vector<std::string>    savedvalues;
    // Check at init if the configuration defines our vars at all. No
    // further processing is needed if it does not.
    bool      active{false}; 
    int       savedkeydirgen{-1};
};

class RclConfig::Internal {
public:
    Internal(RclConfig *p)
        : m_parent(p),
          m_oldstpsuffstate(p, "recoll_noindex"),
          m_stpsuffstate(p, {"noContentSuffixes", "noContentSuffixes+", "noContentSuffixes-"}),
          m_skpnstate(p, {"skippedNames", "skippedNames+", "skippedNames-"}),
          m_onlnstate(p, "onlyNames"),
          m_rmtstate(p, "indexedmimetypes"),
          m_xmtstate(p, "excludedmimetypes"),
          m_mdrstate(p, "metadatacmds") {}

    RclConfig *m_parent;
    int m_ok;
    // Original current working directory. Set once at init before we do any
    // chdir'ing and used for converting user args to absolute paths.
    static std::string o_origcwd;
    std::string m_reason;    // Explanation for bad state
    std::string m_confdir;   // User directory where the customized files are stored
    // Normally same as confdir. Set to store all bulk data elsewhere.
    // Provides defaults top location for dbdir, webcachedir,
    // mboxcachedir, aspellDictDir, which can still be used to
    // override.
    std::string m_cachedir;  
    std::string m_datadir;   // Example: /usr/local/share/recoll
    std::string m_keydir;    // Current directory used for parameter fetches.
    int    m_keydirgen; // To help with knowing when to update computed data.

    std::vector<std::string> m_cdirs; // directory stack for the confstacks

    std::map<std::string, FieldTraits>  m_fldtotraits; // Field to field params
    std::map<std::string, std::string>  m_aliastocanon;
    std::map<std::string, std::string>  m_aliastoqcanon;
    std::set<std::string> m_storedFields;
    std::map<std::string, std::string>  m_xattrtofld;

    unsigned int m_maxsufflen;

    ParamStale   m_oldstpsuffstate; // Values from user mimemap, now obsolete
    ParamStale   m_stpsuffstate;
    std::vector<std::string> m_stopsuffvec;
    // skippedNames state 
    ParamStale   m_skpnstate;
    std::vector<std::string> m_skpnlist;
    // onlyNames state 
    ParamStale   m_onlnstate;
    std::vector<std::string> m_onlnlist;

    // Parameters auto-fetched on setkeydir
    std::string m_defcharset;
    static std::string o_localecharset;
    // Limiting set of mime types to be processed. Normally empty.
    ParamStale    m_rmtstate;
    std::unordered_set<std::string>   m_restrictMTypes; 
    // Exclusion set of mime types. Normally empty
    ParamStale    m_xmtstate;
    std::unordered_set<std::string>   m_excludeMTypes; 
    // Metadata-gathering external commands (e.g. used to reap tagging info: "tmsu tags %f")
    ParamStale    m_mdrstate;
    std::vector<MDReaper> m_mdreapers;

    std::vector<std::pair<int, int> > m_thrConf;

    //////////////////
    // Members needing explicit processing when copying 
    std::unique_ptr<ConfStack<ConfTree>> m_conf;   // Parsed configuration files
    std::unique_ptr<ConfStack<ConfTree>> mimemap;  // The files don't change with keydir, 
    std::unique_ptr<ConfStack<ConfSimple>> mimeconf; // but their content may depend on it.
    std::unique_ptr<ConfStack<ConfSimple>> mimeview; // 
    std::unique_ptr<ConfStack<ConfSimple>> m_fields;
    std::unique_ptr<ConfSimple>            m_ptrans; // Paths translations
#if defined(_WIN32) || defined(NOCASE_PTRANS)
    std::unique_ptr<ConfSimple> m_lptrans; // lowercased paths translations
    bool m_lptrans_stale{true};
#endif
    std::unique_ptr<SuffixStore> m_stopsuffixes;
    ///////////////////

    /** Create initial user configuration */
    bool initUserConfig();
    /** Init all ParamStale members */
    void initParamStale(ConfNull *cnf, ConfNull *mimemap);
    /** Copy from other */
    void initFrom(const RclConfig& r);
    /** Init pointers to 0 */
    void zeroMe();
    bool readFieldsConfig(const std::string& errloc);
};

string RclConfig::Internal::o_localecharset; 
string RclConfig::Internal::o_origcwd; 

bool ParamStale::needrecompute()
{
    LOGDEB1("ParamStale:: needrecompute. parent gen " << parent->m->m_keydirgen <<
            " mine " << savedkeydirgen << "\n");

    if (!conffile) {
        LOGDEB("ParamStale::needrecompute: conffile not set\n");
        return false;
    }

    bool needrecomp = false;
    if (active && parent->m->m_keydirgen != savedkeydirgen) {
        savedkeydirgen = parent->m->m_keydirgen;
        for (unsigned int i = 0; i < paramnames.size(); i++) {
            string newvalue;
            conffile->get(paramnames[i], newvalue, parent->m->m_keydir);
            LOGDEB1("ParamStale::needrecompute: " << paramnames[i] << " -> " <<
                    newvalue << " keydir " << parent->m->m_keydir << "\n");
            if (newvalue.compare(savedvalues[i])) {
                savedvalues[i] = newvalue;
                needrecomp = true;
            }
        }
    }
    return needrecomp;
}


static const char blurb0[] = 
    "# The system-wide configuration files for recoll are located in:\n"
    "#   ";
static const char blurb1[] = 
    "\n"
    "# The default configuration files are commented, you should take a look\n"
    "# at them for an explanation of what can be set (you could also take a look\n"
    "# at the manual instead).\n"
    "# Values set in this file will override the system-wide values for the file\n"
    "# with the same name in the central directory. The syntax for setting\n"
    "# values is identical.\n"
    ;

// Use uni2ascii -a K to generate these from the utf-8 strings
// Swedish and Danish. 
static const char swedish_ex[] = "unac_except_trans = \303\244\303\244 \303\204\303\244 \303\266\303\266 \303\226\303\266 \303\274\303\274 \303\234\303\274 \303\237ss \305\223oe \305\222oe \303\246ae \303\206ae \357\254\201fi \357\254\202fl \303\245\303\245 \303\205\303\245";
// German:
static const char german_ex[] = "unac_except_trans = \303\244\303\244 \303\204\303\244 \303\266\303\266 \303\226\303\266 \303\274\303\274 \303\234\303\274 \303\237ss \305\223oe \305\222oe \303\246ae \303\206ae \357\254\201fi \357\254\202fl";

static std::array<const char *, 5> configfiles{
    "recoll.conf", "mimemap", "mimeconf", "mimeview", "fields"};

// Create initial user config by creating commented empty files
bool RclConfig::Internal::initUserConfig()
{
    // Explanatory text
    std::string blurb = blurb0 + path_cat(m_datadir, "examples") + blurb1;

    auto direxisted = path_exists(m_confdir);
    // Use protective 700 mode to create the top configuration
    // directory: documents can be reconstructed from index data.
    if (!direxisted && !path_makepath(m_confdir, 0700)) {
        m_reason += string("mkdir(") + m_confdir + ") failed: "+ strerror(errno);
        return false;
    }

    // Backends is handled differently. At run time, only the data from the user configuration is
    // used, nothing comes from the shared directory. We copy the default file to the user
    // directory. It's the only one which we recreate if absent, mostly because, otherwise, it would
    // never have been enabled on existing config when the Joplin indexer came out.
    std::string src = path_cat(m_datadir, {"examples", "backends"});
    std::string dst = path_cat(m_confdir, "backends");
    if (!path_exists(dst)) {
        std::string reason;
        if (!copyfile(src.c_str(), dst.c_str(), reason)) {
            m_reason += std::string("Copying the backends file: ") + reason;
            LOGERR(m_reason);
        }
    }

    // Recreating the other optional files after the user may have removed them is ennoying, don't
    if (direxisted)
        return true;
    
    string lang = localelang();
    for (const auto& cnff : configfiles) {
        string dst = path_cat(m_confdir, string(cnff));
        if (!path_exists(dst)) {
            fstream output;
            if (path_streamopen(dst, ios::out, output)) {
                output << blurb << "\n";
                if (!strcmp(cnff, "recoll.conf")) {
                    // Add improved unac_except_trans for some languages
                    if (lang == "se" || lang == "dk" || lang == "no" || lang == "fi") {
                        output << swedish_ex << "\n";
                    } else if (lang == "de") {
                        output << german_ex << "\n";
                    }
                }
            } else {
                m_reason += string("open ") + dst + ": " + strerror(errno);
                return false;
            }
        }
    }

    
    return true;
}

void RclConfig::Internal::zeroMe() {
    m_ok = false; 
    m_keydirgen = 0;
    m_maxsufflen = 0;
    initParamStale(0, 0);
}

void RclConfig::Internal::initFrom(const RclConfig& r)
{
    zeroMe();

    // Copyable fields
    m_ok = r.m->m_ok;
    if (!m_ok)
        return;
    m_reason = r.m->m_reason;
    m_confdir = r.m->m_confdir;
    m_cachedir = r.m->m_cachedir;
    m_datadir = r.m->m_datadir;
    m_keydir = r.m->m_keydir;
    m_keydirgen = r.m->m_keydirgen;
    m_cdirs = r.m->m_cdirs;
    m_fldtotraits = r.m->m_fldtotraits;
    m_aliastocanon = r.m->m_aliastocanon;
    m_aliastoqcanon = r.m->m_aliastoqcanon;
    m_storedFields = r.m->m_storedFields;
    m_xattrtofld = r.m->m_xattrtofld;
    m_maxsufflen = r.m->m_maxsufflen;
    m_stopsuffvec = r.m->m_stopsuffvec;
    m_skpnlist = r.m->m_skpnlist;
    m_onlnlist = r.m->m_onlnlist;
    m_defcharset = r.m->m_defcharset;
    m_restrictMTypes  = r.m->m_restrictMTypes;
    m_excludeMTypes = r.m->m_excludeMTypes;
    m_mdreapers = r.m->m_mdreapers;
    m_thrConf = r.m->m_thrConf;

    // Special treatment
    if (r.m->m_conf)
        m_conf = std::make_unique<ConfStack<ConfTree>>(*(r.m->m_conf));
    if (r.m->mimemap)
        mimemap = std::make_unique<ConfStack<ConfTree>>(*(r.m->mimemap));
    if (r.m->mimeconf)
        mimeconf = std::make_unique<ConfStack<ConfSimple>>(*(r.m->mimeconf));
    if (r.m->mimeview)
        mimeview = std::make_unique<ConfStack<ConfSimple>>(*(r.m->mimeview));
    if (r.m->m_fields)
        m_fields = std::make_unique<ConfStack<ConfSimple>>(*(r.m->m_fields));
    if (r.m->m_ptrans)
        m_ptrans = std::make_unique<ConfSimple>(*(r.m->m_ptrans));
#if defined(_WIN32) || defined(NOCASE_PTRANS)
    m_lptrans_stale = true;
#endif
    if (r.m->m_stopsuffixes)
        m_stopsuffixes = std::make_unique<SuffixStore>(*(r.m->m_stopsuffixes));
    initParamStale(m_conf.get(), mimemap.get());
}

void RclConfig::Internal::initParamStale(ConfNull *cnf, ConfNull *mimemap)
{
    m_oldstpsuffstate.init(mimemap);
    m_stpsuffstate.init(cnf);
    m_skpnstate.init(cnf);
    m_onlnstate.init(cnf);
    m_rmtstate.init(cnf);
    m_xmtstate.init(cnf);
    m_mdrstate.init(cnf);
}


RclConfig::RclConfig(const string *argcnf)
{
    m = std::make_unique<Internal>(this);
    m->zeroMe();

    if (m->o_origcwd.empty()) {
        m->o_origcwd = path_cwd();
        if (m->o_origcwd.empty()) {
            std::cerr << "recollxx: can't retrieve current working "
                "directory: relative path translations will fail\n";
        }
    }

    // Compute our data dir name, typically /usr/local/share/recoll
    m->m_datadir = path_pkgdatadir();
    // We only do the automatic configuration creation thing for the default
    // config dir, not if it was specified through -c or RECOLL_CONFDIR
    bool autoconfdir = false;

    // Command line config name overrides environment
    if (argcnf && !argcnf->empty()) {
        m->m_confdir = path_absolute(*argcnf);
        if (m->m_confdir.empty()) {
            m->m_reason = string("Cant turn [") + *argcnf + "] into absolute path";
            return;
        }
    } else {
        const char *cp = getenv("RECOLL_CONFDIR");
        if (cp) {
            m->m_confdir = path_canon(cp);
        } else {
            autoconfdir = true;
            m->m_confdir=path_cat(path_homedata(), path_defaultrecollconfsubdir());
        }
    }

    // Note: autoconfdir and isDefaultConfig() are normally the same. We just 
    // want to avoid the imperfect test in isDefaultConfig() if we actually know
    // this is the default conf
    if (!autoconfdir && !isDefaultConfig()) {
        if (!path_exists(m->m_confdir)) {
            m->m_reason = std::string("Explicitly specified configuration [") + m->m_confdir +
                "] directory must exist (won't be automatically created). Use mkdir first";
            return;
        }
    }

    // We used to test for the confdir not existing to call initUserConfig() but it's
    // actually better to call initUserConfig() always: it does not clobber existing
    // files, and will copy possibly missing ones to an existing directory. The only
    // problem is that removing, e.g. backends, will be reverted on the next program exec:
    // the user needs to edit or truncate the file instead of removing it.
    if (!m->initUserConfig())
        return;

    // This can't change once computed inside a process. It would be
    // nicer to move this to a static class initializer to avoid
    // possible threading issues but this doesn't work (tried) as
    // things would not be ready. In practise we make sure that this
    // is called from the main thread at once, by constructing a config
    // from recollinit
    if (m->o_localecharset.empty()) {
#ifdef _WIN32
        m->o_localecharset = winACPName();
#elif defined(__APPLE__)
        m->o_localecharset = cstr_utf8;
#else
        const char *cp;
        cp = nl_langinfo(CODESET);
        // We don't keep US-ASCII. It's better to use a superset
        // Ie: me have a C locale and some french file names, and I
        // can't imagine a version of iconv that couldn't translate
        // from iso8859?
        // The 646 thing is for solaris. 
        if (cp && *cp && strcmp(cp, "US-ASCII") 
#ifdef sun
            && strcmp(cp, "646")
#endif
            ) {
            m->o_localecharset = string(cp);
        } else {
            // Use cp1252 instead of iso-8859-1, it's a superset.
            m->o_localecharset = string(cstr_cp1252);
        }
#endif
        LOGDEB1("RclConfig::getDefCharset: localecharset ["  << m->o_localecharset << "]\n");
    }

    const char *cp;

    // Additional config directory, values override user ones
    if ((cp = getenv("RECOLL_CONFTOP"))) {
        m->m_cdirs.push_back(cp);
    } 

    // User config
    m->m_cdirs.push_back(m->m_confdir);

    // Additional config directory, overrides system's, overridden by user's
    if ((cp = getenv("RECOLL_CONFMID"))) {
        m->m_cdirs.push_back(cp);
    } 

    // Base/installation config, and its platform-specific overrides
    std::string defaultsdir = path_cat(m->m_datadir, "examples");
    if (!confsysdir.empty()) {
        std::string sdir = path_cat(defaultsdir, confsysdir);
        if (path_isdir(sdir)) {
            m->m_cdirs.push_back(sdir);
        }
    }
    m->m_cdirs.push_back(defaultsdir);

    string cnferrloc;
    for (const auto& dir : m->m_cdirs) {
        cnferrloc += "[" + dir + "] or ";
    }
    if (cnferrloc.size() > 4) {
        cnferrloc.erase(cnferrloc.size()-4);
    }

    // Read and process "recoll.conf"
    if (!updateMainConfig()) {
        m->m_reason = string("No/bad main configuration file in: ") + cnferrloc;
        return;
    }

    // Other files

    // Note: no need to make mimemap case-insensitive, the caller code lowercases the suffixes, 
    // which is more efficient.
    m->mimemap = std::make_unique<ConfStack<ConfTree>>(ConfSimple::CFSF_RO, "mimemap", m->m_cdirs);
    if (!m->mimemap->ok()) {
        m->m_reason = string("No or bad mimemap file in: ") + cnferrloc;
        return;
    }

    // Maybe create the MIME to suffix association reverse map. Do it
    // in file order so that we can control what suffix is used when
    // there are several. This only uses the distributed file, not any
    // local customization (too complicated).
    if (mime_suffixes.empty()) {
        ConfSimple mm(path_cat(path_cat(m->m_datadir, "examples"), "mimemap").c_str());
        vector<ConfLine> order = mm.getlines();
        for (const auto& entry: order) {
            if (entry.m_kind == ConfLine::CFL_VAR) {
                LOGDEB1("CONFIG: " << entry.m_data << " -> " << entry.m_value << "\n");
                // Remember: insert() only does anything for new keys,
                // so we only have the first value in the map
                mime_suffixes.insert(pair<string,string>(entry.m_value, entry.m_data));
            }
        }
    }

    m->mimeconf = std::make_unique<ConfStack<ConfSimple>>(
        ConfSimple::CFSF_RO, "mimeconf", m->m_cdirs);
    if (!m->mimeconf->ok()) {
        m->m_reason = string("No/bad mimeconf in: ") + cnferrloc;
        return;
    }
    m->mimeview = std::make_unique<ConfStack<ConfSimple>>(
        ConfSimple::CFSF_NONE, "mimeview", m->m_cdirs);
    if (!m->mimeview->ok())
        m->mimeview = std::make_unique<ConfStack<ConfSimple>>(
            ConfSimple::CFSF_RO, "mimeview", m->m_cdirs);
    if (!m->mimeview->ok()) {
        m->m_reason = string("No/bad mimeview in: ") + cnferrloc;
        return;
    }
    if (!m->readFieldsConfig(cnferrloc))
        return;

    // Default is no threading
    m->m_thrConf = {{-1, 0}, {-1, 0}, {-1, 0}};

    m->m_ptrans = std::make_unique<ConfSimple>(
        ConfSimple::CFSF_NONE, path_cat(m->m_confdir, "ptrans"));

    m->m_ok = true;
    setKeyDir(cstr_null);

    m->initParamStale(m->m_conf.get(), m->mimemap.get());

    return;
}

RclConfig::RclConfig(const RclConfig &r) 
{
    m = std::make_unique<Internal>(this);
    m->initFrom(r);
}

RclConfig::~RclConfig()
{
}

RclConfig& RclConfig::operator=(const RclConfig &r)
{
    if (this != &r) {
        m->zeroMe();
        m->initFrom(r);
    }
    return *this;
}


bool RclConfig::isDefaultConfig() const
{
    string defaultconf = path_cat(path_homedata(), path_defaultrecollconfsubdir());
    path_catslash(defaultconf);
    string specifiedconf = path_canon(m->m_confdir);
    path_catslash(specifiedconf);
    return !defaultconf.compare(specifiedconf);
}

bool RclConfig::updateMainConfig()
{
    auto newconf = std::make_unique<ConfStack<ConfTree>>(
        ConfSimple::CFSF_RO|ConfSimple::CFSF_KEYNOCASE, "recoll.conf", m->m_cdirs);

    if (!newconf->ok()) {
        std::cerr << "updateMainConfig: NEW CONFIGURATION READ FAILED. dirs: " <<
            stringsToString(m->m_cdirs) << "\n";
        if (m->m_conf && m->m_conf->ok())
            return false;
        m->m_ok = false;
        m->initParamStale(0, 0);
        return false;
    }

    m->m_conf.swap(newconf);
    m->initParamStale(m->m_conf.get(), m->mimemap.get());
    
    setKeyDir(cstr_null);

    bool bvalue = true;
    if (getConfParam("skippedPathsFnmPathname", &bvalue) && bvalue == false) {
        FsTreeWalker::setNoFnmPathname();
    }
    string nowalkfn;
    getConfParam("nowalkfn", nowalkfn);
    if (!nowalkfn.empty()) {
        FsTreeWalker::setNoWalkFn(nowalkfn);
    }
    
    static int m_index_stripchars_init = 0;
    if (!m_index_stripchars_init) {
        getConfParam("indexStripChars", &o_index_stripchars);
        getConfParam("indexStoreDocText", &o_index_storedoctext);
        getConfParam("testmodifusemtime", &o_uptodate_test_use_mtime);
        getConfParam("stemexpandphrases", &o_expand_phrases);
        getConfParam("notermpositions", &o_no_term_positions);
        m_index_stripchars_init = 1;
    }

    if (getConfParam("cachedir", m->m_cachedir)) {
        m->m_cachedir = path_canon(path_tildexpand(m->m_cachedir));
    }

    return true;
}

bool RclConfig::ok() const
{
    return m->m_ok;
}

const std::string& RclConfig::getReason() const
{
    return m->m_reason;
}

std::string RclConfig::getConfDir() const
{
    return m->m_confdir;
}

const std::string& RclConfig::getDatadir() const
{
    return m->m_datadir;
}

std::string RclConfig::getKeyDir() const
{
    return m->m_keydir;
}

bool RclConfig::getConfParam(const std::string& name, std::string& value, bool shallow) const
{
    if (!m->m_conf->ok()) {
        return false;
    }
    return m->m_conf->get(name, value, m->m_keydir, shallow);
}

std::vector<std::string> RclConfig::getConfNames(const char *pattern) const
{
    return m->m_conf->getNames(m->m_keydir, pattern);
}

ConfSimple *RclConfig::getPTrans()
{
#if defined(_WIN32) || defined(NOCASE_PTRANS)
    m->m_lptrans_stale = true;
#endif
    return m->m_ptrans.get();
}

const std::set<std::string>& RclConfig::getStoredFields() const
{
    return m->m_storedFields;
}

const std::map<std::string, std::string>& RclConfig::getXattrToField() const
{
    return m->m_xattrtofld;
}

bool RclConfig::hasNameAnywhere(const std::string& nm) const
{
    return m->m_conf? m->m_conf->hasNameAnywhere(nm) : false;
}

ConfNull *RclConfig::cloneMainConfig()
{
    ConfNull *conf = new ConfStack<ConfTree>(ConfSimple::CFSF_KEYNOCASE, "recoll.conf", m->m_cdirs);
    if (!conf->ok()) {
        m->m_reason = string("Can't read config");
        return 0;
    }
    return conf;
}

// Remember what directory we're under and prefetch a few common values.
void RclConfig::setKeyDir(const string &dir) 
{
    if (!dir.compare(m->m_keydir))
        return;

    m->m_keydirgen++;
    m->m_keydir = dir;
    if (!m->m_conf->ok())
        return;

    if (!m->m_conf->get("defaultcharset", m->m_defcharset, m->m_keydir))
        m->m_defcharset.erase();
}

bool RclConfig::getConfParam(const string &name, int *ivp, bool shallow) const
{
    string value;
    if (!ivp || !getConfParam(name, value, shallow))
        return false;
    errno = 0;
    long lval = strtol(value.c_str(), nullptr, 0);
    if (lval == 0 && errno)
        return false;
    *ivp = int(lval);
    return true;
}

bool RclConfig::getConfParam(const string &name, double *dvp, bool shallow) const
{
    string value;
    if (!dvp || !getConfParam(name, value, shallow))
        return false;
    errno = 0;
    double dval = strtod(value.c_str(), nullptr);
    if (errno)
        return false;
    *dvp = int(dval);
    return true;
}

bool RclConfig::getConfParam(const string &name, bool *bvp, bool shallow) const
{
    string s;
    if (!bvp || !getConfParam(name, s, shallow))
        return false;
    *bvp = stringToBool(s);
    return true;
}

bool RclConfig::getConfParam(const string &name, vector<string> *svvp, bool shallow) const
{
    string s;
    if (!svvp ||!getConfParam(name, s, shallow))
        return false;
    svvp->clear();
    return stringToStrings(s, *svvp);
}

bool RclConfig::getConfParam(const string &name, unordered_set<string> *out, bool shallow) const
{
    vector<string> v;
    if (!out || !getConfParam(name, &v, shallow)) {
        return false;
    }
    out->clear();
    out->insert(v.begin(), v.end());
    return true;
}

bool RclConfig::getConfParam(const string &name, vector<int> *vip, bool shallow) const
{
    if (!vip) 
        return false;
    vip->clear();
    vector<string> vs;
    if (!getConfParam(name, &vs, shallow))
        return false;
    vip->reserve(vs.size());
    for (unsigned int i = 0; i < vs.size(); i++) {
        char *ep;
        vip->push_back(strtol(vs[i].c_str(), &ep, 0));
        if (ep == vs[i].c_str()) {
            LOGDEB("RclConfig::getConfParam: bad int value in [" << name << "]\n");
            return false;
        }
    }
    return true;
}

void RclConfig::initThrConf()
{
    // Default is no threading
    m->m_thrConf = {{-1, 0}, {-1, 0}, {-1, 0}};

    vector<int> vt;
    vector<int> vq;
    if (!getConfParam("thrQSizes", &vq)) {
        LOGINFO("RclConfig::initThrConf: no thread info (queues)\n");
        goto out;
    }

    // If the first queue size is 0, autoconf is requested.
    if (vq.size() > 0 && vq[0] == 0) {
        CpuConf cpus;
        if (!getCpuConf(cpus) || cpus.ncpus < 1) {
            LOGERR("RclConfig::initThrConf: could not retrieve cpu conf\n");
            cpus.ncpus = 1;
        }
        if (cpus.ncpus != 1) {
            LOGDEB("RclConfig::initThrConf: autoconf requested. " <<
                   cpus.ncpus << " concurrent threads available.\n");
        }

        // Arbitrarily set threads config based on number of CPUS. This also
        // depends on the IO setup actually, so we're bound to be wrong...
        if (cpus.ncpus == 1) {
            // Somewhat counter-intuitively (because of possible IO//)
            // it seems that the best config here is no threading
        } else if (cpus.ncpus < 4) {
            // Untested so let's guess...
            m->m_thrConf = {{2, 2}, {2, 2}, {2, 1}};
        } else if (cpus.ncpus < 6) {
            m->m_thrConf = {{2, 4}, {2, 2}, {2, 1}};
        } else {
            m->m_thrConf = {{2, 5}, {2, 3}, {2, 1}};
        }
        goto out;
    } else if (vq.size() > 0 && vq[0] < 0) {
        // threads disabled by config
        goto out;
    }

    if (!getConfParam("thrTCounts", &vt) ) {
        LOGINFO("RclConfig::initThrConf: no thread info (threads)\n");
        goto out;
    }

    if (vq.size() != 3 || vt.size() != 3) {
        LOGINFO("RclConfig::initThrConf: bad thread info vector sizes\n");
        goto out;
    }

    // Normal case: record info from config
    m->m_thrConf.clear();
    for (unsigned int i = 0; i < 3; i++) {
        m->m_thrConf.push_back({vq[i], vt[i]});
    }

out:
    ostringstream sconf;
    for (unsigned int i = 0; i < 3; i++) {
        sconf << "(" << m->m_thrConf[i].first << ", " << m->m_thrConf[i].second << ") ";
    }

    LOGDEB("RclConfig::initThrConf: chosen config (ql,nt): " << sconf.str() << "\n");
}

pair<int,int> RclConfig::getThrConf(ThrStage who) const
{
    if (m->m_thrConf.size() != 3) {
        LOGERR("RclConfig::getThrConf: bad data in rclconfig\n");
        return pair<int,int>(-1,-1);
    }
    return m->m_thrConf[who];
}

vector<string> RclConfig::getTopdirs(bool formonitor) const
{
    vector<string> tdl;
    if (formonitor) {
        if (!getConfParam("monitordirs", &tdl)) {
            getConfParam("topdirs", &tdl);
        }
    } else {
        getConfParam("topdirs", &tdl);
    }
    if (tdl.empty()) {
        LOGERR("RclConfig::getTopdirs: nothing to index:  topdirs/monitordirs "
               " are not set or have a bad list format\n");
        return tdl;
    }

    for (auto& dir : tdl) {
        dir = path_canon(path_tildexpand(dir));
    }
    return tdl;
}

const string& RclConfig::getLocaleCharset()
{
    return Internal::o_localecharset;
}

// Get charset to be used for transcoding to utf-8 if unspecified by doc
// For document contents:
//  If defcharset was set (from the config or a previous call, this
//   is done in setKeydir), use it.
//  Else, try to guess it from the locale
//  Use cp1252 (as a superset of iso8859-1) as ultimate default
//
// For filenames, same thing except that we do not use the config file value
// (only the locale).
const string& RclConfig::getDefCharset(bool filename) const
{
    if (filename) {
        return m->o_localecharset;
    } else {
        return m->m_defcharset.empty() ? m->o_localecharset : m->m_defcharset;
    }
}

// Get all known document mime values. We get them from the mimeconf
// 'index' submap.
// It's quite possible that there are other mime types in the index
// (defined in mimemap and not mimeconf, or output by "file -i"). We
// just ignore them, because there may be myriads, and their contents
// are not indexed. 
//
// This unfortunately means that searches by file names and mime type
// filtering don't work well together.
vector<string> RclConfig::getAllMimeTypes() const
{
    return m->mimeconf ? m->mimeconf->getNames("index") : vector<string>();
}

// Compute the difference of 1st to 2nd sets and return as plus/minus
// sets. Some args are std::set and some others stringToString()
// strings for convenience
void RclConfig::setPlusMinus(const string& sbase, const set<string>& upd,
                             string& splus, string& sminus)
{
    set<string> base;
    stringToStrings(sbase, base);

    vector<string> diff;
    auto it = set_difference(base.begin(), base.end(), upd.begin(), upd.end(),
                             std::inserter(diff, diff.begin()));
    sminus = stringsToString(diff);

    diff.clear();
    it = set_difference(upd.begin(), upd.end(), base.begin(), base.end(),
                        std::inserter(diff, diff.begin()));
    splus = stringsToString(diff);
}

/* Compute result of substracting strminus and adding strplus to base string.
   All string represent sets of values to be computed with stringToStrings() */
static void computeBasePlusMinus(set<string>& res, const string& strbase,
                                 const string& strplus, const string& strminus)
{
    set<string> plus, minus;
    res.clear();
    stringToStrings(strbase, res);
    stringToStrings(strplus, plus);
    stringToStrings(strminus, minus);
    for (auto& it : minus) {
        auto it1 = res.find(it);
        if (it1 != res.end()) {
            res.erase(it1);
        }
    }
    for (auto& it : plus) {
        res.insert(it);
    }
}

vector<string>& RclConfig::getStopSuffixes()
{
    bool needrecompute = m->m_stpsuffstate.needrecompute();
    needrecompute = m->m_oldstpsuffstate.needrecompute() || needrecompute;
    if (needrecompute || !m->m_stopsuffixes) {
        // Need to initialize the suffixes

        // Let the old customisation have priority: if recoll_noindex from
        // mimemap is set, it the user's (the default value is gone). Else
        // use the new variable
        if (!m->m_oldstpsuffstate.getvalue(0).empty()) {
            stringToStrings(m->m_oldstpsuffstate.getvalue(0), m->m_stopsuffvec);
        } else {
            std::set<string> ss;
            computeBasePlusMinus(ss, m->m_stpsuffstate.getvalue(0), 
                                 m->m_stpsuffstate.getvalue(1), 
                                 m->m_stpsuffstate.getvalue(2));
            m->m_stopsuffvec = vector<string>(ss.begin(), ss.end());
        }

        // Compute the special suffixes store
        m->m_stopsuffixes = std::make_unique<SuffixStore>();
        m->m_maxsufflen = 0;
        for (const auto& entry : m->m_stopsuffvec) {
            m->m_stopsuffixes->insert(SfString(stringtolower(entry)));
            if (m->m_maxsufflen < entry.length())
                m->m_maxsufflen = int(entry.length());
        }
    }
    LOGDEB1("RclConfig::getStopSuffixes: ->" << stringsToString(m->m_stopsuffvec) << "\n");
    return m->m_stopsuffvec;
}

bool RclConfig::inStopSuffixes(const string& fni)
{
    LOGDEB2("RclConfig::inStopSuffixes(" << fni << ")\n");

    // Call getStopSuffixes() to possibly update state, ignore result
    getStopSuffixes();

    // Only need a tail as long as the longest suffix.
    int pos = MAX(0, int(fni.length() - m->m_maxsufflen));
    string fn(fni, pos);

    stringtolower(fn);
    SuffixStore::const_iterator it = m->m_stopsuffixes->find(fn);
    if (it != m->m_stopsuffixes->end()) {
        LOGDEB2("RclConfig::inStopSuffixes: Found (" << fni << ") ["  <<
                ((*it).m->m_str) << "]\n");
        IdxDiags::theDiags().record(IdxDiags::NoContentSuffix, fni);
        return true;
    } else {
        LOGDEB2("RclConfig::inStopSuffixes: not found [" << fni << "]\n");
        return false;
    }
}

string RclConfig::getMimeTypeFromSuffix(const string& suff) const
{
    string mtype;
    m->mimemap->get(suff, mtype, m->m_keydir);
    return mtype;
}

string RclConfig::getSuffixFromMimeType(const string &mt) const
{
    // First try from standard data, ensuring that we can control the value
    // from the order in the configuration file.
    auto rclsuff = mime_suffixes.find(mt);
    if (rclsuff != mime_suffixes.end()) {
        return rclsuff->second;
    }
    // Try again from local data. The map is in the wrong direction,
    // have to walk it.
    vector<string> sfs = m->mimemap->getNames(cstr_null);
    for (const auto& suff : sfs) {
        string mt1;
        if (m->mimemap->get(suff, mt1, cstr_null) && !stringicmp(mt, mt1)) {
            return suff;
        }
    }
    return cstr_null;
}

/** Get list of file categories from mimeconf */
bool RclConfig::getMimeCategories(vector<string>& cats) const
{
    if (!m->mimeconf)
        return false;
    cats = m->mimeconf->getNames("categories");
    return true;
}

bool RclConfig::isMimeCategory(const string& cat) const
{
    vector<string>cats;
    getMimeCategories(cats);
    for (vector<string>::iterator it = cats.begin(); it != cats.end(); it++) {
        if (!stringicmp(*it,cat))
            return true;
    }
    return false;
}

/** Get list of mime types for category from mimeconf */
bool RclConfig::getMimeCatTypes(const string& cat, vector<string>& tps) const
{
    tps.clear();
    if (!m->mimeconf)
        return false;
    string slist;
    if (!m->mimeconf->get(cat, slist, "categories"))
        return false;

    stringToStrings(slist, tps);
    return true;
}

string RclConfig::getMimeHandlerDef(const string &mtype, bool filtertypes, const std::string& fn)
{
    string hs;

    if (filtertypes) {
        if(m->m_rmtstate.needrecompute()) {
            m->m_restrictMTypes.clear();
            stringToStrings(stringtolower((const string&)m->m_rmtstate.getvalue()),
                            m->m_restrictMTypes);
        }
        if (m->m_xmtstate.needrecompute()) {
            m->m_excludeMTypes.clear();
            stringToStrings(stringtolower((const string&)m->m_xmtstate.getvalue()),
                            m->m_excludeMTypes);
        }
        if (!m->m_restrictMTypes.empty() && !m->m_restrictMTypes.count(stringtolower(mtype))) {
            IdxDiags::theDiags().record(IdxDiags::NotIncludedMime, fn, mtype);
            LOGDEB1("RclConfig::getMimeHandlerDef: " << mtype << " not in mime type list\n");
            return hs;
        }
        if (!m->m_excludeMTypes.empty() && m->m_excludeMTypes.count(stringtolower(mtype))) {
            IdxDiags::theDiags().record(IdxDiags::ExcludedMime, fn, mtype);
            LOGDEB1("RclConfig::getMimeHandlerDef: " << mtype << " in excluded mime list (fn " <<
                    fn << ")\n");
            return hs;
        }
    }

    if (!m->mimeconf->get(mtype, hs, "index")) {
        if (mtype.find("text/") == 0) {
            bool alltext{false};
            getConfParam("textunknownasplain", &alltext);
            if (alltext && m->mimeconf->get("text/plain", hs, "index")) {
                return hs;
            }
        }
        if (mtype != "inode/directory") {
            IdxDiags::theDiags().record(IdxDiags::NoHandler, fn, mtype);
            LOGDEB1("getMimeHandlerDef: no handler for '" << mtype << "' (fn " << fn << ")\n");
        }
    }
    return hs;
}

const vector<MDReaper>& RclConfig::getMDReapers()
{
    string hs;
    if (m->m_mdrstate.needrecompute()) {
        m->m_mdreapers.clear();
        // New value now stored in m->m_mdrstate.getvalue(0)
        const string& sreapers = m->m_mdrstate.getvalue(0);
        if (sreapers.empty())
            return m->m_mdreapers;
        string value;
        ConfSimple attrs;
        valueSplitAttributes(sreapers, value, attrs);
        vector<string> nmlst = attrs.getNames(cstr_null);
        for (const auto& nm : nmlst) {
            MDReaper reaper;
            reaper.fieldname = fieldCanon(nm);
            string s;
            attrs.get(nm, s);
            stringToStrings(s, reaper.cmdv);
            m->m_mdreapers.push_back(reaper);
        }
    }
    return m->m_mdreapers;
}

bool RclConfig::getGuiFilterNames(vector<string>& cats) const
{
    if (!m->mimeconf)
        return false;
    cats = m->mimeconf->getNamesShallow("guifilters");
    return true;
}

bool RclConfig::getGuiFilter(const string& catfiltername, string& frag) const
{
    frag.clear();
    if (!m->mimeconf)
        return false;
    if (!m->mimeconf->get(catfiltername, frag, "guifilters"))
        return false;
    return true;
}

bool RclConfig::valueSplitAttributes(const string& whole, string& value, ConfSimple& attrs)
{
    if (whole.empty()) {
        value.clear();
        return false;
    }
    bool inquote{false};
    string::size_type semicol0;    
    for (semicol0 = 0; semicol0 < whole.size(); semicol0++) {
        if (whole[semicol0] == '"') {
            inquote = !inquote;
        } else if (whole[semicol0] == ';' && !inquote) {
            break;
        }
    }
    value = whole.substr(0, semicol0);
    trimstring(value);
    string attrstr;
    if (semicol0 != string::npos && semicol0 < whole.size() - 1) {
        attrstr = whole.substr(semicol0+1);
    }

    // Handle additional attributes. We substitute the semi-colons
    // with newlines and use a ConfSimple
    if (!attrstr.empty()) {
        for (string::size_type i = 0; i < attrstr.size(); i++) {
            if (attrstr[i] == ';')
                attrstr[i] = '\n';
        }
        attrs.reparse(attrstr);
    } else {
        attrs.clear();
    }
    
    return true;
}

bool RclConfig::getMissingHelperDesc(string& out) const
{
    string fmiss = path_cat(getConfDir(), "missing");
    out.clear();
    if (!file_to_string(fmiss, out))
        return false;
    return true;
}

void RclConfig::storeMissingHelperDesc(const string &s)
{
    string fmiss = path_cat(getCacheDir(), "missing");
    fstream fp;
    if (path_streamopen(fmiss, ios::trunc | ios::out, fp)) {
        fp << s;
    }
}

// Read definitions for field prefixes, aliases, and hierarchy and arrange 
// things for speed (theses are used a lot during indexing)
bool RclConfig::Internal::readFieldsConfig(const string& cnferrloc)
{
    LOGDEB2("RclConfig::readFieldsConfig\n");
    m_fields = std::make_unique<ConfStack<ConfSimple>>("fields", m_cdirs, true);
    if (!m_fields->ok()) {
        m_reason = string("No/bad fields file in: ") + cnferrloc;
        return false;
    }

    // Build a direct map avoiding all indirections for field to
    // prefix translation
    // Add direct prefixes from the [prefixes] section
    vector<string> tps = m_fields->getNames("prefixes");
    for (const auto& fieldname : tps) {
        string val;
        m_fields->get(fieldname, val, "prefixes");
        // An empty value can be used to undo the default prefix and avoid indexing the field
        if (val.empty()) {
            LOGDEB0("Empty fields [prefixes] setting for " << fieldname << "\n");
            continue;
        }
        ConfSimple attrs;
        FieldTraits ft;
        // fieldname = prefix ; attr1=val;attr2=val...
        if (!valueSplitAttributes(val, ft.pfx, attrs)) {
            LOGERR("readFieldsConfig: bad config line for ["  << fieldname <<
                   "]: [" << val << "]\n");
            return 0;
        }
        ft.wdfinc = (int)attrs.getInt("wdfinc", 1);
        ft.boost = attrs.getFloat("boost", 1.0);
        ft.pfxonly = attrs.getBool("pfxonly", false);
        ft.noterms = attrs.getBool("noterms", false);
        m_fldtotraits[stringtolower(fieldname)] = ft;
        LOGDEB2("readFieldsConfig: ["  << stringtolower(fieldname) << "] -> ["  << ft.pfx <<
                "] " << ft.wdfinc << " " << ft.boost << "\n");
    }

    // Values section
    tps = m_fields->getNames("values");
    for (const auto& fieldname : tps) {
        string canonic = stringtolower(fieldname); // canonic name
        string val;
        m_fields->get(fieldname, val, "values");
        ConfSimple attrs;
        string svslot;
        // fieldname = valueslot ; attr1=val;attr2=val...
        if (!valueSplitAttributes(val, svslot, attrs)) {
            LOGERR("readFieldsConfig: bad value line for ["  << fieldname <<
                   "]: [" << val << "]\n");
            return 0;
        }
        uint32_t valueslot = uint32_t(atoi(svslot.c_str()));
        if (valueslot == 0) {
            LOGERR("readFieldsConfig: found 0 value slot for [" << fieldname <<
                   "]: [" << val << "]\n");
            continue;
        }

        string tval;
        FieldTraits::ValueType valuetype{FieldTraits::STR};
        if (attrs.get("type", tval)) {
            if (tval == "string") {
                valuetype = FieldTraits::STR;
            } else if (tval == "int") {
                valuetype = FieldTraits::INT;
            } else {
                LOGERR("readFieldsConfig: bad type for value for " <<
                       fieldname << " : " << tval << "\n");
                return 0;
            }
        }
        int valuelen = (int)attrs.getInt("len", 0);
        // Find or insert traits entry
        const auto pit =
            m_fldtotraits.insert(pair<string, FieldTraits>(canonic, FieldTraits())).first;
        pit->second.valueslot = valueslot;
        pit->second.valuetype = valuetype;
        pit->second.valuelen = valuelen;
    }
    
    // Add prefixes for aliases and build alias-to-canonic map while
    // we're at it. Having the aliases in the prefix map avoids an
    // additional indirection at index time.
    tps = m_fields->getNames("aliases");
    for (const auto& fieldname : tps) {
        string canonic = stringtolower(fieldname); // canonic name
        FieldTraits ft;
        const auto pit = m_fldtotraits.find(canonic);
        if (pit != m_fldtotraits.end()) {
            ft = pit->second;
        }
        string aliases;
        m_fields->get(fieldname, aliases, "aliases");
        LOGDEB2("readFieldsConfig: " << canonic << " -> " << aliases << "\n");
        vector<string> l;
        stringToStrings(aliases, l);
        for (const auto& alias : l) {
            auto canonicalias = stringtolower(alias);
            if (pit != m_fldtotraits.end())
                m_fldtotraits[canonicalias] = ft;
            m_aliastocanon[canonicalias] = canonic;
        }
    }

    // Query aliases map
    tps = m_fields->getNames("queryaliases");
    for (const auto& entry: tps) {
        string canonic = stringtolower(entry); // canonic name
        string aliases;
        m_fields->get(entry, aliases, "queryaliases");
        vector<string> l;
        stringToStrings(aliases, l);
        for (const auto& alias : l) {
            m_aliastoqcanon[stringtolower(alias)] = canonic;
        }
    }

#if 0
    for (map<string, FieldTraits>::const_iterator it = m_fldtotraits.begin();
         it != m_fldtotraits.end(); it++) {
        LOGDEB("readFieldsConfig: ["  << entry << "] -> ["  << it->second.pfx <<
               "] " << it->second.wdfinc << " " << it->second.boost << "\n");
    }
#endif

    vector<string> sl = m_fields->getNames("stored");
    for (const auto& fieldname : sl) {
        m_storedFields.insert(m_parent->fieldCanon(stringtolower(fieldname)));
    }

    // Extended file attribute to field translations
    vector<string>xattrs = m_fields->getNames("xattrtofields");
    for (const auto& xattr : xattrs) {
        string val;
        m_fields->get(xattr, val, "xattrtofields");
        m_xattrtofld[xattr] = val;
    }

    return true;
}

// Return specifics for field name:
bool RclConfig::getFieldTraits(const string& _fld, const FieldTraits **ftpp, bool isquery) const
{
    string fld = isquery ? fieldQCanon(_fld) : fieldCanon(_fld);
    map<string, FieldTraits>::const_iterator pit = m->m_fldtotraits.find(fld);
    if (pit != m->m_fldtotraits.end()) {
        *ftpp = &pit->second;
        LOGDEB1("RclConfig::getFieldTraits: [" << _fld << "]->["  << pit->second.pfx << "]\n");
        return true;
    } else {
        LOGDEB1("RclConfig::getFieldTraits: no prefix for field [" << fld << "]\n");
        *ftpp = nullptr;
        return false;
    }
}

set<string> RclConfig::getIndexedFields() const
{
    set<string> flds;
    if (!m->m_fields->ok())
        return flds;

    vector<string> sl = m->m_fields->getNames("prefixes");
    flds.insert(sl.begin(), sl.end());
    return flds;
}

string RclConfig::fieldCanon(const string& f) const
{
    string fld = stringtolower(f);
    const auto it = m->m_aliastocanon.find(fld);
    if (it != m->m_aliastocanon.end()) {
        LOGDEB1("RclConfig::fieldCanon: [" << f << "] -> [" << it->second << "]\n");
        return it->second;
    }
    LOGDEB1("RclConfig::fieldCanon: [" << f << "] -> [" << fld << "]\n");
    return fld;
}

string RclConfig::fieldQCanon(const string& f) const
{
    const auto it = m->m_aliastoqcanon.find(stringtolower(f));
    if (it != m->m_aliastoqcanon.end()) {
        LOGDEB1("RclConfig::fieldQCanon: [" << f << "] -> ["  << it->second << "]\n");
        return it->second;
    }
    return fieldCanon(f);
}

vector<string> RclConfig::getFieldSectNames(const string &sk, const char* patrn) const
{
    if (!m->m_fields->ok())
        return vector<string>();
    return m->m_fields->getNames(sk, patrn);
}

bool RclConfig::getFieldConfParam(const string &name, const string &sk, 
                                  string &value) const
{
    if (!m->m_fields->ok())
        return false;
    return m->m_fields->get(name, value, sk);
}

set<string> RclConfig::getMimeViewerAllEx() const
{
    set<string> res;
    if (!m->mimeview->ok())
        return res;

    string base, plus, minus;
    m->mimeview->get("xallexcepts", base, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): base: " << base << "\n");
    m->mimeview->get("xallexcepts+", plus, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): plus: " << plus << "\n");
    m->mimeview->get("xallexcepts-", minus, "");
    LOGDEB1("RclConfig::getMimeViewerAllEx(): minus: " << minus << "\n");

    computeBasePlusMinus(res, base, plus, minus);
    LOGDEB1("RclConfig::getMimeViewerAllEx(): res: " << stringsToString(res) << "\n");
    return res;
}

bool RclConfig::setMimeViewerAllEx(const set<string>& allex)
{
    if (!m->mimeview->ok())
        return false;

    string sbase;
    m->mimeview->get("xallexcepts", sbase, "");

    string splus, sminus;
    setPlusMinus(sbase, allex, splus, sminus);

    if (!m->mimeview->set("xallexcepts-", sminus, "")) {
        m->m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }
    if (!m->mimeview->set("xallexcepts+", splus, "")) {
        m->m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }

    return true;
}

string RclConfig::getMimeViewerDef(const string &mtype, const string& apptag, bool useall) const
{
    LOGDEB2("RclConfig::getMimeViewerDef: mtype [" << mtype << "] apptag [" << apptag << "]\n");
    string hs;
    if (!m->mimeview->ok())
        return hs;

    if (useall) {
        // Check for exception
        set<string> allex = getMimeViewerAllEx();
        bool isexcept = false;
        for (auto& it : allex) {
            vector<string> mita;
            stringToTokens(it, mita, "|");
            if ((mita.size() == 1 && apptag.empty() && mita[0] == mtype) ||
                (mita.size() == 2 && mita[1] == apptag && mita[0] == mtype)) {
                // Exception to x-all
                isexcept = true;
                break;
            }
        }

        if (isexcept == false) {
            m->mimeview->get("application/x-all", hs, "view");
            return hs;
        }
        // Fallthrough to normal case.
    }

    if (apptag.empty() || !m->mimeview->get(mtype + string("|") + apptag, hs, "view"))
        m->mimeview->get(mtype, hs, "view");

    // Last try for text/xxx if alltext is set
    if (hs.empty() && mtype.find("text/") == 0 && mtype != "text/plain") {
        bool alltext{false};
        getConfParam("textunknownasplain", &alltext);
        if (alltext) {
            return getMimeViewerDef("text/plain", apptag, useall);
        }
    }
        
    return hs;
}

bool RclConfig::getMimeViewerDefs(vector<pair<string, string> >& defs) const
{
    if (!m->mimeview->ok())
        return false;
    vector<string>tps = m->mimeview->getNames("view");
    for (const auto& tp : tps) {
        defs.push_back(pair<string, string>(tp, getMimeViewerDef(tp, "", 0)));
    }
    return true;
}

bool RclConfig::setMimeViewerDef(const string& mt, const string& def)
{
    if (!m->mimeview->ok())
        return false;
    bool status;
    if (!def.empty()) 
        status = m->mimeview->set(mt, def, "view");
    else 
        status = m->mimeview->erase(mt, "view");

    if (!status) {
        m->m_reason = string("RclConfig:: cant set value. Readonly?");
        return false;
    }
    return true;
}

bool RclConfig::mimeViewerNeedsUncomp(const string &mimetype) const
{
    string s;
    vector<string> v;
    if (m->mimeview != 0 && m->mimeview->get("nouncompforviewmts", s, "") && 
        stringToStrings(s, v) && 
        find_if(v.begin(), v.end(), StringIcmpPred(mimetype)) != v.end()) 
        return false;
    return true;
}

string RclConfig::getMimeIconPath(const string &mtype, const string &apptag)
    const
{
    string iconname;
    if (!apptag.empty())
        m->mimeconf->get(mtype + string("|") + apptag, iconname, "icons");
    if (iconname.empty())
        m->mimeconf->get(mtype, iconname, "icons");
    if (iconname.empty())
        iconname = "document";

    string iconpath;
#if defined (__FreeBSD__) && __FreeBSD_version < 500000
    // gcc 2.95 dies if we call getConfParam here ??
    if (m->m_conf->ok())
        m->m_conf->get(string("iconsdir"), iconpath, m->m_keydir);
#else
    getConfParam("iconsdir", iconpath);
#endif

    if (iconpath.empty()) {
        iconpath = path_cat(m->m_datadir, "images");
    } else {
        iconpath = path_tildexpand(iconpath);
    }
    return path_cat(iconpath, iconname) + ".png";
}

// Return path defined by varname. May be absolute or relative to
// confdir, with default in confdir
string RclConfig::getConfdirPath(const char *varname, const char *dflt) const
{
    string result;
    if (!getConfParam(varname, result)) {
        result = path_cat(getConfDir(), dflt);
    } else {
        result = path_tildexpand(result);
        // If not an absolute path, compute relative to config dir
        if (!path_isabsolute(result)) {
            result = path_cat(getConfDir(), result);
        }
    }
    return path_canon(result);
}

string RclConfig::getCacheDir() const
{
    return m->m_cachedir.empty() ? getConfDir() : m->m_cachedir;
}

// Return path defined by varname. May be absolute or relative to
// confdir, with default in confdir
string RclConfig::getCachedirPath(const char *varname, const char *dflt) const
{
    string result;
    if (!getConfParam(varname, result)) {
        result = path_cat(getCacheDir(), dflt);
    } else {
        result = path_tildexpand(result);
        // If not an absolute path, compute relative to cache dir
        if (!path_isabsolute(result)) {
            result = path_cat(getCacheDir(), result);
        }
    }
    return path_canon(result);
}

// Windows: try to translate a possibly non-ASCII path into the shortpath alias. We first create the
// target, as this only works if it exists.
//
// This is mainly useful/necessary for paths based on the configuration directory, when the user has
// a non ASCII name, and for use with code which can't handle properly an Unicode path.
//
// For example, we execute an aspell command to create the spelling dictionary, and the parameter is
// a path to a dictionary file located inside the configuration directory.
// 
// This code is now mostly (only?) necessary for aspell as we use a patched Xapian, modified to
// properly handle Unicode paths, so that the shortpath getDbDir() is probably redundant.
//
// Note that it is not clear from the Windows doc that the shortpath call is going to work in all
// cases (file system types etc.). It returns the original path in case it fails, so it's important
// that the essential code (Xapian) can deal with Unicode paths.
static string maybeshortpath(const std::string& in)
{
#ifdef _WIN32
    path_makepath(in, 0700);
    return path_shortpath(in);
#else
    return in;
#endif
}

string RclConfig::getDbDir() const
{
    return getCachedirPath("dbdir", "xapiandb");
}

string RclConfig::getWebcacheDir() const
{
    return getCachedirPath("webcachedir", "webcache");
}
string RclConfig::getMboxcacheDir() const
{
    return getCachedirPath("mboxcachedir", "mboxcache");
}
string RclConfig::getAspellcacheDir() const
{
    return maybeshortpath(getCachedirPath("aspellDicDir", ""));
}

string RclConfig::getStopfile() const
{
    return getConfdirPath("stoplistfile", "stoplist.txt");
}

string RclConfig::getIdxSynGroupsFile() const
{
    return getConfdirPath("idxsynonyms", "thereisnodefaultidxsynonyms");
}

// The index status file is fast changing, so it's possible to put it outside
// of the config directory (for ssds, not sure this is really useful).
// To enable being quite xdg-correct we should add a getRundirPath()
string RclConfig::getIdxStatusFile() const
{
    return getCachedirPath("idxstatusfile", "idxstatus.txt");
}

const std::string& RclConfig::getOrigCwd() const
{
    return m->o_origcwd;
}

// The pid file is opened r/w every second by the GUI to check if the
// indexer is running. This is a problem for systems which spin down
// their disks. Locate it in XDG_RUNTIME_DIR if this is set.
// Thanks to user Madhu for this fix.
string RclConfig::getPidfile() const
{
    static string fn;
    if (fn.empty()) {
#ifndef _WIN32
        const char *p = getenv("XDG_RUNTIME_DIR");
        string rundir;
        if (nullptr == p) {
            // Problem is, we may have been launched outside the desktop, maybe by cron. Basing
            // everything on XDG_RUNTIME_DIR was a mistake, sometimes resulting in different pidfiles
            // being used by recollindex instances. So explicitely test for /run/user/$uid, still
            // leaving open the remote possibility that XDG_RUNTIME_DIR would be set to something
            // else...
            rundir = path_cat("/run/user", lltodecstr(getuid()));
            if (path_isdir(rundir)) {
                p = rundir.c_str();
            }
        }
        if (p) {
            string base = path_canon(p);
            string digest, hex;
            string cfdir = path_canon(getConfDir());
            path_catslash(cfdir);
            MD5String(cfdir, digest);
            MD5HexPrint(digest, hex);
            fn =  path_cat(base, "recoll-" + hex + "-index.pid");
        } else
#endif // ! _WIN32
        {
            fn = path_cat(getCacheDir(), "index.pid");
        }

        LOGINF("RclConfig: pid/lock file: " << fn << "\n");
    }
    return fn;
}


string RclConfig::getIdxStopFile() const
{
    return path_cat(getCacheDir(), "index.stop");
}

/* Eliminate the common leaf part of file paths p1 and p2. Example: 
 * /mnt1/common/part /mnt2/common/part -> /mnt1 /mnt2. This is used
 * for computing translations for paths when the dataset has been
 * moved. Of course this could be done more efficiently than by splitting 
 * into vectors, but we don't care.*/
static string path_diffstems(const string& p1, const string& p2,
                             string& r1, string& r2)
{
    string reason;
    r1.clear();
    r2.clear();
    vector<string> v1, v2;
    stringToTokens(p1, v1, "/");
    stringToTokens(p2, v2, "/");
    auto l1 = v1.size();
    auto l2 = v2.size();
        
    // Search for common leaf part
    unsigned int cl = 0;
    for (; cl < MIN(l1, l2); cl++) {
        if (v1[l1-cl-1] != v2[l2-cl-1]) {
            break;
        }
    }
    //cerr << "Common length = " << cl << "\n";
    if (cl == 0) {
        reason = "Input paths are empty or have no common part";
        return reason;
    }
    for (unsigned i = 0; i < l1 - cl; i++) {
        r1 += "/" + v1[i];
    }
    for (unsigned i = 0; i < l2 - cl; i++) {
        r2 += "/" + v2[i];
    }
        
    return reason;
}

void RclConfig::urlrewrite(const string& _dbdir, string& url) const
{
    LOGDEB1("RclConfig::urlrewrite: dbdir [" << _dbdir << "] url [" << url << "]\n");

#if defined(_WIN32) || defined(NOCASE_PTRANS)
    // Under Windows we want case-insensitive comparisons. Create an all-lowercase version
    // of m_ptrans (if not already done).
    if (m->m_lptrans_stale) {
        std::string str;
        m->m_ptrans->sortwalk(varprintlower, &str);
        m->m_lptrans = std::make_unique<ConfSimple>(ConfSimple::CFSF_FROMSTRING, str);
        m->m_lptrans_stale = false;
    }
    std::string dbdir{unactolower(_dbdir)};
    ConfSimple *ptrans = m->m_lptrans.get();
#else
    std::string dbdir{_dbdir};
    ConfSimple *ptrans = m->m_ptrans.get();
#endif

    // If orgidxconfdir is set, we assume that this index is for a
    // movable dataset, with the configuration directory stored inside
    // the dataset tree. This allows computing automatic path
    // translations if the dataset has been moved.
    string orig_confdir;
    string cur_confdir;
    string confstemorg, confstemrep;
    if (m->m_conf->get("orgidxconfdir", orig_confdir, "")) {
        if (!m->m_conf->get("curidxconfdir", cur_confdir, "")) {
            cur_confdir = m->m_confdir;
        }
        LOGDEB1("RclConfig::urlrewrite: orgidxconfdir: " << orig_confdir <<
                " cur_confdir " << cur_confdir << "\n");
        string reason = path_diffstems(orig_confdir, cur_confdir, confstemorg, confstemrep);
        if (!reason.empty()) {
            LOGERR("urlrewrite: path_diffstems failed: " << reason << " : orig_confdir [" <<
                   orig_confdir << "] cur_confdir [" << cur_confdir << "\n");
            confstemorg = confstemrep = "";
        }
    }
    
    // Do path translations exist for this index ?
    bool needptrans = true;
    if (!ptrans->ok() || !ptrans->hasSubKey(dbdir)) {
        LOGDEB2("RclConfig::urlrewrite: no paths translations for [" << _dbdir << "]\n");
        needptrans = false;
    }

    if (!needptrans && confstemorg.empty()) {
        return;
    }
    bool computeurl = false;
    
    string path = fileurltolocalpath(url);
    if (path.empty()) {
        LOGDEB2("RclConfig::urlrewrite: not file url\n");
        return;
    }
    
    // Do the movable volume thing.
    if (!confstemorg.empty() && confstemorg.size() <= path.size() &&
        !path.compare(0, confstemorg.size(), confstemorg)) {
        path = path.replace(0, confstemorg.size(), confstemrep);
        computeurl = true;
    }

    if (needptrans) {
#if defined(_WIN32) || defined(NOCASE_PTRANS)
        path = unactolower(path);
#endif // _WIN32
        
        // For each translation check if the prefix matches the input path,
        // replace and return the result if it does.
        vector<string> opaths = ptrans->getNames(dbdir);
        for (const auto& opath: opaths) {
            if (opath.size() <= path.size() && !path.compare(0, opath.size(), opath)) {
                string npath;
                // Key comes from getNames()=> call must succeed
                if (ptrans->get(opath, npath, dbdir)) { 
                    path = path_canon(path.replace(0, opath.size(), npath));
                    computeurl = true;
                }
                break;
            }
        }
    }
    if (computeurl) {
        url = path_pathtofileurl(path);
    }
}

bool RclConfig::sourceChanged() const
{
    if (m->m_conf->ok() && m->m_conf->sourceChanged())
        return true;
    if (m->mimemap->ok() && m->mimemap->sourceChanged())
        return true;
    if (m->mimeconf->ok() && m->mimeconf->sourceChanged())
        return true;
    if (m->mimeview->ok() && m->mimeview->sourceChanged())
        return true;
    if (m->m_fields->ok() && m->m_fields->sourceChanged())
        return true;
    if (m->m_ptrans->ok() && m->m_ptrans->sourceChanged())
        return true;
    return false;
}

string RclConfig::getWebQueueDir() const
{
    string webqueuedir;
    if (!getConfParam("webqueuedir", webqueuedir)) {
#ifdef _WIN32
        webqueuedir = "~/AppData/Local/RecollWebQueue";
#else
        webqueuedir = "~/.recollweb/ToIndex/";
#endif
    }
    webqueuedir = path_tildexpand(webqueuedir);
    return webqueuedir;
}

vector<string>& RclConfig::getSkippedNames()
{
    if (m->m_skpnstate.needrecompute()) {
        set<string> ss;
        computeBasePlusMinus(ss, m->m_skpnstate.getvalue(0),
                             m->m_skpnstate.getvalue(1), m->m_skpnstate.getvalue(2));
        m->m_skpnlist = vector<string>(ss.begin(), ss.end());
    }
    return m->m_skpnlist;
}

vector<string>& RclConfig::getOnlyNames()
{
    if (m->m_onlnstate.needrecompute()) {
        stringToStrings(m->m_onlnstate.getvalue(), m->m_onlnlist);
    }
    return m->m_onlnlist;
}

vector<string> RclConfig::getSkippedPaths() const
{
    vector<string> skpl;
    getConfParam("skippedPaths", &skpl);

    // Always add the dbdir and confdir to the skipped paths. This is 
    // especially important for the rt monitor which will go into a loop if we
    // don't do this.
    skpl.push_back(getDbDir());
    skpl.push_back(getConfDir());
#ifdef _WIN32
    skpl.push_back(TempFile::rcltmpdir());
#endif
    if (getCacheDir().compare(getConfDir())) {
        skpl.push_back(getCacheDir());
    }
    // And the web queue dir
    skpl.push_back(getWebQueueDir());
    for (vector<string>::iterator it = skpl.begin(); it != skpl.end(); it++) {
        *it = path_tildexpand(*it);
        *it = path_canon(*it);
    }
    sort(skpl.begin(), skpl.end());
    vector<string>::iterator uit = unique(skpl.begin(), skpl.end());
    skpl.resize(uit - skpl.begin());
    return skpl;
}

vector<string> RclConfig::getDaemSkippedPaths() const
{
    vector<string> dskpl;
    getConfParam("daemSkippedPaths", &dskpl);

    for (vector<string>::iterator it = dskpl.begin(); it != dskpl.end(); it++) {
        *it = path_tildexpand(*it);
        *it = path_canon(*it);
    }

    vector<string> skpl1 = getSkippedPaths();
    vector<string> skpl;
    if (dskpl.empty()) {
        skpl = skpl1;
    } else {
        sort(dskpl.begin(), dskpl.end());
        merge(dskpl.begin(), dskpl.end(), skpl1.begin(), skpl1.end(), 
              skpl.begin());
        vector<string>::iterator uit = unique(skpl.begin(), skpl.end());
        skpl.resize(uit - skpl.begin());
    }
    return skpl;
}


// Look up an executable filter.  We add $RECOLL_FILTERSDIR,
// and filtersdir from the config file to the PATH, then use execmd::which()
string RclConfig::findFilter(const string &icmd) const
{
    LOGDEB2("findFilter: " << icmd << "\n");
    // If the path is absolute, this is it
    if (path_isabsolute(icmd))
        return icmd;

    const char *cp = getenv("PATH");
    if (!cp) //??
        cp = "";
    string PATH(cp);

    // For historical reasons: check in personal config directory
    PATH = getConfDir() + path_PATHsep() + PATH;

    string temp;
    // Prepend $datadir/filters
    temp = path_cat(m->m_datadir, "filters");
    PATH = temp + path_PATHsep() + PATH;
#ifdef _WIN32
    // Windows only: use the bundled Python and our executable directory (to find rclstartw).
    temp = path_cat(m->m_datadir, "filters");
    temp = path_cat(temp, "python");
    PATH = temp + path_PATHsep() + PATH;
#endif
    PATH = path_thisexecdir() + path_PATHsep() + PATH;
    // Prepend possible configuration parameter?
    if (getConfParam(string("filtersdir"), temp)) {
        temp = path_tildexpand(temp);
        PATH = temp + path_PATHsep() + PATH;
    }

    // Prepend possible environment variable
    if ((cp = getenv("RECOLL_FILTERSDIR"))) {
        PATH = string(cp) + path_PATHsep() + PATH;
    } 

    string cmd;
    if (ExecCmd::which(icmd, cmd, PATH.c_str())) {
        return cmd;
    } else {
        // Let the shell try to find it...
        return icmd;
    }
}

bool RclConfig::processFilterCmd(std::vector<std::string>& cmd) const
{
    LOGDEB0("processFilterCmd: in: " << stringsToString(cmd) << "\n");
    auto it = cmd.begin();

#ifdef _WIN32
    // Special-case interpreters on windows: we used to have an additional 1st argument "python" in
    // mimeconf, but we now rely on the .py extension for better sharing of mimeconf.
    std::string ext = path_suffix(*it);
    if ("py" == ext) {
        it = cmd.insert(it, findFilter("python"));
        it++;
    } else if ("pl" == ext) {
        it = cmd.insert(it, findFilter("perl"));
        it++;
    }
#endif
    
    *it = findFilter(*it);

    LOGDEB0("processFilterCmd: out: " << stringsToString(cmd) << "\n");
    return true;
}

/** 
 * Return decompression command line for given mime type
 */
bool RclConfig::getUncompressor(const string &mtype, vector<string>& cmd) const
{
    string hs;

    m->mimeconf->get(mtype, hs, cstr_null);
    if (hs.empty())
        return false;
    vector<string> tokens;
    stringToStrings(hs, tokens);
    if (tokens.empty()) {
        LOGERR("getUncompressor: empty spec for mtype " << mtype << "\n");
        return false;
    }
    auto it = tokens.begin();
    if (tokens.size() < 2)
        return false;
    if (stringlowercmp("uncompress", *it++)) 
        return false;
    cmd.clear();
    cmd.insert(cmd.end(), it, tokens.end());
    return processFilterCmd(cmd);
}
