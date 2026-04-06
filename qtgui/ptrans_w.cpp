/* Copyright (C) 2006-2022 J.F.Dockes 
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

#include <vector>
#include <utility>
#include <string>

#include <QMessageBox>

#include "recoll.h"
#include "guiutils.h"
#include "conftree.h"

#include "ptrans_w.h"

void PTransEdit::init(const std::string& dbdir)
{
    auto initdbdir = path_canon(dbdir);
    connect(transTW, SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
            this, SLOT(onItemDoubleClicked(QTableWidgetItem *)));
    connect(cancelPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(savePB, SIGNAL(clicked()), this, SLOT(savePB_clicked()));
    connect(addPB, SIGNAL(clicked()), this, SLOT(addPB_clicked()));
    connect(delPB, SIGNAL(clicked()), this, SLOT(delPB_clicked()));
    connect(transTW, SIGNAL(itemSelectionChanged()), this, SLOT(transTW_itemSelectionChanged()));
    connect(whatIdxCMB, SIGNAL(currentTextChanged(const QString&)),
            this, SLOT(curIdxChanged(const QString&)));

    QStringList labels(tr("Path in index"));
    labels.push_back(tr("Translated path"));
    transTW->setHorizontalHeaderLabels(labels);

    resize(QSize(640, 300).expandedTo(minimumSizeHint()));

    // Initialize the combobox with the list of extra indexes
    auto dbsorted = prefs.allExtraDbs;
    dbsorted.push_back(theconfig->getDbDir());
    for (auto& dir: dbsorted) {
        dir = path_canon(dir);
    }
    std::sort(dbsorted.begin(), dbsorted.end(),
              [] (const std::string& l, const std::string& r) {return r < l;});
    for (const auto& dbdir : dbsorted) {
        whatIdxCMB->addItem(path2qs(dbdir));
    }
    if (!initdbdir.empty()) {
        whatIdxCMB->setCurrentText(path2qs(initdbdir));
    }
    curIdxChanged(whatIdxCMB->currentText());
}

void PTransEdit::curIdxChanged(const QString& qidx)
{
    auto dbdir = qs2path(qidx);
    setCurrentDb(dbdir);
}

void PTransEdit::setCurrentDb(const std::string& dbdir)
{
    ConfSimple *conftrans = theconfig->getPTrans();
    if (!conftrans)
        return;
    whatIdxCMB->setCurrentText(path2qs(dbdir));

    int row = 0;
    auto opaths = conftrans->getNames(dbdir);
    for (const auto& opath : opaths) {
        transTW->setRowCount(row+1);
        transTW->setItem(row, 0, new QTableWidgetItem(path2qs(opath)));
        std::string npath;
        conftrans->get(opath, npath, dbdir);
        transTW->setItem(row, 1, new QTableWidgetItem(path2qs(npath)));
        row++;
    }
}

void PTransEdit::onItemDoubleClicked(QTableWidgetItem *item)
{
    transTW->editItem(item);
}

void PTransEdit::savePB_clicked()
{
    ConfSimple *conftrans = theconfig->getPTrans();
    if (!conftrans) {
        QMessageBox::warning(0, "Recoll", tr("Config error"));
        return;
    }
    conftrans->holdWrites(true);

    auto dbdir = qs2path(whatIdxCMB->currentText());
    
    conftrans->eraseKey(dbdir);

    for (int row = 0; row < transTW->rowCount(); row++) {
        QTableWidgetItem *item0 = transTW->item(row, 0);
        auto from = qs2path(item0->text());
        QTableWidgetItem *item1 = transTW->item(row, 1);
        auto to = qs2path(item1->text());
        conftrans->set(from, to, dbdir);
    }
    conftrans->holdWrites(false);
    // The rcldb does not use the same configuration object, but a copy.
    // Force a reopen, this is quick.
    std::string reason;
    maybeOpenDb(reason, true);
    close();
}

void PTransEdit::addPB_clicked()
{
    transTW->setRowCount(transTW->rowCount()+1);
    int row = transTW->rowCount()-1;
    transTW->setItem(row, 0, new QTableWidgetItem(tr("Original path")));
    transTW->setItem(row, 1, new QTableWidgetItem(tr("Local path")));
    transTW->editItem(transTW->item(row, 0));
}

void PTransEdit::delPB_clicked()
{
    QModelIndexList indexes = transTW->selectionModel()->selectedIndexes();
    std::vector<int> rows;
    for (int i = 0; i < indexes.size(); i++) {
        rows.push_back(indexes.at(i).row());
    }
    std::sort(rows.begin(), rows.end());
    rows.resize(unique(rows.begin(), rows.end()) - rows.begin());
    for (int i = static_cast<int>(rows.size()-1); i >= 0; i--) {
        transTW->removeRow(rows[i]);
    }
}

void PTransEdit::transTW_itemSelectionChanged()
{
    QModelIndexList indexes = transTW->selectionModel()->selectedIndexes();
    delPB->setEnabled(indexes.size() >= 1);
}
