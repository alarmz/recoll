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

#ifndef RCLMONRCV_INOTIFY_H
#define RCLMONRCV_INOTIFY_H

#include "rclmonrcv.h"

#include <map>
#include <string>

#ifdef FSWATCH_INOTIFY

#define EVBUFSIZE (32*1024)

class RclIntf : public RclMonitor {
public:
    RclIntf();
    virtual ~RclIntf() override;

    bool virtual addWatch(const std::string& path, bool isdir, bool follow=false) override;
    bool virtual getEvent(RclMonEvent& ev, int msecs = -1) override;
    bool virtual ok() const override;

private:
    bool m_ok;
    int m_fd;
    std::map<int, std::string> m_idtopath; // Watch descriptor to name
    char m_evbuf[EVBUFSIZE]; // Event buffer
    char *m_evp; // Pointer to next event or 0
    char *m_ep;  // Pointer to end of events
    const char *event_name(int code);
    void close();
};

#endif // FSWATCH_INOTIFY

#endif // RCLMONRCV_INOTIFY_H
