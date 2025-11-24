/* Copyright (C) 2005-2019 J.F.Dockes
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

#include <QShortcut>
#include <QMessageBox>

#include "log.h"
#include "internfile.h"
#include "listdialog.h"
#include "confgui/confguiindex.h"
#include "idxsched.h"
#ifdef _WIN32
#include "winschedtool.h"
#else
#include "crontool.h"
#include "rtitool.h"
#endif
#include "snippets_w.h"
#include "fragbuts.h"
#include "specialindex.h"
#include "rclmain_w.h"
#include "webcache.h"
#include "restable.h"
#include "actsearch_w.h"
#include "docseqdocs.h"
#include "uiprefs_w.h"
#include "configswitch.h"
#include "ptrans_w.h"

using namespace std;

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

// Open advanced search dialog.
void RclMain::showAdvSearchDialog()
{
    if (asearchform == 0) {
        asearchform = new AdvSearch(0);
        if (asearchform == 0) {
            return;
        }
        connect(new QShortcut(quitKeySeq, asearchform), SIGNAL (activated()),
                this, SLOT (fileExit()));
        connect(new QShortcut(closeKeySeq, asearchform), SIGNAL (activated()),
                asearchform, SLOT(close()));

        connect(asearchform, SIGNAL(startSearch(std::shared_ptr<Rcl::SearchData>, bool)), 
                this, SLOT(startSearch(std::shared_ptr<Rcl::SearchData>, bool)));
        connect(asearchform, SIGNAL(setDescription(QString)), this, SLOT(onSetDescription(QString)));
        asearchform->show();
    } else {
        // Close and reopen, in hope that makes us visible...
        asearchform->close();
        asearchform->show();
    }
}

void RclMain::showSpellDialog()
{
    if (spellform == 0) {
        spellform = new SpellW(0);
        connect(new QShortcut(quitKeySeq, spellform), SIGNAL (activated()), 
                this, SLOT (fileExit()));
        connect(spellform, SIGNAL(wordSelect(QString)),
                sSearch, SLOT(addTerm(QString)));
        spellform->show();
    } else {
        // Close and reopen, in hope that makes us visible...
        spellform->close();
        spellform->show();
    }
}

void RclMain::showWebcacheDialog()
{
    switch (indexerState()) {
    case RclMain::IXST_UNKNOWN:
        QMessageBox::warning(0, "Recoll", tr("Unknown indexer state. "
                                             "Can't access webcache file."));
        return;
    case RclMain::IXST_RUNNINGMINE:
    case RclMain::IXST_RUNNINGNOTMINE:
        QMessageBox::warning(0, "Recoll", tr("Indexer is running. "
                                             "Can't access webcache file."));
        return;
    case RclMain::IXST_NOTRUNNING:
        break;
    }

    if (!m_pidfile) {
        m_pidfile = new Pidfile(theconfig->getPidfile());
        if (m_pidfile->open() != 0) {
            deleteZ(m_pidfile);
            return;
        }
        if (m_pidfile->write_pid() != 0) {
            deleteZ(m_pidfile);
            return;
        }
    }
    
    if (webcache == 0) {
        webcache = new WebcacheEdit(this);
        webcache->setAttribute(Qt::WA_DeleteOnClose);
        connect(new QShortcut(quitKeySeq, webcache), SIGNAL (activated()), 
                this, SLOT (fileExit()));
        connect(webcache, SIGNAL(destroyed(QObject*)),
                this, SLOT(onWebcacheDestroyed(QObject*)) );
        webcache->show();
    }
}
void RclMain::onWebcacheDestroyed(QObject *)
{
    deleteZ(m_pidfile);
    webcache = 0;
}

void RclMain::showIndexStatistics()
{
    showSpellDialog();
    if (spellform == 0)
        return;
    spellform->setMode(SpellW::TYPECMB_STATS);
}

void RclMain::showFragButs()
{
    if (fragbuts && fragbuts->isStale(0)) {
        deleteZ(fragbuts);
    }
    if (fragbuts == 0) {
        fragbuts = new FragButs(0);
        if (fragbuts->ok()) {
            fragbuts->show();
            connect(new QShortcut(quitKeySeq, fragbuts), SIGNAL (activated()),
                    this, SLOT (fileExit()));
            connect(new QShortcut(closeKeySeq, fragbuts), SIGNAL (activated()),
                    fragbuts, SLOT(close()));
            connect(fragbuts, SIGNAL(fragmentsChanged()), this, SLOT(onFragmentsChanged()));
        } else {
            deleteZ(fragbuts);
        }
    } else {
        // Close and reopen, in hope that makes us visible...
        fragbuts->close();
        fragbuts->show();
    }
}

void RclMain::showSpecIdx()
{
    if (specidx == 0) {
        specidx = new SpecIdxW(0);
        connect(specidx, SIGNAL(accepted()), this, SLOT(specialIndex()));
        specidx->show();
    } else {
        // Close and reopen, in hope that makes us visible...
        specidx->close();
        specidx->show();
    }
}

void RclMain::showPTrans(const QString& qdbdir)
{
    // Delete / recreate in case external indexes changed
    deleteZ(pathTrans);
    pathTrans = new PTransEdit(qs2path(qdbdir), nullptr);
    pathTrans->setCurrentDb(qs2path(qdbdir));
    pathTrans->hide();
    pathTrans->show();
}

void RclMain::delPTrans()
{
    deleteZ(pathTrans);
}

void RclMain::showIndexConfig()
{
    showIndexConfig(false);
}
void RclMain::execIndexConfig()
{
    showIndexConfig(true);
}
void RclMain::showIndexConfig(bool modal)
{
    LOGDEB("showIndexConfig()\n" );
    bool created{false};
    if (indexConfig == 0) {
        created = true;
        indexConfig = new ConfIndexW(0, theconfig);
    }
    indexConfig->showPrefs(modal);
    if (created) {
        connect(new QShortcut(quitKeySeq, indexConfig->getDialog()),
                SIGNAL (activated()), this, SLOT (fileExit()));
    }
}

void RclMain::showIndexSched()
{
    showIndexSched(false);
}
void RclMain::execIndexSched()
{
    showIndexSched(true);
}
void RclMain::showIndexSched(bool modal)
{
    LOGDEB("showIndexSched()\n" );
    if (indexSched == 0) {
        indexSched = new IdxSchedW(this);
        connect(new QShortcut(quitKeySeq, indexSched), SIGNAL (activated()), this, SLOT(fileExit()));
#ifdef _WIN32
        indexSched->cronCLB->setText(tr("Batch scheduling"));
        indexSched->cronCLB->setDescription(
            tr("The tool will let you decide at what time indexing should run. "
               " It uses the Windows task scheduler."));
        indexSched->mainExplainLBL->hide();
        indexSched->rtidxCLB->hide();
#endif
        connect(indexSched->cronCLB, SIGNAL(clicked()), this, SLOT(execCronTool()));
#ifdef RCL_MONITOR
        connect(indexSched->rtidxCLB, SIGNAL(clicked()), this, SLOT(execRTITool()));
#else
        indexSched->rtidxCLB->setEnabled(false);
        indexSched->rtidxCLB->setToolTip(
            tr("Disabled because the real time indexer was not compiled in."));
#endif
    } else {
        // Close and reopen, in hope that makes us visible...
        indexSched->close();
    }
    if (modal) {
        indexSched->exec();
        indexSched->setModal(false);
    } else {
        indexSched->show();
    }
}

void RclMain::showCronTool()
{
    showCronTool(false);
}
void RclMain::execCronTool()
{
    showCronTool(true);
}

void RclMain::showCronTool(bool modal)
{
    LOGDEB("showCronTool()\n" );
    if (cronTool == 0) {
#ifdef _WIN32
        cronTool = new WinSchedToolW(0);
#else
        cronTool = new CronToolW(0);
#endif
        connect(new QShortcut(quitKeySeq, cronTool), SIGNAL (activated()), 
                this, SLOT (fileExit()));
    } else {
        // Close and reopen, in hope that makes us visible...
        cronTool->close();
    }
    if (modal) {
        cronTool->exec();
        cronTool->setModal(false);
    } else {
        cronTool->show();
    }
}

void RclMain::showRTITool()
{
    showRTITool(false);
}
void RclMain::execRTITool()
{
    showRTITool(true);
}
void RclMain::showRTITool(bool modal)
{
#ifndef _WIN32
    LOGDEB("showRTITool()\n" );
    if (rtiTool == 0) {
        rtiTool = new RTIToolW(0);
        connect(new QShortcut(quitKeySeq, rtiTool), SIGNAL (activated()), 
                this, SLOT (fileExit()));
    } else {
        // Close and reopen, in hope that makes us visible...
        rtiTool->close();
    }
    if (modal) {
        rtiTool->exec();
        rtiTool->setModal(false);
    } else {
        rtiTool->show();
    }
#else
    PRETEND_USE(modal);
#endif
}

void RclMain::showUIPrefs()
{
    prefs.useTmpActiveExtraDbs = false;
    prefs.tmpActiveExtraDbs.clear();
    if (uiprefs == 0) {
        uiprefs = new UIPrefsDialog(this);
        connect(uiprefs, SIGNAL(showPTrans(const QString&)), this, SLOT(showPTrans(const QString&)));
        connect(uiprefs, SIGNAL(delPTrans()), this, SLOT(delPTrans()));
        connect(new QShortcut(quitKeySeq, uiprefs), SIGNAL (activated()), 
                this, SLOT (fileExit()));
        connect(uiprefs, SIGNAL(uiprefsDone()), this, SLOT(setUIPrefs()));
        connect(this, SIGNAL(stemLangChanged(const QString&)), 
                uiprefs, SLOT(setStemLang(const QString&)));
    } else {
        // Close and reopen, in hope that makes us visible...
        uiprefs->close();
        rwSettings(false);
        uiprefs->setFromPrefs();
    }
    uiprefs->show();
}

void RclMain::showExtIdxDialog()
{
    showUIPrefs();
    auto w = uiprefs->tabWidget->findChild<QWidget*>("tab_extradbs");
    uiprefs->tabWidget->setCurrentWidget(w);
}

void RclMain::showAboutDialog()
{
    QString vstring = QString("<html><head>") + u8s2qs(prefs.htmlHeaderContents()) +
        "</head><body>" +
        u8s2qs(Rcl::version_string()) +
        "<br><a href='https://www.recoll.org'>www.recoll.org</a>" +
        "<br><a href='https://www.xapian.org'>www.xapian.org</a>";
    QMessageBox::information(this, tr("About Recoll"), vstring);
}

void RclMain::showMissingHelpers()
{
    string miss;
    if (!theconfig->getMissingHelperDesc(miss)) {
        QMessageBox::information(
            this, "", tr("No information: initial indexing not yet performed."));
        return;
    }
    QString msg = QString::fromUtf8("<p>") +
        tr("External applications/commands needed for your file types "
           "and not found, as stored by the last indexing pass in ");
    msg += "<i>";
    msg += path2qs(theconfig->getConfDir());
    msg += "/missing</i>:<pre>\n";
    if (!miss.empty()) {
        msg += QString::fromUtf8(miss.c_str());
    } else {
        msg += tr("No helpers found missing");
    }
    msg += "</pre>";
    QMessageBox::information(this, tr("Missing helper programs"), msg);
}

void RclMain::showActiveTypes()
{
    string reason;
    bool maindberror;
    if (!maybeOpenDb(reason, true, &maindberror)) {
        QMessageBox::warning(0, tr("Error"), 
                             u8s2qs(reason),
                             QMessageBox::Ok, 
                             QMessageBox::NoButton);
        return;
    }

    // All mime types in index. 
    vector<string> vdbtypes;
    if (!rcldb->getAllDbMimeTypes(vdbtypes)) {
        QMessageBox::warning(0, tr("Error"), 
                             tr("Index query error"),
                             QMessageBox::Ok, 
                             QMessageBox::NoButton);
        return;
    }
    set<string> mtypesfromdb;
    mtypesfromdb.insert(vdbtypes.begin(), vdbtypes.end());

    // All types listed in mimeconf:
    vector<string> mtypesfromconfig = theconfig->getAllMimeTypes();

    // Intersect file system types with config types (those not in the
    // config can be indexed by name, not by content)
    set<string> mtypesfromdbconf;
    for (vector<string>::const_iterator it = mtypesfromconfig.begin();
         it != mtypesfromconfig.end(); it++) {
        if (mtypesfromdb.find(*it) != mtypesfromdb.end())
            mtypesfromdbconf.insert(*it);
    }

    // Substract the types for missing helpers (the docs are indexed
    // by name only):
    string miss;
    if (theconfig->getMissingHelperDesc(miss) && !miss.empty()) {
        FIMissingStore st(miss);
        map<string, set<string> >::const_iterator it;
        for (it = st.m_typesForMissing.begin(); 
             it != st.m_typesForMissing.end(); it++) {
            set<string>::const_iterator it1;
            for (it1 = it->second.begin(); 
                 it1 != it->second.end(); it1++) {
                set<string>::iterator it2 = mtypesfromdbconf.find(*it1);
                if (it2 != mtypesfromdbconf.end())
                    mtypesfromdbconf.erase(it2);
            }
        }   
    }
    ListDialog dialog;
    dialog.setWindowTitle(tr("Indexed MIME Types"));

    // Turn the result into a string and display
    dialog.groupBox->setTitle(tr("Content has been indexed for these MIME types:"));

    // We replace the list with an editor so that the user can copy/paste
    deleteZ(dialog.listWidget);
    QTextEdit *editor = new QTextEdit(dialog.groupBox);
    editor->setReadOnly(true);
    dialog.horizontalLayout->addWidget(editor);

    if (mtypesfromdbconf.empty()) {
        editor->append(tr("Types list empty: maybe wait for indexing to "
                          "progress?"));
    } else {
        for (set<string>::const_iterator it = mtypesfromdbconf.begin(); 
             it != mtypesfromdbconf.end(); it++) {
            editor->append(QString::fromUtf8(it->c_str()));
        }
    }
    editor->moveCursor(QTextCursor::Start);
    editor->ensureCursorVisible();
    dialog.exec();
}

void RclMain::newDupsW(const Rcl::Doc, const vector<Rcl::Doc> dups)
{
    if (nullptr == m_dupsw) {
        m_dupsw = new ResTable(nullptr, {"ipath", "url"});
        m_dupsw->setRclMain(this, false);
    }
    auto src = std::make_shared<DocSequenceDocs>(rcldb, dups, qs2utf8s(tr("Duplicates")));
    src->setDescription(qs2utf8s(tr("Duplicates")));
    auto source = std::make_shared<DocSource>(theconfig, src);
    m_dupsw->setDocSource(source);
    m_dupsw->readDocSource();
    m_dupsw->show();
}

void RclMain::showSnippets(Rcl::Doc doc)
{
    if (!m_source)
        return;
    if (!m_snippets) {
        m_snippets = new SnippetsW(doc, m_source);
        connect(m_snippets, SIGNAL(startNativeViewer(Rcl::Doc, int, QString, int)),
                this, SLOT(startNativeViewer(Rcl::Doc, int, QString, int)));
        connect(m_snippets, SIGNAL(zoomIn()), this, SLOT(zoomIn()));
        connect(m_snippets, SIGNAL(zoomOut()), this, SLOT(zoomOut()));
        connect(this, SIGNAL(uiPrefsChanged()), m_snippets, SLOT(onUiPrefsChanged()));
        connect(new QShortcut(quitKeySeq, m_snippets), SIGNAL (activated()),
                this, SLOT (fileExit()));
        connect(new QShortcut(closeKeySeq, m_snippets), SIGNAL (activated()), 
                m_snippets, SLOT (close()));
        if (restable) {
            connect(
                restable,
                SIGNAL(detailDocChanged(Rcl::Doc, std::shared_ptr<DocSequence>)),
                m_snippets,
                SLOT(onSetDoc(Rcl::Doc, std::shared_ptr<DocSequence>)));
        }
    } else {
        m_snippets->onSetDoc(doc, m_source);
    }
    m_snippets->show();
}

void RclMain::showActionsSearch()
{
    if (nullptr == actsearchw) {
        actsearchw = new ActSearchW(this);
        actsearchw->setActList(findChildren<QAction *>());
        connect(actsearchw->actCMB, SIGNAL(editTextChanged(const QString&)),
                actsearchw, SLOT(onTextChanged(const QString&)));
    }
    actsearchw->actCMB->setCurrentIndex(-1);
    actsearchw->actCMB->clearEditText();
    actsearchw->show();
}

void RclMain::showConfigSwitch()
{
    if (nullptr == configswitchw) {
        configswitchw = new ConfigSwitchW(this);
        connect(configswitchw->dirsCMB, SIGNAL(editTextChanged(const QString&)),
                configswitchw, SLOT(onTextChanged(const QString&)));
    }
    configswitchw->dirsCMB->setCurrentIndex(-1);
    configswitchw->dirsCMB->clearEditText();
    configswitchw->m_cancelled = false;
    configswitchw->show();
}
