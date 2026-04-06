/* Copyright (C) 2004-2013 J.F.Dockes
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
#ifndef _DOCSEQDOCS_H_INCLUDED_
#define _DOCSEQDOCS_H_INCLUDED_

#include <memory>

#include "docseq.h"
#include "rcldoc.h"

namespace Rcl {
class Db;
}

/** A DocSequence that's just built from a bunch of docs */
class DocSequenceDocs : public DocSequence {
public:
    DocSequenceDocs(std::shared_ptr<Rcl::Db> d,
                    const std::vector<Rcl::Doc> docs, const std::string &t) 
        : DocSequence(t), m_db(d), m_docs(docs) {}
    virtual ~DocSequenceDocs() {}
    DocSequenceDocs(const DocSequenceDocs&) = delete;
    DocSequenceDocs& operator=(const DocSequenceDocs&) = delete;
    virtual bool getDoc(int num, Rcl::Doc &doc, std::string *sh = 0) override {
        if (sh)
            *sh = std::string();
        if (num < 0 || num >= int(m_docs.size()))
            return false;
        doc = m_docs[num];
        return true;
    }
    virtual int getResCnt() override {
        return static_cast<int>(m_docs.size());
    }
    virtual std::string getDescription() override {
        return m_description;
    }
    void setDescription(const std::string& desc) {
        m_description = desc;
    }
protected:
    virtual std::shared_ptr<Rcl::Db> getDb() override {
        return m_db;
    }
private:
    std::shared_ptr<Rcl::Db> m_db;
    std::string      m_description;
    std::vector<Rcl::Doc> m_docs;
};

#endif /* _DOCSEQ_H_INCLUDED_ */
