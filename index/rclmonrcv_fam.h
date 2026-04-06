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
#ifndef RCLMONRCV_FAM_CPP
#define RCLMONRCV_FAM_CPP

#include "rclmonrcv.h"

#ifdef FSWATCH_FAM
#include <map>
#include <string>

#include <fam.h>

/** FAM based monitor class. We have to keep a record of FAM watch
    request numbers to directory names as the event only contain the
    request number and file name, not the full path */
class RclFAM : public RclMonitor {
public:
    RclFAM();
    virtual ~RclFAM();
    virtual bool addWatch(const std::string& path, bool isdir, bool follow=false) override;
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1) override;
    virtual bool ok() const override {return m_ok;}
    virtual bool generatesExist() const override {return true;}

private:
    bool m_ok;
    FAMConnection m_conn;
    void close() {
        FAMClose(&m_conn);
        m_ok = false;
    }
    std::map<int, std::string> m_idtopath;
    const char *event_name(int code);
};

#endif // FSWATCH_FAM

#endif
