/* Copyright (C) 2014 J.F.Dockes
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

// Measure and display time intervals.

#include "chrono.h"

#include <chrono>

using namespace std;

Chrono::TimePoint Chrono::o_now;

void Chrono::refnow()
{
    o_now = chrono::steady_clock::now();
}

Chrono::Chrono()
    : m_orig(chrono::steady_clock::now())
{
}

int64_t Chrono::restart()
{
    auto nnow = chrono::steady_clock::now();
    auto ms =
        chrono::duration_cast<chrono::milliseconds>(nnow - m_orig);
    m_orig = nnow;
    return ms.count();
}

int64_t Chrono::urestart()
{
    auto nnow = chrono::steady_clock::now();
    auto ms =
        chrono::duration_cast<chrono::microseconds>(nnow - m_orig);
    m_orig = nnow;
    return ms.count();
}

int64_t Chrono::millis(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::milliseconds>
            (o_now - m_orig).count();
    } else {
        return chrono::duration_cast<chrono::milliseconds>
            (chrono::steady_clock::now() - m_orig).count();
    }
}

int64_t Chrono::micros(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::microseconds>
            (o_now - m_orig).count();
    } else {
        return chrono::duration_cast<chrono::microseconds>
            (chrono::steady_clock::now() - m_orig).count();
    }
}

int64_t Chrono::nanos(bool frozen)
{
    if (frozen) {
        return chrono::duration_cast<chrono::nanoseconds>(o_now - m_orig).count();
    } else {
        return chrono::duration_cast<chrono::nanoseconds>
            (chrono::steady_clock::now() - m_orig).count();
    }
}

float Chrono::secs(bool frozen)
{
    if (frozen) {
        return static_cast<float>(chrono::duration_cast<chrono::seconds>(o_now - m_orig).count());
    } else {
        return static_cast<float>((chrono::duration_cast<chrono::seconds>
                                   (chrono::steady_clock::now() - m_orig)).count());
    }
}
