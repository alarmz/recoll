/* Copyright (C) 2023 J.F.Dockes
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
#ifndef _CJKSPLITTER_H_INCLUDED_
#define _CJKSPLITTER_H_INCLUDED_

#include "textsplit.h"

class CJKSplitter : public ExtSplitter {
public:
    CJKSplitter(TextSplit& sink, int ngramlen)
        : ExtSplitter(sink), m_ngramlen(ngramlen) {}
    virtual ~CJKSplitter() = default;
    virtual bool text_to_words(Utf8Iter& it, unsigned int *cp, int& wordpos) override;
    static int max_ngramlen();
protected:
    int m_ngramlen;
};

#endif /* _CJKSPLITTER_H_INCLUDED_ */
