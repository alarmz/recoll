/* Copyright (C) 2024 J.F. Dockes
 * Copyright (C) 2024 Alberti, Andrea
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
// rclmonrcv.h

#ifndef RCLMONRCV_H
#define RCLMONRCV_H

#ifdef RCL_MONITOR

// Set the FSWATCH to the chosen library.
#ifdef _WIN32
    #define FSWATCH_WIN32
    class RclMonitorWin32;
#else // ! _WIN32
    #ifdef RCL_USE_FSEVENTS // darwin (i.e., MACOS)
        #define FSWATCH_FSEVENTS
        class RclFSEvents;
    #else // ! RCL_USE_FSEVENTS
        // We dont compile both the inotify and the fam interface and inotify has preference
        #ifdef RCL_USE_INOTIFY
            #define FSWATCH_INOTIFY
            class RclIntf;
        #else // ! RCL_USE_INOTIFY
            #ifdef RCL_USE_FAM
                #define FSWATCH_FAM
                class RclFAM;
            #endif // RCL_USE_FAM
        #endif // INOTIFY
    #endif // RCL_USE_FSEVENTS

    #ifndef FSWATCH_WIN32
        #ifndef FSWATCH_FSEVENTS
            #ifndef FSWATCH_INOTIFY
                #ifndef FSWATCH_FAM
                    #error "Something went wrong. RCL_MONITOR is true, but no platform flag is defined."
                #endif // FSWATCH_FAM
            #endif // FSWATCH_INOTIFY
        #endif // FSWATCH_FSEVENTS
    #endif // FSWATCH_WIN32
#endif // _WIN32

#include <string>
#include <map>

#include "rclmon.h"
#include "fstreewalk.h"

/** Base class for the actual filesystem monitoring module.**/
class RclMonitor {
public:
    // The functions below must be defined in the class constructed from the template
    // or else an error at compile time will be generated. Unfortunately, C++ does not
    // provide a keyword as `abstract` to state that a function is pure and has to be provided
    // in the derived class.

    RclMonitor();
    virtual ~RclMonitor() {};

    virtual bool addWatch(const std::string& path, bool isDir, bool follow = false) = 0;
    virtual bool ok() const = 0;
    
#ifdef FSWATCH_FSEVENTS
    void virtual startMonitoring(RclMonEventQueue *queue, RclConfig& lconfig, FsTreeWalker& walker) = 0;
#else
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1) = 0;
#endif
    
    // Does this monitor generate 'exist' events at startup?
    virtual bool generatesExist() const { return false; }
    // Is fs watch mechanism recursive and only the root directory needs to be provided?
    virtual bool isRecursive() const { return false; }

    virtual bool isOrphaned();

    // Save significant errno after monitor calls
    int saved_errno{0};

private:
    // Store the pid of the parent process. It is used when the daemon is started with -O option
#ifndef _WIN32
    pid_t m_originalParentPid;
#endif
};

bool rclMonShouldSkip(const std::string& path, RclConfig& lconfig, FsTreeWalker& walker);
bool rclMonAddSubWatches(const std::string& path, FsTreeWalker& walker, RclConfig& lconfig,
            RclMonitor *mon, RclMonEventQueue *queue);
bool eraseWatchSubTree(std::map<int, std::string>& idtopath, const std::string& top);

#endif // RCL_MONITOR

#endif // RCLMONRCV_H
