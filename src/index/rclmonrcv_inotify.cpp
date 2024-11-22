/* Copyright (C) 2024 J.F.Dockes
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

#include "autoconfig.h"
#include "rclmonrcv.h"

#ifdef FSWATCH_INOTIFY

#include "rclmonrcv_inotify.h"

#include <errno.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "log.h"
#include "rclmon.h"
#include "pathut.h"
#include "smallut.h"

using std::string;
using std::map;

//////////////////////////////////////////////////////////////////////////
/** Inotify-based monitor class */
#include <sys/inotify.h>
#include <sys/select.h>

RclIntf::RclIntf()
    : m_ok(false), m_fd(-1), m_evp(nullptr), m_ep(nullptr)
{
    if ((m_fd = inotify_init()) < 0) {
        LOGERR("RclIntf:: inotify_init failed, errno " << errno << "\n");
        return;
    }
    m_ok = true;
}

RclIntf::~RclIntf()
{
    close();
}

bool RclIntf::ok() const
{
    return m_ok;
}

void RclIntf::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_ok = false;
}

const char *RclIntf::event_name(int code) 
{
    code &= ~(IN_ISDIR|IN_ONESHOT);
    switch (code) {
    case IN_ACCESS: return "IN_ACCESS";  // File was accessed
    case IN_MODIFY: return "IN_MODIFY";  // File was modified
    case IN_ATTRIB: return "IN_ATTRIB";  // Metadata changed (e.g., permissions, timestamps)
    case IN_CLOSE_WRITE: return "IN_CLOSE_WRITE";  // Writable file was closed
    case IN_CLOSE_NOWRITE: return "IN_CLOSE_NOWRITE";  // Unwritable file was closed
    case IN_CLOSE: return "IN_CLOSE";  // File was closed
    case IN_OPEN: return "IN_OPEN";  // File was opened
    case IN_MOVED_FROM: return "IN_MOVED_FROM";  // File or directory was moved from this location
    case IN_MOVED_TO: return "IN_MOVED_TO";  // File or directory was moved to this location
    case IN_MOVE: return "IN_MOVE";  // File was moved
    case IN_CREATE: return "IN_CREATE";  // File or directory was created
    case IN_DELETE: return "IN_DELETE";  // File or directory was deleted
    case IN_DELETE_SELF: return "IN_DELETE_SELF";  // The monitored item itself was deleted
    case IN_MOVE_SELF: return "IN_MOVE_SELF";  // The monitored item itself was moved
    case IN_UNMOUNT: return "IN_UNMOUNT";  // Filesystem was unmounted
    case IN_Q_OVERFLOW: return "IN_Q_OVERFLOW";  // Event queue overflowed
    case IN_IGNORED: return "IN_IGNORED";  // Watch was removed
    default: {
        static char msg[50];
        snprintf(msg, sizeof(msg), "Unknown event 0x%x", code);
        return msg;
    }
    };
}

bool RclIntf::addWatch(const string& path, bool, bool follow)
{
    if (!ok()) {
        return false;
    }
    MONDEB("RclIntf::addWatch: adding " << path << " follow " << follow << "\n");
    // CLOSE_WRITE is covered through MODIFY. CREATE is needed for mkdirs
    uint32_t mask = IN_MODIFY | IN_CREATE
        | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE
        // IN_ATTRIB used to be not needed to receive extattr
        // modification events, which was a bit weird because only ctime is
        // set, and now it is...
        | IN_ATTRIB
#ifdef IN_DONT_FOLLOW
        | (follow ? 0 : IN_DONT_FOLLOW)
#endif
#ifdef IN_EXCL_UNLINK
        | IN_EXCL_UNLINK
#endif
        ;
    int wd;
    if ((wd = inotify_add_watch(m_fd, path.c_str(), mask)) < 0) {
        saved_errno = errno;
        LOGSYSERR("RclIntf::addWatch", "inotify_add_watch", path);
        if (errno == ENOSPC) {
            LOGERR("RclIntf::addWatch: ENOSPC error may mean that you should "
                   "increase the inotify kernel constants. See inotify(7)\n");
        }
        return false;
    }
    m_idtopath[wd] = path;
    return true;
}

// Note: return false only for queue empty or error 
// Return EVT_NONE for bad event to keep queue processing going
bool RclIntf::getEvent(RclMonEvent& ev, int msecs)
{
    if (!ok())
        return false;
    ev.m_etyp = RclMonEvent::RCLEVT_NONE;
    MONDEB("RclIntf::getEvent:\n");

    if (nullptr == m_evp) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_fd, &readfds);
        struct timeval timeout;
        if (msecs >= 0) {
            timeout.tv_sec = msecs / 1000;
            timeout.tv_usec = (msecs % 1000) * 1000;
        }
        int ret;
        MONDEB("RclIntf::getEvent: select\n");
        if ((ret = select(m_fd + 1, &readfds, nullptr, nullptr,
                          msecs >= 0 ? &timeout : nullptr)) < 0) {
            LOGSYSERR("RclIntf::getEvent", "select", "");
            close();
            return false;
        } else if (ret == 0) {
            MONDEB("RclIntf::getEvent: select timeout\n");
            // timeout
            return false;
        }
        MONDEB("RclIntf::getEvent: select returned\n");

        if (!FD_ISSET(m_fd, &readfds))
            return false;
        int rret;
        if ((rret=read(m_fd, m_evbuf, sizeof(m_evbuf))) <= 0) {
            LOGSYSERR("RclIntf::getEvent", "read", sizeof(m_evbuf));
            close();
            return false;
        }
        m_evp = m_evbuf;
        m_ep = m_evbuf + rret;
    }

    struct inotify_event *evp = (struct inotify_event *)m_evp;
    m_evp += sizeof(struct inotify_event);
    if (evp->len > 0)
        m_evp += evp->len;
    if (m_evp >= m_ep)
        m_evp = m_ep = nullptr;
    
    map<int,string>::const_iterator it;
    if ((it = m_idtopath.find(evp->wd)) == m_idtopath.end()) {
        LOGERR("RclIntf::getEvent: unknown wd " << evp->wd << "\n");
        return true;
    }
    ev.m_path = it->second;

    if (evp->len > 0) {
        ev.m_path = path_cat(ev.m_path, evp->name);
    }

    MONDEB("RclIntf::getEvent: " << event_name(evp->mask) << " " << ev.m_path << "\n");

    if ((evp->mask & IN_MOVED_FROM) && (evp->mask & IN_ISDIR)) {
        // We get this when a directory is renamed. Erase the subtree
        // entries in the map. The subsequent MOVED_TO will recreate
        // them. This is probably not needed because the watches
        // actually still exist in the kernel, so that the wds
        // returned by future addwatches will be the old ones, and the
        // map will be updated in place. But still, this feels safer
        eraseWatchSubTree(m_idtopath, ev.m_path);
    }

    // IN_ATTRIB used to be not needed, but now it is
    if (evp->mask & (IN_MODIFY|IN_ATTRIB)) {
        ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
    } else if (evp->mask & (IN_DELETE | IN_MOVED_FROM)) {
        ev.m_etyp = RclMonEvent::RCLEVT_DELETE;
        if (evp->mask & IN_ISDIR)
            ev.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
    } else if (evp->mask & (IN_CREATE | IN_MOVED_TO)) {
        if (evp->mask & IN_ISDIR) {
            ev.m_etyp = RclMonEvent::RCLEVT_DIRCREATE;
        } else {
            // We used to return null event because we would get a
            // modify event later, but it seems not to be the case any
            // more (10-2011). So generate MODIFY event
            ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
        }
    } else if (evp->mask & (IN_IGNORED)) {
        if (!m_idtopath.erase(evp->wd)) {
            LOGDEB0("Got IGNORE event for unknown watch\n");
        } else {
            eraseWatchSubTree(m_idtopath, ev.m_path);
        }
    } else {
        LOGDEB("RclIntf::getEvent: unhandled event " << event_name(evp->mask) <<
               " " << evp->mask << " " << ev.m_path << "\n");
        return true;
    }
    return true;
}

#endif // FSWATCH_INOTIFY
