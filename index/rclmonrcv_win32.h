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
#ifndef RCLMONRCV_WIN32_H
#define RCLMONRCV_WIN32_H

#include "rclmonrcv.h"

#ifdef FSWATCH_WIN32

#include <string>
#include <queue>

typedef long WatchID;

// Virtual interface for the monitor callback. Note: this for compatibility with the efsw code, as
// rclmon uses a pull, not push interface. The callback pushes the events to a local queue from
// which they are then pulled by the upper level code.
enum class Action {Add = 1, Delete = 2, Modify = 3, Move = 4};
class FileWatchListener {
public:
    virtual ~FileWatchListener() {}
    virtual void handleFileAction(WatchID watchid, const std::string& dir, const std::string& fn,
                                  Action action, bool isdir, std::string oldfn = "" ) = 0;
};

class RclFSWatchWin32;

// Adapter for the rclmon interface
class RclMonitorWin32 : public RclMonitor, public FileWatchListener {
public:
    RclMonitorWin32();
    virtual ~RclMonitorWin32();
    virtual bool addWatch(const std::string& path, bool /*isDir*/, bool /*follow*/) override;
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1) override;
    virtual bool ok() const override;
    // Does this monitor generate 'exist' events at startup?
    virtual bool generatesExist() const override;
    // Can the caller avoid setting watches on subdirs ?
    virtual bool isRecursive() const override;

    void handleFileAction(WatchID /*watchid*/, const std::string& dir, const std::string& fn,
                                  Action action, bool isdir, std::string oldfn = "");

    // Save significant errno after monitor calls
    int saved_errno{0};

private:
    std::queue<RclMonEvent> m_events;
    RclFSWatchWin32 *m_fswatcher;
};

#endif // FSWATCH_WIN32

#endif // RCLMONRCV_WIN32_H
