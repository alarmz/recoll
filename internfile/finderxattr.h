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
#ifndef _FINDERXATTR_H_INCLUDED_
#define _FINDERXATTR_H_INCLUDED_

#ifdef __APPLE__
#include <utility>
#include <string>
#include <vector>

// Decode a MacOS plist in the very specific case of the data found in a
// com.apple.metadata:kMDItemFinderComment extended attribute value. This contains a single string,
// either bytes or unicode (not sure about the encoding, probably ucs-2?).
// @return a pair of (value,error) strings. If error is not empty the method failed.
extern std::pair<std::string, std::string> decode_comment_plist(const unsigned char *cp0, int sz);

// Decode a MacOS plist in the very specific case of the data found in a
// com.apple.metadata:_kMDItemUserTags extended attribute value.
// This contains an array of bytes or unicode strings.
// Apparently the colors format is color\nNumvalue??
extern std::pair<std::vector<std::string>, std::string> decode_tags_plist(
    const unsigned char *cp0, int sz);
#endif // __APPLE__
#endif /* _FINDERXATTR_H_INCLUDED_ */
