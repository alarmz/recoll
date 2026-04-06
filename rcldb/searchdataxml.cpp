/* Copyright (C) 2006-2024 J.F.Dockes
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

// Handle translation from rcl's SearchData structures to XML. Used for
// complex search history storage in the GUI

#include "autoconfig.h"

#include <stdio.h>

#include <string>
#include <vector>
#include <sstream>

#include "searchdata.h"
#include "log.h"
#include "base64.h"
#include "picoxml.h"

using namespace std;

namespace Rcl {

class SDataWalkerDump : public SdataWalker {
public:
    SDataWalkerDump(ostream& _o, bool _asxml) : o(_o), asxml(_asxml) {}
    virtual ~SDataWalkerDump() {}
    std::string tabs;
    ostream& o;
    bool asxml;
    virtual bool clause(SearchDataClause *clp) override {
        clp->dump(o, tabs, asxml);
        return true;
    }

    virtual bool sdata(SearchData* sdp, bool enter) override{
        if (enter) {
            sdp->dump(o, tabs, asxml);
            tabs += '\t';
        } else {
            sdp->closeDump(o, tabs, asxml);
            if (!tabs.empty())
                tabs.pop_back();
        }
        return true;
    }
};

static string tpToString(SClType tp)
{
    switch (tp) {
    case SCLT_AND: return "AND";
    case SCLT_OR: return "OR";
    case SCLT_FILENAME: return "FN";
    case SCLT_PHRASE: return "PH";
    case SCLT_NEAR: return "NE";
    case SCLT_RANGE: return "RG";
    case SCLT_SUB: return "SU"; // Unsupported actually
    default: return "UN";
    }
}

string SearchData::asXML()
{
    ostringstream os;
    rdump(os, true);
    return os.str();
}

void SearchData::rdump(ostream& o, bool asxml)
{
    SDataWalkerDump printer(o, asxml);
    sdataWalk(this, printer);
}

void SearchData::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        o << "<SD>" << "\n" << "<CL>" << "\n";
        // List conjunction: default is AND, else print it.
        if (m_tp != SCLT_AND)
            o << "<CLT>" << tpToString(m_tp) << "</CLT>" << "\n";
    } else {
        o << tabs <<
            "SearchData: " << tpToString(m_tp) << " qs " << int(m_query.size()) << 
            " ft " << m_filetypes.size() << " nft " << m_nfiletypes.size() << 
            " hd " << m_haveDates <<
#ifdef EXT4_BIRTH_TIME
            " hbrd " << m_haveBrDates <<
#endif
            " maxs " << m_maxSize << " mins " << 
            m_minSize << " wc " << m_haveWildCards << " subsp " << m_subspec << "\n";
    }
}

void SearchData::closeDump(ostream& o, const std::string&, bool asxml) const
{
    if (asxml) {
        o << "</CL>" << "\n";

        if (m_haveDates) {
            if (m_dates.y1 > 0) {
                o << "<DMI>" << 
                    "<D>" << m_dates.d1 << "</D>" <<
                    "<M>" << m_dates.m1 << "</M>" << 
                    "<Y>" << m_dates.y1 << "</Y>" 
                   << "</DMI>" << "\n";
            }
            if (m_dates.y2 > 0) {
                o << "<DMA>" << 
                    "<D>" << m_dates.d2 << "</D>" <<
                    "<M>" << m_dates.m2 << "</M>" << 
                    "<Y>" << m_dates.y2 << "</Y>" 
                   << "</DMA>" << "\n";
            }
        }

        if (m_minSize != -1) {
            o << "<MIS>" << m_minSize << "</MIS>" << "\n";
        }
        if (m_maxSize != -1) {
            o << "<MAS>" << m_maxSize << "</MAS>" << "\n";
        }
        if (!m_filetypes.empty()) {
            o << "<ST>";
            for (const auto& ft : m_filetypes) {
                o << ft << " ";
            }
            o << "</ST>" << "\n";
        }

        if (!m_nfiletypes.empty()) {
            o << "<IT>";
            for (const auto& nft : m_nfiletypes) {
                o << nft << " ";
            }
            o << "</IT>" << "\n";
        }

        o << "</SD>";
    }    
}

void SearchDataClause::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    o << "SearchDataClause??";
}

static void prolog(ostream& o, bool excl, SClType tp, const std::string& fld, const std::string& txt)
{
    o << "<C>" << "\n";
    if (excl)
        o << "<NEG/>" << "\n";
    if (tp != SCLT_AND) {
        o << "<CT>" << tpToString(tp) << "</CT>" << "\n";
    }
    if (!fld.empty()) {
        o << "<F>" << base64_encode(fld) << "</F>" << "\n";
    }
    o << "<T>" << base64_encode(txt) << "</T>" << "\n";
}

void SearchDataClauseSimple::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        prolog(o, getexclude(), getTp(), getfield(), gettext());
        o << "</C>" << "\n";
    } else {
        o << tabs << "ClauseSimple: " << tpToString(m_tp) << " ";
        if (m_exclude)
            o << "- ";
        o << "[" ;
        if (!m_field.empty())
            o << m_field << " : ";
        o << m_text << "]" << "\n";
    }
}

void SearchDataClauseRange::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        prolog(o, getexclude(), getTp(), getfield(), gettext());
        const string& t = gettext2();
        if (!t.empty()) {
            o << "<T2>" << base64_encode(gettext2()) << "</T2>" << "\n";
        }
        o << "</C>" << "\n";
    } else {
        o << tabs << "ClauseRange: ";
        if (m_exclude)
            o << " - ";
        o << "[" << gettext() << "]" << "\n";
    }
}

void SearchDataClauseDist::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        prolog(o, getexclude(), getTp(), getfield(), gettext());
        o << "<S>" << getslack() << "</S>" << "\n";
        o << "</C>" << "\n";
    } else {
        if (m_tp == SCLT_NEAR)
            o << tabs << "ClauseDist: NEAR ";
        else
            o << tabs << "ClauseDist: PHRA ";
            
        if (m_exclude)
            o << " - ";
        o << "[";
        if (!m_field.empty())
            o << m_field << " : ";
        o << m_text << "]" << "\n";
    }
}

void SearchDataClauseFilename::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        prolog(o, getexclude(), getTp(), getfield(), gettext());
        o << "</C>" << "\n";
    } else {
        o << tabs << "ClauseFN: ";
        if (m_exclude)
            o << " - ";
        o << "[" << m_text << "]" << "\n";
    }
}

void SearchDataClausePath::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        if (getexclude()) {
            o << "<ND>" << base64_encode(gettext()) << "</ND>" << "\n";
        } else {
            o << "<YD>" << base64_encode(gettext()) << "</YD>" << "\n";
        }
    } else {        
        o << tabs << "ClausePath: ";
        if (m_exclude)
            o << " - ";
        o << "[" << m_text << "]" << "\n";
    }
}

void SearchDataClauseSub::dump(ostream& o, const std::string& tabs, bool asxml) const
{
    if (asxml) {
        o << "<C>" << "\n";
        if (getexclude())
            o << "<NEG/>" << "\n";
        if (getTp() != SCLT_AND) {
            o << "<CT>" << tpToString(getTp()) << "</CT>" << "\n";
        }
        o << "</C>" << "\n";
    } else {
        o << tabs << "ClauseSub ";
    }
}



///////////////////// Reverse operation: build search data from XML

class SDHXMLHandler : public PicoXMLParser {
public:
    SDHXMLHandler(const std::string& in)
        : PicoXMLParser(in) {
        resetTemps();
    }
    void startElement(
        const std::string& nm,
        const std::map<std::string, std::string>& attrs) {

        LOGDEB2("SDHXMLHandler::startElement: name [" << nm << "]\n");
        if (nm == "SD") {
            // Advanced search history entries have no type. So we're good
            // either if type is absent, or if it's searchdata
            auto attr = attrs.find("type");
            if (attr != attrs.end() && attr->second != "searchdata") {
                LOGDEB("XMLTOSD: bad type: " << attr->second << endl);
                contentsOk = false;
                return;
            }
            resetTemps();
            // A new search descriptor. Allocate data structure
            sd = std::make_shared<SearchData>();
            if (!sd) {
                LOGERR("SDHXMLHandler::startElement: out of memory\n");
                contentsOk = false;
                return;
            }
        }    
        return;
    }

    void endElement(const string & nm) {
        LOGDEB2("SDHXMLHandler::endElement: name [" << nm << "]\n");
        string curtxt{currentText};
        trimstring(curtxt, " \t\n\r");
        if (nm == "CLT") {
            if (curtxt == "OR") {
                sd->setTp(SCLT_OR);
            }
        } else if (nm == "CT") {
            whatclause = curtxt;
        } else if (nm == "NEG") {
            exclude = true;
        } else if (nm == "F") {
            field = base64_decode(curtxt);
        } else if (nm == "T") {
            text = base64_decode(curtxt);
        } else if (nm == "T2") {
            text2 = base64_decode(curtxt);
        } else if (nm == "S") {
            slack = atoi(curtxt.c_str());
        } else if (nm == "C") {
            SearchDataClause *c;
            if (whatclause == "AND" || whatclause.empty()) {
                c = new SearchDataClauseSimple(SCLT_AND, text, field);
                c->setexclude(exclude);
            } else if (whatclause == "OR") {
                c = new SearchDataClauseSimple(SCLT_OR, text, field);
                c->setexclude(exclude);
            } else if (whatclause == "RG") {
                c = new SearchDataClauseRange(text, text2, field);
                c->setexclude(exclude);
            } else if (whatclause == "EX") {
                // Compat with old hist. We don't generete EX
                // (SCLT_EXCL) anymore it's replaced with OR + exclude
                // flag
                c = new SearchDataClauseSimple(SCLT_OR, text, field);
                c->setexclude(true);
            } else if (whatclause == "FN") {
                c = new SearchDataClauseFilename(text);
                c->setexclude(exclude);
            } else if (whatclause == "PH") {
                c = new SearchDataClauseDist(SCLT_PHRASE, text, slack, field);
                c->setexclude(exclude);
            } else if (whatclause == "NE") {
                c = new SearchDataClauseDist(SCLT_NEAR, text, slack, field);
                c->setexclude(exclude);
            } else {
                LOGERR("Bad clause type ["  << whatclause << "]\n");
                contentsOk = false;
                return;
            }
            sd->addClause(c);
            whatclause = "";
            text.clear();
            field.clear();
            slack = 0;
            exclude = false;
        } else if (nm == "D") {
            d = atoi(curtxt.c_str());
        } else if (nm == "M") {
            m = atoi(curtxt.c_str());
        } else if (nm == "Y") {
            y = atoi(curtxt.c_str());
        } else if (nm == "DMI") {
            di.d1 = d;
            di.m1 = m;
            di.y1 = y;
            hasdates = true;
        } else if (nm == "DMA") {
            di.d2 = d;
            di.m2 = m;
            di.y2 = y;
            hasdates = true;
        } else if (nm == "MIS") {
            sd->setMinSize(atoll(curtxt.c_str()));
        } else if (nm == "MAS") {
            sd->setMaxSize(atoll(curtxt.c_str()));
        } else if (nm == "ST") {
            string types = curtxt.c_str();
            vector<string> vt;
            stringToTokens(types, vt);
            for (unsigned int i = 0; i < vt.size(); i++) 
                sd->addFiletype(vt[i]);
        } else if (nm == "IT") {
            vector<string> vt;
            stringToTokens(curtxt, vt);
            for (unsigned int i = 0; i < vt.size(); i++) 
                sd->remFiletype(vt[i]);
        } else if (nm == "YD") {
            string d;
            base64_decode(curtxt, d);
            sd->addClause(new SearchDataClausePath(d));
        } else if (nm == "ND") {
            string d;
            base64_decode(curtxt, d);
            sd->addClause(new SearchDataClausePath(d, true));
        } else if (nm == "SD") {
            // Closing current search descriptor. Finishing touches...
            if (hasdates)
                sd->setDateSpan(&di);
            resetTemps();
            isvalid = contentsOk;
        } 
        currentText.clear();
        return;
    }

    void characterData(const std::string &str) {
        currentText += str;
    }

    // The object we set up
    std::shared_ptr<SearchData> sd;
    bool isvalid{false};
    bool contentsOk{true};
    
private:
    void resetTemps() {
        currentText = whatclause = "";
        text.clear();
        text2.clear();
        field.clear();
        slack = 0;
        d = m = y = di.d1 = di.m1 = di.y1 = di.d2 = di.m2 = di.y2 = 0;
        hasdates = false;
        exclude = false;
    }

    // Temporary data while parsing.
    std::string currentText;
    std::string whatclause;
    std::string field, text, text2;
    int slack;
    int d, m, y;
    DateInterval di;
    bool hasdates;
    bool exclude;
};

std::shared_ptr<Rcl::SearchData> SearchData::fromXML(const string& xml, bool verbose)
{
    SDHXMLHandler handler(xml);
    if (!handler.Parse() || !handler.isvalid) {
        if (verbose) {
            LOGERR("SearchData::fromXML: parse failed for ["  << xml << "]\n");
        }
        return std::shared_ptr<SearchData>();
    }
    return handler.sd;
}

} // namespace Rcl
