/* Copyright (C) 2007-2019 J.F.Dockes
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#include <sstream>
#include <iostream>
#include <list>

#include "cstr.h"
#include "reslistpager.h"
#include "log.h"
#include "rclconfig.h"
#include "smallut.h"
#include "rclutil.h"
#include "plaintorich.h"
#include "mimehandler.h"
#include "transcode.h"
#include "pathut.h"
#include "execmd.h"

using std::list;
using std::map;
using std::string;
using std::vector;

// Default highlighter. No need for locking, this is query-only.
static const string cstr_hlfontcolor("<span style='color: blue;'>");
static const string cstr_hlendfont("</span>");
class PlainToRichHtReslist : public PlainToRich {
public:
    virtual string startMatch(unsigned int) override {
        return cstr_hlfontcolor;
    }
    virtual string endMatch() override {
        return cstr_hlendfont;
    }
};
static PlainToRichHtReslist g_hiliter;

ResListPager::ResListPager(RclConfig *cnf, int pagesize, bool alwaysSnippets) 
    : m_pagesize(pagesize),
      m_alwaysSnippets(alwaysSnippets),
      m_newpagesize(pagesize),
      m_resultsInCurrentPage(0),
      m_winfirst(-1),
      m_hasNext(true),
      m_hiliter(&g_hiliter)
{
    cnf->getConfParam("thumbnailercmd", &m_thumbnailercmd);
}

void ResListPager::resultPageNext()
{
    if (!m_docSource) {
        LOGDEB("ResListPager::resultPageNext: null source\n");
        return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB("ResListPager::resultPageNext: rescnt " << resCnt << ", winfirst " << m_winfirst << "\n");

    if (m_winfirst < 0) {
        m_winfirst = 0;
    } else {
        m_winfirst += int(m_respage.size());
    }
    // Get the next page of results. Note that we look ahead by one to
    // determine if there is actually a next page
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize + 1, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize + 1);

    // Get rid of the possible excess result
    if (pagelen == m_pagesize + 1) {
        npage.resize(m_pagesize);
        pagelen--;
    }

    if (pagelen <= 0) {
        // No results ? This can only happen on the first page or if the actual result list size is
        // a multiple of the page pref (else there would have been no Next on the last page)
        if (m_winfirst > 0) {
            // Have already results. Let them show, just disable the Next button. We'd need to
            // remove the Next link from the page too.
            // Restore the m_winfirst value, let the current result vector alone
            m_winfirst -= int(m_respage.size());
        } else {
            // No results at all (on first page)
            m_winfirst = -1;
        }
        return;
    }
    m_resultsInCurrentPage = pagelen;
    m_respage = npage;
}
static string maybeEscapeHtml(const string& fld)
{
    if (fld.compare(0, cstr_fldhtm.size(), cstr_fldhtm))
        return escapeHtml(fld);
    else
        return fld.substr(cstr_fldhtm.size());
}


void ResListPager::resultPageFor(int docnum)
{
    if (!m_docSource) {
        LOGDEB("ResListPager::resultPageFor: null source\n");
        return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB("ResListPager::resultPageFor(" << docnum << "): rescnt " <<
           resCnt << ", winfirst " << m_winfirst << "\n");
    m_winfirst = (docnum / m_pagesize) * m_pagesize;

    // Get the next page of results.
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize);

    if (pagelen <= 0) {
        m_winfirst = -1;
        return;
    }
    m_respage = npage;
}

static std::string snipsToText(const std::vector<std::string> snippets, const std::string sep)
{
    std::string text;
    for (const auto& snippet : snippets) {
        if (!snippet.empty()) {
            text += snippet;
            text += sep;
        }
    }
    return text;
}

std::string ResListPager::href(const std::string& url, const std::string& txt)
{
    static std::string ahref("<a href=\"");
    return ahref + linkPrefix() + url + "\">" + txt + "</a>";
}

static const std::string nbsp2("&nbsp;&nbsp;");
static const SimpleRegexp pagenumre("(^ *\\[[pP]\\.* [0-9]+])", 0);

void ResListPager::displayDoc(RclConfig *config, int i, Rcl::Doc& doc, 
                              const HighlightData&, const string& sh)
{
    std::string chunk;
    chunk.reserve(2048);
    
    // Determine icon to display if any
    string iconurl = iconUrl(config, doc);
    
    // Printable url: either utf-8 if transcoding succeeds, or url-encoded
    string url;
    printableUrl(config->getDefCharset(), doc.url, url);

    // Same as url, but with file:// possibly stripped. output by %u instead of %U. 
    string urlOrLocal;
    urlOrLocal = fileurltolocalpath(url);
    if (urlOrLocal.empty())
        urlOrLocal = url;

    // Make title out of file name if none yet
    string titleOrFilename;
    string utf8fn;
    doc.getmeta(Rcl::Doc::keytt, &titleOrFilename);
    doc.getmeta(Rcl::Doc::keyfn, &utf8fn);
    if (utf8fn.empty()) {
        utf8fn = path_getsimple(url);   
    }
    if (titleOrFilename.empty()) {
        titleOrFilename = utf8fn;
    }

    // Url for the parent directory. We strip the file:// part for local paths
    string parenturl = url_parentfolder(url);
    {
        string localpath = fileurltolocalpath(parenturl);
        if (!localpath.empty())
            parenturl = localpath;
    }

    // Result number
    int docnumforlinks = m_winfirst + 1 + i;
    std::string numbuf = std::to_string(docnumforlinks);

    // Document date: either doc or file modification times
    string datebuf;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        time_t mtime = doc.dmtime.empty() ? atoll(doc.fmtime.c_str()) : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        datebuf = utf8datestring(dateFormat(), tm);
    }

    // Size information. We print both doc and file if they differ a lot
    int64_t fsize = -1, dsize = -1;
    if (!doc.dbytes.empty())
        dsize = static_cast<int64_t>(atoll(doc.dbytes.c_str()));
    if (!doc.fbytes.empty())
        fsize =  static_cast<int64_t>(atoll(doc.fbytes.c_str()));
    string sizebuf;
    if (dsize > 0) {
        sizebuf = displayableBytes(dsize);
        if (fsize > 10 * dsize && fsize - dsize > 1000)
            sizebuf += string(" / ") + displayableBytes(fsize);
    } else if (fsize >= 0) {
        sizebuf = displayableBytes(fsize);
    }

    // "abstract" made out of text fragments taken around the search terms.
    bool needsnipabs = parFormat().find("%s") != string::npos;
    std::string snippetstext;
    if (needsnipabs) {
        vector<string> snippets;
        m_hiliter->set_inputhtml(false);
        m_docSource->getAbstract(doc, m_hiliter, snippets, true);
        snippetstext = snipsToText(snippets, absSep());
    }
        
    string abstract;
    bool needabstract = parFormat().find("%A") != string::npos;
    if (needabstract && m_docSource) {
        if (needsnipabs) {
            // Then we already did the snippets thing, and we'll display it. Make this the doc
            // abstract if it's set. All this nonsense is to preserve compat with old paragraph
            // formats. Note that in this case we never display the beginning of doc bogus abstract.
            abstract = doc.syntabs ? std::string() : doc.meta[Rcl::Doc::keyabs];
        } else {
            vector<string> snippets;
            m_hiliter->set_inputhtml(false);
            m_docSource->getAbstract(doc, m_hiliter, snippets);
            abstract = snipsToText(snippets, absSep());
        }
    }

    // Links; Uses utilities from mimehandler.h
    std::string linksbuf;
    if (canIntern(&doc, config)) { 
        linksbuf = href(std::string("P") + std::to_string(docnumforlinks), trans("Preview")) + nbsp2;
    }
    if (canOpen(&doc, config, useAll())) {
        linksbuf += href(std::string("E") + std::to_string(docnumforlinks), trans("Open"));
    }
    std::string snipsbuf;
    if (m_alwaysSnippets || doc.haspages) {
        snipsbuf = href(std::string("A") + std::to_string(docnumforlinks), trans("Snippets")) +
            nbsp2;
        linksbuf += nbsp2 + snipsbuf;
    }

    std::string collapscnt;
    if (doc.getmeta(Rcl::Doc::keycc, &collapscnt) && !collapscnt.empty()) {
        int clc = atoi(collapscnt.c_str()) + 1;
        linksbuf += nbsp2 + href(std::string("D") + std::to_string(docnumforlinks),
                                 trans("Dups") + "(" + std::to_string(clc) + ")") + nbsp2;
    }

    // Build the result list paragraph:

    // Subheader: this is used by history
    if (!sh.empty())
        chunk += std::string("<p style='clear: both;'><b>") + sh + "</p>\n<p>";
    else
        chunk += "<p style='margin: 0px;padding: 0px;clear: both;'>";

    std::string xdocidbuf = std::to_string(doc.xdocid);
    
    // Configurable stuff
    map<string, string> subs;
    subs["A"] = abstract;
    subs["D"] = datebuf;
    subs["E"] = snipsbuf;
    subs["I"] = iconurl;
    subs["i"] = doc.ipath;
    subs["K"] = !doc.meta[Rcl::Doc::keykw].empty() ?
        string("[") + maybeEscapeHtml(doc.meta[Rcl::Doc::keykw]) + "]" : "";
    subs["L"] = linksbuf;
    subs["M"] = doc.mimetype;
    subs["N"] = numbuf;
    subs["P"] = parenturl;
    subs["R"] = doc.meta[Rcl::Doc::keyrr];
    subs["S"] = sizebuf;
    subs["s"] = snippetstext;
    subs["T"] = maybeEscapeHtml(titleOrFilename);
    subs["t"] = maybeEscapeHtml(doc.meta[Rcl::Doc::keytt]);
    subs["U"] = url;
    subs["u"] = urlOrLocal;
    subs["x"] = xdocidbuf;
    
    // Let %(xx) access all metadata. HTML-neuter everything:
    for (const auto& entry : doc.meta) {
        if (!entry.first.empty()) 
            subs[entry.first] = maybeEscapeHtml(entry.second);
    }

    string formatted;
    pcSubst(parFormat(), formatted, subs);
    chunk += formatted + "</p>\n";
    
    LOGDEB2("Chunk: [" << chunk << "]\n");
    append(chunk, i, doc);
}

bool ResListPager::getDoc(int num, Rcl::Doc& doc)
{
    if (m_winfirst < 0 || m_respage.size() == 0)
        return false;
    if (num < m_winfirst || num >= m_winfirst + int(m_respage.size()))
        return false;
    doc = m_respage[num-m_winfirst].doc;
    return true;
}

void ResListPager::displayPage(RclConfig *config)
{
    LOGDEB("ResListPager::displayPage. linkPrefix: " << linkPrefix() << "\n");
    if (!m_docSource) {
        LOGDEB("ResListPager::displayPage: null source\n");
        return;
    }
    if (m_winfirst < 0 && !pageEmpty()) {
        LOGDEB("ResListPager::displayPage: sequence error: winfirst < 0\n");
        return;
    }

    std::string chunk;
    
    // Display list header
    // We could use a <title> but the textedit doesnt display
    // it prominently
    // Note: have to append text in chunks that make sense
    // html-wise. If we break things up too much, the editor
    // gets confused. Hence the use of the 'chunk' text
    // accumulator
    // Also note that there can be results beyond the estimated resCnt.
    chunk = std::string("<html><head>\n")
        + "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\n"
        + headerContent()
        + "</head><body " + bodyAttrs() + ">\n"
        + pageTop()
        + "<p><span style=\"font-size:110%;\"><b>"
        + m_docSource->title()
        + "</b></span>&nbsp;&nbsp;&nbsp;";

    if (pageEmpty()) {
        chunk += trans("<p><b>No results found</b><br>");
        string reason = m_docSource->getReason();
        if (!reason.empty()) {
            chunk += std::string("<blockquote>") + escapeHtml(reason) + "</blockquote></p>";
        } else {
            HighlightData hldata;
            m_docSource->getTerms(hldata);
            vector<string> uterms(hldata.uterms.begin(), hldata.uterms.end());
            if (!uterms.empty()) {
                map<string, vector<string> > spellings;
                suggest(uterms, spellings);
                if (!spellings.empty()) {
                    if (o_index_stripchars) {
                        chunk += trans("<p><i>Alternate spellings (accents suppressed): </i>")
                              + "<br /><blockquote>";
                    } else {
                        chunk += trans("<p><i>Alternate spellings: </i>") + "<br /><blockquote>";
                    }

                    for (const auto& entry: spellings) {
                        chunk += std::string("<b>") + entry.first + "</b> : ";
                        for (const auto& spelling : entry.second) {
                            chunk += spelling + " ";
                        }
                        chunk += "<br />";
                    }
                    chunk += "</blockquote></p>";
                }
            }
        }
    } else {
        HighlightData hldata;
        m_docSource->getTerms(hldata);
        if (!hldata.spellexpands.empty()) {
            string msg;
            if (hldata.spellexpands.size() == 1) {
                msg = trans("This spelling guess was added to the search:");
            } else {
                msg = trans("These spelling guesses were added to the search:");
            }
            chunk += std::string("<br><i>") + msg + "</i> " +
                stringsToString(hldata.spellexpands) + "<br/>\n";
        }
        unsigned int resCnt = m_docSource->getResCnt();
        if (m_winfirst + m_respage.size() < resCnt) {
            chunk += trans("Documents") + " <b>" + std::to_string(m_winfirst + 1) + "-" +
                std::to_string(m_winfirst + m_respage.size()) + "</b> " + trans("out of at least") +
                " " + std::to_string(resCnt) + " " + trans("for") + " " ;
        } else {
            chunk += trans("Documents") + " <b>" + std::to_string(m_winfirst + 1) + "-" +
                std::to_string(m_winfirst + m_respage.size())   + "</b> " + trans("for") + " ";
        }
    }
    chunk += detailsLink();
    if (hasPrev() || hasNext()) {
        chunk += nbsp2;
        if (hasPrev()) {
            chunk += href(prevUrl(), std::string("<b>") + trans("Previous") + "</b>") + nbsp2;
        }
        if (hasNext()) {
            chunk += href(nextUrl(), std::string("<b>") + trans("Next") + "</b>");
        }
    }
    chunk += "</p>\n";

    append(chunk);
    chunk.clear();
    if (pageEmpty())
        return;

    HighlightData hdata;
    m_docSource->getTerms(hdata);

    // Emit data for result entry paragraph. Do it in chunks that make sense
    // html-wise, else our client may get confused
    for (int i = 0; i < (int)m_respage.size(); i++) {
        Rcl::Doc& doc(m_respage[i].doc);
        string& sh(m_respage[i].subHeader);
        displayDoc(config, i, doc, hdata, sh);
    }

    // Footer
    chunk += "<p align=\"center\">";
    if (hasPrev() || hasNext()) {
        if (hasPrev()) {
            chunk += href(prevUrl(), std::string("<b>") + trans("Previous") + "</b>") + nbsp2;
        }
        if (hasNext()) {
            chunk += href(nextUrl(), std::string("<b>") + trans("Next") + "</b>");
        }
    }
    chunk += "</p>\n</body></html>\n";
    append(chunk);
    flush();
}

void ResListPager::displaySingleDoc(
    RclConfig *config, int idx, Rcl::Doc& doc, const HighlightData& hdata)
{
    std::string chunk;

    // Header
    // Note: have to append text in chunks that make sense html-wise. If we break things up too
    // much, the editor gets confused.
    string bdtag("<body ");
    bdtag += bodyAttrs();
    rtrimstring(bdtag, " ");
    bdtag += ">";
    chunk += std::string("<html><head>\n") +
        "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\n" +
        headerContent() + "</head>\n" + bdtag + "\n";
    append(chunk);
    // Document
    displayDoc(config, idx, doc, hdata, string());
    // Footer 
    append("</body></html>\n");
    flush();
}


// Default implementations for things that should be implemented by 
// specializations
string ResListPager::nextUrl()
{
    return "n-1";
}

string ResListPager::prevUrl()
{
    return "p-1";
}

string ResListPager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    // If this is a top level doc, check for a thumbnail image
    if (doc.ipath.empty()) {
        vector<string> paths;
        Rcl::docsToPaths({doc}, paths);
        if (!paths.empty()) {
            string path;
            string url = cstr_fileu + paths[0];
            LOGDEB2("ResList::iconUrl: source path [" << paths[0] << "]\n");
            if (thumbPathForUrl(url, 128, path)) {
                LOGDEB2("ResList::iconUrl: icon path [" << path << "]\n");
                return cstr_fileu + path;
            } else {
                LOGDEB2("ResList::iconUrl: no icon: path [" << path << "]\n");
                if (!m_thumbnailercmd.empty()) {
                    std::string thumbpath;
                    thumbPathForUrl(url, 128, thumbpath);
                    ExecCmd cmd;
                    std::vector<std::string> cmdvec{m_thumbnailercmd};
                    cmdvec.push_back(url);
                    cmdvec.push_back(doc.mimetype);
                    cmdvec.push_back("128");
                    cmdvec.push_back(thumbpath);
                    if (cmd.doexec(cmdvec) == 0) {
                        if (thumbPathForUrl(url, 128, path)) {
                            LOGDEB2("ResList::iconUrl: icon path [" << path << "]\n");
                            return cstr_fileu + path;
                        }
                    }
                }
            }
        } else {
            LOGDEB("ResList::iconUrl: docsToPaths failed\n");
        }
    }

    // No thumbnail, look for the MIME type icon.
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    return path_pathtofileurl(config->getMimeIconPath(doc.mimetype, apptag));
}

bool ResListPager::append(const string& data)
{
    fprintf(stderr, "%s", data.c_str());
    return true;
}

string ResListPager::trans(const string& in) 
{
    return in;
}

string ResListPager::detailsLink()
{
    string chunk = href(std::string("H-1"), trans("(show query)"));
    return chunk;
}

const string &ResListPager::parFormat()
{
    static const string cstr_format("<img src=\"%I\" align=\"left\">"
                                    "%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
                                    "%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
                                    "%A %K");
    return cstr_format;
}

const string &ResListPager::dateFormat()
{
    static const string cstr_format("&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z");
    return cstr_format;
}
