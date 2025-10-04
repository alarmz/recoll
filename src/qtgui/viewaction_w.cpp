/* Copyright (C) 2006-2019 J.F.Dockes 
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

#include "viewaction_w.h"

#include <vector>
#include <utility>
#include <string>

#include <QMessageBox>
#include <QSettings>

#include "recoll.h"
#include "log.h"
#include "guiutils.h"
#include "smallut.h"

#ifdef _WIN32
#include "pathut.h"
#endif

static const char *settingskey_fieldwidths="/Recoll/prefs/viewActionWidths";
static const char *settingskey_height="/Recoll/prefs/viewActionHeight";
static const char *settingskey_width="/Recoll/prefs/viewActionWidth";

void ViewAction::init()
{
    selSamePB->setEnabled(false);
    connect(closePB, SIGNAL(clicked()), this, SLOT(onClose()));
    connect(chgActPB, SIGNAL(clicked()), this, SLOT(editActions()));
    connect(actionsLV, SIGNAL(currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
            this, SLOT(onCurrentItemChanged(QTableWidgetItem *, QTableWidgetItem *)));
    useDesktopCB->setChecked(prefs.useDesktopOpen);
    onUseDesktopCBToggled(prefs.useDesktopOpen);
    connect(useDesktopCB, SIGNAL(stateChanged(int)), this, SLOT(onUseDesktopCBToggled(int)));
    connect(setExceptCB, SIGNAL(stateChanged(int)), this, SLOT(onSetExceptCBToggled(int)));
    connect(selSamePB, SIGNAL(clicked()), this, SLOT(onSelSameClicked()));

    QSettings settings;
    auto w = settings.value(settingskey_width).toInt();
    auto h = settings.value(settingskey_height).toInt();
    if (w != 0 && h != 0) {
        resize(QSize(w, h).expandedTo(minimumSizeHint()));
    }
    auto qw = settings.value(settingskey_fieldwidths).toString();
    if (!qw.isEmpty()) {
        auto *header = actionsLV->horizontalHeader();
        std::vector<std::string> vw;
        stringToStrings(qs2utf8s(qw), vw);
        if (int(vw.size()) == header->count()) {
            for (unsigned int i = 0; i < vw.size(); i++) {
                header->resizeSection(i, atoi(vw[i].c_str()));
            }
        }
    }
}


void ViewAction::onClose()
{
    QSettings settings;
    auto qw = settings.value(settingskey_fieldwidths).toString();
    auto *header = actionsLV->horizontalHeader();
    std::vector<std::string> vw;
    for (int i = 0; i < 2; i++){
        vw.push_back(std::to_string(header->sectionSize(i)));
    }
    auto s = stringsToString(vw);
    settings.setValue(settingskey_fieldwidths, u8s2qs(s));
    settings.setValue(settingskey_width, width());
    settings.setValue(settingskey_height, height());
    close();
}

void ViewAction::onUseDesktopCBToggled(int onoff)
{
    prefs.useDesktopOpen = onoff != 0;
    fillLists();
    setExceptCB->setEnabled(prefs.useDesktopOpen);
}

void ViewAction::onSetExceptCBToggled(int onoff)
{
    newActionLE->setEnabled(onoff != 0);
}

void ViewAction::fillLists()
{
    currentLBL->clear();
    actionsLV->clear();
    actionsLV->verticalHeader()->setDefaultSectionSize(20); 
    std::vector<std::pair<std::string, std::string> > defs;
    theconfig->getMimeViewerDefs(defs);
    actionsLV->setRowCount(static_cast<int>(defs.size()));

    std::set<std::string> viewerXs;
    if (prefs.useDesktopOpen) {
        viewerXs = theconfig->getMimeViewerAllEx();
    }

    int row = 0;
    for (const auto& def : defs) {
        actionsLV->setItem(row, 0, new QTableWidgetItem(u8s2qs(def.first)));
        if (!prefs.useDesktopOpen || viewerXs.find(def.first) != viewerXs.end()) {
            actionsLV->setItem(row, 1, new QTableWidgetItem(u8s2qs(def.second)));
        } else {
            actionsLV->setItem(row, 1, new QTableWidgetItem(tr("Desktop Default")));
        }
        row++;
    }
    QStringList labels(tr("MIME type"));
    labels.push_back(tr("Command"));
    actionsLV->setHorizontalHeaderLabels(labels);
}

void ViewAction::selectMT(const QString& mt)
{
    actionsLV->clearSelection();
    QList<QTableWidgetItem *>items =
        actionsLV->findItems(mt, Qt::MatchFixedString|Qt::MatchCaseSensitive);
    for (QList<QTableWidgetItem *>::iterator it = items.begin(); it != items.end(); it++) {
        (*it)->setSelected(true);
        actionsLV->setCurrentItem(*it, QItemSelectionModel::Columns);
    }
}

void ViewAction::onSelSameClicked()
{
    actionsLV->clearSelection();
    QString value = currentLBL->text();
    if (value.isEmpty())
        return;
    std::string action = qs2utf8s(value);
    LOGDEB1("ViewAction::onSelSameClicked: value: " << action << endl);

    std::vector<std::pair<std::string, std::string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (const auto& def : defs) {
        if (def.second == action) {
            QList<QTableWidgetItem *>items = actionsLV->findItems(
                u8s2qs(def.first), Qt::MatchFixedString|Qt::MatchCaseSensitive);
            for (QList<QTableWidgetItem *>::iterator it = items.begin(); it != items.end(); it++) {
                (*it)->setSelected(true);
                actionsLV->item((*it)->row(), 1)->setSelected(true);
            }
        }
    }
}

void ViewAction::onCurrentItemChanged(QTableWidgetItem *item, QTableWidgetItem *)
{
    currentLBL->clear();
    selSamePB->setEnabled(false);
    if (nullptr == item) {
        return;
    }
    QTableWidgetItem *item0 = actionsLV->item(item->row(), 0);
    std::string mtype = qs2utf8s(item0->text());

    std::vector<std::pair<std::string, std::string> > defs;
    theconfig->getMimeViewerDefs(defs);
    for (const auto& def : defs) {
        if (def.first == mtype) {
            currentLBL->setText(u8s2qs(def.second));
            selSamePB->setEnabled(true);
            return;
        }
    }
}

void ViewAction::editActions()
{
    QString action0;
    bool except0 = false;

    std::set<std::string> viewerXs = theconfig->getMimeViewerAllEx();
    std::vector<std::string> mtypes;
    bool dowarnmultiple = true;
    for (int row = 0; row < actionsLV->rowCount(); row++) {
        QTableWidgetItem *item0 = actionsLV->item(row, 0);
        if (!item0->isSelected())
            continue;
        std::string mtype = qs2utf8s(item0->text());
        mtypes.push_back(mtype);
        QTableWidgetItem *item1 = actionsLV->item(row, 1);
        QString action = item1->text();
        bool except = viewerXs.find(mtype) != viewerXs.end();
        if (action0.isEmpty()) {
            action0 = action;
            except0 = except;
        } else {
            if ((action != action0 || except != except0) && dowarnmultiple) {
                switch (QMessageBox::warning(
                            0, "Recoll", tr("Changing entries with different current values"),
                            QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Cancel)) {
                case QMessageBox::Ignore:
                    dowarnmultiple = false;
                    break;
                case QMessageBox::Cancel:
                default:
                    return;
                }
            }
        }
    }

    if (action0.isEmpty())
        return;
    std::string sact = qs2utf8s(newActionLE->text());
    if (!sact.empty()) {
        trimstring(sact);
#ifdef _WIN32
        path_slashize(sact);
#endif
    }
    for (const auto& entry : mtypes) {
        auto xit = viewerXs.find(entry);
        if (setExceptCB->isChecked()) {
            if (xit == viewerXs.end()) {
                viewerXs.insert(entry);
            }
        } else {
            if (xit != viewerXs.end()) {
                viewerXs.erase(xit);
            }
        }
        // An empty action will restore the default (erase from
        // topmost conftree). IF there is no default in the system
        // files, and the entry only came from the user file, it will
        // be erased and the mime will go away from the list, which is
        // not what we want ! This not trivial to test because
        // rclconfig has no way to tell us if the value comes from the
        // system config or the user file. So just check if the value
        // disappears after setting it, and restore it if it does.
        std::string oldvalue = theconfig->getMimeViewerDef(entry, "", 0);
        theconfig->setMimeViewerDef(entry, sact);
        std::string newvalue = theconfig->getMimeViewerDef(entry, "", 0);
        if (!oldvalue.empty() && newvalue.empty()) {
            theconfig->setMimeViewerDef(entry, oldvalue);
        }
    }

    theconfig->setMimeViewerAllEx(viewerXs);
    fillLists();
}
