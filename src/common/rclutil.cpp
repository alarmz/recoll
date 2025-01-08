/* Copyright (C) 2016-2019 J.F.Dockes
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
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "safeunistd.h"
#include "cstr.h"
#include "execmd.h"
#include "fstreewalk.h"

#ifdef _WIN32

#ifndef NOMINMAX
# define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <Shlobj.h>

#else // _WIN32

#include <sys/param.h>
#include <pwd.h>
#include <sys/file.h>

#endif // !_WIN32

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <errno.h>
#include <sys/types.h>

#include <mutex>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <numeric>

#include "rclutil.h"
#include "pathut.h"
#include "wipedir.h"
#include "transcode.h"
#include "md5ut.h"
#include "log.h"
#include "smallut.h"
#include "rclconfig.h"
#include "damlev.h"
#include "utf8iter.h"

using namespace std;

template <class T> void map_ss_cp_noshr(T s, T *d)
{
    for (const auto& ent : s) {
        d->insert(
            pair<string, string>(string(ent.first.begin(), ent.first.end()),
                                 string(ent.second.begin(), ent.second.end())));
    }
}
template void map_ss_cp_noshr<map<string, string> >(
    map<string, string> s, map<string, string>*d);
template void map_ss_cp_noshr<unordered_map<string, string> >(
    unordered_map<string,string> s, unordered_map<string,string>*d);

// Add data to metadata field, store multiple values as (pseudo) CSV, avoid multiple identical
// instances. This ends up with commas at both ends to make duplicate search simpler
static void maybecommas(std::string& out, const std::string &val)
{
    if (val[0] != ',' && (out.empty() || out[out.size()-1] != ','))
        out += ',';
    out += val;
    if (out[out.size()-1] != ',')
        out += ',';
}
template <class T> void addmeta(T& store, const string& nm, const string& value)
{
    static const std::string cstr_comma{','};
    LOGDEB2("addmeta: [" << nm << "] value [" << value << "]\n");
    if (value.empty())
        return;
    
    auto it = store.find(nm);
    if (it == store.end()) {
        bool _;
        std::tie(it, _) = store.insert({nm, std::string()});
    }
    std::string& pval = it->second;
    if (pval.empty()) {
        maybecommas(pval, value);
    } else {
        std::string nval;
        maybecommas(nval, value);
        if (pval.find(nval) == string::npos) {
            // No end comma should not happen, but make sure
            if (pval[pval.size()-1] != ',') {
                pval += nval;
            } else {
                pval += nval.substr(1);
            }
        }
    }
}
template <class T> void trimmeta(T& store)
{
    for (auto& [_,value] : store) {
        if (value.empty())
            continue;
        if (value.back() == ',')
            value.pop_back();
        if (!value.empty() && value[0] == ',')
            value.erase(0, 1);
    }
}
template void addmeta<map<string, string>>(map<string, string>&, const string&, const string&);
template void addmeta<unordered_map<string, string>>(
    unordered_map<string, string>&, const string&, const string&);
template void trimmeta<map<string, string>>(map<string, string>&);
template void trimmeta<unordered_map<string, string>>(unordered_map<string, string>&);

#ifdef _WIN32
// On Windows, we use a subdirectory named "rcltmp" inside the windows
// temp location to create the temporary files in.
static const string& path_wingetrcltmpdir()
{
    // Constant: only need to compute once
    static string tdir;
    if (tdir.empty()) {
        wchar_t dbuf[MAX_PATH + 1];
        GetTempPathW(MAX_PATH, dbuf);
        if (!wchartoutf8(dbuf, tdir)) {
            LOGERR("path_wingetrcltmpdir: wchartoutf8 failed. Using c:/Temp\n");
            tdir = "C:/Temp";
        }
        LOGDEB1("path_wingetrcltmpdir(): gettemppathw ret: " << tdir << "\n");
        tdir = path_cat(tdir, "rcltmp");
        if (!path_exists(tdir)) {
            if (!path_makepath(tdir, 0700)) {
                LOGSYSERR("path_wingettempfilename", "path_makepath", tdir);
            }
        }
    }
    return tdir;
}

static bool path_gettempfilename(string& filename, string&)
{
    string tdir = tmplocation();
    LOGDEB0("path_gettempfilename: tdir: [" << tdir << "]\n");
    wchar_t dbuf[MAX_PATH + 1];
    utf8towchar(tdir, dbuf, MAX_PATH);

    wchar_t buf[MAX_PATH + 1];
    static wchar_t prefix[]{L"rcl"};
    GetTempFileNameW(dbuf, prefix, 0, buf);
    wchartoutf8(buf, filename);

    // Windows will have created a temp file, we delete it.
    if (!DeleteFileW(buf)) {
        LOGSYSERR("path_wingettempfilename", "DeleteFileW", filename);
    } else {
        LOGDEB1("path_wingettempfilename: DeleteFile " << filename << " Ok\n");
    }
    path_slashize(filename);
    LOGDEB1("path_gettempfilename: filename: [" << filename << "]\n");
    return true;
}

// Convert X:/path to /X/path for path splitting inside the index
string path_slashdrive(const string& path)
{
    string npath;
    if (path_hasdrive(path)) {
        npath.append(1, '/');
        npath.append(1, path[0]);
        if (path_isdriveabs(path)) {
            npath.append(path.substr(2));
        } else {
            // This should be an error really
            npath.append(1, '/');
            npath.append(path.substr(2));
        }
    } else {
        npath = path; ///??
    }
    return npath;
}

#else // _WIN32 above

static bool path_gettempfilename(string& filename, string& reason)
{
    filename = path_cat(tmplocation(), "rcltmpfXXXXXX");
    char *cp = strdup(filename.c_str());
    if (!cp) {
        reason = "Out of memory (for file name !)\n";
        return false;
    }

    // Using mkstemp this way is awful (bot the suffix adding and
    // using mkstemp() instead of mktemp just to avoid the warnings)
    int fd;
    if ((fd = mkstemp(cp)) < 0) {
        free(cp);
        reason = "TempFileInternal: mkstemp failed\n";
        return false;
    }
    close(fd);
    path_unlink(cp);
    filename = cp;
    free(cp);
    return true;
}
#endif // posix

bool path_samepath(const std::string& p1, const std::string& p2)
{
#ifdef _WIN32
    auto wp1 = utf8towchar(p1);
    auto wp2 = utf8towchar(p2);
    return _wcsicmp(wp1.get(), wp2.get()) == 0;
#else
    return p1 == p2;
#endif
}

// The default place to store the default config and other stuff (e.g webqueue)
string path_homedata()
{
#ifdef _WIN32
    wchar_t *cp;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &cp);
    string dir;
    if (cp != 0) {
        wchartoutf8(cp, dir);
    }
    if (!dir.empty()) {
        dir = path_canon(dir);
    } else {
        dir = path_cat(path_home(), "AppData/Local/");
    }
    return dir;
#else
    // We should use an xdg-conforming location, but, history...
    return path_home();
#endif
}

// Check if path is either non-existing or an empty directory.
bool path_empty(const string& path)
{
    if (path_isdir(path)) {
        string reason;
        std::set<string> entries;
        if (!listdir(path, reason, entries) || entries.empty()) {
            return true;
        }
        return false;
    } else {
        return !path_exists(path);
    }
}

string path_defaultrecollconfsubdir()
{
#ifdef _WIN32
    return "Recoll";
#else
    return ".recoll";
#endif
}

// Location for sample config, filters, etc. E.g. /usr/share/recoll/ on linux
// or c:/program files (x86)/recoll/share on Windows
const string& path_pkgdatadir()
{
    static string datadir;
    if (!datadir.empty()) {
        return datadir;
    }

    // All platforms: use environment variable if set
    const char *cdatadir = getenv("RECOLL_DATADIR");
    if (nullptr != cdatadir) {
        datadir = cdatadir;
        return datadir;
    }
    
#if defined(_WIN32)
    // Try a path relative with the exec. This works if we are
    // recoll/recollindex etc.
    // But maybe we are the python module, and execpath is the python
    // exe which could be anywhere. Try the default installation
    // directory, else tell the user to set the environment
    // variable.
    vector<string> paths{path_thisexecdir(), "c:/program files (x86)/recoll",
        "c:/program files/recoll"};
    for (const auto& path : paths) {
        datadir = path_cat(path, "Share");
        if (path_exists(path_cat(datadir, {"examples", "recoll.conf"}))) {
            return datadir;
        }
    }
    // Not found
    std::cerr << "Could not find the recoll installation data. It is usually "
        "a subfolder of the installation directory. \n"
        "Please set the RECOLL_DATADIR environment variable to point to it\n"
        "(e.g. setx RECOLL_DATADIR \"C:/Program Files (X86)/Recoll/Share)\"\n";
#elif defined(__APPLE__) && defined(RECOLL_AS_MAC_BUNDLE)
    // The package manager builds (Macports, Homebrew, Nixpkgs ...) all arrange to set a proper
    // compiled value for RECOLL_DATADIR. We can't do this when building a native bundle with
    // QCreator, in which case we use the executable location.
    datadir = path_cat(path_getfather(path_thisexecdir()), "Resources");
#else
    // If not in environment, try to use the compiled-in constant.
    datadir = RECOLL_DATADIR;
    if (!path_isdir(datadir)) {
        auto top = path_getfather(path_thisexecdir());
        vector<string> paths{"share/recoll", "usr/share/recoll"};
        for (const auto& path : paths) {
            datadir = path_cat(top, path);
            if (path_exists(datadir)) {
                return datadir;
            }
        }
    }
#endif
    return datadir;
}

/* 
  Encode the path part of a file:// url so that we can use it to compute (MD5) a thumbnail path
  according to the freedesktop.org thumbnail spec, which itself does not define what should be
  escaped. We choose to exactly escape what gio does, as implemented in
  glib/gconvert.c:g_escape_uri_string(uri, UNSAFE_PATH).  Hopefully, the other desktops have the
  same set of escaped chars.  Note that $ is not encoded, so the value is not shell-safe.
 */
string path_pcencode(const string& url, string::size_type offs)
{
    string out = url.substr(0, offs);
    const char *cp = url.c_str();
    for (string::size_type i = offs; i < url.size(); i++) {
        unsigned int c;
        const char *h = "0123456789ABCDEF";
        c = cp[i];
        if (c <= 0x20 ||
            c >= 0x7f ||
            c == '"' ||
            c == '#' ||
            c == '%' ||
            c == ';' ||
            c == '<' ||
            c == '>' ||
            c == '?' ||
            c == '[' ||
            c == '\\' ||
            c == ']' ||
            c == '^' ||
            c == '`' ||
            c == '{' ||
            c == '|' ||
            c == '}') {
            out += '%';
            out += h[(c >> 4) & 0xf];
            out += h[c & 0xf];
        } else {
            out += char(c);
        }
    }
    return out;
}

string url_gpath(const string& url)
{
    // Remove the access schema part (or whatever it's called)
    string::size_type colon = url.find_first_of(":");
    if (colon == string::npos || colon == url.size() - 1) {
        return url;
    }
    // If there are non-alphanum chars before the ':', then there
    // probably is no scheme. Whatever...
    for (string::size_type i = 0; i < colon; i++) {
        if (!isalnum(url.at(i))) {
            return url;
        }
    }

    // In addition we canonize the path to remove empty host parts (for compatibility with older
    // versions of recoll where file:// was hardcoded, but the local path was used for doc
    // identification.
    return path_canon(url.substr(colon + 1));
}

string url_parentfolder(const string& url)
{
    // In general, the parent is the directory above the full path
    string parenturl = path_getfather(url_gpath(url));
    // But if this is http, make sure to keep the host part. Recoll
    // only has file or http urls for now.
    bool isfileurl = urlisfileurl(url);
    if (!isfileurl && parenturl == "/") {
        parenturl = url_gpath(url);
    }
    return isfileurl ? cstr_fileu + parenturl : string("http://") + parenturl;
}


// Convert to file path if url is like file:
// Note: this only works with our internal pseudo-urls which are not
// encoded/escaped
string fileurltolocalpath(string url)
{
    if (url.find(cstr_fileu) == 0) {
        url = url.substr(7, string::npos);
    } else {
        return string();
    }

    // If this looks like a Windows path: absolute file urls are like: file:///c:/mydir/...
    // Get rid of the initial '/'
    if (url.size() >= 3 && url[0] == '/' && isalpha(url[1]) && url[2] == ':') {
        url = url.substr(1);
    }

    // Removing the fragment part. This is exclusively used when
    // executing a viewer for the recoll manual, and we only strip the
    // part after # if it is preceded by .html
    string::size_type pos;
    if ((pos = url.rfind(".html#")) != string::npos) {
        url.erase(pos + 5);
    } else if ((pos = url.rfind(".htm#")) != string::npos) {
        url.erase(pos + 4);
    }

    return url;
}

string path_pathtofileurl(const string& path)
{
    // We're supposed to receive a canonic absolute path, but on windows we
    // may need to add a '/' in front of the drive spec
    string url(cstr_fileu);
    if (path.empty() || path[0] != '/') {
        url.push_back('/');
    }
    url += path;
    return url;
}

bool urlisfileurl(const string& url)
{
    return url.find(cstr_fileu) == 0;
}

// Printable url: this is used to transcode from the system charset
// into either utf-8 if transcoding succeeds, or url-encoded
bool printableUrl(const string& fcharset, const string& in, string& out)
{
#ifdef _WIN32
    PRETEND_USE(fcharset);
    // On windows our paths are always utf-8
    out = in;
#else
    int ecnt = 0;
    if (!transcode(in, out, fcharset, cstr_utf8, &ecnt) || ecnt) {
        out = path_pcencode(in, 7);
    }
#endif
    return true;
}

string url_gpathS(const string& url)
{
#ifdef _WIN32
    return path_slashdrive(url_gpath(url));
#else
    return url_gpath(url);
#endif
}

std::string compute_utf8fn(const RclConfig *config, const std::string& ifn, bool simple)
{
#ifdef _WIN32
    PRETEND_USE(config);
    // On windows file names are read as UTF16 wchar_t and converted to UTF-8
    // while scanning directories
    return simple ? path_getsimple(ifn) : ifn;
#else
    std::string lfn(simple ? path_getsimple(ifn) : ifn);
    std::string charset = config->getDefCharset(true);
    std::string utf8fn; 
    int ercnt;
    if (!transcode(lfn, utf8fn, charset, cstr_utf8, &ercnt)) {
        LOGERR("compute_utf8fn: fn transcode failure from ["  << charset <<
               "] to UTF-8 for: [" << lfn << "]\n");
    } else if (ercnt) {
        LOGDEB("compute_utf8fn: "  << ercnt << " transcode errors from [" <<
               charset << "] to UTF-8 for: ["  << lfn << "]\n");
    }
    LOGDEB1("compute_utf8fn: transcoded from ["  << lfn << "] to ["  <<
            utf8fn << "] ("  << charset << "->"  << "UTF-8)\n");
    return utf8fn;
#endif
}

std::string utf8datestring(const std::string& format, struct tm *tm)
{
    string u8date;
#ifdef _WIN32
    wchar_t wformat[200];
    utf8towchar(format, wformat, 199);
    wchar_t wdate[250];
    wcsftime(wdate, 250, wformat, tm);
    wchartoutf8(wdate, u8date);
#else
    char datebuf[200];
    strftime(datebuf, 199, format.c_str(), tm);
    transcode(datebuf, u8date, RclConfig::getLocaleCharset(), cstr_utf8);
#endif
    return u8date;
}

const string& tmplocation()
{
    static string stmpdir;
    if (stmpdir.empty()) {
        const char *tmpdir = getenv("RECOLL_TMPDIR");

#ifndef _WIN32
        /* Don't use these under windows because they will return
         * non-ascii non-unicode stuff (would have to call _wgetenv()
         * instead. path_wingetrcltmpdir() will manage */
        if (tmpdir == nullptr) {
            tmpdir = getenv("TMPDIR");
        }
        if (tmpdir == nullptr) {
            tmpdir = getenv("TMP");
        }
        if (tmpdir == nullptr) {
            tmpdir = getenv("TEMP");
        }
#endif

        if (tmpdir == nullptr) {
#ifdef _WIN32
            stmpdir = path_wingetrcltmpdir();
#else
            stmpdir = "/tmp";
#endif
        } else {
            stmpdir = tmpdir;
        }
        stmpdir = path_canon(stmpdir);
    }

    return stmpdir;
}

bool maketmpdir(string& tdir, string& reason)
{
#ifndef _WIN32
    tdir = path_cat(tmplocation(), "rcltmpXXXXXX");

    char *cp = strdup(tdir.c_str());
    if (!cp) {
        reason = "maketmpdir: out of memory (for file name !)\n";
        tdir.erase();
        return false;
    }

    // There is a race condition between name computation and
    // mkdir. try to make sure that we at least don't shoot ourselves
    // in the foot
#if !defined(HAVE_MKDTEMP)
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);
#endif

    if (!
#ifdef HAVE_MKDTEMP
        mkdtemp(cp)
#else
        mktemp(cp)
#endif // HAVE_MKDTEMP
        ) {
        free(cp);
        reason = "maketmpdir: mktemp failed for [" + tdir + "] : " +
            strerror(errno);
        tdir.erase();
        return false;
    }
    tdir = cp;
    free(cp);
#else // _WIN32
    // There is a race condition between name computation and
    // mkdir. try to make sure that we at least don't shoot ourselves
    // in the foot
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);
    if (!path_gettempfilename(tdir, reason)) {
        return false;
    }
#endif

    // At this point the directory does not exist yet except if we used
    // mkdtemp

#if !defined(HAVE_MKDTEMP) || defined(_WIN32)
    if (!path_makepath(tdir, 0700)) {
        reason = string("maketmpdir: mkdir ") + tdir + " failed";
        tdir.erase();
        return false;
    }
#endif

    return true;
}


class TempFile::Internal {
public:
    Internal(const std::string& suffix);
    ~Internal();
    friend class TempFile;
private:
    std::string m_filename;
    std::string m_reason;
    bool m_noremove{false};
};

TempFile::TempFile(const string& suffix)
    : m(new Internal(suffix))
{
}

TempFile::TempFile()
{
    m = std::shared_ptr<Internal>();
}

const char *TempFile::filename() const
{
    return m ? m->m_filename.c_str() : "";
}

const std::string& TempFile::getreason() const
{
    static string fatal{"fatal error"};
    return m ? m->m_reason : fatal;
}

void TempFile::setnoremove(bool onoff)
{
    if (m)
        m->m_noremove = onoff;
}

bool TempFile::ok() const
{
    return m ? !m->m_filename.empty() : false;
}

TempFile::Internal::Internal(const string& suffix)
{
    // Because we need a specific suffix, can't use mkstemp
    // well. There is a race condition between name computation and
    // file creation. try to make sure that we at least don't shoot
    // our own selves in the foot. maybe we'll use mkstemps one day.
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);

    if (!path_gettempfilename(m_filename, m_reason)) {
        return;
    }
    m_filename += suffix;
    std::fstream fout;
    if (!path_streamopen(m_filename, ios::out|ios::trunc, fout)) {
        m_reason = string("Open/create error. errno : ") +
            lltodecstr(errno) + " file name: " + m_filename;
        LOGSYSERR("Tempfile::Internal::Internal", "open/create", m_filename);
        m_filename.erase();
    }
}

const std::string& TempFile::rcltmpdir()
{
    return tmplocation();
}

#ifdef _WIN32
static list<string> remainingTempFileNames;
static std::mutex remTmpFNMutex;
#endif

TempFile::Internal::~Internal()
{
    if (!m_filename.empty() && !m_noremove) {
        LOGDEB1("TempFile:~: unlinking " << m_filename << endl);
        if (!path_unlink(m_filename)) {
            LOGSYSERR("TempFile:~", "unlink", m_filename);
#ifdef _WIN32
            {
                std::unique_lock<std::mutex> lock(remTmpFNMutex);
                remainingTempFileNames.push_back(m_filename);
            }
#endif
        } else {
            LOGDEB1("TempFile:~: unlink " << m_filename << " Ok\n");
        }
    }
}

// On Windows we sometimes fail to remove temporary files because
// they are open. It's difficult to make sure this does not
// happen, so we add a cleaning pass after clearing the input
// handlers cache (which should kill subprocesses etc.)
void TempFile::tryRemoveAgain()
{
#ifdef _WIN32
    LOGDEB1("TempFile::tryRemoveAgain. List size: " <<
            remainingTempFileNames.size() << endl);
    std::unique_lock<std::mutex> lock(remTmpFNMutex);
    std::list<string>::iterator pos = remainingTempFileNames.begin();
    while (pos != remainingTempFileNames.end()) {
        if (!path_unlink(*pos)) {
            LOGSYSERR("TempFile::tryRemoveAgain", "unlink", *pos);
            pos++;
        } else {
            pos = remainingTempFileNames.erase(pos);
        }
    }
#endif
}

TempDir::TempDir()
{
    if (!maketmpdir(m_dirname, m_reason)) {
        m_dirname.erase();
        return;
    }
    LOGDEB("TempDir::TempDir: -> " << m_dirname << endl);
}

TempDir::~TempDir()
{
    if (!m_dirname.empty()) {
        LOGDEB("TempDir::~TempDir: erasing " << m_dirname << endl);
        (void)wipedir(m_dirname, true, true);
        m_dirname.erase();
    }
}

bool TempDir::wipe()
{
    if (m_dirname.empty()) {
        m_reason = "TempDir::wipe: no directory !\n";
        return false;
    }
    if (wipedir(m_dirname, false, true)) {
        m_reason = "TempDir::wipe: wipedir failed\n";
        return false;
    }
    return true;
}

// Freedesktop standard paths for cache directory (thumbnails are now in there)
static const string& xdgcachedir()
{
    static string xdgcache;
    if (xdgcache.empty()) {
        const char *cp = getenv("XDG_CACHE_HOME");
        if (nullptr == cp) {
            xdgcache = path_cat(path_home(), ".cache");
        } else {
            xdgcache = string(cp);
        }
    }
    return xdgcache;
}

static const string& thumbnailsdir()
{
    static string thumbnailsd;
    if (thumbnailsd.empty()) {
        thumbnailsd = path_cat(xdgcachedir(), "thumbnails");
        if (access(thumbnailsd.c_str(), 0) != 0) {
            thumbnailsd = path_cat(path_home(), ".thumbnails");
        }
    }
    return thumbnailsd;
}

// Place for 1024x1024 files
static const string thmbdirxxlarge = "xx-large";
// Place for 512x512 files
static const string thmbdirxlarge = "x-large";
// Place for 256x256 files
static const string thmbdirlarge = "large";
// 128x128
static const string thmbdirnormal = "normal";

static const vector<string> thmbdirs{thmbdirxxlarge, thmbdirxlarge, thmbdirlarge, thmbdirnormal};

static void thumbname(const string& url, string& name)
{
    string digest;
    string l_url = path_pcencode(url);
    MD5String(l_url, digest);
    MD5HexPrint(digest, name);
    name += ".png";
}

bool thumbPathForUrl(const string& url, int size, string& path)
{
    string name, path128, path256, path512, path1024;
    thumbname(url, name);
    if (size <= 128) {
        path = path_cat(thumbnailsdir(), thmbdirnormal);
        path = path_cat(path, name);
        path128 = path;
    } else if (size <= 256) {
        path = path_cat(thumbnailsdir(), thmbdirlarge);
        path = path_cat(path, name);
        path256 = path;
    } else if (size <= 512) {
        path = path_cat(thumbnailsdir(), thmbdirxlarge);
        path = path_cat(path, name);
        path512 = path;
    } else {
        path = path_cat(thumbnailsdir(), thmbdirxxlarge);
        path = path_cat(path, name);
        path1024 = path;
    }
    if (access(path.c_str(), R_OK) == 0) {
        return true;
    }

    // Not found in requested size. Try to find any size and return it. Let the client scale.
    for (const auto& tdir : thmbdirs) {
        path = path_cat(thumbnailsdir(), tdir);
        path = path_cat(path, name);
        if (access(path.c_str(), R_OK) == 0) {
            return true;
        }
    }

    // File does not exist. Return appropriate path anyway.
    if (size <= 128) {
        path = path128;
    } else if (size <= 256) {
        path = path256;
    } else if (size <= 512) {
        path = path512;
    } else {
        path = path1024;
    }
    return false;
}

// Compare charset names, removing the more common spelling variations
bool samecharset(const string& cs1, const string& cs2)
{
    auto mcs1 = std::accumulate(cs1.begin(), cs1.end(), "", [](const char* m, char i) { return (i != '_' && i != '-') ? m + ::tolower(i) : m; });
    auto mcs2 = std::accumulate(cs2.begin(), cs2.end(), "", [](const char* m, char i) { return (i != '_' && i != '-') ? m + ::tolower(i) : m; });
    return mcs1 == mcs2;
}

static const std::unordered_map<string, string> lang_to_code {
    {"be", "cp1251"},
    {"bg", "cp1251"},
    {"cs", "iso-8859-2"},
    {"el", "iso-8859-7"},
    {"he", "iso-8859-8"},
    {"hr", "iso-8859-2"},
    {"hu", "iso-8859-2"},
    {"ja", "eucjp"},
    {"kk", "pt154"},
    {"ko", "euckr"},
    {"lt", "iso-8859-13"},
    {"lv", "iso-8859-13"},
    {"pl", "iso-8859-2"},
    {"rs", "iso-8859-2"},
    {"ro", "iso-8859-2"},
    {"ru", "koi8-r"},
    {"sk", "iso-8859-2"},
    {"sl", "iso-8859-2"},
    {"sr", "iso-8859-2"},
    {"th", "iso-8859-11"},
    {"tr", "iso-8859-9"},
    {"uk", "koi8-u"},
        };

string langtocode(const string& lang)
{
    const auto it = lang_to_code.find(lang);

    // Use cp1252 by default...
    if (it == lang_to_code.end()) {
        return cstr_cp1252;
    }

    return it->second;
}

string localelang()
{
    const char *lang = getenv("LANG");

    if (lang == nullptr || *lang == 0 || !strcmp(lang, "C") ||
        !strcmp(lang, "POSIX")) {
        return "en";
    }
    string locale(lang);
    string::size_type under = locale.find_first_of('_');
    if (under == string::npos) {
        return locale;
    }
    return locale.substr(0, under);
}

class IntString {
public:
    IntString(const std::string& utf8) {
        m_len = utf8len(utf8);
        m_vec = (int*)malloc(m_len * sizeof(int));
        Utf8Iter it(utf8);
        size_t i = 0;
        for (; !it.eof(); it++) {
            if (it.error()) {
                LOGERR("IntString: Illegal seq at byte position " << it.getBpos()  <<"\n");
                goto error;
            }
            unsigned int value = *it;
            if (value == (unsigned int)-1) {
                LOGERR("IntString: Conversion error\n");
                goto error;
            }
            if (i >= m_len) {
                LOGFAT("IntString:: OVERFLOW!?!\n");
                abort();
            }
            m_vec[i++] = value;
        }
        return;
    error:
        if (m_vec) {
            free(m_vec);
            m_vec = nullptr;
        }
        m_len = 0;
    }
    ~IntString() {
        if (m_vec)
            free(m_vec);
    }
    size_t size() const {
        return m_len;
    }
    const int& operator[](int i) const {
        return m_vec[i];
    }
private:
    int *m_vec{nullptr};
    size_t m_len{0};
};

int u8DLDistance(const std::string& str1, const std::string str2)
{
    IntString istr1(str1);
    IntString istr2(str2);
    if ((str1.size() && istr1.size() == 0) || (str2.size() && istr2.size() == 0)) {
        return -1;
    }
    return DLDistance(istr1, istr2);
}

// Extract MIME type from a string looking like: ": text/plain; charset=us-ascii".
string growmimearoundslash(string mime)
{
    string::size_type start;
    string::size_type nd;
    // File -i will sometimes return strange stuff (ie: "very small file")
    if((start = nd = mime.find("/")) == string::npos) {
        return string();
    }
    while (start > 0) {
        start--;
        if (!isalpha(mime[start])) {
            start++;
            break;
        }
    }
    const static string allowedpunct("+-.");
    while (nd < mime.size() - 1) {
        nd++;
        if (!isalnum(mime[nd]) && allowedpunct.find(mime[nd]) == string::npos) {
            nd--;
            break;
        }
    }
    mime = mime.substr(start, nd-start+1);
    return mime;
}


// Date is Y[-M[-D]]
static bool parsedate(std::vector<std::string>::const_iterator& it,
                      std::vector<std::string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    if (it->length() > 4 || it->empty() ||
        it->find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->y1) != 1) {
        return false;
    }
    if (it == end || *it == "/") {
        return true;
    }
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || it->empty() ||
        it->find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->m1) != 1) {
        return false;
    }
    if (it == end || *it == "/") {
        return true;
    }
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || it->empty() ||
        it->find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->d1) != 1) {
        return false;
    }

    return true;
}

// Called with the 'P' already processed. Period ends at end of string
// or at '/'. We dont' do a lot effort at validation and will happily
// accept 10Y1Y4Y (the last wins)
static bool parseperiod(std::vector<std::string>::const_iterator& it,
                        std::vector<std::string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    while (it != end) {
        int value;
        if (it->find_first_not_of("0123456789") != std::string::npos) {
            return false;
        }
        if (sscanf(it++->c_str(), "%d", &value) != 1) {
            return false;
        }
        if (it == end || it->empty()) {
            return false;
        }
        switch (it->at(0)) {
        case 'Y':
        case 'y':
            dip->y1 = value;
            break;
        case 'M':
        case 'm':
            dip->m1 = value;
            break;
        case 'D':
        case 'd':
            dip->d1 = value;
            break;
        default:
            return false;
        }
        it++;
        if (it == end) {
            return true;
        }
        if (*it == "/") {
            return true;
        }
    }
    return true;
}

#if 0
static void cerrdip(const std::string& s, DateInterval *dip)
{
    cerr << s << dip->y1 << "-" << dip->m1 << "-" << dip->d1 << "/"
         << dip->y2 << "-" << dip->m2 << "-" << dip->d2
         << endl;
}
#endif

// Compute date + period. Won't work out of the unix era.
// or pre-1970 dates. Just convert everything to unixtime and
// seconds (with average durations for months/years), add and convert
// back
static bool addperiod(DateInterval* dp, const DateInterval* pp)
{
    // Create a struct tm with possibly non normalized fields and let
    // timegm sort it out
    struct tm tm = {};
    tm.tm_year = dp->y1 - 1900 + pp->y1;
    tm.tm_mon = dp->m1 + pp->m1 - 1;
    tm.tm_mday = dp->d1 + pp->d1;
    time_t tres = mktime(&tm);
    localtime_r(&tres, &tm);
    dp->y1 = tm.tm_year + 1900;
    dp->m1 = tm.tm_mon + 1;
    dp->d1 = tm.tm_mday;
    //cerrdip("Addperiod return", dp);
    return true;
}

int monthdays(int mon, int year)
{
    switch (mon) {
        // We are returning a few too many 29 days februaries, no problem
    case 2:
        return (year % 4) == 0 ? 29 : 28;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    default:
        return 30;
    }
}
bool parsedateinterval(const std::string& s, DateInterval *dip)
{
    std::vector<std::string> vs;
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    DateInterval p1, p2, d1, d2;
    p1 = p2 = d1 = d2 = *dip;
    bool hasp1 = false, hasp2 = false, hasd1 = false, hasd2 = false,
        hasslash = false;

    if (!stringToStrings(s, vs, "PYMDpymd-/")) {
        return false;
    }
    if (vs.empty()) {
        return false;
    }

    auto it = vs.cbegin();
    if (*it == "P" || *it == "p") {
        it++;
        if (!parseperiod(it, vs.end(), &p1)) {
            return false;
        }
        hasp1 = true;
        //cerrdip("p1", &p1);
        p1.y1 = -p1.y1;
        p1.m1 = -p1.m1;
        p1.d1 = -p1.d1;
    } else if (*it == "/") {
        hasslash = true;
        goto secondelt;
    } else {
        if (!parsedate(it, vs.end(), &d1)) {
            return false;
        }
        hasd1 = true;
    }

    // Got one element and/or /
secondelt:
    if (it != vs.end()) {
        if (*it != "/") {
            return false;
        }
        hasslash = true;
        it++;
        if (it == vs.end()) {
            // ok
        } else if (*it == "P" || *it == "p") {
            it++;
            if (!parseperiod(it, vs.end(), &p2)) {
                return false;
            }
            hasp2 = true;
        } else {
            if (!parsedate(it, vs.end(), &d2)) {
                return false;
            }
            hasd2 = true;
        }
    }

    // 2 periods dont' make sense
    if (hasp1 && hasp2) {
        return false;
    }
    // Nothing at all doesn't either
    if (!hasp1 && !hasd1 && !hasp2 && !hasd2) {
        return false;
    }

    // Empty part means today IF other part is period, else means
    // forever (stays at 0)
    time_t now = time(nullptr);
    struct tm *tmnow = gmtime(&now);
    if ((!hasp1 && !hasd1) && hasp2) {
        d1.y1 = 1900 + tmnow->tm_year;
        d1.m1 = tmnow->tm_mon + 1;
        d1.d1 = tmnow->tm_mday;
        hasd1 = true;
    } else if ((!hasp2 && !hasd2) && hasp1) {
        d2.y1 = 1900 + tmnow->tm_year;
        d2.m1 = tmnow->tm_mon + 1;
        d2.d1 = tmnow->tm_mday;
        hasd2 = true;
    }

    // Incomplete dates have different meanings depending if there is
    // a period or not (actual or infinite indicated by a / + empty)
    //
    // If there is no explicit period, an incomplete date indicates a
    // period of the size of the uncompleted elements. Ex: 1999
    // actually means 1999/P12M
    //
    // If there is a period, the incomplete date should be extended
    // to the beginning or end of the unspecified portion. Ex: 1999/
    // means 1999-01-01/ and /1999 means /1999-12-31
    if (hasd1) {
        if (!(hasslash || hasp2)) {
            if (d1.m1 == 0) {
                p2.m1 = 12;
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
                p2.d1 = monthdays(d1.m1, d1.y1);
            }
            hasp2 = true;
        } else {
            if (d1.m1 == 0) {
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
            }
        }
    }
    // if hasd2 is true we had a /
    if (hasd2) {
        if (d2.m1 == 0) {
            d2.m1 = 12;
            d2.d1 = 31;
        } else if (d2.d1 == 0) {
            d2.d1 = monthdays(d2.m1, d2.y1);
        }
    }
    if (hasp1) {
        // Compute d1
        d1 = d2;
        if (!addperiod(&d1, &p1)) {
            return false;
        }
    } else if (hasp2) {
        // Compute d2
        d2 = d1;
        if (!addperiod(&d2, &p2)) {
            return false;
        }
    }

    dip->y1 = d1.y1;
    dip->m1 = d1.m1;
    dip->d1 = d1.d1;
    dip->y2 = d2.y1;
    dip->m2 = d2.m1;
    dip->d2 = d2.d1;
    return true;
}

struct MyConfFinderCB : public FsTreeWalkerCB {
    std::vector<std::string> dirs;
    FsTreeWalker::Status processone(
        const std::string &path, FsTreeWalker::CbFlag flg, const struct PathStat&) override {
        if (flg == FsTreeWalker::FtwDirEnter && path_exists(path_cat(path, "recoll.conf")))
            dirs.push_back(path);
        return FsTreeWalker::FtwOk;
    }
};

std::vector<std::string> guess_recoll_confdirs(const std::string& where)
{
    FsTreeWalker walker;
    walker.setOpts(FsTreeWalker::FtwTravBreadthThenDepth); 
    walker.setMaxDepth(1);
    MyConfFinderCB cb;
    std::string top = where.empty() ? path_homedata() : where;
    walker.walk(top, cb);
    return cb.dirs;
}

void rclutil_init_mt()
{
    path_pkgdatadir();
    tmplocation();
    thumbnailsdir();
    // Init langtocode() static table
    langtocode("");
}
