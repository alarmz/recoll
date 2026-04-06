/* Copyright (C) 2005 J.F.Dockes 
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

#include "copyfile.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include "safeunistd.h"

#ifndef _WIN32
#include <sys/time.h>
#include <utime.h>
#include <sys/stat.h>
#endif

#include <cstring>

#include "pathut.h"
#include "log.h"

using namespace std;

#define CPBSIZ 8192

bool copyfile(const char *src, const char *dst, string &reason, int flags)
{
    int sfd = -1;
    int dfd = -1;
    bool ret = false;
    char buf[CPBSIZ];
    int oflags = O_WRONLY|O_CREAT|O_TRUNC|O_BINARY;

    LOGDEB("copyfile: "  << (src) << " to "  << (dst) << "\n" );

    if ((sfd = ::open(src, O_RDONLY, 0)) < 0) {
        reason += string("open ") + src + ": " + strerror(errno);
        goto out;
    }

    if (flags & COPYFILE_EXCL) {
        oflags |= O_EXCL;
    }

    if ((dfd = ::open(dst, oflags, 0644)) < 0) {
        reason += string("open/creat ") + dst + ": " + strerror(errno);
        // If we fail because of an open/truncate error, we do not
        // want to unlink the file, we might succeed...
        flags |= COPYFILE_NOERRUNLINK;
        goto out;
    }

    for (;;) {
        auto didread = sys_read(sfd, buf, CPBSIZ);
        if (didread < 0) {
            reason += string("read src ") + src + ": " + strerror(errno);
            goto out;
        }
        if (didread == 0)
            break;
        if (sys_write(dfd, buf, didread) != didread) {
            reason += string("write dst ") + src + ": " + strerror(errno);
            goto out;
        }
    }

    ret = true;
out:
    if (ret == false && !(flags&COPYFILE_NOERRUNLINK))
        path_unlink(dst);
    if (sfd >= 0)
        ::close(sfd);
    if (dfd >= 0)
        ::close(dfd);
    return ret;
}

bool stringtofile(const string& dt, const char *dst, string& reason,
                  int flags)
{
    LOGDEB("stringtofile:\n" );
    int dfd = -1;
    bool ret = false;
    int oflags = O_WRONLY|O_CREAT|O_TRUNC|O_BINARY;

    LOGDEB("stringtofile: "  << ((unsigned int)dt.size()) << " bytes to "  << (dst) << "\n" );

    if (flags & COPYFILE_EXCL) {
        oflags |= O_EXCL;
    }

    if ((dfd = ::open(dst, oflags, 0644)) < 0) {
        reason += string("open/creat ") + dst + ": " + strerror(errno);
        // If we fail because of an open/truncate error, we do not
        // want to unlink the file, we might succeed...
        flags |= COPYFILE_NOERRUNLINK;
        goto out;
    }
    if (sys_write(dfd, dt.c_str(), dt.size()) != static_cast<ssize_t>(dt.size())) {
        reason += string("write dst ") + ": " + strerror(errno);
        goto out;
    }

    ret = true;
out:
    if (ret == false && !(flags&COPYFILE_NOERRUNLINK))
        path_unlink(dst);
    if (dfd >= 0)
        ::close(dfd);
    return ret;
}

bool renameormove(const char *src, const char *dst, string &reason)
{
#ifdef _WIN32
    // Windows refuses to rename to an existing file. It appears that
    // there are workarounds (See MoveFile, MoveFileTransacted), but
    // anyway we are not expecting atomicity here.
    path_unlink(dst);
#endif
    
    // First try rename(2). If this succeeds we're done. If this fails
    // with EXDEV, try to copy. Unix really should have a library function
    // for this.
    if (rename(src, dst) == 0) {
        return true;
    }
    if (errno != EXDEV) {
        reason += string("rename(2) failed: ") + strerror(errno);
        return false;
    } 

    if (!copyfile(src, dst, reason))
        return false;

#ifndef _WIN32
    struct stat st;
    if (stat(src, &st) < 0) {
        reason += string("Can't stat ") + src + " : " + strerror(errno);
        return false;
    }
    struct stat st1;
    if (stat(dst, &st1) < 0) {
        reason += string("Can't stat ") + dst + " : " + strerror(errno);
        return false;
    }
    // Try to preserve modes, owner, times. This may fail for a number
    // of reasons
    if ((st1.st_mode & 0777) != (st.st_mode & 0777)) {
        if (chmod(dst, st.st_mode&0777) != 0) {
            reason += string("Chmod ") + dst + "Error : " + strerror(errno);
        }
    }
    if (st.st_uid != st1.st_uid || st.st_gid != st1.st_gid) {
        if (chown(dst, st.st_uid, st.st_gid) != 0) {
            reason += string("Chown ") + dst + "Error : " + strerror(errno);
        }
    }
#endif

    struct PathStat pst;
    path_fileprops(src, &pst);
    struct path_timeval times[2];
    times[0].tv_sec = time(0);
    times[0].tv_usec = 0;
    times[1].tv_sec = pst.pst_mtime;
    times[1].tv_usec = 0;
    path_utimes(dst, times);

    // All ok, get rid of origin
    if (!path_unlink(src)) {
        reason += string("Can't unlink ") + src + "Error : " + strerror(errno);
    }

    return true;
}
