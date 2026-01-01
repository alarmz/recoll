/* Copyright (C) 2005-2024 J.F.Dockes 
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

#include "xmltosd.h"

#include <memory>

#include "ssearch_w.h"
#include "guiutils.h"
#include "log.h"
#include "smallut.h"
#include "recoll.h"
#include "picoxml.h"
#include "base64.h"

using namespace std;

// Handler for parsing saved simple search data
class SSHXMLHandler : public PicoXMLParser {
public:
    SSHXMLHandler(const std::string& in)
        : PicoXMLParser(in) {
        resetTemps();
    }

    void startElement(const std::string &nm,
                      const std::map<std::string, std::string>& attrs) override {
        LOGDEB2("SSHXMLHandler::startElement: name [" << nm << "]\n");
        if (nm == "SD") {
            // Simple search saved data has a type='ssearch' attribute.
            auto attr = attrs.find("type");
            if (attr == attrs.end() || attr->second != "ssearch") {
                if (attr == attrs.end()) {
                    LOGDEB("XMLTOSSS: bad type\n");
                } else {
                    LOGDEB("XMLTOSSS: bad type: " << attr->second << endl);
                }
                contentsOk = false;
            }
            resetTemps();
        }
    }
        
    void endElement(const string& nm) override {
        LOGDEB2("SSHXMLHandler::endElement: name ["  << nm << "]\n");
        std::string curtxt{currentText};
        trimstring(curtxt, " \t\n\r");
        if (nm == "SL") {
            stringToStrings(curtxt, data.stemlangs);
        } else if (nm == "T") {
            base64_decode(curtxt, data.text);
        } else if (nm == "EX") {
            data.extindexes.push_back(base64_decode(curtxt));
        } else if (nm == "SM") {
            if (curtxt == "QL") {
                data.mode = SSearch::SST_LANG;
            } else if (curtxt == "FN") {
                data.mode = SSearch::SST_FNM;
            } else if (curtxt == "OR") {
                data.mode = SSearch::SST_ANY;
            } else if (curtxt == "AND") {
                data.mode = SSearch::SST_ALL;
            } else if (curtxt == "SEM") {
                data.mode = SSearch::SST_SEM;
            } else {
                LOGERR("BAD SEARCH MODE: [" << curtxt << "]\n");
                contentsOk = false;
                return;
            }
        } else if (nm == "AS") {
            stringToStrings(curtxt, data.autosuffs);
        } else if (nm == "AP") {
            data.autophrase = true;
        } else if (nm == "SD") {
            // Closing current search descriptor. Finishing touches...
            resetTemps();
            isvalid = contentsOk;
        } 
        currentText.clear();
        return ;
    }

    void characterData(const std::string &str) override {
        currentText += str;
    }

    // The object we set up
    SSearchDef data;
    bool isvalid{false};
    bool contentsOk{true};
    
private:
    void resetTemps() {
        currentText = whatclause = "";
        text.clear();
    }

    // Temporary data while parsing.
    std::string currentText;
    std::string whatclause;
    string text;
};

bool xmlToSSearch(const string& xml, SSearchDef& data)
{
    SSHXMLHandler handler(xml);
    if (!handler.Parse() || !handler.isvalid) {
        LOGERR("xmlToSSearch: parse failed for ["  << xml << "]\n");
        return false;
    }
    data = handler.data;
    return true;
}

