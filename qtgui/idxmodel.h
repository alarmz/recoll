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

#ifndef _IDXMODEL_H
#define _IDXMODEL_H

#include <vector>
#include <string>
#include <QStandardItemModel>

class IdxTreeModel : public QStandardItemModel {
    Q_OBJECT
public:
    IdxTreeModel(int depth, const std::vector<std::string>& edbs, QWidget *parent = nullptr)
        : QStandardItemModel(0, 0, (QObject*)parent), m_depth(depth), m_extradbs(edbs) {}
    ~IdxTreeModel() {}
    void populate();
    int getDepth() {return m_depth;}
    const std::vector<std::string> &getEDbs() {return m_extradbs;}
  private:
    int m_depth;
    std::vector<std::string> m_extradbs;
};

#endif // _IDXMODEL_H
