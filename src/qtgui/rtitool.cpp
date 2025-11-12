#ifndef _WIN32
/* Copyright (C) 2005 J.F.Dockes
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
#include <signal.h>

#include <string>

#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QString>

#include "recoll.h"
#include "rtitool.h"
#include "smallut.h"
#include "pathut.h"
#include "copyfile.h"
#include "readfile.h"
#include "execmd.h"


std::string RTIToolW::getautostartfn()
{
	const static QString settingskey_confignick("/Recoll/prefs/index/confignickname");
	std::string autostartfile;
	std::string confighome;
	auto xdg = getenv("XDG_CONFIG_HOME");
	if (xdg) {
		confighome = xdg;
	} else {
		confighome = path_cat(path_home(), ".config/autostart");
	}
	if (theconfig->isDefaultConfig()) {
		return path_cat(confighome, "recollindex.desktop");
	} 

	if (confignick.empty()) {
		// Not the default configuration. Check if we stored the nickname, else ask for it
		QSettings settings(
			u8s2qs(path_cat(theconfig->getConfDir(), "recollgui.ini")), QSettings::IniFormat);
		QString qnm = settings.value(settingskey_confignick).toString();
		if (qnm.isEmpty()) {
			qnm = QInputDialog::getText(
				this, tr("Configuration name"), tr("Short alphanumeric nickname for this config"));
			if (qnm.isEmpty()) {
				return std::string();
			}
			settings.setValue(settingskey_confignick, qnm);
		}
		confignick = qs2path(qnm);
	}
	return path_cat(confighome, std::string("recollindex-") + confignick + ".desktop");
}

void RTIToolW::init()
{
    connect(this->sesCB, SIGNAL(clicked(bool)), this, SLOT(sesclicked(bool)));
    std::string autostartfile = getautostartfn();
    if (!autostartfile.empty() && path_exists(autostartfile)) {
        sesCB->setChecked(true);
    }
}

void RTIToolW::sesclicked(bool on)
{
    nowCB->setEnabled(on);
    if (!on)
        nowCB->setChecked(false);
}

void RTIToolW::accept()
{
    bool exitdial = false;

	auto autostartfile = getautostartfn();
	if (autostartfile.empty())
		return;

    if (sesCB->isChecked()) {
        // Setting up daemon indexing autostart

        if (path_exists(autostartfile)) {
            QString msg = tr("Replacing: ") + path2qs(autostartfile);
            QMessageBox::Button rep = QMessageBox::question(
                this, tr("Replacing file"), msg, QMessageBox::Ok | QMessageBox::Cancel);
            if (rep != QMessageBox::Ok) {
				return;
            }
        }

		std::string sourcefile = path_cat(theconfig->getDatadir(), "examples");
		sourcefile = path_cat(sourcefile, "recollindex.desktop");
        std::string prototext;
		if (path_exists(sourcefile)) {
			file_to_string(sourcefile, prototext);
		}
        if (prototext.empty()) {
			QMessageBox::warning(0, "Recoll", tr("Could not find ") + path2qs(sourcefile));
			return;
		}
		std::string text;
		pcSubst(prototext , text, {{'c', theconfig->getConfDir()}});
		
        // Try to create .config and autostart anyway. If they exists this will 
        // do nothing. An error will be detected when we try to create the file
        auto dir = path_cat(path_home(), ".config");
        path_makepath(dir, 0700);
        dir = path_cat(dir, "autostart");
        path_makepath(dir, 0700);

        std::string reason;
        if (!stringtofile(text, autostartfile.c_str(), reason)) {
            QString msg = tr("Can't create: ") + path2qs(autostartfile);
            QMessageBox::warning(0, tr("Warning"), msg, QMessageBox::Ok);
            return;
        }

        if (nowCB->isChecked()) {
            ExecCmd cmd;
            std::vector<std::string> args; 
            int status;

            args.push_back("-m");
            args.push_back("-w");
            args.push_back("0");
			args.push_back("-c");
			args.push_back(theconfig->getConfDir());
            status = cmd.doexec("recollindex", args, 0, 0);
            if (status) {
                QMessageBox::warning(0, tr("Warning"), tr("Could not execute recollindex"), 
                                     QMessageBox::Ok);
                goto out;
            }
        }

        exitdial = true;
    } else {
        // Turning autostart off
        if (path_exists(autostartfile)) {
            QString msg = tr("Deleting: ") + path2qs(autostartfile);
            QMessageBox::Button rep = QMessageBox::question(
				this, tr("Deleting file"), msg, QMessageBox::Ok | QMessageBox::Cancel);
            if (rep == QMessageBox::Ok) {
                exitdial = true;
                unlink(autostartfile.c_str());
                if (theconfig) {
                    Pidfile pidfile(theconfig->getPidfile());
                    pid_t pid;
                    if ((pid = pidfile.open()) != 0) {
                        QMessageBox::Button rep = 
                            QMessageBox::question(
                                this, tr("Removing autostart"), 
                                tr("Autostart file deleted. Kill current process too ?"),
                                QMessageBox::Yes | QMessageBox::No);
                        if (rep == QMessageBox::Yes) {
                            kill(pid, SIGTERM);
                        }
                    }
                }
            }
        } else {
            exitdial = true;
        }
    }

out:
    if (exitdial)
        QDialog::accept();
}

#endif // _WIN32
