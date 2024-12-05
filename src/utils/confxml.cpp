/* Copyright (C) 2016-2024 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// This program uses conftree's "commentsAsXML()" method to extract the XML data embedded in the
// comments of some conftree files. E.g.:
//
//  # <var name="logfilename" type="fn">
//  # <brief>Log file # name.</brief>
//  # <descr>Defaults to stderr. This can also be specified as -d
//  # logfilename.</descr></var>
//  #logfilename =
//
// It can then do various things with the data, mostly convert it to asciidoc for integration in a
// manual.
//
// confxml can also print a semi-stripped version of the file, with just the "brief" elements and
// the commented assignments, to make the file more readable as installation default.

#include "conftree.h"

#include <getopt.h>

#include <sstream>
#include <iostream>
#include <vector>

#include "smallut.h"
#include "picoxml.h"

using namespace std;

static char *thisprog;
#define LOGDEB(X) {std::cerr << X << '\n';}

static const string& mapfind(const string& nm, const map<string, string>& mp)
{
    static string strnull;
    map<string, string>::const_iterator it;
    it = mp.find(nm);
    if (it == mp.end())
        return strnull;
    return it->second;
}

static string looksLikeAssign(const string& data)
{
    //LOGDEB("looksLikeAssign. data: [" << data << "]");
    vector<string> toks;
    stringToTokens(data, toks, "\n\r\t ");
    if (toks.size() >= 2 && !toks[1].compare("=")) {
        return toks[0];
    }
    return string();
}

static bool xmlToAsciiDoc(const string& xml)
{
    //LOGDEB("xmlToDoc: [" << xml << "]");
    
    class XMLToDoc : public PicoXMLParser {
    public:
        XMLToDoc(const string& x, stringstream& out)
            : PicoXMLParser(x), m_out(out) {
        }

        virtual void startElement(const string& tagname,
                                  const map<string, string>& attrs) {
            if (!tagname.compare("var")) {
                m_curvar = mapfind("name", attrs);
                //LOGDEB("Curvar: " << m_curvar);
                if (m_curvar.empty()) {
                    throw std::runtime_error("Var tag with no name");
                } else {
                    // This does not actually work because asciidoc
                    // does not currently accept an anchor attribute
                    // for a dlist entry (only for paragraphs and
                    // others). As an exception, the anchor for the
                    // first variable can be used for referencing the
                    // section.
                    m_out << "[[" << m_curvar << "]]" << '\n';
                    m_out << m_curvar << ":: ";
                    m_brief.clear();
                    m_descr.clear();
                }
            } else if (!tagname.compare("filetitle") ||
                       !tagname.compare("grouptitle")) {
                m_other.clear();
            }
        }

        virtual void endElement(const string& tagname) {
            if (!tagname.compare("var")) {
                m_out << m_brief << " " << m_descr << '\n' <<'\n';
            } else if (!tagname.compare("filetitle")) {
                m_out << "== " << m_other << '\n' << '\n';
                m_other.clear();
            } else if (!tagname.compare("grouptitle")) {
                m_out << "=== " << m_other << '\n' << '\n';
                m_other.clear();
            }
        }

        virtual void characterData(const string& data) {
            if (!tagStack().back().compare("brief")) {
                m_brief += data;
            } else if (!tagStack().back().compare("descr")) {
                m_descr += data;
            } else if (!tagStack().back().compare("filetitle") ||
                       !tagStack().back().compare("grouptitle")) {
                // We don't want \n in there
                m_other += neutchars(data, "\n\r");
                m_other += " ";
            } else {
                string nvarname = looksLikeAssign(data);
                if (!nvarname.empty() && nvarname.compare(m_curvar)) {
                    cerr << "Var assigned [" << nvarname << "] mismatch "
                        "with current variable [" << m_curvar << "]\n";
                }
            }
        }

        stringstream& m_out;
        string m_curvar;
        string m_brief;
        string m_descr;
        string m_other;
    };

    stringstream otxt;
    XMLToDoc parser(xml, otxt);
    try {
        if (!parser.parse()) {
            cerr << "Parse failed: " << parser.getLastErrorMessage() << '\n';
            return false;
        }
    } catch (const std::runtime_error& e) {
        cerr << e.what() << '\n';
        return false;
    }
    cout << otxt.str() << '\n';
    return true;
}

string idprefix = "RCL.INSTALL.CONFIG.RECOLLCONF";

static bool xmlToDocbook(const string& xml)
{
    //LOGDEB("xmlToDocbook: [" << xml << "]");
    
    class XMLToDoc : public PicoXMLParser {
    public:
        XMLToDoc(const string& x, stringstream& out)
            : PicoXMLParser(x), m_out(out), m_sect3(false) {
        }

        virtual void startElement(const string& tagname,
                                  const map<string, string>& attrs) {
            m_id = mapfind("id", attrs);
            m_type = mapfind("type", attrs);
            
            if (!tagname.compare("var")) {
                m_curvar = mapfind("name", attrs);
                //LOGDEB("Curvar: " << m_curvar);
                if (m_curvar.empty()) {
                    throw std::runtime_error("Var tag with no name");
                } else {
                    m_out << "<varlistentry id=\"" <<
                        idprefix + "." +
                        stringtoupper((const string&)m_curvar) << 
                        "\">\n<term><varname>" <<
                        m_curvar << "</varname></term>\n<listitem><para>";
                    m_brief.clear();
                    m_descr.clear();
                }
            } else if (!tagname.compare("filetitle") ||
                       !tagname.compare("grouptitle")) {
                m_other.clear();
            }
        }

        virtual void endElement(const string& tagname) {
            if (!tagname.compare("var")) {
                m_out << m_brief << " " << m_descr <<
                    "</para></listitem></varlistentry>" << '\n';
            } else if (!tagname.compare("filetitle")) {
                // Note: to use xinclude, the included file must be
                // valid xml (needs a top element. So we need to
                // include everything in a sectX (this can't be just a
                // list of sectX+1)
                m_out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
                    "<sect2 id=\"" << idprefix << "\">\n<title>" << 
                    m_other << "</title>\n";
                m_other.clear();
            } else if (!tagname.compare("grouptitle")) {
                if (m_sect3) {
                    m_out << "</variablelist></sect3>\n";
                }
                m_out << "<sect3 id=\"" << idprefix << "." <<
                    stringtoupper((const string&)m_id) << "\">\n<title>" << 
                    m_other << "</title><variablelist>" << '\n';
                m_sect3 = true;
                m_other.clear();
            }
        }

        virtual void characterData(const string& data) {
            if (!tagStack().back().compare("brief")) {
                m_brief += data;
            } else if (!tagStack().back().compare("descr")) {
                std::vector<std::string> lines;
                stringToTokens(data, lines, "\n", true, true);
                for (const auto& line : lines) {
                    if (line.empty()) {
                        m_descr += "</para><para>\n";
                    } else {
                        m_descr += line + "\n";
                    }
                }
            } else if (!tagStack().back().compare("filetitle") ||
                       !tagStack().back().compare("grouptitle")) {
                // We don't want \n in there
                m_other += neutchars(data, "\n\r");
                m_other += " ";
            } else {
                string nvarname = looksLikeAssign(data);
                if (!nvarname.empty() && nvarname.compare(m_curvar)) {
                    cerr << "Var assigned [" << nvarname << "] mismatch "
                        "with current variable [" << m_curvar << "]\n";
                }
            }
        }

        stringstream& m_out;
        string m_curvar;
        string m_brief;
        string m_descr;
        string m_id;
        string m_type;
        string m_other;
        bool m_sect3;
    };

    stringstream otxt;
    XMLToDoc parser(xml, otxt);
    try {
        if (!parser.parse()) {
            cerr << "Parse failed: " << parser.getLastErrorMessage() << '\n';
            return false;
        }
    } catch (const std::runtime_error& e) {
        cerr << e.what() << '\n';
        return false;
    }
    cout << otxt.str();
    if (parser.m_sect3) {
        cout << "</variablelist></sect3>\n";
    }
    cout << "</sect2>\n";
    return true;
}


static bool xmlToMan(const string& xml)
{
    class XMLToMan : public PicoXMLParser {
    public:
        XMLToMan(const string& x, stringstream& out)
            : PicoXMLParser(x), m_out(out) {
        }

        virtual void startElement(const string& tagname,
                                  const map<string, string>& attrs) {
            m_id = mapfind("id", attrs);
            m_type = mapfind("type", attrs);
            
            if (!tagname.compare("var")) {
                m_curvar = mapfind("name", attrs);
                //LOGDEB("Curvar: " << m_curvar);
                if (m_curvar.empty()) {
                    throw std::runtime_error("Var tag with no name");
                } else {
                    m_out << ".TP\n.BI " << "\"" << m_curvar << " = \"" <<
                        m_type << "\n";
                    m_brief.clear();
                    m_descr.clear();
                }
            }
        }

        virtual void endElement(const string& tagname) {
            if (!tagname.compare("var")) {
                m_out << m_brief << " " << m_descr << '\n';
            }
        }

        virtual void characterData(const string& data) {
            if (!tagStack().back().compare("brief")) {
                m_brief += data;
            } else if (!tagStack().back().compare("descr")) {
                m_descr += data;
            } else if (!tagStack().back().compare("filetitle") ||
                       !tagStack().back().compare("grouptitle")) {
                // We don't want \n in there
                m_other += neutchars(data, "\n\r");
                m_other += " ";
            } else {
                string nvarname = looksLikeAssign(data);
                if (!nvarname.empty() && nvarname.compare(m_curvar)) {
                    cerr << "Var assigned [" << nvarname << "] mismatch "
                        "with current variable [" << m_curvar << "]\n";
                }
            }
        }

        stringstream& m_out;
        string m_curvar;
        string m_brief;
        string m_descr;
        string m_id;
        string m_type;
        string m_other;
    };

    stringstream otxt;
    XMLToMan parser(xml, otxt);
    try {
        if (!parser.parse()) {
            cerr << "Parse failed: " << parser.getLastErrorMessage() << '\n';
            return false;
        }
    } catch (const std::runtime_error& e) {
        cerr << e.what() << '\n';
        return false;
    }
    cout << otxt.str();
    return true;
}


// Output stripped version of file, with no XML and just the brief variable descriptions. Hopefully
// easier to read and edit by hand.
static bool xmlToStripped(const string& xml)
{
    class XMLToStripped : public PicoXMLParser {
    public:
        XMLToStripped(const string& x, stringstream& out)
            : PicoXMLParser(x), m_out(out) {
        }

        virtual void characterData(const string& data) {
            if (!tagStack().back().compare("confcomments")) {
                vector<string> lines;
                stringToTokens(data, lines, "\n");
                for (const auto& line: lines) {
                    m_out << "#" << line << "\n";
                }
            } else if (!tagStack().back().compare("filetitle")) {
                m_out << "\n# " << neutchars(data, "\n\r") << "\n\n";
            } else if (!tagStack().back().compare("grouptitle")) {
                m_out << "\n# " << neutchars(data, "\n\r") << "\n\n";
            } else if (!tagStack().back().compare("brief")) {
                m_out << "# " << neutchars(data, "\n\r") << "\n";
            } else if (!tagStack().back().compare("subkey")) {
                m_out << "[" << data << "]" << "\n";
            } else if (!tagStack().back().compare("varsetting")) {
                m_out << data << "\n";
            }
        }

        stringstream& m_out;
    };

    stringstream otxt;
    XMLToStripped parser(xml, otxt);
    try {
        if (!parser.parse()) {
            cerr << "Parse failed: " << parser.getLastErrorMessage() << '\n';
            return false;
        }
    } catch (const std::runtime_error& e) {
        cerr << e.what() << '\n';
        return false;
    }
    cout << otxt.str();
    return true;
}


static char usage [] =
    "confxml [opts] filename\n"
    "--extract|-x : extract and print xml-formatted comments\n"
    "--asciidoc|-a : extract xml-formatted comments and convert to asciidoc\n"
    "--docbook|-d : extract xml-formatted comments and convert to docbook\n"
    "--idprefix|-i : id for the top element (dflt: RCL.INSTALL.CONFIG.RECOLLCONF)\n"
    "--man|-m : extract xml-formatted comments and convert to man page\n"
    "--strip : write out the configuration, just keeping the brief comments\n"
    ;

void Usage()
{
    std::cerr << thisprog << ": " << usage << '\n';
    std::cerr << "Exactly one of -extract/asciidoc/docbook/man/strip must be set\n";
    exit(1);
}

static struct option long_options[] = {
    {"extract", 0, 0, 'x'},
    {"asciidoc", 0, 0, 'a'},
    {"docbook", 0, 0, 'd'},
    {"man", 0, 0, 'm'},
    {"strip", 0, 0, 's'},
    {"idprefix", 1, 0, 'i'},
    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    thisprog = argv[0];
    int ret;
    int option_index = 0;
    int what = 0;
    while ((ret = getopt_long(argc, argv, "xadmsi:", long_options, &option_index)) != -1) {
        switch (ret) {
        case 'x': if (what) Usage();what = ret; break;
        case 'a': if (what) Usage();what = ret; break;
        case 'd': if (what) Usage();what = ret; break;
        case 'm': if (what) Usage();what = ret; break;
        case 's': if (what) Usage();what = ret; break;

        case 'i': idprefix=optarg; break;
            
        default: Usage();
        }
    }

    if (optind > argc - 1) {
        Usage();
    }

    // The following arguments are configuration file names
    vector<string> flist;
    while (optind < argc) {
        flist.push_back(argv[optind++]);
    }

    bool ro = true;
    ConfTree *conftree = 0;
    switch (flist.size()) {
    case 1:
        conftree = new ConfTree(flist.front().c_str(), ro);
        break;
    case 0:
        Usage();
        break;
    }

    if (what == 'x') {
        bool ok = conftree->commentsAsXML(cout);
        if (ok) {
            exit(0);
        } else {
            std::cerr << "XML comment extraction (commentsAsXML) failed\n";
            exit(1);
        }
    }

    stringstream stream;
    bool ok = conftree->commentsAsXML(stream);
    if (!ok) {
        exit(1);
    }
    if (what == 'a') {
        ok = xmlToAsciiDoc(stream.str());
    } else if (what == 'd') {
        ok = xmlToDocbook(stream.str());
    } else if (what == 'm') {
        ok = xmlToMan(stream.str());
    } else if (what == 's') {
        ok = xmlToStripped(stream.str());
    } else {
        Usage();
    }
    exit(ok ? 0 : 1);
}
