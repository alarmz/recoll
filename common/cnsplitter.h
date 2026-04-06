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

#ifndef _CNSPLITTER_H_INCLUDED_
#define _CNSPLITTER_H_INCLUDED_

#include <string>
#include <memory>

#include "textsplit.h"

class CNSplitter : public ExtSplitter {
public:
    CNSplitter(TextSplit& sink);
    virtual ~CNSplitter();
    virtual bool text_to_words(Utf8Iter& it, unsigned int *cp, int& wordpos) override;
    class Internal;
private:
    std::unique_ptr<Internal> m;
};

class RclConfig;
void cnStaticConfInit(RclConfig *config, const std::string& tagger);


#endif /* _CNSPLITTER_H_INCLUDED_ */
