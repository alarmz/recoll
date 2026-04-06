/* Copyright (C) 2024 Alberti, Andrea
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
#ifndef RCLMONRCV_FSEVENTS_H
#define RCLMONRCV_FSEVENTS_H

#include "rclmonrcv.h"

#ifdef FSWATCH_FSEVENTS

#include <CoreServices/CoreServices.h>
#include <iostream>
#include <map>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

class RclFSEvents : public RclMonitor {
public:
    RclFSEvents();
    virtual ~RclFSEvents() override;

    static void fsevents_callback(
        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]);

    bool virtual ok() const override;
#ifdef FSWATCH_FSEVENTS
    void virtual startMonitoring(RclMonEventQueue *queue, RclConfig& lconfig, FsTreeWalker& walker) override;
#endif
    void virtual setupAndStartStream();
    bool virtual addWatch(const std::string& path, bool isDir, bool follow = false) override;

    bool virtual isRecursive() const override;

public:
    RclMonEventQueue *m_queue{NULL};

private:
    int eraseWatchSubTree(CFStringRef topDirectory);
    static void signalHandler(int signum);
    void releaseFSEventStream();
    RclConfig* m_lconfigPtr;
    FsTreeWalker* m_walkerPtr;
    bool m_ok;
    FSEventStreamRef m_stream;
    std::string m_rootPath;    
};

// Custom context for the idle loop
typedef struct  {
    bool shouldExit;
    RclFSEvents *monitor;
} IdleLoopContext;

#endif // FSWATCH_FSEVENTS

#endif // RCLMONRCV_FSEVENTS_H
