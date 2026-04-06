/* Copyright (C) 2024 J.F.Dockes
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
#ifndef _CONFIGSWITCH_H_INCLUDED_
#define _CONFIGSWITCH_H_INCLUDED_

// Dialog for switching to another configuration. Activating execs another recoll process.

#include <QDialog>
#include <QList>

#include <vector>
#include <string>

#include "ui_configswitch.h"

class QCompleter;
class QString;

class ConfigSwitchW : public QDialog, public Ui::ConfigSwitchDLG {
    Q_OBJECT
public:
    ConfigSwitchW(QWidget* parent = 0) 
        : QDialog(parent) {
        setupUi(this);
        init();
    }
private slots:
    virtual void done(int);
private:
    void init();
    QStringList m_qdirs;
};

#endif /* _CONFIGSWITCH_H_INCLUDED_ */
