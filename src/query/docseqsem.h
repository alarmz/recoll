/* Copyright (C) 2004-2021 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _DOCSEQSEM_H_INCLUDED_
#define _DOCSEQSEM_H_INCLUDED_

#include "autoconfig.h"
#ifdef ENABLE_SEMANTIC

#include <memory>

#include "docseq.h"

/** A DocSequence from a semantic query */
class DocSequenceSem : public DocSequence {
public:
    DocSequenceSem(const std::string &t, std::shared_ptr<Rcl::Db> db, const std::string &query);
    virtual ~DocSequenceSem();

    DocSequenceSem(const DocSequenceSem&) = delete;
    DocSequenceSem& operator=(const DocSequenceSem&) = delete;

    virtual bool getDoc(int num, Rcl::Doc &doc, std::string * = nullptr) override;
    virtual int getResCnt() override;
    virtual bool getAbstract(Rcl::Doc &doc, PlainToRich *ptr, std::vector<std::string>&,
                             bool forcesnips) override;
    virtual std::string getDescription() override {return "";}
protected:
    virtual std::shared_ptr<Rcl::Db> getDb() override;
    class Internal;
private:
    Internal *m{nullptr};
};
#endif /* ENABLE_SEMANTIC */
#endif /* _DOCSEQSEM_H_INCLUDED_ */
