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
/* The code for the Win32 version of the monitor was largely copied from efsw:
 * https://github.com/SpartanJ/efsw
 * LICENSE for the original WIN32 code:
 * Copyright (c) 2020 Martin Lucas Golini
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This software is a fork of the "simplefilewatcher" by James Wynn (james@jameswynn.com)
 * http://code.google.com/p/simplefilewatcher/ also MIT licensed.
 */


#include "autoconfig.h"
#include "rclmonrcv.h"

#ifdef FSWATCH_WIN32
/*
 * WIN32 VERSION NOTES: 
 *
 * - When using non-recursive watches (one per dir), it appeared that
 *   watching a subdirectory of a given directory prevented renaming
 *   the top directory, Windows says: can't rename because open or a
 *   file in it is open. This is mostly why we use recursive watches 
 *   on the topdirs only.
 */

#include "rclmonrcv_win32.h"

#include <string>
#include <vector>
#include <thread>
#include <queue>
#include <errno.h>
#include <cstdio>
#include <cstring>

#include "safeunistd.h"
#include "log.h"
#include "rclmon.h"
#include "pathut.h"
#include "smallut.h"


#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

class WatcherWin32;
class RclFSWatchWin32;


// Internal watch data. This piggy-back our actual data pointer to the MS overlapped pointer. This
// is a bit of a hack, and we could probably use event Ids instead.
struct WatcherStructWin32
{
    OVERLAPPED Overlapped;
    WatcherWin32 *Watch;
};

// Actual data structure for one directory watch
class WatcherWin32 {
public:
    WatchID ID;
    FileWatchListener *Listener{nullptr};
    bool Recursive;
    std::string DirName;
    std::string OldFileName;

    HANDLE DirHandle{nullptr};
    // do NOT make this bigger than 64K because it will fail if the folder being watched is on the
    // network! (see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365465(v=vs.85).aspx)
    BYTE Buffer[8 * 1024];
    DWORD NotifyFilter{0};
    bool StopNow{false};
    RclFSWatchWin32 *Watch{nullptr};
};

// The efsw top level file system watcher: manages all the directory watches.
class RclFSWatchWin32 {
public:
    RclFSWatchWin32();

    virtual ~RclFSWatchWin32();

    // Add a directory watch
    // On error returns -1
    WatchID addWatch(const std::string& directory, FileWatchListener *watcher, bool recursive);

    // 2nd stage of action processing (after the static handler which just reads the data)
    void handleAction(WatcherWin32 *watch, const std::string& fn, unsigned long action);

    bool ok() const {
        return mInitOK;
    }

    // Fetch events, with msecs timeout if there are no more
    void run(DWORD msecs);

private:
    HANDLE mIOCP;
    // Using a vector because we don't remove watches. Change to list if needed.
    std::vector<WatcherStructWin32*> mWatches;
    bool mInitOK{false};
    WatchID mLastWatchID{0};

    std::mutex mWatchesLock;

    bool pathInWatches(const std::string& path);
    /// Remove all directory watches.
    void removeAllWatches();
};

RclMonitorWin32::RclMonitorWin32()
    : m_fswatcher(new RclFSWatchWin32)
{
}

RclMonitorWin32::~RclMonitorWin32()
{
    delete m_fswatcher;
}

bool RclMonitorWin32::addWatch(const std::string& path, bool /*isDir*/, bool /*follow*/)
{
    MONDEB("RclMonitorWin32::addWatch: " << path << "\n");
    return m_fswatcher->addWatch(path, this, true) != -1;
}

bool RclMonitorWin32::getEvent(RclMonEvent& ev, int msecs)
{
    PRETEND_USE(msecs);
    if (!m_events.empty()) {
        ev = m_events.front();
        m_events.pop();
        return true;
    }
    m_fswatcher->run(msecs);
    if (!m_events.empty()) {
        ev = m_events.front();
        m_events.pop();
        return true;
    }
    return false;
}

bool RclMonitorWin32::ok() const {
    return m_fswatcher->ok();
}

bool RclMonitorWin32::generatesExist() const {
    return false;
}

bool RclMonitorWin32::isRecursive() const {
    return true;
}

void RclMonitorWin32::handleFileAction(WatchID /*watchid*/, const std::string& dir, const std::string& fn,
                                  Action action, bool isdir, std::string oldfn) {
        MONDEB("RclMonitorWin32::handleFileAction: dir [" << dir << "] fn [" << fn << "] act " <<
               int(action) << " isdir " << isdir << " oldfn [" << oldfn << "]\n");
        RclMonEvent event;
        switch (action) {
        case Action::Move:
        case Action::Add: event.m_etyp = isdir ?
                RclMonEvent::RCLEVT_DIRCREATE : RclMonEvent::RCLEVT_MODIFY; break;
        case Action::Delete:
            event.m_etyp = RclMonEvent::RCLEVT_DELETE;
            if (isdir) {
                event.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
            }
            break;
        case Action::Modify: event.m_etyp = RclMonEvent::RCLEVT_MODIFY; break;
        }
        event.m_path = path_cat(dir, fn);
        m_events.push(event);
    }

/// Stops monitoring a directory.
void DestroyWatch(WatcherStructWin32 *pWatch)
{
    if (pWatch) {
        WatcherWin32 *ww32 = pWatch->Watch;
        ww32->StopNow = true;
        CancelIoEx(ww32->DirHandle, &pWatch->Overlapped);
        CloseHandle(ww32->DirHandle);
        delete ww32;
        // Shouldn't we call heapfree on the parameter here ??
    }
}

/// Refreshes the directory monitoring.
bool RefreshWatch(WatcherStructWin32 *pWatch)
{
    WatcherWin32 *ww32 = pWatch->Watch;
    return ReadDirectoryChangesW(
        ww32->DirHandle,
        ww32->Buffer,
        sizeof(ww32->Buffer),
        ww32->Recursive,
        ww32->NotifyFilter,
        NULL,
        &pWatch->Overlapped,
        NULL
        ) != 0;
}

/// Starts monitoring a directory.
WatcherStructWin32 *CreateWatch(LPCWSTR szDirectory, bool recursive, DWORD NotifyFilter, HANDLE iocp)
{
    WatcherStructWin32 *wsw32;
    size_t ptrsize = sizeof(*wsw32);
    wsw32 =static_cast<WatcherStructWin32*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptrsize));

    WatcherWin32 *ww32 = new WatcherWin32();
    wsw32->Watch = ww32;

    ww32->DirHandle = CreateFileW(
        szDirectory,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (ww32->DirHandle != INVALID_HANDLE_VALUE &&
        CreateIoCompletionPort(ww32->DirHandle, iocp, 0, 1)) {
        ww32->NotifyFilter = NotifyFilter;
        ww32->Recursive = recursive;

        if (RefreshWatch(wsw32)) {
            return wsw32;
        }
    }

    CloseHandle(ww32->DirHandle);
    delete ww32;
    HeapFree(GetProcessHeap(), 0, wsw32);
    return NULL;
}


RclFSWatchWin32::RclFSWatchWin32()
    : mLastWatchID(0)
{
    mIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (mIOCP && mIOCP != INVALID_HANDLE_VALUE)
        mInitOK = true;
}

RclFSWatchWin32::~RclFSWatchWin32()
{
    mInitOK = false;

    if (mIOCP && mIOCP != INVALID_HANDLE_VALUE) {
        PostQueuedCompletionStatus(mIOCP, 0, reinterpret_cast<ULONG_PTR>(this), NULL);
    }

    removeAllWatches();

    CloseHandle(mIOCP);
}

WatchID RclFSWatchWin32::addWatch(const std::string& _dir,FileWatchListener *watcher,bool recursive)
{
    LOGDEB("RclFSWatchWin32::addWatch: " << _dir << " recursive " << recursive << "\n");
    std::string dir(_dir);
    path_slashize(dir);
    if (!path_isdir(dir)) {
        LOGDEB("RclFSWatchWin32::addWatch: not a directory: " << dir << "\n");
        return 0;
    }
    if (!path_readable(dir)) {
        LOGINF("RclFSWatchWin32::addWatch: not readable: " << dir << "\n");
        return 0;
    }
    path_catslash(dir);
    auto wdir = utf8towchar(dir);

    std::unique_lock<std::mutex> lock(mWatchesLock);

    if (pathInWatches(dir)) {
        MONDEB("RclFSWatchWin32::addWatch: already in watches: " << dir << "\n");
        return 0;
    }

    WatchID watchid = ++mLastWatchID;

    WatcherStructWin32 *watch = CreateWatch(
        wdir.get(), recursive,
        FILE_NOTIFY_CHANGE_CREATION |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_SIZE,
        mIOCP
        );

    if (nullptr == watch) {
        LOGINF("RclFSWatchWin32::addWatch: CreateWatch failed\n");
        return -1;
    }

    // Add the handle to the handles vector
    watch->Watch->ID = watchid;
    watch->Watch->Watch = this;
    watch->Watch->Listener = watcher;
    watch->Watch->DirName = dir;

    mWatches.push_back(watch);

    return watchid;
}

void RclFSWatchWin32::removeAllWatches()
{
    std::unique_lock<std::mutex> lock(mWatchesLock);
    for( auto& watchp : mWatches) {
        DestroyWatch(watchp);
    }
    mWatches.clear();
}

// Unpacks events and passes them to the event processor
void CALLBACK WatchCallback(DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    if (dwNumberOfBytesTransfered == 0 || NULL == lpOverlapped) {
        return;
    }

    WatcherStructWin32 *wsw32 = (WatcherStructWin32*)lpOverlapped;
    WatcherWin32 *ww32 = wsw32->Watch;

    PFILE_NOTIFY_INFORMATION pNotify;
    size_t offset = 0;
    do {
        pNotify = (PFILE_NOTIFY_INFORMATION) &ww32->Buffer[offset];
        offset += pNotify->NextEntryOffset;

        std::string sfn;
        wchartoutf8(pNotify->FileName, sfn, pNotify->FileNameLength / sizeof(WCHAR));
        ww32->Watch->handleAction(ww32, sfn, pNotify->Action);
    } while (pNotify->NextEntryOffset != 0);

    if (!ww32->StopNow) {
        RefreshWatch(wsw32);
    }
}

void RclFSWatchWin32::run(DWORD msecs)
{
    if (!mWatches.empty()) {
        DWORD numOfBytes = 0;
        OVERLAPPED* ov = NULL;
        ULONG_PTR compKey = 0;
        BOOL res = FALSE;
        DWORD ms = msecs == -1 ? INFINITE : msecs;
        while ((res = GetQueuedCompletionStatus(mIOCP, &numOfBytes, &compKey, &ov, ms))) {
            if (compKey != 0 && compKey == reinterpret_cast<ULONG_PTR>(this)) {
                // Called from ~RclFSWatchWin32. Must exit.
                MONDEB("RclFSWatchWin32::run: queuedcompletion said need exit\n");
                return;
            } else {
                std::unique_lock<std::mutex> lock(mWatchesLock);
                WatchCallback(numOfBytes, ov);
            }
        }
    } else {
        // No watches yet.
        MONDEB("RclFSWatchWin32::run: no watches yet\n");
        DWORD ms = msecs == -1 ? 1000 : msecs;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

void RclFSWatchWin32::handleAction(WatcherWin32 *watch, const std::string& _fn, unsigned long action)
{
    std::string fn(_fn);
    Action fwAction;
    path_slashize(fn);
    MONDEB("handleAction: fn [" << fn << "] action " << action << "\n");

    // In case fn is not a simple name but a relative path (probably
    // possible/common if recursive is set ?), sort out the directory
    // path and simple file name.
    std::string newpath = path_cat(watch->DirName, fn);
    bool isdir = path_isdir(newpath);
    std::string simplefn = path_getsimple(newpath);
    std::string folderPath = path_getfather(newpath);

    switch (action) {
    case FILE_ACTION_RENAMED_OLD_NAME:
        watch->OldFileName = fn;
        /* FALLTHROUGH */
    case FILE_ACTION_REMOVED:
        fwAction = Action::Delete;
        // The system does not tell us if this was a directory, but we
        // need the info. Check if it was in the watches.
        // TBD: for a delete, we should delete all watches on the subtree !
        path_catslash(newpath);
        for (auto& watchp : mWatches) {
            if (watchp->Watch->DirName == newpath) {
                isdir = true;
                break;
            }
        }
        break;
    case FILE_ACTION_ADDED:
        fwAction = Action::Add;
        break;
    case FILE_ACTION_MODIFIED:
        fwAction = Action::Modify;
        break;
    case FILE_ACTION_RENAMED_NEW_NAME: {
        fwAction = Action::Move;

        // If this is a directory, possibly update the watches.  TBD: this seems wrong because we
        // should process the whole subtree ? Also probably not needed at all because we are
        // recursive and only set watches on the top directories.
        if (isdir) {
            // Update the new directory path
            std::string oldpath = path_cat(watch->DirName, watch->OldFileName);
            path_catslash(oldpath);
            for (auto& watchp : mWatches) {
                if (watchp->Watch->DirName == oldpath) {
                    watchp->Watch->DirName = newpath;
                    break;
                }
            }
        }

        std::string oldFolderPath = watch->DirName +
            watch->OldFileName.substr(0, watch->OldFileName.find_last_of("/\\"));

        if (folderPath == oldFolderPath) {
            watch->Listener->handleFileAction(watch->ID, folderPath, simplefn, fwAction, isdir,
                                              path_getsimple(watch->OldFileName));
        } else {
            // Calling the client with non-simple paths??
            watch->Listener->handleFileAction(watch->ID, watch->DirName, fn, fwAction, isdir,
                                              watch->OldFileName);
        }
        return;
    }
    default:
        return;
    };

    watch->Listener->handleFileAction(watch->ID, folderPath, simplefn, fwAction, isdir);
}

bool RclFSWatchWin32::pathInWatches(const std::string& path)
{
    for (const auto& wsw32 : mWatches) {
        if (wsw32->Watch->DirName == path ) {
            return true;
        }
    }
    return false;
}

#endif // FSWATCH_WIN32
