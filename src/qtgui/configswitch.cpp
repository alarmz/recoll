/* Copyright (C) 2020-2021 J.F.Dockes
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

#include <QShortcut>
#include <QCompleter>
#include <QKeyEvent>
#include <QMessageBox>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QTimer>

#include "log.h"
#include "recoll.h"
#include "configswitch.h"
#include "rclutil.h"
#include "pathut.h"
#include "execmd.h"

void ConfigSwitchW::init()
{
    std::vector<std::string> sdirs = guess_recoll_confdirs();
    for (const auto& e : sdirs) {
        if (!path_samepath(e, theconfig->getConfDir()))
            m_qdirs.push_back(path2qs(e));
    }
    m_qdirs.push_back(tr("Choose other"));
    for (const auto& e : qAsConst(m_qdirs)) {
        dirsCMB->addItem(e);
    }
    connect(this, SIGNAL(finished(int)), this, SLOT(done(int)));
}

void ConfigSwitchW::done(int result)
{
    if (result != QDialog::Accepted) {
        hide();
        return;
    }
    auto index = dirsCMB->currentIndex();
    if (index < 0 || index >= int(m_qdirs.size()))
        return;
    QString qconf;
    if (index == m_qdirs.size() - 1) {
        qconf = myGetFileName(true,tr("Choose configuration directory"),true,path2qs(path_home()));
        if (qconf.isEmpty())
            return;
    } else {
        qconf = m_qdirs[index];
    }
    auto recoll = path_cat(path_thisexecdir(), "recoll");
    std::vector<std::string> args{"-c", qs2path(qconf)};
    ExecCmd cmd(ExecCmd::EXF_SHOWWINDOW);
    if (cmd.startExec(recoll, args, false, false) == 0) {
        _exit(0);
    }
}
