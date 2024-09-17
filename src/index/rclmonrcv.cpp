/* Copyright (C) 2006-2024 J.F.Dockes 
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

/**
 * Recoll real time monitor event receiver. This file has code to interface 
 * to FAM, inotify, etc. and place events on the event queue.
 */

#include "autoconfig.h"

#ifdef RCL_MONITOR

#include "rclmonrcv.h"

#include <errno.h>
#include <cstdio>
#include <cstring>
#include "safeunistd.h"

#include "log.h"
#include "rclmon.h"
#include "rclinit.h"
#include "fstreewalk.h"
#include "pathut.h"
#include "smallut.h"

using std::string;
using std::vector;
using std::map;


RclMonitor::RclMonitor()
#ifndef _WIN32
    : m_originalParentPid(getppid())
#endif
{
}

bool RclMonitor::isOrphaned() {
#ifdef _WIN32
    return false;
#else
    pid_t currentParentPid = getppid();

    // For the case currentParentPid==1 see https://stackoverflow.com/a/68648482/4216175
    // Essentially, in Linux and macOS, when the process looses its parent,
    // a new id is automatically assigned. In this case, the id 1 of the init process
    // is automatically assigned.
    return currentParentPid == 1 || currentParentPid != m_originalParentPid;
#endif
}

// Monitor factory. We only have one compiled-in kind at a time, no
// need for a 'kind' parameter
static RclMonitor *makeMonitor();

/* ==== CLASS WalkCB: definition of member functions ==== */

/** 
 * Create directory watches during the initial file system tree walk.
 *
 * This class is a callback for the file system tree walker
 * class. The callback method alternatively creates the directory
 * watches and flushes the event queue (to avoid a possible overflow
 * while we create the watches)
 */
class WalkCB : public FsTreeWalkerCB {
public:
    WalkCB(RclConfig *conf, RclMonitor *mon, RclMonEventQueue *queue, FsTreeWalker& walker)
        : m_config(conf), m_mon(mon), m_queue(queue), m_walker(walker) {}
    virtual ~WalkCB() {}

    virtual FsTreeWalker::Status processone(
        const string &fn, FsTreeWalker::CbFlag flg, const struct PathStat&) override {
        MONDEB("walkerCB: processone " << fn <<  " m_mon " << m_mon <<
               " m_mon->ok " << (m_mon ? m_mon->ok() : false) << "\n");

        // We set the watch follow links flag for the topdir only.
        bool initfollow = m_initfollow;
        m_initfollow = false;
            
        if (flg == FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwDirReturn) {
            m_config->setKeyDir(fn);
            // Set up skipped patterns for this subtree. 
            m_walker.setSkippedNames(m_config->getSkippedNames());
        }

        if (flg == FsTreeWalker::FtwDirEnter) {
#ifndef FSWATCH_FSEVENTS
            
            // Create watch when entering directory, but first empty
            // whatever events we may already have on queue
            while (m_queue->ok() && m_mon->ok()) {
                RclMonEvent ev;
                if (m_mon->getEvent(ev, 0)) {
                    if (ev.m_etyp !=  RclMonEvent::RCLEVT_NONE)
                        m_queue->pushEvent(ev);
                } else {
                    MONDEB("walkerCB: no event pending\n");
                    break;
                }
            }
#endif
            if (!m_mon || !m_mon->ok())
                return FsTreeWalker::FtwError;
            // We do nothing special if addWatch fails for a reasonable reason
            if (!m_mon->isRecursive() && !m_mon->addWatch(fn, true, initfollow)) {
                if (m_mon->saved_errno != EACCES && m_mon->saved_errno != ENOENT) {
                    LOGINF("walkerCB: addWatch failed\n");
                    return FsTreeWalker::FtwError;
                }
            }
        } else if (!m_mon->generatesExist() && flg == FsTreeWalker::FtwRegular) {
            // Have to synthetize events for regular files existence
            // at startup because the monitor does not do it
            // Note 2011-09-29: no sure this is actually needed. We just ran
            //  an incremental indexing pass (before starting the
            //  monitor). Why go over the files once more ? The only
            //  reason I can see would be to catch modifications that
            //  happen between the incremental and the start of
            //  monitoring ? There should be another way: maybe start
            //  monitoring without actually handling events (just
            //  queue), then run incremental then start handling
            //  events ? ** But we also have to do it on a directory
            //  move! So keep it ** We could probably skip it on the initial run though.
            RclMonEvent ev;
            ev.m_path = fn;
            ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
            m_queue->pushEvent(ev);
        }
        return FsTreeWalker::FtwOk;
    }

private:
    RclConfig         *m_config;
    RclMonitor        *m_mon;
    RclMonEventQueue  *m_queue;
    FsTreeWalker&      m_walker;
    bool m_initfollow{true};
};

static bool rclMonAddTopWatches(
    FsTreeWalker& walker, RclConfig& lconfig, RclMonitor *mon, RclMonEventQueue *queue)
{
    // Get top directories from config. Special monitor sublist if
    // set, else full list.
    vector<string> tdl = lconfig.getTopdirs(true);
    if (tdl.empty()) {
        LOGERR("rclMonRcvRun:: top directory list (topdirs param.) not found "
               "in configuration or topdirs list parse error");
        queue->setTerminate();
        return false;
    }
    // Walk the directory trees to add watches
    for (const auto& topdir : tdl) {
        lconfig.setKeyDir(topdir);
        // Adjust the follow symlinks options
        bool follow{false};
        if (lconfig.getConfParam("followLinks", &follow) && follow) {
            walker.setOpts(FsTreeWalker::FtwFollow);
        } else {
            walker.setOpts(FsTreeWalker::FtwOptNone);
        }
        if (path_isdir(topdir, true)) {
            LOGDEB("rclMonRcvRun: walking " << topdir <<" monrecurs "<< mon->isRecursive() << "\n");
            // If the fs watcher is recursive, we add the watches for the topdirs here, and walk the
            // tree just for generating initial events.
            if (mon->isRecursive() && !mon->addWatch(topdir, true, true)) {
                if (mon->saved_errno != EACCES && mon->saved_errno != ENOENT) {
                    LOGERR("rclMonAddTopWatches: addWatch failed for [" << topdir << "]\n");
                    return false;
                }
            }
            // Note: need to rebuild the walkcb each time to reset the initial followlinks (always
            // true for the topdir)
            WalkCB walkcb(&lconfig, mon, queue, walker);
            if (walker.walk(topdir, walkcb) != FsTreeWalker::FtwOk) {
                LOGERR("rclMonRcvRun: tree walk failed\n");
                return false;
            }
            if (walker.getErrCnt() > 0) {
                LOGINFO("rclMonRcvRun: fs walker errors: " << walker.getReason() << "\n");
            }
        } else {
            // We have to special-case regular files which are part of the topdirs list because the
            // tree walker only adds watches for directories
            MONDEB("rclMonRcvRun: adding watch for non dir topdir " << topdir << "\n");
            if (!mon->addWatch(topdir, false, true)) {
                LOGSYSERR("rclMonRcvRun", "addWatch", topdir);
            }
        }
    }

    bool doweb{false};
    lconfig.getConfParam("processwebqueue", &doweb);
    if (doweb) {
        string webqueuedir = lconfig.getWebQueueDir();
        if (!mon->addWatch(webqueuedir, true)) {
            LOGERR("rclMonRcvRun: addwatch (webqueuedir) failed\n");
            if (mon->saved_errno != EACCES && mon->saved_errno != ENOENT)
                return false;
        }
    }
    return true;
}

bool rclMonAddSubWatches(
    const std::string& path, FsTreeWalker& walker, RclConfig& lconfig,
    RclMonitor *mon, RclMonEventQueue *queue)
{
    WalkCB walkcb(&lconfig, mon, queue, walker);
    if (walker.walk(path, walkcb) != FsTreeWalker::FtwOk) {
        LOGERR("rclMonRcvRun: walking new dir " << path << " : " << walker.getReason() << "\n");
        return false;
    }
    if (walker.getErrCnt() > 0) {
        LOGINFO("rclMonRcvRun: fs walker errors: " << walker.getReason() << "\n");
    }
    return true;
}

// Don't push events for skipped files. This would get filtered on the processing side
// anyway, but causes unnecessary wakeups and messages. Do not test skippedPaths here,
// this would be incorrect (because a topdir can be under a skippedPath and this was
// handled while adding the watches). Also we let the other side process onlyNames.
bool rclMonShouldSkip(const std::string& path, RclConfig& lconfig, FsTreeWalker& walker)
{
    lconfig.setKeyDir(path_getfather(path));
    walker.setSkippedNames(lconfig.getSkippedNames());
    if (walker.inSkippedNames(path_getsimple(path)))
        return true;
    return false;
}

// Main thread routine: create watches, then forever wait for and queue events
void *rclMonRcvRun(void *q)
{
    RclMonEventQueue *queue = (RclMonEventQueue *)q;

    LOGDEB("rclMonRcvRun: running\n");
    recoll_threadinit();
    // Make a local copy of the configuration as it doesn't like
    // concurrent accesses. It's ok to copy it here as the other
    // thread will not work before we have sent events.
    RclConfig lconfig(*queue->getConfig());

    // Create the fam/whatever interface object
    RclMonitor *mon = makeMonitor();
    if (mon == nullptr) {
        LOGERR("rclMonRcvRun: makeMonitor failed\n");
        queue->setTerminate();
        return nullptr;
    }

    FsTreeWalker walker;
    walker.setSkippedPaths(lconfig.getDaemSkippedPaths());

    if (!rclMonAddTopWatches(walker, lconfig, mon, queue)) {
        LOGERR("rclMonRcvRun: addtopwatches failed\n");
        goto terminate;
    }

    // Forever wait for monitoring events and add them to queue:
    MONDEB("rclMonRcvRun: waiting for events. q->ok(): " << queue->ok() << "\n");

#ifdef FSWATCH_FSEVENTS
    LOGINFO("rclMonRcvRun: started monitoring\n");
    mon->startMonitoring(queue,lconfig,walker);
    LOGINFO("rclMonRcvRun: gracefully stopped monitoring\n");
    LOGINFO("rclMonRcvRun: signaling the queue to gracefully terminate\n");
    goto terminate;    
#else // ! FSWATCH_FSEVENTS
    while (queue->ok() && mon->ok()) {
        RclMonEvent ev;
        // Note: I could find no way to get the select call to return when a signal is delivered to
        // the process (it goes to the main thread, from which I tried to close or write to the
        // select fd, with no effect). So set a timeout so that an intr will be detected
        if (mon->getEvent(ev, 2000)) {
            if (rclMonShouldSkip(ev.m_path, lconfig, walker))
                continue;

            if (ev.m_etyp == RclMonEvent::RCLEVT_DIRCREATE) {
                // Recursive addwatch: there may already be stuff inside this directory. E.g.: files
                // were quickly created, or this is actually the target of a directory move. This is
                // necessary for inotify, but it seems that fam/gamin is doing the job for us so
                // that we are generating double events here (no big deal as prc will sort/merge).
                LOGDEB("rclMonRcvRun: walking new dir " << ev.m_path << "\n");
                if (!rclMonAddSubWatches(ev.m_path, walker, lconfig, mon, queue)) {
                    goto terminate;
                }
            }

            if (ev.m_etyp !=  RclMonEvent::RCLEVT_NONE)
                queue->pushEvent(ev);
        }

        if(queue->getopt(RCLMON_NOORPHAN) && mon->isOrphaned()) {
            goto terminate;
        } 
    }
#endif // FSWATCH_FSEVENTS 

terminate:
    delete mon;
    queue->setTerminate();
    LOGINFO("rclMonRcvRun: monrcv thread routine returning\n");
    return nullptr;
}

// Utility routine used by both the fam/gamin and inotify versions to get 
// rid of the id-path translation for a moved dir
bool eraseWatchSubTree(map<int, string>& idtopath, const string& top)
{
    bool found = false;
    MONDEB("Clearing map for [" << top << "]\n");
    map<int,string>::iterator it = idtopath.begin();
    while (it != idtopath.end()) {
        if (it->second.find(top) == 0) {
            found = true;
            it = idtopath.erase(it);
        } else {
            it++;
        }
    }
    return found;
}


///////////////////////////////////////////////////////////////////////
// The monitor 'factory'
#include "rclmonrcv_fsevents.h"
#include "rclmonrcv_inotify.h"
#include "rclmonrcv_fam.h"
#include "rclmonrcv_win32.h"
static RclMonitor *makeMonitor()
{
#ifdef FSWATCH_WIN32
    return new RclMonitorWin32;
#endif
#ifdef FSWATCH_FSEVENTS
    return new RclFSEvents;
#endif
#ifdef FSWATCH_INOTIFY
    return new RclIntf;
#endif
#ifdef FSWATCH_FAM
    return new RclFAM;
#endif
    // This part of the code will never be reached. However, to be safe, we can keep it.
    LOGINFO("RclMonitor: none of the following, Inotify, Fam, fsevents was compiled as file system "
                "change notification interface\n");
    return nullptr;
}
///////////////////////////////////////////////////////////////////////

#endif // RCL_MONITOR
