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
#include "autoconfig.h"

#include "safeunistd.h"

#include <list>

#include <QMessageBox>
#include <QSettings>
#include <memory>

#include "qxtconfirmationmessage.h"

#include "log.h"
#include "fileudi.h"
#include "execmd.h"
#include "transcode.h"
#include "docseqhist.h"
#include "docseqdb.h"
#include "internfile.h"
#include "rclmain_w.h"
#include "rclzg.h"
#include "pathut.h"
#include "unacpp.h"
#include "uiprefs_w.h"
#include "cstr.h"

using namespace std;

// Browser list used if xdg-open fails for opening the help doc
static const vector<string> browser_list{
    "opera", "google-chrome", "chromium-browser",
    "palemoon", "iceweasel", "firefox", "konqueror", "epiphany"};

// Start native viewer or preview for input Doc. This is used to allow using recoll from another app
// (e.g. Unity Scope) to view embedded result docs (docs with an ipath). We act as a proxy to
// extract the data and start a viewer. The URLs are encoded as file://path#ipath
void RclMain::viewUrl()
{
    if (m_urltoview.isEmpty() || !rcldb)
        return;

    QUrl qurl(m_urltoview);
    LOGDEB("RclMain::viewUrl: Path [" << qs2path(qurl.path()) <<
           "] fragment [" << qs2path(qurl.fragment()) << "]\n");

    /* In theory, the url might not be for a file managed by the fs
       indexer so that the make_udi() call here would be
       wrong(). When/if this happens we'll have to hide this part
       inside internfile and have some url magic to indicate the
       appropriate indexer/identification scheme */
    string udi;
    fileUdi::make_udi(qs2path(qurl.path()), qs2path(qurl.fragment()), udi);
    
    Rcl::Doc doc;
    Rcl::Doc idxdoc; // idxdoc.idxi == 0 -> works with base index only
    if (!rcldb->getDoc(udi, idxdoc, doc) || doc.pc == -1)
        return;

    // StartNativeViewer needs a db source to call getEnclosing() on.
    Rcl::Query *query = new Rcl::Query(rcldb.get());
    DocSequenceDb *src = new DocSequenceDb(
        rcldb, std::shared_ptr<Rcl::Query>(query), "", std::make_shared<Rcl::SearchData>());
    m_source = std::shared_ptr<DocSequence>(src);


    // Start a native viewer if the mimetype has one defined, else a preview.
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    string viewer = theconfig->getMimeViewerDef(doc.mimetype, apptag, prefs.useDesktopOpen);
    if (viewer.empty()) {
        startPreview(doc);
    } else {
        hide();
        startNativeViewer(doc);
        // We have a problem here because xdg-open will exit
        // immediately after starting the command instead of waiting
        // for it, so we can't wait either and we don't know when we
        // can exit (deleting the temp file). As a bad workaround we
        // sleep some time then exit. The alternative would be to just
        // prevent the temp file deletion completely, leaving it
        // around forever. Better to let the user save a copy if he
        // wants I think.
        millisleep(60*1000);
        fileExit();
    }
}

/* Look for HTML browser. We make a special effort for html because it's
 * used for reading help. This is only used if the normal approach 
 * (xdg-open etc.) failed */
static bool lookForHtmlBrowser(string &exefile)
{
    const char *path = getenv("PATH");
    if (path == 0) {
        path = "/usr/local/bin:/usr/bin:/bin";
    }
    // Look for each browser 
    for (const auto& entry : browser_list) {
        if (ExecCmd::which(entry, exefile, path)) 
            return true;
    }
    exefile.clear();
    return false;
}

void RclMain::openWith(Rcl::Doc doc, string cmdspec)
{
    LOGDEB("RclMain::openWith: " << cmdspec << "\n");

    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmdspec, lcmd)) {
        QMessageBox::warning(0, "Recoll", tr("Bad desktop app spec for %1: [%2]\n"
                                             "Please check the desktop file")
                             .arg(u8s2qs(doc.mimetype)).arg(path2qs(cmdspec)));
        return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execname = lcmd.front();
    lcmd.erase(lcmd.begin());
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);

    // Try to keep the letters used more or less consistent with the reslist paragraph format.
    map<string, string> subs;
#ifdef _WIN32
    path_backslashize(fn);
#endif
    subs["F"] = fn;
    subs["f"] = fn;
    // Our file:// URLs are actually raw paths. Others should be proper URLs. Only encode file://..
    subs["U"] = beginswith(url, cstr_fileu) ? path_pcencode(url) : url;
    subs["u"] = url;

    execViewer(subs, false, execname, lcmd, cmdspec, doc);
}

static bool pagenumNeeded(const std::string& cmd)
{
    return cmd.find("%p") != std::string::npos;
}
static bool linenumNeeded(const std::string& cmd)
{
    return cmd.find("%l") != std::string::npos;
}
static bool termNeeded(const std::string& cmd)
{
    return cmd.find("%s") != std::string::npos;
}

void RclMain::startNativeViewer(Rcl::Doc doc, int pagenum, QString qterm, int linenum)
{
    std::string term = qs2utf8s(qterm);
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    LOGDEB("RclMain::startNativeViewer: mtype [" << doc.mimetype <<
           "] apptag ["  << apptag << "] page "  << pagenum << " term ["  <<
           term << "] url ["  << doc.url << "] ipath [" << doc.ipath << "]\n");

    // Look for appropriate viewer
    string cmdplusattr = theconfig->getMimeViewerDef(doc.mimetype, apptag, prefs.useDesktopOpen);
    if (cmdplusattr.empty()) {
        QMessageBox::warning(0, "Recoll", tr("No external viewer configured for mime type [")
                             + doc.mimetype.c_str() + "]");
        return;
    }
    LOGDEB("StartNativeViewer: viewerdef from config: " << cmdplusattr << "\n");

    // Separate command string and viewer attributes (if any)
    ConfSimple viewerattrs;
    string cmd;
    theconfig->valueSplitAttributes(cmdplusattr, cmd, viewerattrs);
    bool ignoreipath = false;
    int execwflags = 0;
    if (viewerattrs.get("ignoreipath", cmdplusattr))
        ignoreipath = stringToBool(cmdplusattr);
    if (viewerattrs.get("maximize", cmdplusattr)) {
        if (stringToBool(cmdplusattr)) {
            execwflags |= ExecCmd::EXF_MAXIMIZED;
        }
    }
    
    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmd, lcmd)) {
        QMessageBox::warning(0, "Recoll", tr("Bad viewer command line for %1: [%2]\n"
                                             "Please check the mimeview file")
            .arg(u8s2qs(doc.mimetype)).arg(path2qs(cmd)));
        return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execpath;
    if (!ExecCmd::which(lcmd.front(), execpath)) {
        execpath = theconfig->findFilter(lcmd.front());
        // findFilter returns its input param if the filter is not in
        // the normal places. As we already looked in the path, we
        // have no use for a simple command name here (as opposed to
        // mimehandler which will just let execvp do its thing). Erase
        // execpath so that the user dialog will be started further
        // down.
        if (!execpath.compare(lcmd.front())) 
            execpath.erase();

        // Specialcase text/html because of the help browser need
        if (execpath.empty() && !doc.mimetype.compare("text/html") && 
            apptag.empty()) {
            if (lookForHtmlBrowser(execpath)) {
                lcmd.clear();
                lcmd.push_back(execpath);
                lcmd.push_back("%u");
            }
        }
    }

    // Command not found: start the user dialog to help find another one:
    if (execpath.empty()) {
        QString mt = QString::fromUtf8(doc.mimetype.c_str());
        QString message = tr("The viewer specified in mimeview for %1: %2"
                             " is not found.\nDo you want to start the preferences dialog ?")
            .arg(mt).arg(path2qs(lcmd.front()));

        switch(QMessageBox::warning(0, "Recoll", message, 
                                    QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        case QMessageBox::Yes: 
            showUIPrefs();
            if (uiprefs)
                uiprefs->showViewAction(mt);
            break;
        case QMessageBox::No:
        default:
            break;
        }
        // The user will have to click on the link again to try the
        // new command.
        return;
    }
    // Get rid of the command name. lcmd is now argv[1...n]
    lcmd.erase(lcmd.begin());

    // Process the command arguments to determine if we need to create a temporary file.

    // If the command has a %i parameter it will manage the
    // un-embedding. Else if ipath is not empty, we need a temp file.
    // This can be overridden with the "ignoreipath" attribute
    bool groksipath = (cmd.find("%i") != string::npos) || ignoreipath;

    // We used to try being clever here, but actually, the only case
    // where we don't need a local file copy of the document (or
    // parent document) is the case of ??an HTML page?? with a non-file
    // URL (http or https). Trying to guess based on %u or %f is
    // doomed because we pass %u to xdg-open. 2023-01: can't see why the text/html
    // test any more. The type of URL should be enough ? Can't need a file if it's not file:// ?
    // If this does not work, we'll need an explicit attribute in the configuration.
    // Change needed for enabling an external indexer script, with, for example URLs like
    // joplin://x-callback-url/openNote?id=xxx and a non HTML MIME.
    bool wantsfile = false;
    bool wantsparentfile = cmd.find("%F") != string::npos;
    if (!wantsparentfile && (cmd.find("%f") != string::npos || urlisfileurl(doc.url))) {
        wantsfile = true;
    } 

    if (wantsparentfile && !urlisfileurl(doc.url)) {
        QMessageBox::warning(0, "Recoll", tr("Viewer command line for %1 specifies "
                                             "parent file but URL is not file:// : unsupported")
                             .arg(QString::fromUtf8(doc.mimetype.c_str())));
        return;
    }
    if (wantsfile && wantsparentfile) {
        QMessageBox::warning(0, "Recoll", tr("Viewer command line for %1 specifies both "
                                             "file and parent file value: unsupported")
                             .arg(QString::fromUtf8(doc.mimetype.c_str())));
        return;
    }
    
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);
    Rcl::Doc pdoc;
    if (wantsparentfile) {
        // We want the path for the parent document. For example to
        // open the chm file, not the internal page. Note that we just
        // override the other file name in this case.
        if (!m_source || !m_source->getEnclosing(doc, pdoc)) {
            QMessageBox::warning(0, "Recoll", tr("Cannot find parent document"));
            return;
        }
        // Override fn with the parent's : 
        fn = fileurltolocalpath(pdoc.url);

        // If the parent document has an ipath too, we need to create
        // a temp file even if the command takes an ipath
        // parameter. We have no viewer which could handle a double
        // embedding. Will have to change if such a one appears.
        if (!pdoc.ipath.empty()) {
            groksipath = false;
        }
    }

    // Can't remember what enterHistory was actually for. Set it to
    // true always for now
    bool enterHistory = true;
    bool istempfile = false;
    
    LOGDEB("StartNativeViewer: groksipath " << groksipath << " wantsf " <<
           wantsfile << " wantsparentf " << wantsparentfile << "\n");

    bool wantedfile_doc_has_ipath =
        (wantsfile && !doc.ipath.empty()) || (wantsparentfile && !pdoc.ipath.empty());
        
    // If the command wants a file but this is not a file url, or
    // there is an ipath that it won't understand, we need a temp file:
    theconfig->setKeyDir(fn.empty() ? "" : path_getfather(fn));
    if (((wantsfile || wantsparentfile) && fn.empty()) ||
        (!groksipath && wantedfile_doc_has_ipath) ) {
        TempFile temp;
        Rcl::Doc& thedoc = wantsparentfile ? pdoc : doc;
        if (!FileInterner::idocToFile(temp, string(), theconfig, thedoc)) {
            QMessageBox::warning(0, "Recoll",
                                 tr("Cannot extract document or create temporary file"));
            return;
        }
        enterHistory = true;
        istempfile = true;
        rememberTempFile(temp);
        fn = temp.filename();
        url = path_pathtofileurl(fn);
    }

    // If using an actual file, check that it exists, and if it is
    // compressed, we may need an uncompressed version
    if (!fn.empty() && theconfig->mimeViewerNeedsUncomp(doc.mimetype)) {
        if (!path_readable(fn)) {
            QMessageBox::warning(0, "Recoll", tr("Can't access file: ") + u8s2qs(fn));
            return;
        }
        TempFile temp;
        if (FileInterner::isCompressed(fn, theconfig)) {
            if (!FileInterner::maybeUncompressToTemp(temp, fn, theconfig,doc)) {
                QMessageBox::warning(0, "Recoll", tr("Can't uncompress file: ") + path2qs(fn));
                return;
            }
        }
        if (temp.ok()) {
            istempfile = true;
            rememberTempFile(temp);
            fn = temp.filename();
            url = path_pathtofileurl(fn);
        }
    }

    if (istempfile) {
        QxtConfirmationMessage confirm(
            QMessageBox::Warning, "Recoll",
            tr("Opening a temporary copy. Edits will be lost if you don't save"
               "<br/>them to a permanent location."),
            tr("Do not show this warning next time (use GUI preferences to restore)."));
        confirm.setOverrideSettingsKey("/Recoll/prefs/showTempFileWarning");
        confirm.exec();
        QSettings settings;
        prefs.showTempFileWarning = settings.value("/Recoll/prefs/showTempFileWarning").toInt();
    }

    // If we are not called with a page number (which would happen for a call from the snippets
    // window), see if we can compute a page number anyway. 
    if (m_source &&
        ((pagenum == -1 && pagenumNeeded(cmd)) || (term.empty() && termNeeded(cmd)))) {
        pagenum = m_source->getFirstMatchPage(doc, term);
    }
    if (pagenum < 0)
        pagenum = 1;

    if (linenum < 1 && m_source && !term.empty() && linenumNeeded(cmd)) {
        if (doc.text.empty()) {
            rcldb->getDocRawText(doc);
        }
        linenum = m_source->getFirstMatchLine(doc, term);
    }
    if (linenum < 0)
        linenum = 1;

    // Substitute %xx inside arguments
    string efftime;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        efftime = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    } else {
        efftime = "0";
    }
    // Try to keep the letters used more or less consistent with the reslist
    // paragraph format.
    map<string, string> subs;
    subs["D"] = efftime;
#ifdef _WIN32
    path_backslashize(fn);
#endif
    subs["f"] = fn;
    subs["F"] = fn;
    subs["i"] = FileInterner::getLastIpathElt(doc.ipath);
    subs["l"] = ulltodecstr(linenum);
    subs["M"] = doc.mimetype;
    subs["p"] = ulltodecstr(pagenum);
    subs["s"] = term;
    // Our file:// URLs are actually raw paths. Others should be proper URLs. Only encode file://..
    subs["U"] = beginswith(url, cstr_fileu) ? path_pcencode(url) : url;
    subs["u"] = url;
    // Let %(xx) access all metadata.
    for (const auto& ent :doc.meta) {
        subs[ent.first] = ent.second;
    }
    execViewer(subs, enterHistory, execpath, lcmd, cmd, doc, execwflags);
}

void RclMain::execViewer(
    const map<string, string>& subs, bool enterHistory, const string& execpath,
    const vector<string>& _lcmd, const string& cmd, Rcl::Doc doc, int flags)
{
    vector<string> lcmd;
    for (const auto& oparm : _lcmd) {
        string nparm;
        pcSubst(oparm, nparm, subs);
        LOGDEB0("" << oparm << "->"  << nparm << "\n");
        lcmd.push_back(nparm);
    }

    // Also substitute inside the unsplit command line for display in status bar
    string ncmd;
    pcSubst(cmd, ncmd, subs);

#ifndef _WIN32
    ncmd += " &";
#endif
    QStatusBar *stb = statusBar();
    if (stb) {
        string prcmd;
#ifdef _WIN32
        prcmd = ncmd;
#else
        string fcharset = theconfig->getDefCharset(true);
        transcode(ncmd, prcmd, fcharset, cstr_utf8);
#endif
        QString msg = tr("Executing: [") + u8s2qs(prcmd) + "]";
        stb->showMessage(msg, 10000);
    }

    if (enterHistory)
        historyEnterDoc(rcldb.get(), g_dynconf, doc);
    
    // Do the zeitgeist thing
    zg_send_event(ZGSEND_OPEN, doc);

    // We keep pushing back and never deleting. This can't be good...
    ExecCmd *ecmd = new ExecCmd(ExecCmd::EXF_SHOWWINDOW | flags);
    m_viewers.push_back(ecmd);
    ecmd->startExec(execpath, lcmd, false, false);
}

void RclMain::startManual()
{
    startManual(string());
}

void RclMain::startManual(const string& index)
{
    string docdir = path_cat(theconfig->getDatadir(), "doc");

    // The single page user manual is nicer if we have an index. Else
    // the webhelp one is nicer if it is present
    string usermanual = path_cat(docdir, "usermanual.html");
    string webhelp = path_cat(docdir, "webhelp");
    webhelp = path_cat(webhelp, "index.html");
    bool has_wh = path_exists(webhelp);
    
    LOGDEB("RclMain::startManual: help index is " << (index.empty() ? "(null)" : index) << "\n");

#ifndef _WIN32
    // On Windows I could not find any way to pass the fragment through rclstartw (tried to set
    // text/html as exception with rclstartw %u).
    if (!index.empty()) {
        usermanual += "#";
        usermanual += index;
    }
#endif
    
    Rcl::Doc doc;
    if (has_wh && index.empty()) {
        doc.url = path_pathtofileurl(webhelp);
    } else {
        doc.url = path_pathtofileurl(usermanual);
    }
    doc.mimetype = "text/html";
    doc.meta[Rcl::Doc::keyapptg] = "rclman";
    startNativeViewer(doc);
}

void RclMain::startOnlineManual()
{
    Rcl::Doc doc;
    doc.url = "https://www.recoll.org/usermanual/webhelp/docs/index.html";
    doc.mimetype = "text/html";
    startNativeViewer(doc);
}
