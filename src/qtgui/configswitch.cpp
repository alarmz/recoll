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
    new QShortcut(QKeySequence("Esc"), this, SLOT(cancel()));
    connect(dirsCMB, SIGNAL(activated(int)), this, SLOT(onActivated(int)));

    std::vector<std::string> sdirs = guess_recoll_confdirs();
    for (const auto& e : sdirs) {
        if (!path_samepath(e, theconfig->getConfDir()))
            m_qdirs.push_back(path2qs(e));
    }
    m_qdirs.push_back(tr("Choose other"));
    for (const auto& e : qAsConst(m_qdirs)) {
        dirsCMB->addItem(e);
    }

    m_completer = new QCompleter(m_qdirs, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    if (m_match_contains) {
        m_completer->setFilterMode(Qt::MatchContains);
    }
    dirsCMB->setCompleter(m_completer);
    m_completer->popup()->installEventFilter(this);
    dirsCMB->installEventFilter(this);
}

void ConfigSwitchW::cancel()
{
    LOGDEB("ConfigSwitch: cancel\n");
    dirsCMB->setCurrentIndex(-1);
    m_cancelled = true;
    hide();
}

// This is to avoid that if the user types Backspace or Del while we
// have inserted / selected the current completion, the lineedit text
// goes back to what it was, the completion fires, and it looks like
// nothing was typed. Disable the completion after Del or Backspace
// is typed.
bool ConfigSwitchW::eventFilter(QObject *target, QEvent *event)
{
    Q_UNUSED(target);
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Backspace || keyEvent->key()==Qt::Key_Delete) {
        dirsCMB->setCompleter(nullptr);
        return false;
    } else {
        if (nullptr == dirsCMB->completer()) {
            dirsCMB->setCompleter(m_completer);
        }
    }        
    return false;
}

void ConfigSwitchW::onTextChanged(const QString&)
{
    if (dirsCMB->completer() && dirsCMB->completer()->completionCount() == 1) {
        // We append the completion part to the end of the current input, line, and select it so
        // that the user has a clear indication of what will happen if they type Enter.
        auto le = dirsCMB->lineEdit();
        int pos = le->cursorPosition();
        auto text = dirsCMB->completer()->currentCompletion();
        int len = text.size() - dirsCMB->currentText().size();
        le->setText(text);
        if (!m_match_contains) {
            le->setCursorPosition(pos);
            le->setSelection(pos, len);
        }
    }
}

void ConfigSwitchW::onActivated(int index)
{
    LOGDEB("ConfigWwitch: onActivated: index  " << index << " cancel " << m_cancelled << "\n");
    if (m_cancelled || index < 0 || index >= int(m_qdirs.size()))
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
