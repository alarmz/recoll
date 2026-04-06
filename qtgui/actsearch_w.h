/* Copyright (C) 2021 J.F.Dockes
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
#ifndef _ACTSEARCH_H_INCLUDED_
#define _ACTSEARCH_H_INCLUDED_

// A window for letting the user to search among menu entries (actions) and execute the one found.

#include <QDialog>
#include <QList>

#include <vector>

#include "ui_actsearch.h"

class QCompleter;
class QAction;
class QString;

class ActSearchW : public QDialog, public Ui::ActSearchDLG {
    Q_OBJECT
public:
    ActSearchW(QWidget* parent = 0) 
        : QDialog(parent) {
        setupUi(this);
        init();
    }
    void setActList(QList<QAction *>);
    virtual bool eventFilter(QObject *target, QEvent *event);

private slots:
    void onActivated(int);
    void onTextChanged(const QString&);

private:
    void init();
    std::vector<QAction *> m_actions;
    QCompleter *m_completer{nullptr};
    // Decides if we match the start only or anywhere. Anywhere seems better. May be made a pref one
    // day.
    bool m_match_contains{true};
};

#endif /* _ACTSEARCH_H_INCLUDED_ */
