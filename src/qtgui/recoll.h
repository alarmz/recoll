/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _RECOLL_H_INCLUDED_
#define _RECOLL_H_INCLUDED_

#include <string>
#include <memory>

#include "rclconfig.h"
#include "rcldb.h"
#include "rclutil.h"

#include <QString>

// Misc declarations in need of sharing between the UI files

// Open the database if needed. We now force a close/open by default
extern bool maybeOpenDb(std::string &reason, bool force, bool *maindberror = nullptr);
extern const std::vector<std::string> *getCurrentExtraDbs();

/** Retrieve configured stemming languages */
bool getStemLangs(std::vector<std::string>& langs);

extern RclConfig *theconfig;

#ifdef ENABLE_SEMANTIC
extern bool semantic_enabled;
#endif

class RclDynConf;
extern RclDynConf *g_dynconf;
class AdvSearchHist;
extern AdvSearchHist *g_advshistory;


extern TempFile *rememberTempFile(TempFile);
extern void forgetTempFile(std::string &fn);
extern void deleteAllTempFiles();

extern std::shared_ptr<Rcl::Db> rcldb;
extern int recollNeedsExit;
extern void startManual(const std::string& helpindex);

extern void applyStyle();

inline std::string qs2utf8s(const QString& qs)
{
    auto qb = qs.toUtf8();
    return std::string(qb.data(), qb.size());
}
inline std::string qs2u8s(const QString& qs)
{
    auto qb = qs.toUtf8();
    return std::string(qb.data(), qb.size());
}
inline QString u8s2qs(const std::string& us)
{
    return QString::fromUtf8(us.c_str(), us.size());
}
inline QString path2qs(const std::string& us)
{
#ifdef _WIN32
    return QString::fromUtf8(us.c_str());
#else
    return QString::fromLocal8Bit(us.c_str());
#endif
}
inline std::string qs2path(const QString& qs)
{
#ifdef _WIN32
    return qs2utf8s(qs);
#else
    return (const char*)qs.toLocal8Bit().data();
#endif
}

/** Specialized version of the qt file dialog. Can't use getOpenFile() etc. cause they hide dot
    files... Need something more adaptable than the static functions but less complex than the full
    dialog */

// Also : can't keep adding parms with default values, we now use an object as parameter.
class MyGFNParams {
public:
    QString caption;
    bool filenosave{false};
    QString dirlocation; // Note: this holds the new location on return
    QString dfltnm;
    std::vector<std::string> sidedirs;
    bool readonly{false};
};
extern QString myGetFileName(bool isdir, QString caption = QString(),
                             bool filenosave = false,
                             QString dirlocation = QString(),
                             QString dlftnm = QString()
    );

extern QString myGetFileName(bool isdir, MyGFNParams &parms);

#endif /* _RECOLL_H_INCLUDED_ */
