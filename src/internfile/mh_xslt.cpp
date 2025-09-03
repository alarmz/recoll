/* Copyright (C) 2005-2025 J.F.Dockes 
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

#if defined(HAVE_MALLOC_H)
#include <malloc.h>
#elif defined(HAVE_MALLOC_MALLOC_H)
#include <malloc/malloc.h>
#endif
#include <fnmatch.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/transform.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "cstr.h"
#include "mh_xslt.h"
#include "log.h"
#include "smallut.h"
#include "md5ut.h"
#include "rclconfig.h"
#include "readfile.h"
#include "pathut.h"

using namespace std;

// Do we need this? It would need to be called from recollinit
// Call once, not reentrant
// xmlInitParser();
// LIBXML_TEST_VERSION;
// Probably not:    xmlCleanupParser();
        
// XML parser as FileScan sink. We just call libxml with the data chunks. Once done, call getDoc to
// obtain the parsed document.
class FileScanXML : public FileScanDo {
public:
    virtual ~FileScanXML() {
        if (ctxt) {
            xmlFreeParserCtxt(ctxt);
            // This should not be necessary (done by free), but see
            // http://xmlsoft.org/xmlmem.html#Compacting The
            // malloc_trim() and mallopt() doc seems to be a bit
            // misleading, there is probably a frag size under which
            // free() does not try to malloc_trim() at all
#ifdef HAVE_MALLOC_TRIM
            malloc_trim(0);
#endif /* HAVE_MALLOC_TRIM */
        }
    }

    xmlDocPtr getDoc() {
        int ret;
        if ((ret = xmlParseChunk(ctxt, nullptr, 0, 1))) {
            const xmlError *error = xmlGetLastError();
            LOGERR("FileScanXML: final xmlParseChunk failed with error " << ret << " error: " <<
                   (error ? error->message : " null return from xmlGetLastError()") << "\n");
            return nullptr;
        }
        return ctxt->myDoc;
    }

    virtual bool init(int64_t, string *) {
        ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
        if (ctxt == nullptr) {
            LOGERR("FileScanXML: xmlCreatePushParserCtxt failed\n");
            return false;
        } else {
            // Replaces setting global variables through the following in the module init:
            // xmlSubstituteEntitiesDefault(0); // No XML_PARSE_NOENT
            // xmlLoadExtDtdDefaultValue = 0;   // No XML_PARSE_DTDLOAD
            xmlCtxtUseOptions(ctxt, 0);
            return true;
        }
    }
    
    virtual bool data(const char *buf, int cnt, string*) {
        if (0) {
            string dt(buf, cnt);
            LOGDEB1("FileScanXML: data: cnt " << cnt << " data " << dt << '\n');
        } else {
            LOGDEB1("FileScanXML: data: cnt " << cnt << '\n');
        }            
        int ret;
        if ((ret = xmlParseChunk(ctxt, buf, cnt, 0))) {
            const xmlError *error = xmlGetLastError();
            LOGERR("FileScanXML: xmlParseChunk failed with error " << ret << " for [" << buf <<
                   "] error " <<
                   (error ? error->message : " null return from xmlGetLastError()") << "\n");
            return false;
        } else {
            LOGDEB1("xmlParseChunk ok (sent " << cnt << " bytes)\n");
            return true;
        }
    }

private:
    xmlParserCtxtPtr ctxt{nullptr};
};

/// Helper code to get a list of a zip archive members which match a wildcard expression
class ZLister : public FileScanDo {
public:
    ZLister(const std::string& pattern) : m_pattern(pattern) {}
    bool data(const char *buf, int cnt, std::string *reason) override {
        if (fnmatch(m_pattern.c_str(), buf, 0) == 0) {
            m_result.emplace_back(buf, cnt);
        }
        return true;
    }
    std::vector<std::string> m_result;
    std::string m_pattern;
};
static std::vector<std::string>
nameList(std::shared_ptr<FileScanSourceZip> zip, const std::string& pattern)
{
    // If there are no wildcard characters, the pattern is the result
    if (pattern.find_first_of("*?") == std::string::npos) {
        // Note that we don't bother with escaping or supporting
        // character ranges. This should be enough for our current use
        return {pattern};
    }

    ZLister doer(pattern);
    if (!zip_scan(zip, "*", &doer)) {
        LOGERR("nameList: listing members failed\n");
        return {};
    }
    sortAlphanumStrings(doer.m_result);
    return doer.m_result;
}


// Handler for XML-based documents. The data can come from a memory string or from a
// file. Additionally, it can be stored in zip archive format (e.g.: openxml, opendocument etc.). In
// this case, there can be multiple members to process and multiple style sheets, and the relevant
// methods get the member name (internal path) to process.
// We have two jobs:
//  - Read and parse the XSLT style sheets associated to metadata and body parts, as defined in
//    mimeconf, which is done during initialisation in the public constructor.
//  - For each document we then get passed, apply the style sheets to obtain
//    a concatenated HTML document.
class MimeHandlerXslt::Internal {
public:
    Internal(MimeHandlerXslt *_p)
        : p(_p) {}
    ~Internal() {
        for (auto& entry : metaOrAllSS) {
            xsltFreeStylesheet(entry.second);
        }
        for (auto& entry : bodySS) {
            xsltFreeStylesheet(entry.second);
        }
    }

    xsltStylesheet *prepare_stylesheet(const string& ssnm);
    bool process_doc_or_string(bool forpv, const string& fn, const string& data);
    bool apply_stylesheet(
        const string& fn, const string& data, std::shared_ptr<FileScanSourceZip> zip,
        const string& member, xsltStylesheet *ssp, string& result, string *md5p);

    MimeHandlerXslt *p;
    bool ok{false};

    // Pairs of zip archive member names and style sheet names for the
    // metadata, and map of style sheets refd by their names.
    // Exception: there can be a single entry which does meta and
    // body, in which case bodymembers/bodySS are empty.
    vector<pair<string,string>> metaMembers;
    map <string, xsltStylesheet*> metaOrAllSS;
    // Same for body data
    vector<pair<string,string>> bodyMembers;
    map<string, xsltStylesheet*> bodySS;
    string result;
    string filtersdir;
};

MimeHandlerXslt::~MimeHandlerXslt()
{
    delete m;
}

MimeHandlerXslt::MimeHandlerXslt(
    RclConfig *cnf, const std::string& id, const std::vector<std::string>& params)
    : RecollFilter(cnf, id), m(new Internal(this))
{
    LOGDEB("MimeHandlerXslt: params: " << stringsToString(params) << '\n');
    m->filtersdir = path_cat(cnf->getDatadir(), "filters");

    // params can be "xsltproc stylesheetall" or
    // "xslt meta/body memberpath stylesheetnm [... ... ...] ...
    if (params.size() == 2) {
        auto ss = m->prepare_stylesheet(params[1]);
        if (ss) {
            m->ok = true;
            m->metaOrAllSS[""] = ss;
        }
    } else if (params.size() > 3 && params.size() % 3 == 1) {
        auto it = params.begin();
        it++;
        // Read and prepare the style sheets and associate them to the body or meta names (a style
        // sheet can be used for several parts).
        // We have a list of <member name, stylesheet name> pairs and a map of stylesheet named to
        // parsed style sheet data.
        while (it != params.end()) {
            // meta/body membername ssname
            const string& tp = *it++;
            const string& znm = *it++;
            const string& ssnm = *it++;
            vector<pair<string, string>> *mbrv;
            map<string,xsltStylesheet*> *ssmp;
            if (tp == "meta") {
                mbrv = &m->metaMembers;
                ssmp = &m->metaOrAllSS;
            } else if (tp == "body") {
                mbrv = &m->bodyMembers;
                ssmp = &m->bodySS;
            } else {
                LOGERR("MimeHandlerXslt: bad member type " << tp << '\n');
                return;
            }
            if (ssmp->find(ssnm) == ssmp->end()) {
                auto ss = m->prepare_stylesheet(ssnm);
                if (nullptr == ss) {
                    return;
                }
                ssmp->insert({ssnm, ss});
            }
            mbrv->push_back({znm, ssnm});
        }
        m->ok = true;
    } else {
        LOGERR("MimeHandlerXslt: constructor with wrong param vector: " <<
               stringsToString(params) << '\n');
    }
}

xsltStylesheet *MimeHandlerXslt::Internal::prepare_stylesheet(const string& ssnm)
{
    string ssfn;
    if (path_isabsolute(ssnm)) {
        ssfn = ssnm;
    } else {
        ssfn = path_cat(filtersdir, ssnm);
    }
    FileScanXML XMLstyle;
    string reason;
    if (!file_scan(ssfn, &XMLstyle, &reason)) {
        LOGERR("MimeHandlerXslt: file_scan error for: " << ssfn << " : " << reason << '\n');
        return nullptr;
    }
    xmlDoc *stl = XMLstyle.getDoc();
    if (stl == nullptr) {
        LOGERR("MimeHandlerXslt: getDoc failed for style sheet " << ssfn << '\n');
        return nullptr;
    }
    return xsltParseStylesheetDoc(stl);
}

// Apply a given style sheet to some data, which can be stored in a system file, a memory string, or
// a zip file object. In the latter case, we also get a member name.
bool MimeHandlerXslt::Internal::apply_stylesheet(
    const string& fn, const string& data, std::shared_ptr<FileScanSourceZip> zip,
    const string& member, xsltStylesheet *ssp, string& result, string *md5p)
{
    FileScanXML XMLdoc;
    string md5, reason;
    bool res;
    LOGDEB0("MimeHandlerXslt::Internal::apply_stylesheet: fn [" << fn << "] data.size() " <<
            data.size() << " zip " << zip << " member " << member << "\n");
    if (member.empty()) {
        // Not a zip
        if (!fn.empty()) {
            res = file_scan(fn, &XMLdoc, 0, -1, &reason, md5p);
        } else {
            res = string_scan(data.c_str(), data.size(), &XMLdoc, &reason, md5p);
        }
    } else {
        res = zip_scan(zip, member, &XMLdoc);
    }
    if (!res) {
        LOGERR("MimeHandlerXslt::set_document_: scan failed for "<<
               fn << " " << member << " : " << reason << '\n');
        return false;
    }

    xmlDocPtr doc = XMLdoc.getDoc();
    if (nullptr == doc) {
        LOGERR("MimeHandlerXslt::set_document_: no parsed doc\n");
        return false;
    }
    xmlDocPtr transformed = xsltApplyStylesheet(ssp, doc, NULL);
    if (nullptr == transformed) {
        LOGERR("MimeHandlerXslt::set_document_: xslt transform failed\n");
        xmlFreeDoc(doc);
        return false;
    }
    xmlChar *outstr;
    int outlen;
    xsltSaveResultToString(&outstr, &outlen, transformed, ssp);
    result = string((const char*)outstr, outlen);
    xmlFree(outstr);
    xmlFreeDoc(transformed);
    xmlFreeDoc(doc);
    return true;
}

// Call apply_stylesheet() for the single file or data string, or, for a zip to all members which
// need processing.
bool MimeHandlerXslt::Internal::process_doc_or_string(
    bool forpreview, const string& fn, const string& data)
{
    p->m_metaData[cstr_dj_keycharset] = cstr_utf8;
    if (bodySS.empty()) {
        auto ssp = metaOrAllSS.find("");
        if (ssp == metaOrAllSS.end()) {
            LOGERR("MimeHandlerXslt::process: no style sheet !\n");
            return false;
        }
        string md5;
        if (apply_stylesheet(fn, data, std::shared_ptr<FileScanSourceZip>(), std::string(),
                             ssp->second, result, forpreview ? nullptr : &md5)) {
            if (!forpreview) {
                p->m_metaData[cstr_dj_keymd5] = md5;
            }
            return true;
        }
        return false;
    } else {
        result = "<html>\n<head>\n<meta http-equiv=\"Content-Type\""
            "content=\"text/html; charset=UTF-8\">";

        // We initialize the zip object once, and reuse it for all catalog or data accesses.
        std::shared_ptr<FileScanSourceZip> zip;
        std::string reason;
        if (fn.empty()) {
            zip = init_scan(data.c_str(), data.size(), &reason);
        } else {
            zip = init_scan(fn, &reason);
        }
        for (auto& member : metaMembers) {
            auto it = metaOrAllSS.find(member.second);
            if (it == metaOrAllSS.end()) {
                LOGERR("MimeHandlerXslt::process: no style sheet found for " <<
                       member.first << ":" << member.second << "!\n");
                return false;
            }
            auto names = nameList(zip, member.first);
            for (const auto& nm : names) {
                string part;
                if (!apply_stylesheet(fn, data, zip, nm, it->second, part, nullptr)) {
                    LOGERR("apply_stylesheet failed: " << reason << '\n');
                    return false;
                }
                result += part;
            }
        }
        result += "</head>\n<body>\n";
        
        for (auto& member : bodyMembers) {
            auto it = bodySS.find(member.second);
            if (it == bodySS.end()) {
                LOGERR("MimeHandlerXslt::process: no style sheet found for " <<
                       member.first << ":" << member.second << "!\n");
                return false;
            }
            auto names = nameList(zip, member.first);
            for (const auto& nm : names) {
                string part;
                if (!apply_stylesheet(fn, data, zip, nm, it->second, part, nullptr)) {
                    LOGERR("apply_stylesheet failed: " << reason << '\n');
                    return false;
                }
                if (forpreview)
                    result += std::string("<h2>") + nm + "</h2>";
                result += part;
            }
        }
        result += "</body></html>";
    }
    return true;
}

bool MimeHandlerXslt::set_document_file_impl(const string&, const string &fn)
{
    LOGDEB0("MimeHandlerXslt::set_document_file_: fn: " << fn << '\n');
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, fn, string());
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::set_document_string_impl(const string&, const string& txt)
{
    LOGDEB0("MimeHandlerXslt::set_document_string_\n");
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, string(), txt);
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::next_document()
{
    if (!m || !m->ok) {
        return false;
    }
    if (m_havedoc == false)
        return false;
    m_havedoc = false;
    m_metaData[cstr_dj_keymt] = cstr_texthtml;
    m_metaData[cstr_dj_keycontent].swap(m->result);
    LOGDEB1("MimeHandlerXslt::next_document: result: [" << m_metaData[cstr_dj_keycontent] << "]\n");
    return true;
}

void MimeHandlerXslt::clear_impl()
{
    m_havedoc = false;
    m->result.clear();
}
