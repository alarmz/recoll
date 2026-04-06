/* Copyright (C) 2015 J.F.Dockes
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
#ifndef _PREVIEW_PLAINTORICH_H_INCLUDED_
#define _PREVIEW_PLAINTORICH_H_INCLUDED_
#include "autoconfig.h"

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <QThread>
#include <QStringList>

#include "plaintorich.h"

/** Preview text highlighter */
class PlainToRichQtPreview : public PlainToRich {
public:
    PlainToRichQtPreview();
    void clear();
    bool haveAnchors();
    virtual std::string header();
    virtual std::string startMatch(unsigned int grpidx);
    virtual std::string endMatch();
    virtual std::string termAnchorName(int i) const;
    virtual std::string startChunk();
    // Advance of rewind the current anchor. Returns the current anchor number inside the group
    // (which may be the total if we're walking the full list), as [1-N].
    int nextAnchorNum(int grpidx);
    int prevAnchorNum(int grpidx);
    int anchorCount(int grpidx);
    QString curAnchorName() const;

private:
    // Lists of anchor numbers (hit locations) for the term/groups in the query. The map key is an
    // index into the HighlightData.index_term_groups vector. Using a map and not a vector parallel
    // to the hldata one because the hits for this specific document will usually be a subset of
    // index_term_groups
    std::map<unsigned int, std::vector<int>> m_groupanchors;
    // Total number of anchors, all terms/groups counfounded. This is equal to the sum of sizes of
    // the above vectors, kept for convenience.
    int m_lastanchor;
    // Current anchor number when walking the hits with an empty search term. This goes from 0 to
    // m_lastanchor.
    int m_curanchor;
    // Walking the lists of matches inside a group: this stores the current index into the anchor
    // vector from m_groupanchors for the group of identical key.
    std::map<unsigned int, unsigned int> m_groupcuranchors;
    bool m_spacehack{false};
};

/* A thread to convert to rich text (mark search terms) */
class ToRichThread : public QThread {
    Q_OBJECT
    
public:
    ToRichThread(const std::string &i, const HighlightData& hd,
                 std::shared_ptr<PlainToRichQtPreview> ptr,
                 QStringList& qrichlst, // Output
                 QObject *parent = 0);
    virtual void run();

private:
    const std::string &m_input;
    const HighlightData &m_hdata;
    std::shared_ptr<PlainToRichQtPreview> m_ptr;
    QStringList &m_output;
};

#endif /* _PREVIEW_PLAINTORICH_H_INCLUDED_ */
