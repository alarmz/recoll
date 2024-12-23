/* Copyright (C) 2005-2023 J.F.Dockes
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

#include <cstdlib>
#include <list>
#include <iostream>
#include <memory>

#include <QApplication>
#ifdef QAPPLICATION_CLASS
// QAPPLICATION_CLASS is defined by singleapplication.pri
#include "singleapplication/singleapplication.h"
#endif
#include <qtranslator.h>
#include <qtimer.h>
#include <qthread.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <QLocale>
#include <QLibraryInfo>
#include <QFileDialog>
#include <QUrl>
#include <QNetworkProxy>

#include "rcldb.h"
#include "rclutil.h"
#include "rclconfig.h"
#include "pathut.h"
#include "recoll.h"
#include "smallut.h"
#include "rclinit.h"
#include "log.h"
#include "rclmain_w.h"
#include "ssearch_w.h"
#include "guiutils.h"
#include "smallut.h"
#include "readfile.h"
#include "uncomp.h"
#include "cstr.h"
#include "dynconf.h"
#include "recollq.h"

using std::string;
using std::list;
using std::vector;

extern RclConfig *theconfig;
AdvSearchHist *g_advshistory;

std::mutex thetempfileslock;
// Use a list not a vector so that contained objects have stable
// addresses when extending.
static list<TempFile>  o_tempfiles;
/* Keep an array of temporary files for deletion at exit. It happens that we
   erase some of them before exiting (ie: when closing a preview tab), we don't 
   reuse the array holes for now */
TempFile *rememberTempFile(TempFile temp)
{
    std::unique_lock<std::mutex> locker(thetempfileslock);
    o_tempfiles.push_back(temp);
    return &o_tempfiles.back();
}    

void forgetTempFile(string &fn)
{
    if (fn.empty())
        return;
    std::unique_lock<std::mutex> locker(thetempfileslock);
    for (auto& entry : o_tempfiles) {
        if (entry.ok() && !fn.compare(entry.filename())) {
            entry = TempFile();
        }
    }
    fn.erase();
}    

void deleteAllTempFiles()
{
    std::unique_lock<std::mutex> locker(thetempfileslock);
    o_tempfiles.clear();
    Uncomp::clearcache();
}

std::shared_ptr<Rcl::Db> rcldb;
int recollNeedsExit;
RclMain *mainWindow;

void startManual(const string& helpindex)
{
    if (mainWindow)
        mainWindow->startManual(helpindex);
}

const vector<string> *getCurrentExtraDbs()
{
    auto edbs = &prefs.activeExtraDbs;
    if (prefs.useTmpActiveExtraDbs) {
        edbs = &prefs.tmpActiveExtraDbs;
    }
    return edbs;
}

bool maybeOpenDb(string &reason, bool force, bool *maindberror)
{
    LOGDEB1("maybeOpenDb: force " << force << "\n");

    if (force || nullptr == rcldb) {
        rcldb = std::make_shared<Rcl::Db>(theconfig);
    }
    rcldb->rmQueryDb("");
    auto edbs = getCurrentExtraDbs();
    if (!edbs->empty()) {
        rcldb->setExtraQueryDbs(*edbs);
    }
    
    Rcl::Db::OpenError error;
    if (!rcldb->isopen() && !rcldb->open(Rcl::Db::DbRO, &error)) {
        reason = "Could not open database";
        if (maindberror) {
            reason +=  " in " +  theconfig->getDbDir() + " : " + rcldb->getReason();
            *maindberror = (error == Rcl::Db::DbOpenMainDb) ? true : false;
        }
        return false;
    }
    rcldb->setAbstractParams(-1, prefs.syntAbsLen, prefs.syntAbsCtx);
    rcldb->setUseSpellFuzz(prefs.autoSpell);
    rcldb->setMaxSpellDist(prefs.autoSpellMaxDist);
    return true;
}

// Retrieve the list currently active stemming languages. We try to
// get this from the db, as some may have been added from recollindex
// without changing the config. If this fails, use the config. This is
// used for setting up choice menus, not updating the configuration.
bool getStemLangs(vector<string>& vlangs)
{
    // Try from db
    string reason;
    if (maybeOpenDb(reason, false)) {
        vlangs = rcldb->getStemLangs();
        LOGDEB0("getStemLangs: from index: " << stringsToString(vlangs) <<"\n");
        return true;
    } else {
        // Cant get the langs from the index. Maybe it just does not
        // exist yet. So get them from the config
        string slangs;
        if (theconfig->getConfParam("indexstemminglanguages", slangs)) {
            stringToStrings(slangs, vlangs);
            return true;
        }
        return false;
    }
}

// This is never called because we _Exit() in rclmain_w.cpp
static void recollCleanup()
{
    LOGDEB2("recollCleanup: closing database\n" );
    rcldb.reset();
    deleteZ(theconfig);

    deleteAllTempFiles();
    LOGDEB2("recollCleanup: done\n" );
}

void applyStyleSheet(const QString& qssfn)
{
    auto comfn = path_cat(path_cat(theconfig->getDatadir(), "examples"), "recoll-common.qss");
    std::string qss;
    file_to_string(comfn, qss);
    if (!qssfn.isEmpty()) {
        LOGDEB0("Using custom style sheet: [" << qs2path(qssfn) << "]\n");
        string customqss;
        file_to_string(qs2path(qssfn), customqss);
        qss += customqss;
    }
    qss = prefs.scaleFonts(qss, prefs.wholeuiscale);
    qApp->setStyleSheet(u8s2qs(qss));
    prefs.checkAppFont();
}

#ifdef QAPPLICATION_CLASS
#ifdef Q_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void raiseWidget(QWidget* widget) {
#ifdef Q_OS_WINDOWS
    HWND hwnd = (HWND)widget->winId();
    // check if widget is minimized to Windows task bar
    if (::IsIconic(hwnd)) {
        ::ShowWindow(hwnd, SW_RESTORE);
    }
    ::SetForegroundWindow(hwnd);
#else
    widget->show();
    widget->raise();
    widget->activateWindow();
#endif
}
#endif // QAPPLICATION_CLASS

extern void qInitImages_recoll();

static const char *thisprog;

// BEWARE COMPATIBILITY WITH recollq OPTIONS letters
static int    op_flags;
#define OPT_a 0x1   
#define OPT_c 0x2       
#define OPT_f 0x4       
#define OPT_h 0x8       
#define OPT_L 0x10      
#define OPT_l 0x20      
#define OPT_o 0x40      
#define OPT_q 0x80      
#define OPT_t 0x100      
#define OPT_v 0x200     
#define OPT_W 0x400
#define OPT_w 0x800

static const char usage [] =
    "\n"
    "recoll [-h] [-c <configdir>] [-q query]\n"
    "  -h : Print help and exit\n"
    "  -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
    "  -L <lang> : force language for GUI messages (e.g. -L fr)\n"
    "  [-o|l|f|a] [-t] -q 'query' : search query to be executed as if entered\n"
    "      into simple search. The default is to interpret the argument as a \n"
    "      query language string (but see modifier options)\n"
    "      In most cases, the query string should be quoted with single-quotes to\n"
    "      avoid shell interpretation\n"
    "     -a : the query will be interpreted as an AND query.\n"
    "     -o : the query will be interpreted as an OR query.\n"
    "     -f : the query will be interpreted as a filename search\n"
    "     -l : the query will be interpreted as a query language string (default)\n"
    "  -t : terminal display: no gui. Results go to stdout. MUST be given\n"
    "       explicitly as -t (not ie, -at), and -q <query> MUST\n"
    "       be last on the command line if this is used.\n"
    "       Use -t -h to see the additional non-gui options\n"
    "  -w : open minimized\n"
    "  -W : do not open at all, only system tray icon (needs system tray!)\n"
    "recoll -v : print version\n"
    "recoll <url>\n"
    "   This is used to open a recoll url (including an ipath), and called\n"
    "   typically from another search interface like the Unity Dash\n"
    ;
static void
Usage(void)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s\n", Rcl::version_string().c_str());
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    exit((op_flags & OPT_h)==0);
}

// Extract the recoll_confdir from the args or env. Done early to setup the singleapp before the
// full args processing, because we need it before creating the qapplication.
static std::string get_config(int argc, char **argv)
{
    std::string aconf;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-c")) {
            if (i < argc-1) {
                aconf = argv[++i];
                break;
            }
        }
    }
    if (!aconf.empty())
        aconf = path_canon(aconf);
    std::string econf;
    auto cp = getenv("RECOLL_CONFDIR");
    if (cp) {
        econf = path_canon(cp);
    }
    return aconf.empty() ? econf : aconf;
}

int main(int argc, char **argv)
{
    pathut_setargv0(argv[0]);
    
    // if we are named recollq or option "-t" is present at all, we
    // don't do the GUI thing and pass the whole to recollq for
    // command line / pipe usage.
    if (path_getsimple(argv[0]) == "recollq")
        exit(recollq(&theconfig, argc, argv));
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t")) {
            exit(recollq(&theconfig, argc, argv));
        }
    }

    QCoreApplication::setOrganizationName("Recoll.org");
    QCoreApplication::setApplicationName("recoll");

    // Do this very early because of the singleapplication choice. We'll have to do it again once we
    // have a db config
    rwSettings(false);

#ifdef QAPPLICATION_CLASS
    // If the pref for single app is not set, then allow further instances.
    bool allowsecund = !prefs.singleapp;
#ifdef _WIN32
    // On Windows, allow further instances even if prefs.singleapp is set, but then the
    // secundary will just ping the primary and exit.
    allowsecund = true;
#endif // _WIN32
    auto confforsa = get_config(argc, argv);
    SingleApplication app(argc, argv, allowsecund, SingleApplication::User, 1000, u8s2qs(confforsa));
#ifdef _WIN32
    if (prefs.singleapp && app.isSecondary()) {
        AllowSetForegroundWindow(DWORD(app.primaryPid()));
        app.sendMessage("RAISE_WIDGET");
        return 0;
    }
#endif // _WIN32
#else
    QApplication app(argc, argv);
#endif

#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)
    // Prevent WebEngine HTTP connections by setting a bogus proxy. This is for preventing network
    // accesses from webengine when it is used to implement the preview window: depending on the
    // loaded document, these could be unsafe. Otoh this also means that the display will be less
    // nice than it could be if we let the engine load necessary javascript etc.  Note that this may
    // still be less safe than using a QTextBrowser (or webkit probably) for the Preview.
    QNetworkProxy proxy;
    proxy.setType(QNetworkProxy::Socks5Proxy);
    proxy.setHostName("127.0.0.1");
    proxy.setUser("recoll");
    proxy.setPassword("");
    QNetworkProxy::setApplicationProxy(proxy);
#endif // Real browser engines
    
    string a_config;
    string a_lang;
    string question;
    string urltoview;

    // Avoid disturbing argc and argv. Especially, setting argc to 0
    // prevents WM_CLASS to be set from argv[0] (it appears that qt
    // keeps a ref to argc, and that it is used at exec() time to set
    // WM_CLASS from argv[0]). Curiously, it seems that the argv
    // pointer can be modified without consequences, but we use a copy
    // to play it safe
    int myargc = argc;
    char **myargv = argv;
    thisprog = myargv[0];
    myargc--; myargv++;
    
    while (myargc > 0 && **myargv == '-') {
        (*myargv)++;
        if (!(**myargv))
            Usage();
        while (**myargv)
            switch (*(*myargv)++) {
            case 'a': op_flags |= OPT_a; break;
            case 'c':   op_flags |= OPT_c; if (myargc < 2)  Usage();
                a_config = *(++myargv);
                myargc--; goto b1;
            case 'f': op_flags |= OPT_f; break;
            case 'h': op_flags |= OPT_h; Usage();break;
            case 'L':   op_flags |= OPT_L; if (myargc < 2)  Usage();
                a_lang = *(++myargv);
                myargc--; goto b1;
            case 'l': op_flags |= OPT_l; break;
            case 'o': op_flags |= OPT_o; break;
            case 'q':   op_flags |= OPT_q; if (myargc < 2)  Usage();
                question = *(++myargv);
                myargc--; goto b1;
            case 't': op_flags |= OPT_t; break;
            case 'v': op_flags |= OPT_v;
                fprintf(stdout, "%s\n", Rcl::version_string().c_str());
                return 0;
            case 'W': op_flags |= OPT_W; break;
            case 'w': op_flags |= OPT_w; break;
            default: Usage();
            }
    b1: myargc--; myargv++;
    }

    // If -q was given, all remaining non-option args are concatenated
    // to the query. This is for the common case recoll -q x y z to
    // avoid needing quoting "x y z"
    if (op_flags & OPT_q)
        while (myargc > 0) {
            question += " ";
            question += *myargv++;
            myargc--;
        }

    // Else the remaining argument should be an URL to be opened
    if (myargc == 1) {
        urltoview = *myargv++;myargc--;
        if (urltoview.compare(0, 7, cstr_fileu)) {
            Usage();
        }
    } else if (myargc > 0)
        Usage();


    string reason;
    theconfig = recollinit(0, recollCleanup, 0, reason, &a_config);
    if (!theconfig || !theconfig->ok()) {
        QString msg = "Configuration problem: ";
        msg += QString::fromUtf8(reason.c_str());
        QMessageBox::critical(0, "Recoll",  msg);
        exit(1);
    }
    //    fprintf(stderr, "recollinit done\n");

    // Translations for Qt standard widgets
    QString slang;
    if (op_flags & OPT_L) {
        slang = u8s2qs(a_lang);
    } else {
        slang = QLocale::system().name();
    }
    QTranslator qt_trans(0);
    if (qt_trans.load(QString("qt_%1").arg(slang), 
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                      QLibraryInfo::path
#else
                      QLibraryInfo::location
#endif
                      (QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qt_trans);
    }

    // Translations for Recoll
    string translatdir = path_cat(theconfig->getDatadir(), "translations");
    QTranslator translator(0);
    auto loaded = translator.load(QString("recoll_") + slang,
                                  translatdir.c_str());
    if (loaded)
        app.installTranslator(&translator);

    //    fprintf(stderr, "Translations installed\n");

    string historyfile = path_cat(theconfig->getConfDir(), "history");
    g_dynconf = new RclDynConf(historyfile);
    if (!g_dynconf || !g_dynconf->ok()) {
        QString msg = app.translate("Main", "\"history\" file is damaged, please check "
                                    "or remove it: ") + path2qs(historyfile);
        QMessageBox::critical(0, "Recoll",  msg);
        exit(1);
    }
    g_advshistory = new AdvSearchHist;

    //    fprintf(stderr, "History done\n");
    rwSettings(false);
    //    fprintf(stderr, "Settings done\n");

    applyStyleSheet(prefs.qssFile);
    
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/images/recoll.png"));
    app.setWindowIcon(icon);

    // Create main window and set its size to previous session's
    RclMain w;
    mainWindow = &w;

#ifdef QAPPLICATION_CLASS
#ifdef Q_OS_WINDOWS
    QObject::connect(&app, &SingleApplication::receivedMessage, &w, [&w] () { raiseWidget(&w); } );
#else
    QObject::connect(&app, &SingleApplication::instanceStarted, &w, [&w] () { raiseWidget(&w); } );
#endif
#endif

    string dbdir = theconfig->getDbDir();
    if (dbdir.empty()) {
        QMessageBox::critical(
            0, "Recoll", app.translate("Main", "No db directory in configuration"));
        exit(1);
    }

    maybeOpenDb(reason, false);

    if (op_flags & OPT_w) {
        mainWindow->showMinimized();
    } else if (op_flags & OPT_W) {
        // Don't show anything except the systray icon
        if (!prefs.showTrayIcon) {
            QMessageBox::critical(
                0, "Recoll", app.translate(
                    "Main", "Needs \"Show system tray icon\" to be set in preferences!\n"));
            mainWindow->show();
        }
    } else {
        switch (prefs.showmode) {
        case PrefsPack::SHOW_NORMAL: mainWindow->show(); break;
        case PrefsPack::SHOW_MAX: mainWindow->showMaximized(); break;
        case PrefsPack::SHOW_FULL: mainWindow->showFullScreen(); break;
        }
    }
    QTimer::singleShot(0, mainWindow, SLOT(initDbOpen()));

    mainWindow->sSearch->searchTypCMB->setCurrentIndex(prefs.ssearchTyp);
    mainWindow->sSearch->onSearchTypeChanged(prefs.ssearchTyp);
    if (op_flags & OPT_q) {
        SSearch::SSearchType stype;
        if (op_flags & OPT_o) {
            stype = SSearch::SST_ANY;
        } else if (op_flags & OPT_f) {
            stype = SSearch::SST_FNM;
        } else if (op_flags & OPT_a) {
            stype = SSearch::SST_ALL;
        } else {
            stype = SSearch::SST_LANG;
        }
        mainWindow->sSearch->searchTypCMB->setCurrentIndex(int(stype));
        mainWindow->sSearch->setSearchString(QString::fromLocal8Bit(question.c_str()));
    } else if (!urltoview.empty()) {
        LOGDEB("MAIN: got urltoview [" << urltoview << "]\n");
        mainWindow->setUrlToView(QString::fromLocal8Bit(urltoview.c_str()));
    }
    return app.exec();
}

QString myGetFileName(bool isdir, QString caption, bool filenosave, QString dirloc, QString dfltnm)
{
    MyGFNParams parms;
    parms.caption = caption;
    parms.filenosave = filenosave;
    parms.dirlocation = dirloc;
    parms.dfltnm = dfltnm;
    return myGetFileName(isdir, parms);
}

QString myGetFileName(bool isdir, MyGFNParams &parms)
{
    LOGDEB1("myGetFileName: isdir " << isdir << "\n");
    QFileDialog dialog(0, parms.caption);

#ifdef _WIN32
    // The default initial directory on Windows is the Recoll install,
    // which is not appropriate. Change it, only for the first call
    // (next will start with the previous selection).
    static bool first{true};
    if (first && parms.dirlocation.isEmpty()) {
        first = false;
        // See https://doc.qt.io/qt-5/qfiledialog.html#setDirectoryUrl
        // about the clsid magic (this one points to the desktop).
        dialog.setDirectoryUrl(QUrl("clsid:B4BFCC3A-DB2C-424C-B029-7FE99A87C641"));
    }
#endif
    if (!parms.dirlocation.isEmpty()) {
        dialog.setDirectory(parms.dirlocation);
    }
    if (!parms.dfltnm.isEmpty()) {
        dialog.selectFile(parms.dfltnm);
    }

    // DontUseNativeDialog is needed for sidebarurls
    QFileDialog::Options opts = QFileDialog::DontUseNativeDialog;
    if (parms.readonly) {
        opts |= QFileDialog::ReadOnly;
    }
    if (isdir) {
        dialog.setFileMode(QFileDialog::Directory);
        opts |= QFileDialog::ShowDirsOnly;
    } else {
        dialog.setFileMode(QFileDialog::AnyFile);
        if (parms.filenosave)
            dialog.setAcceptMode(QFileDialog::AcceptOpen);
        else
            dialog.setAcceptMode(QFileDialog::AcceptSave);
    }
    dialog.setOptions(opts);
    dialog.setViewMode(QFileDialog::List);
    QFlags<QDir::Filter> flags = QDir::NoDotAndDotDot | QDir::Hidden; 
    if (isdir)
        flags |= QDir::Dirs;
    else 
        flags |= QDir::Dirs | QDir::Files;
    dialog.setFilter(flags);

    if (!parms.sidedirs.empty()) {
        QList<QUrl> sidebarurls;
        for (const auto& dir : parms.sidedirs) {
            sidebarurls.push_back(QUrl::fromLocalFile(path2qs(dir)));
        }
        dialog.setSidebarUrls(sidebarurls);
    }
    if (dialog.exec() == QDialog::Accepted) {
        parms.dirlocation = dialog.directory().absolutePath();
        return dialog.selectedFiles().value(0);
    }
    return QString();
}
