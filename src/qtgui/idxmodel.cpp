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
#include <string>
#include <iostream>
#include <stack>

#include <QStandardItemModel>

#include "idxmodel.h"

#include "log.h"
#include "smallut.h"
#include "fstreewalk.h"
#include "recoll.h"

// Note: we originally used a file system tree walk to populate the tree. This was wrong
// because the file system may have changed since the index was created.
// We now build a directory tree directly from the index data, but still use the previous tree walk
// callback. In case you need an explanation of how we got here, look at the git history.
class WalkerCB : public FsTreeWalkerCB {
public:
    WalkerCB(const std::string& topstring, IdxTreeModel *model, const QModelIndex& index)
        : m_topstring(topstring), m_model(model) {
        LOGDEB1("WalkerCB: topstring [" << topstring << "]\n");
        m_indexes.push(index);
        m_rows.push(0);
    }
    virtual FsTreeWalker::Status processone(const std::string& path, FsTreeWalker::CbFlag flg,
                                            const struct PathStat& = PathStat()) override;

    std::string m_topstring;
    IdxTreeModel *m_model;
    std::stack<QModelIndex> m_indexes;
    std::stack<int> m_rows;
};

FsTreeWalker::Status WalkerCB::processone(
    const std::string& path, FsTreeWalker::CbFlag flg, const struct PathStat&)
{
    if (flg == FsTreeWalker::FtwDirReturn) {
        m_indexes.pop();
        m_rows.pop();
        return FsTreeWalker::FtwOk;
    }
    if (flg == FsTreeWalker::FtwDirEnter) {
        //std::cerr << "ENTER: " << path << "\n";
        if (m_model->columnCount(m_indexes.top()) == 0) {
            if (!m_model->insertColumn(0, m_indexes.top()))
                return FsTreeWalker::FtwError;
        }
        if (!m_model->insertRow(m_rows.top(), m_indexes.top()))
            return FsTreeWalker::FtwError;
        const QModelIndex child = m_model->index(m_rows.top(), 0, m_indexes.top());
        // Setting the short path in DisplayRole and the real one in EditRole does not seem to work,
        // the treeview shows the EditRole?? So use the ToolTip to store the full value
        std::string disp;
        if (m_topstring.empty()) {
            disp = path_getsimple(path);
        } else {
            disp = m_topstring;
            m_topstring.clear();
        }
        m_model->setData(child, QVariant(path2qs(disp)), Qt::DisplayRole);
        m_model->setData(child, QVariant(path2qs(path)), Qt::ToolTipRole);
        ++m_rows.top();
        m_indexes.push(child);
        m_rows.push(0);
    }
    return FsTreeWalker::FtwOk;
}

// Assemble a path from its components up to lst
std::string toksToPath(std::vector<std::string>& path, int lst)
{
    if (path.empty()) {
        // ??
#ifdef _WIN32
        return "C:/";
#else
        return "/";
#endif
    }
    std::string out{
#ifdef _WIN32
        path[0]
#else
        "/" + path[0]
#endif
    };
    for (int i = 1; i <= lst; i++) {
        out += "/" + path[i];
    }
    return out;
}

// Process a sorted list of directory paths, generating a sequence of enter/exit calls equivalent to
// what would happen for a recursive tree walk of the original tree.
static void treelist(const std::string& top, const std::vector<std::string>& lst, WalkerCB &cb)
{
    if (lst.empty()) {
        return;
    }
    std::vector<std::string> curpath;
    stringToTokens(top, curpath, "/");
    LOGDEB0("treelist: " << "top [" << top << "] TOP depth is " << curpath.size() << "\n");
    for (const auto& dir : lst) {
        LOGDEB1("DIR: " << dir << "\n");
        std::vector<std::string> npath;
        // Compute the new directory stack
        stringToTokens(dir, npath, "/");
        // Walk the stacks until we find a differing entry, and then unwind the old stack to the new
        // base, and issue enter calls for new entries over the base.
        int i = 0;
        for (; i < int(std::min(curpath.size(), npath.size())); i++) {
            if (npath[i] != curpath[i] && int(curpath.size()) > 0) {
                // Differing at i, unwind old stack and break the main loop
                for (int j = int(curpath.size()) - 1; j >= i; j--) {
                    LOGDEB1("treelist: exiting  " <<  toksToPath(curpath, j) << "\n");
                    cb.processone(toksToPath(curpath, j), FsTreeWalker::FtwDirReturn);
                }
                break;
            }
        }
        // Callbacks for new entries above the base.
        for (int j = i; j < int(npath.size()); j++) {
            LOGDEB1("treelist: entering " << toksToPath(npath, j) << "\n");
            cb.processone(toksToPath(npath, j), FsTreeWalker::FtwDirEnter);
        }
        curpath.swap(npath);
    }
}

void IdxTreeModel::populate()
{
    LOGDEB0("IdxTreeModel::populate\n");
    if (m_depth == 0)
        return;
    std::vector<std::string> thedirs;
    std::string prefix;
    rcldb->dirlist(m_depth, prefix, thedirs);
    LOGDEB1("IdxTreeModel::populate: prefix [" << prefix << "] thedirs: " <<
            stringsToString(thedirs) << "\n");

    QModelIndex index = this->index(0,0);
    if (this->columnCount(index) == 0) {
        if (!this->insertColumn(0, index))
            return;
    }
    const QModelIndex child = this->index(0, 0, index);
    WalkerCB cb(path_isroot(prefix) ? std::string() : prefix, this, child);
    if (!prefix.empty())
        prefix = path_getfather(prefix);
    treelist(prefix, thedirs, cb);
}
