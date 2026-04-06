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
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <tuple>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

inline uint32_t char2toint(const unsigned char *f)
{
    return (
               (uint32_t)(f[1]) +
               ((uint32_t)(f[0]) << 8));
}
static inline uint32_t char4toint(const unsigned char *f)
{
    return (
               ((uint32_t)(f[3])) +
               ((uint32_t)(f[2]) << 8) +
               ((uint32_t)(f[1]) << 16) +
               ((uint32_t)(f[0]) << 24));
}
inline uint64_t char8toint(const unsigned char *f)
{
    return (
               ((uint64_t)(f[7])      ) +
               ((uint64_t)(f[6]) << 8 ) +
               ((uint64_t)(f[5]) << 16) +
               ((uint64_t)(f[4]) << 24) +
               ((uint64_t)(f[3]) << 32) +
               ((uint64_t)(f[2]) << 40) +
               ((uint64_t)(f[1]) << 48) +
               ((uint64_t)(f[0]) << 56)
        );
}

struct plist_trailer {
    // Size of offsets in the offsets table. These point to a marker in the object table as an
    // offset from the beginning of the plist
    unsigned int offset_table_offset_size;
    // Size of offsets into the offsets table
    unsigned int object_ref_size;
    // Total count of objects
    uint64_t num_objects;
    // Offset from the start of the offset table where we find the offset of the first object table
    // marker. Usage? Usually 0.
    uint64_t top_object_offset;
    // Start of offset table, from start of file
    uint64_t offset_table_start;
    void dump(std::ostream& s) {
        s << "offset_table_offset_size " << offset_table_offset_size <<
            " object_ref_size " << object_ref_size <<
            " num_objects " << num_objects <<
            " top_object_offset " << top_object_offset <<
            " offset_table_start " << offset_table_start;
    }
};

std::pair<plist_trailer, bool> decode_trailer(const unsigned char *plist, size_t sz)
{
    plist_trailer t;
    if (sz < 32) {
        //std::cerr << "Size " << sz << " too small\n";
        return {t,false};
    }
    const unsigned char *cp = plist + sz - 32;
    // 0 to 4 are unused. Byte 5: sort version ??
    cp += 6;
    t.offset_table_offset_size = *cp++;
    t.object_ref_size = *cp++;
    t.num_objects = char8toint(cp); cp += 8;
    t.top_object_offset = char8toint(cp); cp += 8;
    t.offset_table_start = char8toint(cp); cp += 8;
    return {t,true};
}

/**
 * Decode a marker.
 * 
 * @param cp0 the plist data base. Used for size overflow computations only.
 * @param [inout] cp the current pointer into the plist data. Updated to point after the marker.
 * @param sz the plist data size.
 * @return A tuple with the marker type, the item count, and a std::string message which 
 *   indicates an error if not empty.
 */
std::tuple<unsigned int, unsigned int, std::string> decode_marker(
    const unsigned char* cp0, const unsigned char*& cp, int sz)
{
    auto format = ((*cp) & 0xf0) >> 4;
    auto numitems = (*cp) & 0x0f;
    // std::cerr << "format is " << format << " numitems " << numitems << "\n";
    if (numitems == 15) {
        // Then the actual length is in the following bytes.
        cp++;
        if (cp - cp0 > sz) {
            return {format, numitems, "Input size too small"};
        }
            
        // std::cerr << "*cp now " << std::hex << (unsigned int)*cp << std::dec << "\n";
        if (((*cp) & 0xf0)>>4 != 1) {
            return {format, numitems, "Int len high nibble not 1"};
        }
        auto pow = (*cp) & 0x0f;
        auto ilen = 1 << pow;
        // std::cerr << "ilen is " << ilen << "\n";
        // The next ilen bytes are a big endian integer.
        // TBD: what ilen values are possible ? For now assume any value <= 4. Could be expanded
        // and simplified if it was a (1,2,4,8) set for example (use the charXtoint routines).
        if (ilen > 4) {
            return {format, numitems, "Bad integer size " + std::to_string(ilen)};
        }
        cp++;
        if (cp + ilen - cp0 > sz) {
            return {format, numitems, "input size too small"};
        }
        unsigned char ar[4] = {0,0,0,0};
        // Fill the array down from lsb (byte 3) down to msb depending on ilen
        for (int i = ilen-1; i >= 0; i--) {
            ar[i + (4-ilen)] = cp[i];
        }
        numitems = char4toint(ar);
        cp += ilen;
    } else {
        cp++;
        if (cp - cp0 > sz) {
            return {format, numitems, "input size too small"};
        }
    }
    return {format, numitems, ""};
}

// Read an expected string object, pointed by its marker. Returns an error if the marker is not for
// a string.
std::pair<std::string, std::string> readstring(
    const unsigned char *cp0, const unsigned char *cp, int sz)
{
    auto [format, numchars, error] = decode_marker(cp0, cp, sz);
    if (!error.empty()) {
        return {"", error};
    }
    bool unicode;
    if (format == 5) {
        unicode = false;
    } else if (format == 6) {
        unicode = true;
    } else {
        return {"", "format " + std::to_string(format) + " neither bytes nor unicode."};
    }
    auto bytelen = unicode ? numchars * 2 : numchars;
    if (cp + bytelen - cp0 > sz) {
        return {"", "input size too small"};
    }
    
    // std::cerr << "numchars " << numchars << " numbytes " << bytelen << "\n";
    return {std::string((const char *)cp, bytelen), ""};
}


// Read an expected vector of strings, pointed to by its marker. (e.g. Finder tags xattr). 
std::pair<std::vector<std::string>, std::string> read_string_array(
    const unsigned char *cp0, const unsigned char *cp, int sz, const plist_trailer& trailer)
{
    std::vector<std::string> out;
    auto [format, numitems, error] = decode_marker(cp0, cp, sz);
    if (format != 10) {
        return {out, "format " + std::to_string(format) + " is not 10"};
    }
    //std::cerr << "Got " << numitems << " entries\n";

    // TBD: Change the following to a single loop: no need to store the object_refs
    out.reserve(numitems);
    int offsidx;
    for (unsigned int i = 0; i < numitems; i++) {
        switch(trailer.object_ref_size) {
        case 1: offsidx = *cp++;                  break;
        case 2: offsidx = char2toint(cp);cp += 2; break;
        case 4: offsidx = char4toint(cp);cp += 4; break;
        case 8: offsidx = char8toint(cp);cp += 8; break;
        default: return {out,
            std::string("bad object ref size ") + std::to_string(trailer.object_ref_size)};
        }
        //std::cerr << "Offset table index: " << offsidx << "\n";
        uint64_t offset;
        const unsigned char *ref = cp0 + trailer.offset_table_start +
            offsidx * trailer.offset_table_offset_size;
        switch(trailer.offset_table_offset_size) {
        case 1: offset = *ref;            break;
        case 2: offset = char2toint(ref); break;
        case 4: offset = char4toint(ref); break;
        case 8: offset = char8toint(ref); break;
        default: return {out, std::string("bad offset table offset size ") +
            std::to_string(trailer.offset_table_offset_size)};
        }
        //std::cerr << "Offset: " << offset << " ";
        auto [str, error] = readstring(cp0, cp0 + offset, sz);
        if (!error.empty()) {
            return {out, error};
        }
        out.push_back(str);
        //std::cerr << "string: [" << str << "]\n";
    }
    return {out, ""};
}

// Decode a MacOS plist in the very specific case of the data found in a
// com.apple.metadata:_kMDItemUserTags extended attribute value.
// This contains an array of bytes or unicode strings.
// Apparently the colors format is color\nNumvalue??
std::pair<std::vector<std::string>, std::string> decode_tags_plist(
    const unsigned char *cp0, int sz)
{
    if (memcmp(cp0, "bplist00", 8) != 0) {
        return {{}, "not bplist00"};
    }
    // Check the plist trailer.
    auto [trailer,status] = decode_trailer(cp0, sz);
    if (!status) {
        return {{}, "Trailer decode failed (data too small ?)"};
    }

    // trailer.dump(std::cerr); std::cerr <<"\n";
    const unsigned char *cp = cp0+8;
    return read_string_array(cp0, cp, sz, trailer);
}

// Decode a MacOS plist in the very specific case of the data found in a
// com.apple.metadata:kMDItemFinderComment extended attribute value. This contains a single string,
// either bytes or unicode (not sure about the encoding, probably ucs-2?).
std::pair<std::string, std::string> decode_comment_plist(const unsigned char *cp0, int sz)
{
    if (memcmp(cp0, "bplist00", 8) != 0) {
        return {"", "not bplist00"};
    }
    // Check the plist trailer.
    auto [trailer,status] = decode_trailer(cp0, sz);
    if (!status) {
        return {"", "Trailer decode failed (data too small ?)"};
    }

    // trailer.dump(std::cerr); std::cerr <<"\n";
    const unsigned char *cp = cp0+8;
    return readstring(cp0, cp, sz);
}
