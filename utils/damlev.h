/* Copyright (C) 2022 J.F.Dockes
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

// Damerau-Levenshtein distance between two strings.
// Implements the algorithm from:
// https://en.wikipedia.org/wiki/Damerau–Levenshtein_distance
//    "Distance with adjacent transpositions"
//
// The function is usable with regular std::string or any class implementing operator[] and size(),
// such as some kind of int array to be used after a conversion to utf-32 (the algorithm will NOT
// work with UTF-8 because of the variable length multi-char8 characters).

#include <string>
#include <map>
#include <algorithm>
#include <iostream>

namespace MedocUtils {

// Two-dimensional array with configurable lower bounds
class Mat2 {
public:
    Mat2(int w, int h, int xs = 0, int ys = 0)
        : m_w(w), m_xs(xs), m_ys(ys) {
        ds = (int*)malloc(sizeof(int) * w * h);
    }
    ~Mat2() {
        if (ds) free(ds);
    }
    int& operator()(int x, int y) {
        return ds[(y - m_ys) * m_w + (x - m_xs)];
    }
private:
    int m_w, m_xs, m_ys;
    int *ds{nullptr};
};

// https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance#Algorithm
template<class T> int DLDistance(const T& str1, const T& str2)
{
    // This replaces an array of the size of the alphabet, initialized to 0.
    std::map<int, int> da;
    int size1 = static_cast<int>(str1.size());
    int size2 = static_cast<int>(str2.size());
    
    Mat2 d(size1 + 2, size2 + 2, -1, -1);
    int maxdist = size1 + size2;
    d(-1,-1) = maxdist;
    for (int x = 0; x <= size1; x++) {
        d(x, -1) = maxdist;
        d(x, 0) = x;
    }
    for (int y = 0; y <= size2; y++) {
        d(-1, y) = maxdist;
        d(0, y) = y;
    }
    // The strings in the algo are 1-indexed, so we adjust accessses (e.g. a[x-1])
    for (int x = 1; x <= size1; x++) {
        int db = 0;
        for (int y = 1; y <= size2; y++) {
            auto it = da.find(str2[y-1]);
            int k = it == da.end() ? 0 : da[str2[y-1]];
            int l = db;
            int cost;
            if (str1[x-1] == str2[y-1]) {
                cost = 0;
                db = y;
            } else {
                cost = 1;
            }
            d(x, y) =  std::min({d(x-1, y-1) + cost, d(x, y-1) + 1, d(x-1, y) + 1,
                    d(k-1, l-1) + (x-k-1) + 1 + (y-l-1)});
        }
        da[str1[x-1]] = x;
    }
    return d(size1, size2);
}

}

