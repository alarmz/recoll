#ifndef _POWERSTATUS_H_INCLUDED_
#define _POWERSTATUS_H_INCLUDED_
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

// Utility for checking power supply status for this system: battery or AC

class PowerStatus {
public:
    // Return singleton
    static PowerStatus *instance();
    enum powerstatus {ONAC, ONBATTERY};
    powerstatus get();

    // On some systems (Windows), the status is set by external events
    PowerStatus::powerstatus set(powerstatus status);
private:
    PowerStatus();
};


#endif /* _POWERSTATUS_H_INCLUDED_ */
