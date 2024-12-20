/* Copyright (C) 2004-2022 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * flock emulation:
 *   Emulate flock on platforms that lack it, primarily Windows and MinGW.
 *
 *   This is derived from sqlite3 sources.
 *   https://www.sqlite.org/src/finfo?name=src/os_win.c
 *   https://www.sqlite.org/copyright.html
 *
 *   Written by Richard W.M. Jones <rjones.at.redhat.com>
 *
 *   Copyright (C) 2008-2019 Free Software Foundation, Inc.
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef BUILDING_RECOLL
#include "autoconfig.h"
#else
#include "config.h"
#endif

#include "pathut.h"

#include "smallut.h"
#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdio.h>
#include <vector>
#include <fcntl.h>

// Listing directories: we include the normal dirent.h on Unix-derived
// systems, and on MinGW, where it comes with a supplemental wide char
// interface. When building with MSVC, we use our bundled msvc_dirent.h,
// which is equivalent to the one in MinGW
#ifdef _MSC_VER
#include "msvc_dirent.h"
#else // !_MSC_VER
#include <dirent.h>
#endif // _MSC_VER


#ifdef _WIN32

#ifndef _MSC_VER
#undef WINVER
#define WINVER 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#define LOGFONTW void
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <io.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <Stringapiset.h>

#include <sys/utime.h>
#include <sys/stat.h>
#include <direct.h>

#if !defined(S_IFLNK)
#define S_IFLNK 0
#endif
#ifndef S_ISDIR
# define S_ISDIR(ST_MODE) (((ST_MODE) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
# define S_ISREG(ST_MODE) (((ST_MODE) & _S_IFMT) == _S_IFREG)
#endif
#define MAXPATHLEN PATH_MAX
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#ifndef R_OK
#define R_OK 4
#endif

#define STAT _wstati64
#define FSTAT _fstati64
#define LSTAT win_wlstat
#define STATBUF _stati64
#define ACCESS _waccess
#define OPENDIR ::_wopendir
#define DIRHDL _WDIR
#define CLOSEDIR _wclosedir
#define READDIR ::_wreaddir
#define REWINDDIR ::_wrewinddir
#define DIRENT _wdirent
#define DIRHDL _WDIR
#define MKDIR(a,b) _wmkdir(a)
#define RENAME(o,n) _wrename(o,n)
#define OPEN ::_wopen
#define UNLINK _wunlink
#define RMDIR _wrmdir
#define CHDIR _wchdir

#define SYSPATH(PATH, SPATH) wchar_t PATH ## _buf[2048];        \
    utf8towchar(PATH, PATH ## _buf, 2048);                      \
    wchar_t *SPATH = PATH ## _buf;

#define ftruncate _chsize_s

#ifdef _MSC_VER
// For getpid
#include <process.h>
#define getpid _getpid
#endif // _MSC_VER

#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

inline ssize_t sys_read(int fd, void* buf, size_t cnt)
{
    return static_cast<ssize_t>(::read(fd, buf, static_cast<int>(cnt)));
}
inline ssize_t sys_write(int fd, const void* buf, size_t cnt)
{
    return static_cast<ssize_t>(::write(fd, buf, static_cast<int>(cnt)));
}

#else /* !_WIN32 -> */

#include <unistd.h>
#include <sys/time.h>
#include <sys/param.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#define STAT stat
#define LSTAT lstat
#define FSTAT fstat
#define STATBUF stat
#define ACCESS access
#define OPENDIR ::opendir
#define DIRHDL DIR
#define CLOSEDIR closedir
#define READDIR ::readdir
#define REWINDDIR ::rewinddir
#define DIRENT dirent
#define DIRHDL DIR
#define MKDIR(a,b) mkdir(a,b)
#define RENAME(o,n) rename(o,n)
#define OPEN ::open
#define UNLINK ::unlink
#define RMDIR ::rmdir
#define CHDIR ::chdir

#define SYSPATH(PATH, SPATH) const char *SPATH = PATH.c_str()

#define sys_read ::read
#define sys_write ::write

#endif /* !_WIN32 */

namespace MedocUtils {

#ifdef _WIN32

/// Convert \ separators to /
void path_slashize(std::string& s)
{
    for (std::string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '\\') {
            s[i] = '/';
        }
    }
}
void path_backslashize(std::string& s)
{
    for (std::string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '/') {
            s[i] = '\\';
        }
    }
}
static bool path_strlookslikedrive(const std::string& s)
{
    return s.size() == 2 && isalpha(s[0]) && s[1] == ':';
}

bool path_hasdrive(const std::string& s)
{
    if (s.size() >= 2 && isalpha(s[0]) && s[1] == ':') {
        return true;
    }
    return false;
}
bool path_isdriveabs(const std::string& s)
{
    if (s.size() >= 3 && isalpha(s[0]) && s[1] == ':' && s[2] == '/') {
        return true;
    }
    return false;
}

/* Operations for the 'flock' call (same as Linux kernel constants).  */
# define LOCK_SH 1       /* Shared lock.  */
# define LOCK_EX 2       /* Exclusive lock.  */
# define LOCK_UN 8       /* Unlock.  */

/* Can be OR'd in to one of the above.  */
# define LOCK_NB 4       /* Don't block when locking.  */

/* Determine the current size of a file.  Because the other braindead
 * APIs we'll call need lower/upper 32 bit pairs, keep the file size
 * like that too.
 */
static BOOL
file_size (HANDLE h, DWORD * lower, DWORD * upper)
{
    *lower = GetFileSize (h, upper);
    /* It appears that we can't lock an empty file, a lock is always
       over a data section. But we seem to be able to set a lock
       beyond the current file size, which is enough to get Pidfile
       working */
    if (*lower == 0 && *upper == 0) {
        *lower = 100;
    }
    return 1;
}

/* LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems. */
# ifndef LOCKFILE_FAIL_IMMEDIATELY
#  define LOCKFILE_FAIL_IMMEDIATELY 1
# endif

/* Acquire a lock. */
static BOOL
do_lock (HANDLE h, int non_blocking, int exclusive)
{
    BOOL res;
    DWORD size_lower, size_upper;
    OVERLAPPED ovlp;
    int flags = 0;

    /* We're going to lock the whole file, so get the file size. */
    res = file_size (h, &size_lower, &size_upper);
    if (!res)
        return 0;

    /* Start offset is 0, and also zero the remaining members of this struct. */
    memset (&ovlp, 0, sizeof ovlp);

    if (non_blocking)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (exclusive)
        flags |= LOCKFILE_EXCLUSIVE_LOCK;

    return LockFileEx (h, flags, 0, size_lower, size_upper, &ovlp);
}

/* Unlock reader or exclusive lock. */
static BOOL
do_unlock (HANDLE h)
{
    int res;
    DWORD size_lower, size_upper;

    res = file_size (h, &size_lower, &size_upper);
    if (!res)
        return 0;

    return UnlockFile (h, 0, 0, size_lower, size_upper);
}

/* Now our BSD-like flock operation. */
int
flock (int fd, int operation)
{
    HANDLE h = (HANDLE) _get_osfhandle (fd);
    DWORD res;
    int non_blocking;

    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }

    non_blocking = operation & LOCK_NB;
    operation &= ~LOCK_NB;

    switch (operation) {
    case LOCK_SH:
        res = do_lock (h, non_blocking, 0);
        break;
    case LOCK_EX:
        res = do_lock (h, non_blocking, 1);
        break;
    case LOCK_UN:
        res = do_unlock (h);
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    /* Map Windows errors into Unix errnos.  As usual MSDN fails to
     * document the permissible error codes.
     */
    if (!res) {
        DWORD err = GetLastError ();
        switch (err){
            /* This means someone else is holding a lock. */
        case ERROR_LOCK_VIOLATION:
            errno = EAGAIN;
            break;

            /* Out of memory. */
        case ERROR_NOT_ENOUGH_MEMORY:
            errno = ENOMEM;
            break;

        case ERROR_BAD_COMMAND:
            errno = EINVAL;
            break;

            /* Unlikely to be other errors, but at least don't lose the
             * error code.
             */
        default:
            errno = err;
        }

        return -1;
    }

    return 0;
}

std::string path_shortpath(const std::string& path)
{
    SYSPATH(path, syspath);
    wchar_t wspath[MAX_PATH];
    int ret = GetShortPathNameW(syspath, wspath, MAX_PATH);
    if (ret == 0) {
        LOGERR("GetShortPathNameW failed for [" << path << "]\n");
        return path;
    } else if (ret >= MAX_PATH) {
        LOGERR("GetShortPathNameW [" << path << "] too long " <<
               path.size() << " MAX_PATH " << MAX_PATH << "\n");
        return path;
    }
    std::string shortpath;
    wchartoutf8(wspath, shortpath);
    return shortpath;
}

static int win_wlstat(const wchar_t *wpath, struct _stati64 *buffer)
{
    DWORD attrs = GetFileAttributesW(wpath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        auto error = GetLastError();
        std::string upath;
        wchartoutf8(wpath, upath);
        LOGDEB0("GetFileAttributesW failed with " << error << " for " << upath << '\n');
        return -1;
    }
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
        // Symbolic link or other strange beast (junction, or a myriad of other strange things)
        // Just return a bogus stat struct.
        memset(buffer, 0, sizeof(struct _stati64));
        // Note that tje st_mode field is a short and there is only 4
        // bits for the mode and they're full (IFDIR, IFCHR, IFIFO,
        // IFREG. So no place for S_IFLNK, which is currently defined
        // as 0. So we set an impossible value, which path_fileprops
        // will interpret specifically for win32.
        buffer->st_mode = _S_IFIFO|_S_IFCHR;
        return 0;
    }
    return _wstati64(wpath, buffer);
}

#endif /* _WIN32 */


// Note: this is actually only used on Linux, but it's no big deal to
// implement it everywhere to avoid more ifdefs.
static std::string argv0;
void pathut_setargv0(const char *a0)
{
    if (a0)
        argv0 = a0;
}

#ifdef _WIN32
std::string path_thisexecdir()
{
    wchar_t text[MAX_PATH];
    GetModuleFileNameW(NULL, text, MAX_PATH);
#ifdef NTDDI_WIN8_future
    PathCchRemoveFileSpec(text, MAX_PATH);
#else
    PathRemoveFileSpecW(text);
#endif
    std::string path;
    wchartoutf8(text, path);
    if (path.empty()) {
        path = "c:/";
    }

    return path;
}

#elif defined(__APPLE__)

#include <mach-o/dyld.h>

std::string path_thisexecdir()
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    char *path= (char*)malloc(size+1);
    _NSGetExecutablePath(path, &size);
    std::string ret = path_getfather(path);
    free(path);
    return ret;
}

#else

std::string path_which(const std::string& cmdname)
{
    auto cpathenv = getenv("PATH");
    if (cpathenv != nullptr) {
        // Need writable buffer
        auto pathenv = strdup(cpathenv);
        for (char *trdir = strtok(pathenv, ":"); trdir != 0; trdir = strtok(0, ":")) {
            auto path = path_cat(trdir, cmdname);
            auto candidate = path.c_str();
            struct stat fin;
            /* XXX work around access(2) false positives for superuser */
            if (access(candidate, X_OK) == 0 && stat(candidate, &fin) == 0 && S_ISREG(fin.st_mode) &&
                (getuid() != 0 ||  (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
                free(pathenv);
                return path;
            }
        }
        free(pathenv);
    }
    return std::string();
}

// https://stackoverflow.com/questions/606041/how-do-i-get-the-path-of-a-process-in-unix-linux
std::string path_thisexecdir()
{
    char pathbuf[PATH_MAX];
    /* Works on Linux */
    if (ssize_t buff_len = readlink("/proc/self/exe", pathbuf, PATH_MAX - 1); buff_len != -1) {
        return path_getfather(std::string(pathbuf, buff_len));
    }

    /* If argv0 is null we're doomed: execve("foobar", nullptr, nullptr) */
    if (argv0.empty()) {
        return std::string();
    }

    // Try argv0 as relative path
    if (nullptr != realpath(argv0.c_str(), pathbuf) && access(pathbuf, F_OK) == 0) {
        return path_getfather(pathbuf);
    }

    /* Current path ?? This would seem to assume that . is in the PATH so would be covered
       later. Not sure I understand the case */
    std::string cmdname = path_getsimple(argv0);
    std::string path = path_cat(path_cwd(), cmdname);
    if (access(path.c_str(), F_OK) == 0) {
        return path_getfather(path);
    }

    /* Try the PATH. */
    path = path_which(cmdname);
    if (!path.empty())
        return path_getfather(path);
    return std::string();
}
#endif // !_WIN32 && !__APPLE__


// This is only actually used on Windows currently, but compiled everywhere so that it can be
// tested, as there are no real Windows dependencies in there.
// The input is a slashized UNC path (like //host/share/path), or not, which we determine, returning
// true or false depending.
// On return, uncvolume contains the //host/share part. We take care to reject values with empty
// host or share parts which maybe somehow generated by other strange parts in recoll.
bool path_isunc(const std::string& s, std::string& uncvolume)
{
    if (s.size() < 5 || s[0] != '/' || s[1] != '/') {
        return false;
    }
    auto slash2 = s.find('/', 2);
    if (slash2 == std::string::npos || slash2 == s.size() - 1 || slash2 == 2) {
        return false;
    }
    auto slash3 = s.find('/', slash2 + 1);
    if (slash3 == slash2 + 1) {
        return false;
    }
    if (slash3 == std::string::npos ) {
        uncvolume = s;
    } else {
        uncvolume = s.substr(0, slash3);
    }
    return true;
}

bool fsocc(const std::string& path, int *pc, long long *avmbs)
{
    static const int FSOCC_MB = 1024 * 1024;
#ifdef _WIN32
    ULARGE_INTEGER freebytesavail;
    ULARGE_INTEGER totalbytes;
    SYSPATH(path, syspath);
    if (!GetDiskFreeSpaceExW(syspath, &freebytesavail, &totalbytes, NULL)) {
        return false;
    }
    if (pc) {
        *pc = int((100 * freebytesavail.QuadPart) / totalbytes.QuadPart);
    }
    if (avmbs) {
        *avmbs = int(totalbytes.QuadPart / FSOCC_MB);
    }
    return true;
#else /* !_WIN32 */

    struct statvfs buf;
    if (statvfs(path.c_str(), &buf) != 0) {
        return false;
    }

    if (pc) {
        double fsocc_used = double(buf.f_blocks - buf.f_bfree);
        double fsocc_totavail = fsocc_used + double(buf.f_bavail);
        double fpc = 100.0;
        if (fsocc_totavail > 0) {
            fpc = 100.0 * fsocc_used / fsocc_totavail;
        }
        *pc = int(fpc);
    }
    if (avmbs) {
        *avmbs = 0;
        if (buf.f_bsize > 0) {
            int ratio = buf.f_frsize > FSOCC_MB ? buf.f_frsize / FSOCC_MB :
                FSOCC_MB / buf.f_frsize;

            *avmbs = buf.f_frsize > FSOCC_MB ?
                ((long long)buf.f_bavail) * ratio :
                ((long long)buf.f_bavail) / ratio;
        }
    }
    return true;
#endif /* !_WIN32 */
}


const std::string& path_PATHsep()
{
    static const std::string w(";");
    static const std::string u(":");
#ifdef _WIN32
    return w;
#else
    return u;
#endif
}

void path_catslash(std::string& s)
{
#ifdef _WIN32
    path_slashize(s);
#endif
    if (s.empty() || s[s.length() - 1] != '/') {
        s += '/';
    }
}

std::string path_cat(const std::string& s1, const std::string& s2)
{
    std::string res = s1.empty() ? "./" : s1;
    if (!s2.empty()) {
        path_catslash(res);
        res +=  s2;
    }
    return res;
}

std::string path_cat(const std::string& s1, std::initializer_list<std::string> pathelts)
{
    std::string res = s1.empty() ? "./" : s1;
    for (const auto& p : pathelts) {
        if (!p.empty()) {
            res = path_cat(res, p);
        }
    }
    return res;
}

std::string path_getfather(const std::string& s)
{
    std::string father = s;
#ifdef _WIN32
    path_slashize(father);
#endif

    // ??
    if (father.empty()) {
        return "./";
    }

    if (path_isroot(father)) {
        return father;
    }

    if (father[father.length() - 1] == '/') {
        // Input ends with /. Strip it, root special case was tested above
        father.erase(father.length() - 1);
    }

    std::string::size_type slp = father.rfind('/');
    if (slp == std::string::npos) {
        return "./";
    }

    father.erase(slp);
    path_catslash(father);
    return father;
}

std::string path_getsimple(const std::string& s)
{
    std::string simple = s;
#ifdef _WIN32
    path_slashize(simple);
#endif

    if (simple.empty()) {
        return simple;
    }

    std::string::size_type slp = simple.rfind('/');
    if (slp == std::string::npos) {
        return simple;
    }

    simple.erase(0, slp + 1);
    return simple;
}

// Unlike path_getsimple(), we ignore right-side '/' chars, like the basename command does.
#ifdef _WIN32
std::string path_basename(const std::string& _s, const std::string& suff)
{
    std::string s{_s};
    path_slashize(s);
#else
std::string path_basename(const std::string& s, const std::string& suff)
{
#endif
    if (path_isroot(s))
        return s;
    std::string simple(s);
    rtrimstring(simple, "/");
    simple = path_getsimple(simple);
    std::string::size_type pos = std::string::npos;
    if (suff.length() && simple.length() > suff.length()) {
        pos = simple.rfind(suff);
        if (pos != std::string::npos && pos + suff.length() == simple.length()) {
            return simple.substr(0, pos);
        }
    }
    return simple;
}

std::string path_suffix(const std::string& s)
{
    std::string::size_type dotp = s.rfind('.');
    if (dotp == std::string::npos) {
        return std::string();
    }
    return s.substr(dotp + 1);
}

std::string path_home()
{
#ifdef _WIN32
    std::string dir;
    // Using wgetenv does not work well, depending on the
    // environment I get wrong values for the accented chars (works
    // with recollindex started from msys command window, does not
    // work when started from recoll. SHGet... fixes this
    //const wchar_t *cp = _wgetenv(L"USERPROFILE");
    wchar_t *cp;
    SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &cp);
    if (cp != 0) {
        wchartoutf8(cp, dir);
    }
    if (dir.empty()) {
        cp = _wgetenv(L"HOMEDRIVE");
        wchartoutf8(cp, dir);
        if (cp != 0) {
            std::string dir1;
            const wchar_t *cp1 = _wgetenv(L"HOMEPATH");
            wchartoutf8(cp1, dir1);
            if (cp1 != 0) {
                dir = path_cat(dir, dir1);
            }
        }
    }
    if (dir.empty()) {
        dir = "C:/";
    }
    dir = path_canon(dir);
    path_catslash(dir);
    return dir;
#else
    const char *cp = getenv("HOME");
    if (nullptr == cp) {
        uid_t uid = getuid();
        struct passwd *entry = getpwuid(uid);
        if (nullptr == entry) {
            return "/";
        }
        cp = entry->pw_dir;
    }
    std::string homedir{cp};
    path_catslash(homedir);
    return homedir;
#endif
}

std::string path_cachedir()
{
#ifdef _WIN32
    std::string dir;
    wchar_t *cp;
    SHGetKnownFolderPath(FOLDERID_InternetCache, 0, nullptr, &cp);
    if (cp != 0) {
        wchartoutf8(cp, dir);
    }
    if (dir.empty()) {
        cp = _wgetenv(L"HOMEDRIVE");
        wchartoutf8(cp, dir);
        if (cp != 0) {
            std::string dir1;
            const wchar_t *cp1 = _wgetenv(L"HOMEPATH");
            wchartoutf8(cp1, dir1);
            if (cp1 != 0) {
                dir = path_cat(dir, dir1);
            }
        }
    }
    if (dir.empty()) {
        dir = "C:/";
    }
    dir = path_canon(dir);
    path_catslash(dir);
    return dir;
#else
    static std::string xdgcache;
    if (xdgcache.empty()) {
        const char *cp = getenv("XDG_CACHE_HOME");
        if (nullptr == cp) {
            xdgcache = path_cat(path_home(), ".cache");
        } else {
            xdgcache = std::string(cp);
        }
        path_catslash(xdgcache);
    }
    return xdgcache;
#endif
}

std::string path_tildexpand(const std::string& s)
{
    if (s.empty() || s[0] != '~') {
        return s;
    }
    std::string o = s;
#ifdef _WIN32
    path_slashize(o);
#endif

    if (s.length() == 1) {
        o.replace(0, 1, path_home());
    } else if (s[1] == '/') {
        o.replace(0, 2, path_home());
    } else {
        std::string::size_type pos = s.find('/');
        std::string::size_type l = (pos == std::string::npos) ? s.length() - 1 : pos - 1;
#ifdef _WIN32
        // Dont know what this means. Just replace with HOME
        o.replace(0, l + 1, path_home());
#else
        struct passwd *entry = getpwnam(s.substr(1, l).c_str());
        if (entry) {
            o.replace(0, l + 1, entry->pw_dir);
        }
#endif
    }
    return o;
}

bool path_isroot(const std::string& path)
{
    if (path.size() == 1 && path[0] == '/') {
        return true;
    }
#ifdef _WIN32
    if (path.size() == 3 && isalpha(path[0]) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
#endif
    return false;
}

bool path_isdesc(const std::string& _top, const std::string& _sub)
{
    if (_top.empty() || _sub.empty())
        return false;
    std::string top = path_canon(_top);
    std::string sub = path_canon(_sub);
    path_catslash(top);
    path_catslash(sub);
    for (;;) {
        if (sub == top) {
            return true;
        }
        std::string::size_type l = sub.size();
        sub = path_getfather(sub);
        if (sub.size() == l || sub.size() < top.size()) {
            // At root or sub shorter than top: done
            if (sub == top) {
                return true;
            } else {
                return false;
            }
        }
    }
}

bool path_isabsolute(const std::string& path)
{
    if (!path.empty() && (path[0] == '/'
#ifdef _WIN32
                          || path_isdriveabs(path)
#endif
            )) {
        return true;
    }
    return false;
}

std::string path_absolute(const std::string& is)
{
    if (is.length() == 0) {
        return is;
    }
    std::string s = is;
#ifdef _WIN32
    path_slashize(s);
#endif
    if (!path_isabsolute(s)) {
        s = path_cat(path_cwd(), s);
#ifdef _WIN32
        path_slashize(s);
#endif
    }
    return s;
}

std::string path_canon(const std::string& is, const std::string* cwd)
{
    std::string s = is;
#ifdef _WIN32
    path_slashize(s);
    std::string uncvolume;
    if (path_isunc(s, uncvolume)) {
        s = s.substr(uncvolume.size());
        if (s.empty())
            s = "/";
    } else {
        // fix possible path from file: absolute url
        if (s.size() && s[0] == '/' && path_hasdrive(s.substr(1))) {
            s = s.substr(1);
        }
    }
#endif

    if (!path_isabsolute(s)) {
        if (cwd) {
            s = path_cat(*cwd, s);
        } else {
            s = path_cat(path_cwd(), s);
        }
    }
    std::vector<std::string> elems;
    stringToTokens(s, elems, "/");
    std::vector<std::string> cleaned;
    for (const auto& elem : elems) {
        if (elem == "..") {
            if (!cleaned.empty()) {
                cleaned.pop_back();
            }
        } else if (elem.empty() || elem == ".") {
        } else {
            cleaned.push_back(elem);
        }
    }
    std::string ret;
    if (!cleaned.empty()) {
        for (const auto& elem : cleaned) {
            ret += "/";
#ifdef _WIN32
            if (ret == "/" && path_strlookslikedrive(elem)) {
                // Get rid of just added initial "/"
                ret.clear();
            }
#endif
            ret += elem;
        }
    } else {
        ret = "/";
    }

#ifdef _WIN32
    if (uncvolume.size()) {
        ret = uncvolume + ret;
    } else if (path_strlookslikedrive(ret)) {
        // Raw drive needs a final /
        path_catslash(ret);
    }
#endif

    return ret;
}

bool path_makepath(const std::string& ipath, int mode)
{
    std::string path = path_canon(ipath);
    std::vector<std::string> elems;
    stringToTokens(path, elems, "/");
    path = "/";
    for (const auto& elem : elems) {
#ifdef _WIN32
        PRETEND_USE(mode);
        if (path == "/" && path_strlookslikedrive(elem)) {
            path = "";
        }
#endif
        path += elem;
        // Not using path_isdir() here, because this cant grok symlinks
        // If we hit an existing file, no worry, mkdir will just fail.
        LOGDEB1("path_makepath: testing existence: ["  << path << "]\n");
        if (!path_exists(path)) {
            LOGDEB1("path_makepath: creating directory ["  << path << "]\n");
            SYSPATH(path, syspath);
            if (MKDIR(syspath, mode) != 0)  {
                //cerr << "mkdir " << path << " failed, errno " << errno << "\n";
                return false;
            }
        }
        path += "/";
    }
    return true;
}

bool path_rename(const std::string& oldpath, const std::string& newpath)
{
    SYSPATH(oldpath, oldsyspath);
    SYSPATH(newpath, newsyspath);
    if (RENAME(oldsyspath, newsyspath) != 0) {
        return false;
    }
    return true;
}

bool path_chdir(const std::string& path)
{
    SYSPATH(path, syspath);
    return CHDIR(syspath) == 0;
}

std::string path_cwd()
{
#ifdef _WIN32
    wchar_t *wd = _wgetcwd(nullptr, 0);
    if (nullptr == wd) {
        return std::string();
    }
    std::string sdname;
    wchartoutf8(wd, sdname);
    free(wd);
    path_slashize(sdname);
    return sdname;
#else
    char wd[MAXPATHLEN+1];
    if (nullptr == getcwd(wd, MAXPATHLEN+1)) {
        return std::string();
    }
    return wd;
#endif
}

bool path_unlink(const std::string& path)
{
    SYSPATH(path, syspath);
    return UNLINK(syspath) == 0;
}

bool path_rmdir(const std::string& path)
{
    SYSPATH(path, syspath);
    return RMDIR(syspath) == 0;
}

bool path_utimes(const std::string& path, struct path_timeval _tv[2])
{
#ifdef _WIN32
    struct _utimbuf times;
    if (nullptr == _tv) {
        times.actime = times.modtime = time(0L);
    } else {
        times.actime = _tv[0].tv_sec;
        times.modtime = _tv[1].tv_sec;
    }
    SYSPATH(path, syspath);
    return _wutime(syspath, &times) != -1;
#else
    struct timeval tvb[2];
    if (nullptr == _tv) {
        gettimeofday(tvb, nullptr);
        tvb[1].tv_sec = tvb[0].tv_sec;
        tvb[1].tv_usec = tvb[0].tv_usec;
    } else {
        tvb[0].tv_sec = _tv[0].tv_sec;
        tvb[0].tv_usec = _tv[0].tv_usec;
        tvb[1].tv_sec = _tv[1].tv_sec;
        tvb[1].tv_usec = _tv[1].tv_usec;
    }
    return utimes(path.c_str(), tvb) == 0;
#endif
}

bool path_streamopen(const std::string& path, int mode, std::fstream& outstream)
{
#if defined(_WIN32) && defined (_MSC_VER)
    // MSC STL has support for using wide chars in fstream
    // constructor. We need this if, e.g. the user name/home directory
    // is not ASCII. Actually don't know how to do this with gcc
    wchar_t wpath[MAX_PATH + 1];
    utf8towchar(path, wpath, MAX_PATH);
    outstream.open(wpath, std::ios_base::openmode(mode));
#else
    outstream.open(path, std::ios_base::openmode(mode));
#endif
    if (!outstream.is_open()) {
        return false;
    }
    return true;
}

bool path_isdir(const std::string& path, bool follow)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    int ret = follow ? STAT(syspath, &st) : LSTAT(syspath, &st);
    if (ret < 0) {
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

bool path_isfile(const std::string& path, bool follow)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    int ret = follow ? STAT(syspath, &st) : LSTAT(syspath, &st);
    if (ret < 0) {
        return false;
    }
    if (S_ISREG(st.st_mode)) {
        return true;
    }
    return false;
}

long long path_filesize(const std::string& path)
{
    struct STATBUF st;
    SYSPATH(path, syspath);
    if (STAT(syspath, &st) < 0) {
        return -1;
    }
    return (long long)st.st_size;
}

bool path_samefile(const std::string& p1, const std::string& p2)
{
#ifdef _WIN32
    std::string cp1, cp2;
    cp1 = path_canon(p1);
    cp2 = path_canon(p2);
    return cp1 == cp2;
#else
    struct stat st1, st2;
    if (stat(p1.c_str(), &st1))
        return false;
    if (stat(p2.c_str(), &st2))
        return false;
    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
        return true;
    }
    return false;
#endif
}

#if defined(STATX_TYPE)

#include <sys/syscall.h>

#define MINORBITS       20
#define MKDEV(ma,mi)    (((ma) << MINORBITS) | (mi))

static ssize_t _statx(int dfd, const char *filename, unsigned flags,
               unsigned int mask, struct statx *buffer)
{
    return syscall(__NR_statx, dfd, filename, flags, mask, buffer);
}

static int statx(const char *filename, struct statx *buffer)
{
    int ret, atflag = 0;
    unsigned int mask = STATX_BASIC_STATS | STATX_BTIME;

    ret = _statx(AT_FDCWD, filename, atflag, mask,  buffer);
    if (ret < 0) {
        perror(filename);
    }

    return ret;
}

static int lstatx(const char *filename, struct statx *buffer)
{
    int ret, atflag = AT_SYMLINK_NOFOLLOW;
    unsigned int mask = STATX_BASIC_STATS | STATX_BTIME;

    ret = _statx(AT_FDCWD, filename, atflag, mask,  buffer);
    if (ret < 0) {
        perror(filename);
    }

    return ret;
}

static int fstatx(int fd, struct statx *buffer)
{
    int ret, atflag = AT_EMPTY_PATH;
    unsigned int mask = STATX_BASIC_STATS | STATX_BTIME;

    ret = _statx(fd, "", atflag, mask,  buffer);
    if (ret < 0) {
        perror("fstatx");
    }
    return ret;
}

#define ST_SIZE stx_size
#define ST_MODE stx_mode
#define ST_MTIME stx_mtime.tv_sec
#define ST_CTIME stx_ctime.tv_sec
#define ST_BTIME stx_btime.tv_sec
#define ST_INO stx_ino
#define ST_DEVICE(ST) MKDEV((ST).stx_dev_major, (ST).stx_dev_minor)
#define ST_BLOCKS  stx_blocks
#define ST_BLKSIZE stx_blksize
#define ST_MODE stx_mode
#define STATXSTRUCT statx
#define STATXCALL statx
#define FSTATXCALL fstatx
#define LSTATXCALL lstatx

#else /* -> !defined(STATX_TYPE) */

/* Using traditional stat */
#define ST_SIZE st_size
#define ST_MODE st_mode
#define ST_MTIME st_mtime
#define ST_CTIME st_ctime
#define ST_BTIME st_ctime
#define ST_INO st_ino
#define ST_DEVICE(ST) (ST).st_dev
#define ST_BLOCKS  st_blocks
#define ST_BLKSIZE st_blksize
#define ST_MODE st_mode
#define STATXSTRUCT STATBUF
#define STATXCALL STAT
#define FSTATXCALL FSTAT
#define LSTATXCALL LSTAT

#endif  /* Not using statx */


static void copystat(struct PathStat *stp, struct STATXSTRUCT& mst)
{
    stp->pst_size = mst.ST_SIZE;
    stp->pst_mode = mst.ST_MODE;
    stp->pst_mtime = mst.ST_MTIME;
    stp->pst_btime = mst.ST_BTIME;
    switch (mst.ST_MODE & S_IFMT) {
    case S_IFDIR: stp->pst_type = PathStat::PST_DIR;break;
    case S_IFLNK:  stp->pst_type = PathStat::PST_SYMLINK;break;
    case S_IFREG: stp->pst_type = PathStat::PST_REGULAR;break;
    default: stp->pst_type = PathStat::PST_OTHER;break;
    }
#ifdef _WIN32
    stp->pst_ctime = mst.ST_MTIME;
    if ((mst.ST_MODE & S_IFMT) == (_S_IFIFO|_S_IFCHR)) {
        stp->pst_type = PathStat::PST_SYMLINK;
    }
#else
    stp->pst_ino = mst.ST_INO;
    stp->pst_dev = ST_DEVICE(mst);
    stp->pst_ctime = mst.ST_CTIME;
    stp->pst_blocks = mst.ST_BLOCKS;
    stp->pst_blksize = mst.ST_BLKSIZE;
#endif
}

int path_fileprops(const std::string path, struct PathStat *stp, bool follow)
{
    if (nullptr == stp) {
        return -1;
    }
    *stp = PathStat{PathStat::PST_INVALID,0,0,0,0,0,0,0,0,0};
    struct STATXSTRUCT mst;
    SYSPATH(path, syspath);
    int ret = follow ? STATXCALL(syspath, &mst) : LSTATXCALL(syspath, &mst);
    if (ret != 0) {
        stp->pst_type = PathStat::PST_INVALID;
        return ret;
    }
    copystat(stp, mst);
    return 0;
}

int path_fileprops(int fd, struct PathStat *stp)
{
    if (nullptr == stp) {
        return -1;
    }
    *stp = PathStat{PathStat::PST_INVALID,0,0,0,0,0,0,0,0,0};
    struct STATXSTRUCT mst;
    int ret = FSTATXCALL(fd, &mst);
    if (ret != 0) {
        stp->pst_type = PathStat::PST_INVALID;
        return ret;
    }
    copystat(stp, mst);
    return 0;
}


bool path_exists(const std::string& path)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, 0) == 0;
}
bool path_readable(const std::string& path)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, R_OK) == 0;
}
bool path_access(const std::string& path, int mode)
{
    SYSPATH(path, syspath);
    return ACCESS(syspath, mode) == 0;
}


/// Directory reading interface. UTF-8 on Windows.
class PathDirContents::Internal {
public:
    ~Internal() {
        if (dirhdl) {
            CLOSEDIR(dirhdl);
        }
    }
        
    DIRHDL *dirhdl{nullptr};
    PathDirContents::Entry entry;
    std::string dirpath;
};

PathDirContents::PathDirContents(const std::string& dirpath)
{
    m = new Internal;
    m->dirpath = dirpath;
}

PathDirContents::~PathDirContents()
{
    delete m;
}

bool PathDirContents::opendir()
{
    if (m->dirhdl) {
        CLOSEDIR(m->dirhdl);
        m->dirhdl = nullptr;
    }
    const std::string& dp{m->dirpath};
    SYSPATH(dp, sysdir);
    m->dirhdl = OPENDIR(sysdir);
#ifdef _WIN32
    if (nullptr == m->dirhdl) {
        int rc = GetLastError();
        LOGERR("opendir failed: LastError " << rc << "\n");
        if (rc == ERROR_NETNAME_DELETED) {
            // 64: share disconnected.
            // Not too sure of the errno in this case.
            // Make sure it's not one of the permissible ones
            errno = ENODEV;
        }
    }
#endif
    return nullptr != m->dirhdl;
}

void PathDirContents::rewinddir()
{
    REWINDDIR(m->dirhdl);
}

const struct PathDirContents::Entry* PathDirContents::readdir()
{
    struct DIRENT *ent = READDIR(m->dirhdl);
    if (nullptr == ent) {
        return nullptr;
    }
#ifdef _WIN32
    std::string sdname;
    if (!wchartoutf8(ent->d_name, sdname)) {
        LOGERR("wchartoutf8 failed for " << ent->d_name << "\n");
        return nullptr;
    }
    const char *dname = sdname.c_str();
#else
    const char *dname = ent->d_name;
#endif
    m->entry.d_name = dname;
    return &m->entry;
}


bool listdir(const std::string& dir, std::string& reason, std::set<std::string>& entries)
{
    std::ostringstream msg;
    PathDirContents dc(dir);
    
    if (!path_isdir(dir)) {
        msg << "listdir: " << dir <<  " not a directory";
        goto out;
    }
    if (!path_access(dir, R_OK)) {
        msg << "listdir: no read access to " << dir;
        goto out;
    }

    if (!dc.opendir()) {
        msg << "listdir: cant opendir " << dir << ", errno " << errno;
        goto out;
    }
    const struct PathDirContents::Entry *ent;
    while ((ent = dc.readdir()) != nullptr) {
        if (ent->d_name == "." || ent->d_name == "..") {
            continue;
        }
        entries.insert(ent->d_name);
    }

out:
    reason = msg.str();
    if (reason.empty()) {
        return true;
    }
    return false;
}

// We do not want to mess with the pidfile content in the destructor:
// the lock might still be in use in a child process. In fact as much
// as we'd like to reset the pid inside the file when we're done, it
// would be very difficult to do it right and it's probably best left
// alone.
Pidfile::~Pidfile()
{
    this->close();
}

#ifdef _WIN32
// It appears that we can't read the locked file on Windows, so we use
// separate files for locking and holding the data.
static std::string pid_data_path(const std::string& path)
{
    // Remove extension. append -data to name, add back extension.
    auto ext = path_suffix(path);
    auto spath = path_cat(path_getfather(path), path_basename(path, ext));
    if (spath.back() == '.')
        spath.pop_back();
    if (!ext.empty())
        spath += std::string("-data") + "." + ext;
    return spath;
}
#endif // _WIN32

int Pidfile::read_pid()
{
#ifdef _WIN32
    // It appears that we can't read the locked file on Windows, so use an aux file
    auto path = pid_data_path(m_path);
    SYSPATH(path, syspath);
#else
    SYSPATH(m_path, syspath);
#endif
    int fd = OPEN(syspath, O_RDONLY);
    if (fd == -1) {
        if (errno != ENOENT)
            m_reason = "Open RDONLY failed: [" + m_path + "]: " + strerror(errno);
        return -1;
    }

    char buf[20];
    auto i = sys_read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (i <= 0) {
        m_reason = "Read failed: [" + m_path + "]: " + strerror(errno);
        return -1;
    }
    buf[i] = '\0';
    char *endptr;
    int pid = strtol(buf, &endptr, 10);
    if (endptr != &buf[i]) {
        m_reason = "Bad pid contents: [" + m_path + "]: " + strerror(errno);
        return - 1;
    }
    return pid;
}

int path_open(const std::string& path, int flags, int mode)
{
    SYSPATH(path, syspath);
    return OPEN(syspath, flags, mode);
}

int Pidfile::flopen()
{
    SYSPATH(m_path, syspath);
    if ((m_fd = OPEN(syspath, O_RDWR | O_CREAT, 0644)) == -1) {
        m_reason = "Open failed: [" + m_path + "]: " + strerror(errno);
        return -1;
    }

#ifdef sun
    struct flock lockdata;
    lockdata.l_start = 0;
    lockdata.l_len = 0;
    lockdata.l_type = F_WRLCK;
    lockdata.l_whence = SEEK_SET;
    if (fcntl(m_fd, F_SETLK,  &lockdata) != 0) {
        int serrno = errno;
        this->close();
        errno = serrno;
        m_reason = "fcntl lock failed";
        return -1;
    }
#else
    int operation = LOCK_EX | LOCK_NB;
    if (flock(m_fd, operation) == -1) {
        int serrno = errno;
        this->close();
        errno = serrno;
        m_reason = "flock failed";
        return -1;
    }
#endif // ! sun

    if (ftruncate(m_fd, 0) != 0) {
        /* can't happen [tm] */
        int serrno = errno;
        this->close();
        errno = serrno;
        m_reason = "ftruncate failed";
        return -1;
    }
    return 0;
}

int Pidfile::open()
{
    if (flopen() < 0) {
        return read_pid();
    }
    return 0;
}

int Pidfile::write_pid()
{
#ifdef _WIN32
    // It appears that we can't read the locked file on Windows, so use an aux file
    auto path = pid_data_path(m_path);
    SYSPATH(path, syspath);
    int fd;
    if ((fd = OPEN(syspath, O_RDWR | O_CREAT, 0644)) == -1) {
        m_reason = "Open failed: [" + path + "]: " + strerror(errno);
        return -1;
    }
#else
    int fd = m_fd;
#endif
    /* truncate to allow multiple calls */
    if (ftruncate(fd, 0) == -1) {
        m_reason = "ftruncate failed";
        return -1;
    }
    std::string pidstr = std::to_string(getpid());
    ::lseek(fd, 0, 0);
    if (sys_write(fd, pidstr.c_str(), pidstr.size()) != static_cast<ssize_t>(pidstr.size())) {
        m_reason = "write failed";
        return -1;
    }
#ifdef _WIN32
    ::close(fd);
#endif
    return 0;
}

int Pidfile::close()
{
    int ret = -1;
    if (m_fd >= 0) {
        ret = ::close(m_fd);
        m_fd = -1;
    }
    return ret;
}

int Pidfile::remove()
{
    SYSPATH(m_path, syspath);
    return UNLINK(syspath);
}

// Call funcs that need static init (not initially reentrant)
void pathut_init_mt()
{
    path_home();
}

} // End namespace MedocUtils
