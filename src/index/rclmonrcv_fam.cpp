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

#ifdef FSWATCH_FAM

#include "rclmonrcv_fam.h"

#include <errno.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "log.h"
#include "rclmon.h"
#include "pathut.h"
#include "smallut.h"


//////////////////////////////////////////////////////////////////////////
/** Fam/gamin -based monitor class */
#include <fam.h>
#include <sys/select.h>
#include <setjmp.h>
#include <signal.h>

// Translate event code to string (debug)
const char *RclFAM::event_name(int code)
{
    static const char *famevent[] = {
        "",
        "FAMChanged",
        "FAMDeleted",
        "FAMStartExecuting",
        "FAMStopExecuting",
        "FAMCreated",
        "FAMMoved",
        "FAMAcknowledge",
        "FAMExists",
        "FAMEndExist"
    };
    static char unknown_event[30];
 
    if (code < FAMChanged || code > FAMEndExist) {
        snprintf(unknown_event, sizeof(unknown_event), "unknown (%d)", code);
        return unknown_event;
    }
    return famevent[code];
}

RclFAM::RclFAM()
    : m_ok(false)
{
    if (FAMOpen2(&m_conn, "Recoll")) {
        LOGERR("RclFAM::RclFAM: FAMOpen2 failed, errno " << errno << "\n");
        return;
    }
    m_ok = true;
}

RclFAM::~RclFAM()
{
    if (ok())
        FAMClose(&m_conn);
}

static jmp_buf jbuf;
static void onalrm(int sig)
{
    longjmp(jbuf, 1);
}
bool RclFAM::addWatch(const string& path, bool isdir, bool)
{
    if (!ok())
        return false;
    bool ret = false;

    MONDEB("RclFAM::addWatch: adding " << path << "\n");

    // It happens that the following call block forever. 
    // We'd like to be able to at least terminate on a signal here, but
    // gamin forever retries its write call on EINTR, so it's not even useful
    // to unblock signals. SIGALRM is not used by the main thread, so at least
    // ensure that we exit after gamin gets stuck.
    if (setjmp(jbuf)) {
        LOGERR("RclFAM::addWatch: timeout talking to FAM\n");
        return false;
    }
    signal(SIGALRM, onalrm);
    alarm(20);
    FAMRequest req;
    if (isdir) {
        if (FAMMonitorDirectory(&m_conn, path.c_str(), &req, 0) != 0) {
            LOGERR("RclFAM::addWatch: FAMMonitorDirectory failed\n");
            goto out;
        }
    } else {
        if (FAMMonitorFile(&m_conn, path.c_str(), &req, 0) != 0) {
            LOGERR("RclFAM::addWatch: FAMMonitorFile failed\n");
            goto out;
        }
    }
    m_idtopath[req.reqnum] = path;
    ret = true;

out:
    alarm(0);
    return ret;
}

// Note: return false only for queue empty or error 
// Return EVT_NONE for bad event to keep queue processing going
bool RclFAM::getEvent(RclMonEvent& ev, int msecs)
{
    if (!ok())
        return false;
    MONDEB("RclFAM::getEvent:\n");

    fd_set readfds;
    int fam_fd = FAMCONNECTION_GETFD(&m_conn);
    FD_ZERO(&readfds);
    FD_SET(fam_fd, &readfds);

    MONDEB("RclFAM::getEvent: select. fam_fd is " << fam_fd << "\n");
    // Fam / gamin is sometimes a bit slow to send events. Always add
    // a little timeout, because if we fail to retrieve enough events,
    // we risk deadlocking in addwatch()
    if (msecs == 0)
        msecs = 2;
    struct timeval timeout;
    if (msecs >= 0) {
        timeout.tv_sec = msecs / 1000;
        timeout.tv_usec = (msecs % 1000) * 1000;
    }
    int ret;
    if ((ret=select(fam_fd+1, &readfds, 0, 0, msecs >= 0 ? &timeout : 0)) < 0) {
        LOGERR("RclFAM::getEvent: select failed, errno " << errno << "\n");
        close();
        return false;
    } else if (ret == 0) {
        // timeout
        MONDEB("RclFAM::getEvent: select timeout\n");
        return false;
    }

    MONDEB("RclFAM::getEvent: select returned " << ret << "\n");

    if (!FD_ISSET(fam_fd, &readfds))
        return false;

    // ?? 2011/03/15 gamin v0.1.10. There is initially a single null
    // byte on the connection so the first select always succeeds. If
    // we then call FAMNextEvent we stall. Using FAMPending works
    // around the issue, but we did not need this in the past and this
    // is most weird.
    if (FAMPending(&m_conn) <= 0) {
        MONDEB("RclFAM::getEvent: FAMPending says no events\n");
        return false;
    }

    MONDEB("RclFAM::getEvent: call FAMNextEvent\n");
    FAMEvent fe;
    if (FAMNextEvent(&m_conn, &fe) < 0) {
        LOGERR("RclFAM::getEvent: FAMNextEvent: errno " << errno << "\n");
        close();
        return false;
    }
    MONDEB("RclFAM::getEvent: FAMNextEvent returned\n");
    
    map<int,string>::const_iterator it;
    if ((!path_isabsolute(fe.filename)) && 
        (it = m_idtopath.find(fe.fr.reqnum)) != m_idtopath.end()) {
        ev.m_path = path_cat(it->second, fe.filename);
    } else {
        ev.m_path = fe.filename;
    }

    MONDEB("RclFAM::getEvent: " << event_name(fe.code) < " " << ev.m_path << "\n");

    switch (fe.code) {
    case FAMCreated:
        if (path_isdir(ev.m_path)) {
            ev.m_etyp = RclMonEvent::RCLEVT_DIRCREATE;
            break;
        }
        /* FALLTHROUGH */
    case FAMChanged:
    case FAMExists:
        // Let the other side sort out the status of this file vs the db
        ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
        break;

    case FAMMoved: 
    case FAMDeleted:
        ev.m_etyp = RclMonEvent::RCLEVT_DELETE;
        // We would like to signal a directory here to enable cleaning
        // the subtree (on a dir move), but can't test the actual file
        // which is gone, and fam doesn't tell us if it's a dir or reg. 
        // Let's rely on the fact that a directory should be watched
        if (eraseWatchSubTree(m_idtopath, ev.m_path)) 
            ev.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
        break;

    case FAMStartExecuting:
    case FAMStopExecuting:
    case FAMAcknowledge:
    case FAMEndExist:
    default:
        // Have to return something, this is different from an empty queue,
        // esp if we are trying to empty it...
        if (fe.code != FAMEndExist)
            LOGDEB("RclFAM::getEvent: got other event " << fe.code << "!\n");
        ev.m_etyp = RclMonEvent::RCLEVT_NONE;
        break;
    }
    return true;
}
#endif // FSWATCH_FAM
