/* Copyright (C) 2014 J.F.Dockes
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
#ifndef XMLTOSD_H_INCLUDED
#define XMLTOSD_H_INCLUDED

#include <string>
#include <memory>
#include <vector>

/** XML from saved simple searches (see searchdata.h for the advanced search history format).
 *
 * <SD type='ssearch'>
 *   <T>base64-encoded query text</T>
 *   <SM>OR|AND|FN|QL</SM>
 *   <SL>space-separated lang list</SL>
 *   <AP/>                                   <!-- autophrase -->
 *   <AS>space-separated suffix list</AS>    <!-- autosuffs -->
 *   <EX>base64-encoded config path>/EX>     <!-- [multiple] ext index -->
 * </SD>
 */ 

/* Resulting structure */
struct SSearchDef {
    SSearchDef() : autophrase(false), mode(0) {}
    std::vector<std::string> stemlangs;
    std::vector<std::string> autosuffs;
    std::vector<std::string> extindexes;
    std::string text;
    bool autophrase;
    int mode;
};

/* Parse XML into SSearchDef */
bool xmlToSSearch(const std::string& xml, SSearchDef&);

#endif /* XMLTOSD_H_INCLUDED */
