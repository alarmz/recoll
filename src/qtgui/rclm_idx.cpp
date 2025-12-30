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

#include <signal.h>
#include "safeunistd.h"

#ifdef _WIN32
// getpid()
#include <process.h>
#endif // _WIN32

#include <QMessageBox>
#include <QTimer>

#include "execmd.h"
#include "log.h"
#include "transcode.h"
#include "indexer.h"
#include "rclmain_w.h"
#include "specialindex.h"
#include "readfile.h"
#include "snippets_w.h"
#include "idxstatus.h"
#include "conftree.h"

using namespace std;

static std::string recollindex;

// This is called from periodic100 if we started an indexer, or from
// the rclmain idxstatus file watcher, every time the file changes.
// Returns true if a real time indexer is currently monitoring, false else.
bool RclMain::updateIdxStatus()
{
    bool ret{false};
    DbIxStatus status;
    readIdxStatus(theconfig, status);
    QString msg = tr("Indexing in progress: ");
    QString phs;
    switch (status.phase) {
    case DbIxStatus::DBIXS_NONE:phs=tr("None");break;
    case DbIxStatus::DBIXS_FILES: phs=tr("Updating");break;
    case DbIxStatus::DBIXS_FLUSH: phs=tr("Flushing");break;
    case DbIxStatus::DBIXS_PURGE: phs=tr("Purge");break;
    case DbIxStatus::DBIXS_STEMDB: phs=tr("Stemdb");break;
    case DbIxStatus::DBIXS_CLOSING:phs=tr("Closing");break;
    case DbIxStatus::DBIXS_DONE:phs=tr("Done");break;
    case DbIxStatus::DBIXS_MONITOR:phs=tr("Monitor"); ret = true;break;
    default: phs=tr("Unknown");break;
    }
    msg += phs + " ";
    if (status.phase == DbIxStatus::DBIXS_FILES) {
        QString sdocs = status.docsdone > 1 ?tr("documents") : tr("document");
        QString sfiles = status.filesdone > 1 ? tr("files") : tr("file");
        QString serrors = status.fileerrors > 1 ? tr("errors") : tr("error");
        QString stats;
        if (status.dbtotdocs > 0) {
            stats = QString("(%1 ") + sdocs + "/%2 " + sfiles +
                "/%3 " + serrors + "/%4 " + tr("total files)");
            stats = stats.arg(status.docsdone).arg(status.filesdone).
                arg(status.fileerrors).arg(status.totfiles);
        } else {
            stats = QString("(%1 ") + sdocs + "/%2 " + sfiles +
                "/%3 " + serrors + ") ";
            stats = stats.arg(status.docsdone).arg(status.filesdone).
                arg(status.fileerrors);
        }
        msg += stats + " ";
    }
    string mf;int ecnt = 0;
    string fcharset = theconfig->getDefCharset(true);
    // If already UTF-8 let it be, else try to transcode, or url-encode
    if (!transcode(status.fn, mf, cstr_utf8, cstr_utf8, &ecnt) || ecnt) {
        if (!transcode(status.fn, mf, fcharset, cstr_utf8, &ecnt) || ecnt) {
            mf = path_pcencode(status.fn, 0);
        }
    }
    msg += QString::fromUtf8(mf.c_str());
    statusBar()->showMessage(msg, 4000);
    return ret;
}

// This is called by a periodic timer to check the status of 
// indexing, a possible need to exit, and cleanup exited viewers
// We don't need thread locking, but we're not reentrant either
static bool periodic100busy(false);
class Periodic100Guard {
public:
    Periodic100Guard() { periodic100busy = true;}
    ~Periodic100Guard() { periodic100busy = false;}
};
void RclMain::periodic100()
{
    if (periodic100busy)
        return;
    Periodic100Guard guard;
    
    LOGDEB2("Periodic100\n" );
    if (recollindex.empty()) {
        // In many cases (_WIN32, appimage, we are not in the PATH. make recollindex a full path
        recollindex = path_cat(path_thisexecdir(), "recollindex");
    }
    if (!m_idxreasontmp || !m_idxreasontmp->ok()) {
        // We just store the pointer and let the tempfile cleaner deal
        // with delete on exiting
        TempFile temp(".txt");
        m_idxreasontmp = rememberTempFile(temp);
    }
    bool isMonitor{false};
    if (m_idxproc) {
        // An indexing process was launched. If its' done, see status.
        int status;
        bool exited = m_idxproc->maybereap(&status);
        if (exited) {
            QString reasonmsg;
            if (m_idxreasontmp && m_idxreasontmp->ok()) {
                string reasons;
                file_to_string(m_idxreasontmp->filename(), reasons);
                if (!reasons.empty()) {
                    ConfSimple rsn(reasons);
                    vector<string> sects = rsn.getNames("");
                    for (const auto& nm : sects) {
                        string val;
                        if (rsn.get(nm, val)) {
                            reasonmsg.append(u8s2qs(string("<br>") +
                                                    nm + " : " + val));
                        }
                    }
                }
            }
            deleteZ(m_idxproc);
            if (status) {
                if (m_idxkilled) {
                    QMessageBox::warning(0, "Recoll", tr("Indexing interrupted"));
                    m_idxkilled = false;
                } else {
                    QString msg(tr("Indexing failed"));
                    if (!reasonmsg.isEmpty()) {
                        msg.append(tr(" with additional message: "));
                        msg.append(reasonmsg);
                    }
                    QMessageBox::warning(0, "Recoll", msg);
                }
            } else {
                // On the first run, show missing helpers. We only do this once
                if (m_firstIndexing)
                    showMissingHelpers();
                if (!reasonmsg.isEmpty()) {
                    QString msg = tr("Non-fatal indexing message: ");
                    msg.append(reasonmsg);
                    QMessageBox::warning(0, "Recoll", msg);
                }
            }
            string reason;
            maybeOpenDb(reason, 1);
            populateSideFilters(SFUR_INDEXCONTENTS);
        } else {
            // update/show status even if the status file did not
            // change (else the status line goes blank during
            // lengthy operations).
            isMonitor = updateIdxStatus();
        }
    }
    // Update the "start/stop indexing" menu entry, can't be done from
    // the "start/stop indexing" slot itself
    IndexerState prevstate = m_indexerState;
    if (m_idxproc) {
        m_indexerState = IXST_RUNNINGMINE;
        fileToggleIndexingAction->setText(tr("Stop &Indexing"));
        fileToggleIndexingAction->setEnabled(true);
        fileStartMonitorAction->setEnabled(false);
        fileBumpIndexingAction->setEnabled(isMonitor);
        fileRebuildIndexAction->setEnabled(false);
        actionSpecial_Indexing->setEnabled(false);
        periodictimer->setInterval(200);
    } else {
        Pidfile pidfile(theconfig->getPidfile());
        pid_t pid = pidfile.open();
        fileBumpIndexingAction->setEnabled(false);
        if (pid == getpid()) {
            // Locked by me
            m_indexerState = IXST_NOTRUNNING;
            fileToggleIndexingAction->setText(tr("Index locked"));
            fileToggleIndexingAction->setEnabled(false);
            fileStartMonitorAction->setEnabled(false);
            fileBumpIndexingAction->setEnabled(false);
            fileRebuildIndexAction->setEnabled(false);
            actionSpecial_Indexing->setEnabled(false);
            periodictimer->setInterval(1000);
        } else if (pid == 0) {
            m_indexerState = IXST_NOTRUNNING;
            fileToggleIndexingAction->setText(tr("Update &Index"));
            fileToggleIndexingAction->setEnabled(true);
            fileStartMonitorAction->setEnabled(true);
            fileBumpIndexingAction->setEnabled(false);
            fileRebuildIndexAction->setEnabled(true);
            actionSpecial_Indexing->setEnabled(true);
            periodictimer->setInterval(1000);
        } else {
            // Real time or externally started batch indexer running
            m_indexerState = IXST_RUNNINGNOTMINE;
            fileToggleIndexingAction->setText(tr("Stop &Indexing"));
            fileToggleIndexingAction->setEnabled(true);
            DbIxStatus status;
            readIdxStatus(theconfig, status);
            if (status.hasmonitor) {
                // Real-time indexer running. We can trigger an
                // incremental pass
                fileBumpIndexingAction->setEnabled(true);
            }
            fileStartMonitorAction->setEnabled(false);
            fileRebuildIndexAction->setEnabled(false);
            actionSpecial_Indexing->setEnabled(false);
            periodictimer->setInterval(200);
        }           
    }

    if ((prevstate == IXST_RUNNINGMINE || prevstate == IXST_RUNNINGNOTMINE)
        && m_indexerState == IXST_NOTRUNNING) {
        showTrayMessage(tr("Indexing done"));
    }

    // Possibly cleanup the dead viewers
    for (auto it = m_viewers.begin(); it != m_viewers.end(); ) {
        int status;
        if ((*it)->maybereap(&status)) {
            delete *it;
            it = m_viewers.erase(it);
        } else {
            it++;
        }
    }
}

bool RclMain::checkIdxPaths()
{
    string badpaths;
    vector<string> args{recollindex, "-c", theconfig->getConfDir(), "-E"};
    ExecCmd::backtick(args, badpaths);
    if (!badpaths.empty()) {
        int rep = QMessageBox::warning(
            0, tr("Bad paths"),
            tr("Empty or non-existant paths in configuration file. "
               "Click Ok to start indexing anyway "
               "(absent data will not be purged from the index):\n") +
            path2qs(badpaths),
            QMessageBox::Ok | QMessageBox::Cancel);
        if (rep == QMessageBox::Cancel)
            return false;
    }
    return true;
}

// This gets called when the "Update index/Stop indexing" action is
// activated. It executes the requested action. The menu
// entry will be updated by the indexing status check
void RclMain::toggleIndexing()
{
    switch (m_indexerState) {
    case IXST_RUNNINGMINE:
        if (m_idxproc) {
            // Indexing was in progress, request stop. Let the periodic
            // routine check for the results.
            if (m_idxproc->requestChildExit()) {
                m_idxkilled = true;
            }
        }
        break;
    case IXST_RUNNINGNOTMINE:
    {
#ifdef _WIN32
        // No simple way to signal the process. Use the stop file
        std::fstream ost;
        if (!path_streamopen(theconfig->getIdxStopFile(), std::fstream::out, ost)) {
            LOGSYSERR("toggleIndexing", "path_streamopen", theconfig->getIdxStopFile());
        }
#else
        Pidfile pidfile(theconfig->getPidfile());
        pid_t pid = pidfile.open();
        if (pid > 0)
            kill(pid, SIGTERM);
#endif // !_WIN32
    }
    break;
    case IXST_NOTRUNNING:
    {
        // Could also mean that no helpers are missing, but then we won't try to show a message
        // anyway (which is what firstIndexing is used for)
        string mhd;
        m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);

        if (!checkIdxPaths()) {
            return;
        }
        vector<string> args{"-c", theconfig->getConfDir()};
        if (m_idxreasontmp && m_idxreasontmp->ok()) {
            args.push_back("-R");
            args.push_back(m_idxreasontmp->filename());
        }
        m_idxproc = new ExecCmd;
        m_idxproc->startExec(recollindex, args, false, false);
    }
    break;
    case IXST_UNKNOWN:
        return;
    }
}

void RclMain::startMonitor()
{
    DbIxStatus status;
    readIdxStatus(theconfig, status);
    if (nullptr == m_idxproc && m_indexerState == IXST_NOTRUNNING) {
        if (!checkIdxPaths()) {
            return;
        }
        vector<string> args{"-c", theconfig->getConfDir()};
        if (m_idxreasontmp && m_idxreasontmp->ok()) {
            args.push_back("-R");
            args.push_back(m_idxreasontmp->filename());
        }
        args.push_back("-mw");
        args.push_back("0");
        m_idxproc = new ExecCmd;
        m_idxproc->startExec(recollindex, args, false, false);
    }
}

void RclMain::bumpIndexing()
{
    DbIxStatus status;
    readIdxStatus(theconfig, status);
    if (status.hasmonitor) {
        path_utimes(path_cat(theconfig->getConfDir(), "recoll.conf"), nullptr);
    }
}

static void delay(int millisecondsWait)
{
    QEventLoop loop;
    QTimer t;
    t.connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
    t.start(millisecondsWait);
    loop.exec();
}

void RclMain::rebuildIndex()
{
    if (!m_idxreasontmp || !m_idxreasontmp->ok()) {
        // We just store the pointer and let the tempfile cleaner deal
        // with delete on exiting
        TempFile temp(".txt");
        m_idxreasontmp = rememberTempFile(temp);
    }
    if (m_indexerState == IXST_UNKNOWN) {
        delay(1500);
    }
    switch (m_indexerState) {
    case IXST_UNKNOWN:
    case IXST_RUNNINGMINE:
    case IXST_RUNNINGNOTMINE:
        return; //?? Should not have been called
    case IXST_NOTRUNNING:
    {
        if (m_idxproc) {
            LOGERR("RclMain::rebuildIndex: current indexer exec not null\n" );
            return;
        }
        int rep = QMessageBox::Ok;
        if (path_exists(theconfig->getDbDir())) {
            rep = QMessageBox::warning(
                0, tr("Erasing index"), tr("Reset the index and start from scratch ?"),
                QMessageBox::Ok | QMessageBox::Cancel);
        }
        if (rep == QMessageBox::Ok) {
#ifdef _WIN32
            // Under windows, it is necessary to close the db here,
            // else Xapian won't be able to do what it wants with the
            // (open) files. Of course if there are several GUI
            // instances, this won't work... Also it's quite difficult
            // to make sure that there are no more references to the
            // db because, for example of the Enquire objects inside
            // Query inside Docsource etc.
            //
            // !! At this moment, this does not work if a preview has
            // !! been opened. Could not find the reason (mysterious
            // !! Xapian::Database reference somewhere?). The indexing
            // !! fails, leaving a partial index directory. Then need
            // !! to restart the GUI to succeed in reindexing.
            if (rcldb) {
                resetSearch();
                deleteZ(m_snippets);
                rcldb->close();
            }
#endif // _WIN32
            // Could also mean that no helpers are missing, but then we
            // won't try to show a message anyway (which is what
            // firstIndexing is used for)
            string mhd;
            m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);

            if (!checkIdxPaths()) {
                return;
            }
            
            vector<string> args{"-c", theconfig->getConfDir(), "-z"};
            if (m_idxreasontmp && m_idxreasontmp->ok()) {
                args.push_back("-R");
                args.push_back(m_idxreasontmp->filename());
            }
            m_idxproc = new ExecCmd;
            m_idxproc->startExec(recollindex, args, false, false);
        }
    }
    break;
    }
}

void SpecIdxW::onTargBrowsePB_clicked()
{
    QString dir = myGetFileName(true, tr("Top indexed entity"), true);
    targLE->setText(dir);
}

void SpecIdxW::onDiagsBrowsePB_clicked()
{
    QString fn = myGetFileName(false, tr("Diagnostics file"));
    diagsLE->setText(fn);
}

bool SpecIdxW::noRetryFailed()
{
    return !retryFailedCB->isChecked();
}

bool SpecIdxW::eraseFirst()
{
    return eraseBeforeCB->isChecked();
}

std::vector<std::string> SpecIdxW::selpatterns()
{
    vector<string> pats;
    string text = qs2utf8s(selPatsLE->text());
    if (!text.empty()) {
        stringToStrings(text, pats);
    }
    return pats;
}

std::string SpecIdxW::toptarg()
{
    return qs2utf8s(targLE->text());
}

std::string SpecIdxW::diagsfile()
{
    return qs2utf8s(diagsLE->text());
}

void SpecIdxW::onTargLE_textChanged(const QString& text)
{
    if (text.isEmpty())
        selPatsLE->setEnabled(false);
    else
        selPatsLE->setEnabled(true);
}

static string execToString(const string& cmd, const vector<string>& args)
{
    string command = cmd + " ";
    for (vector<string>::const_iterator it = args.begin();
         it != args.end(); it++) {
        command += "{" + *it + "} ";
    }
    return command;
}

void RclMain::specialIndex()
{
    LOGDEB("RclMain::specialIndex\n" );
    if (!m_idxreasontmp || !m_idxreasontmp->ok()) {
        // We just store the pointer and let the tempfile cleaner deal
        // with delete on exiting
        TempFile temp(".txt");
        m_idxreasontmp = rememberTempFile(temp);
    }
    switch (m_indexerState) {
    case IXST_UNKNOWN:
    case IXST_RUNNINGMINE:
    case IXST_RUNNINGNOTMINE:
        return; //?? Should not have been called
    case IXST_NOTRUNNING:
    default:
        break;
    }
    if (m_idxproc) {
        LOGERR("RclMain::rebuildIndex: current indexer exec not null\n" );
        return;
    }
    if (!specidx) // ??
        return;

    vector<string> args{"-c", theconfig->getConfDir()};
    if (m_idxreasontmp && m_idxreasontmp->ok()) {
        args.push_back("-R");
        args.push_back(m_idxreasontmp->filename());
    }

    string top = specidx->toptarg();
    if (!top.empty()) {
        args.push_back("-r");
    }

    string diagsfile = specidx->diagsfile();
    if (!diagsfile.empty()) {
        args.push_back("--diagsfile");
        args.push_back(diagsfile);
    }

    if (specidx->eraseFirst()) {
        if (top.empty()) {
            args.push_back("-Z");
        } else {
            args.push_back("-e");
            // -e also needs -i, else we don't reindex, just erase
            args.push_back("-i");
        }
    } 

    if (!specidx->noRetryFailed()) {
        args.push_back("-k");
    } else {
        args.push_back("-K");
    }
        
    vector<string> selpats = specidx->selpatterns();
    if (!selpats.empty() && top.empty()) {
        QMessageBox::warning(0, tr("Selection patterns need topdir"), 
                             tr("Selection patterns can only be used with a "
                                "start directory"),
                             QMessageBox::Ok, QMessageBox::NoButton);
        return;
    }

    for (vector<string>::const_iterator it = selpats.begin();
         it != selpats.end(); it++) {
        args.push_back("-p");
        args.push_back(*it);
    }
    if (!top.empty()) {
        args.push_back(top);
    }
    m_idxproc = new ExecCmd;
    LOGINFO("specialIndex: exec: " << execToString(recollindex, args) << std::endl);
    m_idxproc->startExec(recollindex, args, false, false);
}

void RclMain::updateIdxForDocs(vector<Rcl::Doc>& docs)
{
    if (m_idxproc) {
        QMessageBox::warning(0, tr("Warning"), tr("Can't update index: indexer running"),
                             QMessageBox::Ok, QMessageBox::NoButton);
        return;
    }
        
    vector<string> paths;
    if (Rcl::docsToPaths(docs, paths)) {
        vector<string> args{"-c", theconfig->getConfDir(), "-i",};
        if (m_idxreasontmp && m_idxreasontmp->ok()) {
            args.push_back("-R");
            args.push_back(m_idxreasontmp->filename());
        }
        args.insert(args.end(), paths.begin(), paths.end());
        m_idxproc = new ExecCmd;
        m_idxproc->startExec(recollindex, args, false, false);
        // Call periodic100 to update the menu entries states
        periodic100();
    } else {
        QMessageBox::warning(0, tr("Warning"), tr("Can't update index: internal error"),
                             QMessageBox::Ok, QMessageBox::NoButton);
        return;
    }
}
