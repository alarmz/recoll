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
#include <QAbstractItemView>
#include <QLineEdit>
#include <QAction>
#include <QKeyEvent>

#include "log.h"
#include "actsearch_w.h"

// A window for letting the user to search among menu entries (actions) and execute the one found.

void ActSearchW::init()
{
    new QShortcut(QKeySequence("Esc"), this, SLOT(hide()));
    connect(actCMB, SIGNAL(activated(int)), this, SLOT(onActivated(int)));
}

void ActSearchW::setActList(QList<QAction*> actlist)
{
    for (int i = 0; i < actlist.size(); i++) {
        if (!actlist[i]->text().isEmpty()) {
            m_actions.push_back(actlist[i]);
        }
    }
    std::sort(m_actions.begin(), m_actions.end(),
              [] (const QAction *act1, const QAction *act2) {
                  auto txt1 = act1->text().remove('&');
                  auto txt2 = act2->text().remove('&');
                  return txt1.compare(txt2, Qt::CaseInsensitive) < 0;});
    QStringList sl;
    for (const auto act : m_actions) {
        auto txt = act->text().remove('&');
        actCMB->addItem(txt);
        sl.push_back(txt);
    }
    actCMB->setCurrentText("");
    m_completer = new QCompleter(sl, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    if (m_match_contains) {
        m_completer->setFilterMode(Qt::MatchContains);
    }
    actCMB->setCompleter(m_completer);
    m_completer->popup()->installEventFilter(this);
    actCMB->installEventFilter(this);
}

// This is to avoid that if the user types Backspace or Del while we
// have inserted / selected the current completion, the lineedit text
// goes back to what it was, the completion fires, and it looks like
// nothing was typed. Disable the completion after Del or Backspace
// is typed.
bool ActSearchW::eventFilter(QObject *target, QEvent *event)
{
    Q_UNUSED(target);
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Backspace || keyEvent->key()==Qt::Key_Delete) {
        actCMB->setCompleter(nullptr);
        return false;
    } else {
        if (nullptr == actCMB->completer()) {
            actCMB->setCompleter(m_completer);
        }
    }        
    return false;
}

void ActSearchW::onTextChanged(const QString&)
{
    if (actCMB->completer() && actCMB->completer()->completionCount() == 1) {
        // We append the completion part to the end of the current input, line, and select it so
        // that the user has a clear indication of what will happen if they type Enter.
        auto le = actCMB->lineEdit();
        int pos = le->cursorPosition();
        auto text = actCMB->completer()->currentCompletion();
        int len = text.size() - actCMB->currentText().size();
        le->setText(text);
        if (!m_match_contains) {
            le->setCursorPosition(pos);
            le->setSelection(pos, len);
        }
    }
}

void ActSearchW::onActivated(int index)
{
    if (index < 0 || index >= int(m_actions.size()))
        return;
    m_actions[index]->trigger();
    hide();
}
