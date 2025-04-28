/* Copyright (C) 2025 J.F.Dockes
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
#include "powerstatus.h"

#include <mutex>

PowerStatus *theInstance;

std::mutex theMutex;

static PowerStatus::powerstatus theStatus{PowerStatus::ONAC};

PowerStatus::PowerStatus()
{
}

PowerStatus *PowerStatus::instance()
{
    std::lock_guard<std::mutex> lock(theMutex);
    if (nullptr == theInstance) {
        theInstance = new PowerStatus;
    }
    return theInstance;
}

PowerStatus::powerstatus PowerStatus::set(PowerStatus::powerstatus n)
{
    std::lock_guard<std::mutex> lock(theMutex);
    auto o = theStatus;
    theStatus = n;
    return o;
}

static void systemgetpowerstatus();

PowerStatus::powerstatus PowerStatus::get()
{
    systemgetpowerstatus();
    return theStatus;
}

#if defined(_WIN32)
static void systemgetpowerstatus()
{
    // On Windows the status is obtained by a hidden window receiving a system event and calling
    // set(), nothing to do. See recollinit.cpp
}
#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

static bool isRunningOnBattery()
{
    // Get power source information
    CFTypeRef snapshot = IOPSCopyPowerSourcesInfo();
    if (!snapshot) {
        return false; // Assume AC power if unable to retrieve info
    }

    // Check power source state
    CFStringRef powerSourceState = IOPSGetProvidingPowerSourceType(snapshot);
    bool onBattery =
        CFStringCompare(powerSourceState, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo;

    // Clean up
    CFRelease(snapshot);

    return onBattery;
}

static void systemgetpowerstatus()
{
    theStatus = PowerStatus::ONAC;
    if (isRunningOnBattery()) {
        theStatus = PowerStatus::ONBATTERY;
    }
}

#else // Not Windows neither MacOS

#include <fcntl.h>
#include <unistd.h>
// The following is for Linux, will need updates for the BSDs
const static char *statusfile = "/sys/class/power_supply/AC/online";
static void systemgetpowerstatus()
{
    theStatus = PowerStatus::ONAC;
    int fd = -1;
    if (access(statusfile, R_OK) == 0) {
        fd = open(statusfile, 0);
        if (fd >= 0) {
            char buf[1];
            if (read(fd, buf, 1) == 1) {
                if (buf[0] == '0') {
                    theStatus = PowerStatus::ONBATTERY;
                }
            }
        }
    }
    if (fd >= 0)
        close(fd);
}
#endif
