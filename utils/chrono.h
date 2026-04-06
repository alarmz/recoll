/* Copyright (C) 2016 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _CHRONO_H_
#define _CHRONO_H_
#include <chrono>
#include <cstdint>

/** Easy interface to measuring time intervals */
class Chrono {
public:
    /** Initialize, setting the origin time */
    Chrono();

    /** Re-store current time and return mS since init or last call */
    int64_t restart();
    /** Re-store current time and return uS since init or last call */
    int64_t urestart();

    /** Snapshot current time to static storage */
    static void refnow();

    /** Return interval value in various units.
     *
     * Frozen means give time since the last refnow call (this is to
     * allow for using one actual system call to get values from many
     * chrono objects, like when examining timeouts in a queue
     */
    int64_t millis(bool frozen = false);
    int64_t micros(bool frozen = false);
    int64_t nanos(bool frozen = false);
    float secs(bool frozen = false);

private:
    typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;
    TimePoint m_orig;
    static TimePoint o_now;
};

#endif /* _CHRONO_H_ */
