/* Copyright (C) 2004-2024 J.F.Dockes
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

#include "appformime.h"

#include <iostream>

#include "conftree.h"
#include "fstreewalk.h"
#include "pathut.h"
#include "smallut.h"

static const std::string topappsdir("/usr/share/applications");
static const std::string desktopext("desktop");

static DesktopDb *theDb;

class FstCb : public FsTreeWalkerCB {
public:
    FstCb(DesktopDb::AppMap *appdefs)
        : m_appdefs(appdefs) {}
    virtual FsTreeWalker::Status 
    processone(const std::string &, FsTreeWalker::CbFlag, const struct PathStat&) override;
    DesktopDb::AppMap *m_appdefs;
};

FsTreeWalker::Status FstCb::processone(
    const std::string& fn, FsTreeWalker::CbFlag flg, const struct PathStat&)
{
    if (flg != FsTreeWalker::FtwRegular)
        return FsTreeWalker::FtwOk;

    if (path_suffix(fn) != desktopext) {
        //cerr << fn << " does not end with .desktop" << '\n';
        return FsTreeWalker::FtwOk;
    }

    ConfSimple dt(fn.c_str(), true);
    if (!dt.ok()) {
        std::cerr << fn << " cant parse" << '\n';
        return FsTreeWalker::FtwOk;
    }
    std::string tp, nm, cmd, mt;
    if (!dt.get("Type", tp, "Desktop Entry")) {
        //std::cerr << fn << " no Type" << '\n';
        return FsTreeWalker::FtwOk;
    }
    if (tp != "Application") {
        //std::cerr << fn << " wrong Type " << tp << '\n';
        return FsTreeWalker::FtwOk;
    }
    if (!dt.get("Exec", cmd, "Desktop Entry")) {
        //std::cerr << fn << " no Exec" << '\n';
        return FsTreeWalker::FtwOk;
    }
    if (!dt.get("Name", nm, "Desktop Entry")) {
        //std::cerr << fn << " no Name" << '\n';
        nm = path_basename(fn, desktopext);
    }
    if (!dt.get("MimeType", mt, "Desktop Entry")) {
        //std::cerr << fn << " no MimeType" << '\n';
        return FsTreeWalker::FtwOk;
    }
    // Breakup mime type list, and push app to mime entries
    std::vector<std::string> mimes;
    stringToTokens(mt, mimes, ";");
    for (const auto& mime: mimes) {
        (*m_appdefs)[mime].emplace_back(nm, cmd);
    }
    return FsTreeWalker::FtwOk;
}

DesktopDb* DesktopDb::getDb()
{
    if (nullptr == theDb) {
        theDb = new DesktopDb();
    }
    if (theDb && theDb->m_ok)
        return theDb;
    return nullptr;
}

void DesktopDb::build(const std::string& dir)
{
    FstCb procapp(&m_appMap);
    FsTreeWalker walker;
    if (walker.walk(dir, procapp) != FsTreeWalker::FtwOk) {
        m_ok = false;
        m_reason = walker.getReason();
    }
    m_ok = true;
}

DesktopDb::DesktopDb()
{
    build(topappsdir);
}

DesktopDb::DesktopDb(const std::string& dir)
{
    build(dir);
}

bool DesktopDb::appForMime(const std::string& mime, std::vector<AppDef> *apps, std::string *reason)
{
    auto it = m_appMap.find(mime);
    if (it == m_appMap.end()) {
        if (reason)
            *reason = std::string("No application found for ") + mime;
        return false;
    }
    *apps = it->second;
    return true;
}

bool DesktopDb::allApps(std::vector<AppDef> *apps)
{
    std::set<AppDef> allaps;
    for (const auto& mimeapps : m_appMap) {
        for (const auto& app : mimeapps.second) {
            allaps.insert(app);
        }
    }
    for (const auto& app : allaps) {
        apps->emplace_back(app.name, app.command);
    }
    return true;
}

bool DesktopDb::appByName(const std::string& nm, AppDef& appout)
{
    for (const auto& mimeapps : m_appMap) {
        for (const auto& app : mimeapps.second) {
            if (nm == app.name) {
                appout = app;
                return true;
            }
        }
    }
    return false;
}

const std::string& DesktopDb::getReason()
{
    return m_reason;
}

bool mimeIsImage(const std::string& tp)
{
    return !tp.compare(0, 6, "image/") &&
        tp.compare("image/vnd.djvu") && tp.compare("image/svg+xml");
}
