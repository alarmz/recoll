/* Copyright (C) 2022 J.F.Dockes
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
#include "autoconfig.h"

#include <vector>
#include <string>

#include "log.h"
#include "rclmain_w.h"
#include "idxmodel.h"
#include "guiutils.h"

void RclMain::populateSideFilters(SideFilterUpdateReason reason)
{
    if (m_idxtreemodel && reason == SFUR_USERCONFIG &&
        (m_idxtreemodel->getDepth() == prefs.idxFilterTreeDepth &&
         m_idxtreemodel->getEDbs() == *getCurrentExtraDbs())) {
        // The filter depths and set of external indexes are the only things which may impact us
        // after the user has changed the GUI preferences. So no need to lose the current user
        // selections if these did not change.
        return;
    }

    auto old_idxtreemodel = m_idxtreemodel;

    m_idxtreemodel = new IdxTreeModel(prefs.idxFilterTreeDepth, *getCurrentExtraDbs(), idxTreeView);
    m_idxtreemodel->populate();
    m_idxtreemodel->setHeaderData(0, Qt::Horizontal, QVariant(tr("Filter directories")));
    idxTreeView->setModel(m_idxtreemodel);

    // Reset the current filter spec, the selection is gone.
    sideFilterChanged();
    
    if (nullptr != old_idxtreemodel)
        old_idxtreemodel->deleteLater();

    if (reason == SFUR_INIT) {
        idxTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        idxTreeView->setSelectionMode(QAbstractItemView::MultiSelection);
        connect(idxTreeView, SIGNAL(clicked(const QModelIndex&)), this, SLOT(sideFilterChanged()));

        minDateFilterDTEDT->setCalendarPopup(true);
        maxDateFilterDTEDT->setCalendarPopup(true);
        minDateFilterDTEDT->setDate(QDate::currentDate());
        auto dateformat = prefs.sidefilterdateformat;
        if (!dateformat.isEmpty()) 
            minDateFilterDTEDT->setDisplayFormat(dateformat);
        maxDateFilterDTEDT->setDate(QDate::currentDate());
        if (!dateformat.isEmpty()) 
            maxDateFilterDTEDT->setDisplayFormat(dateformat);
        connect(minDateFilterDTEDT, SIGNAL(dateChanged(const QDate&)),
                this, SLOT(sideFilterChanged()));
        connect(maxDateFilterDTEDT, SIGNAL(dateChanged(const QDate&)),
                this, SLOT(sideFilterChanged()));
        connect(dateFilterCB, SIGNAL(toggled(bool)), this, SLOT(sideFilterChanged()));
        connect(dateFilterCB, SIGNAL(toggled(bool)), minDateFilterDTEDT, SLOT(setEnabled(bool)));
        connect(dateFilterCB, SIGNAL(toggled(bool)), maxDateFilterDTEDT, SLOT(setEnabled(bool)));

#ifdef EXT4_BIRTH_TIME
        birthDateFilterCB->show();
        minBirthDateFilterDTEDT->show();
        maxBirthDateFilterDTEDT->show();

        minBirthDateFilterDTEDT->setCalendarPopup(true);
        maxBirthDateFilterDTEDT->setCalendarPopup(true);
        minBirthDateFilterDTEDT->setDate(QDate::currentDate());
        maxBirthDateFilterDTEDT->setDate(QDate::currentDate());
        connect(minBirthDateFilterDTEDT, SIGNAL(dateChanged(const QDate&)),
                this, SLOT(sideFilterChanged()));
        connect(maxBirthDateFilterDTEDT, SIGNAL(dateChanged(const QDate&)),
                this, SLOT(sideFilterChanged()));

        connect(birthDateFilterCB, SIGNAL(toggled(bool)), this, SLOT(sideFilterChanged()));
        connect(birthDateFilterCB, SIGNAL(toggled(bool)),
                minBirthDateFilterDTEDT, SLOT(setEnabled(bool)));
        connect(birthDateFilterCB, SIGNAL(toggled(bool)),
                maxBirthDateFilterDTEDT, SLOT(setEnabled(bool)));
#else
        birthDateFilterCB->hide();
        minBirthDateFilterDTEDT->hide();
        maxBirthDateFilterDTEDT->hide();
#endif

    }
}

void RclMain::sideFilterChanged()
{
    setFiltSpec();
    initiateQuery();
}

void RclMain::enableSideFilters(bool enable)
{
    idxTreeView->setEnabled(enable);
    dateFilterCB->setEnabled(enable);
    minDateFilterDTEDT->setEnabled(enable && dateFilterCB->isChecked());
    maxDateFilterDTEDT->setEnabled(enable && dateFilterCB->isChecked());
}

void RclMain::clearDirFilter()
{
    idxTreeView->clearSelection();
}

std::vector<std::string> RclMain::idxTreeGetDirs()
{
    std::vector<std::string> out;
    auto selmodel = idxTreeView->selectionModel();
    if (nullptr != selmodel) {
        auto selected = selmodel->selectedIndexes();
        std::string clause;
        for (int i = 0; i < selected.size(); i++) {
            out.push_back(qs2path(selected[i].data(Qt::ToolTipRole).toString()));
        }
    }
    return out;
}
