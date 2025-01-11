/* Copyright (C) 2004-2022 J.F.Dockes
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
#include <cstring>
#include <exception>
#include "safeunistd.h"
#include <time.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <memory>

using namespace std;

#include "xapian.h"

#include "rclconfig.h"
#include "log.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "stemdb.h"
#include "textsplit.h"
#include "transcode.h"
#include "unacpp.h"
#include "conftree.h"
#include "pathut.h"
#include "rclutil.h"
#include "smallut.h"
#include "chrono.h"
#include "rclvalues.h"
#include "md5ut.h"
#include "cancelcheck.h"
#include "termproc.h"
#include "expansiondbs.h"
#include "rclinit.h"
#include "wipedir.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif
#include "zlibut.h"
#include "idxstatus.h"
#include "rcldoc.h"
#include "daterange.h"


namespace Rcl {
// Empty string md5s 
static const string cstr_md5empty("d41d8cd98f00b204e9800998ecf8427e");
// Characters we strip inside field data
static const string cstr_nc("\n\r\x0c\\");

Db::Native::Native(Db *db) 
    : m_rcldb(db)
#ifdef IDX_THREADS
    , m_wqueue("DbUpd", m_rcldb->m_config->getThrConf(RclConfig::ThrDbWrite).first),
      m_mwqueue("DbMUpd", 2)
#endif // IDX_THREADS
{ 
    LOGDEB1("Native::Native: me " << this << "\n");
}

Db::Native::~Native() 
{ 
    LOGDEB1("Native::~Native: me " << this << "\n");
#ifdef IDX_THREADS
    if (m_havewriteq) {
        void *status = m_wqueue.setTerminateAndWait();
        if (status) {
            LOGDEB1("Native::~Native: worker status " << status << "\n");
        }
        if (m_tmpdbinitidx > 0) {
            status = m_mwqueue.setTerminateAndWait();
            if (status) {
                LOGDEB1("Native::~Native: worker status " << status << "\n");
            }
        }
    }
#endif // IDX_THREADS
}

#ifdef IDX_THREADS
// Index update primary (unique) worker thread. If we are using multiple temporary indexes, new
// documents (only) will be queued by addOrUpdateWrite() to the temp indexes queue which has one
// worker thread per temporary index.
void *DbUpdWorker(void* vdbp)
{
    recoll_threadinit();
    Db::Native *ndbp = (Db::Native *)vdbp;
    WorkQueue<DbUpdTask*> *tqp = &(ndbp->m_wqueue);

    DbUpdTask *tsk = nullptr;
    for (;;) {
        size_t qsz = -1;
        if (!tqp->take(&tsk, &qsz)) {
            tqp->workerExit();
            return (void*)1;
        }
        bool status = false;
        switch (tsk->op) {
        case DbUpdTask::AddOrUpdate:
            LOGDEB("DbUpdWorker: got add/update task, ql " << qsz << "\n");
            status = ndbp->addOrUpdateWrite(
                tsk->udi, tsk->uniterm, std::move(tsk->doc), tsk->txtlen, tsk->rawztext);
            break;
        case DbUpdTask::Delete:
            LOGDEB("DbUpdWorker: got delete task, ql " << qsz << "\n");
            status = ndbp->purgeFileWrite(false, tsk->udi, tsk->uniterm);
            break;
        case DbUpdTask::PurgeOrphans:
            LOGDEB("DbUpdWorker: got orphans purge task, ql " << qsz << "\n");
            status = ndbp->purgeFileWrite(true, tsk->udi, tsk->uniterm);
            break;
        default:
            LOGERR("DbUpdWorker: unknown op " << tsk->op << " !!\n");
            break;
        }
        if (!status) {
            LOGERR("DbUpdWorker: xxWrite failed\n");
            tqp->workerExit();
            delete tsk;
            return (void*)0;
        }
        delete tsk;
    }
}

// Temporary index update worker thread. If we are configured to use temporary indexes,
// addOrUpdateWrite() pushes new documents to our input queue instead of updating the main
// index. The workqueue is configured with as many workers as there are temp indexes. Each thread
// picks up the first available index when starting up (m_tmpdbinitidx counter), then fetches docs
// from the queue and updates its index.
void *DbMUpdWorker(void* vdbp)
{
    Db::Native *ndbp = (Db::Native *)vdbp;
    WorkQueue<DbUpdTask*> *tqp = &(ndbp->m_mwqueue);

    // Starting up. Grab the first available temporary index.
    int dbidx;
    {
        std::lock_guard<std::mutex> lock(ndbp->m_initidxmutex);
        dbidx = ndbp->m_tmpdbinitidx++;
        if (dbidx >= ndbp->m_tmpdbcnt) {
            LOGERR("DbMUpdWorker: dbidx >= ndbp->m_tmpdbcnt\n");
            abort();
        }
    }
    Xapian::WritableDatabase& xwdb = ndbp->m_tmpdbs[dbidx];
    LOGINF("DbMUpdWorker: thread for index " << dbidx << " started\n");

    // Fetch documents from the queue and update my index.
    DbUpdTask *tsk = nullptr;
    for (;;) {
        size_t qsz = -1;
        if (!tqp->take(&tsk, &qsz)) {
            LOGDEB0("DbMUpdWorker: eoq, flushing index " << dbidx << "\n");
            xwdb.commit();
            tqp->workerExit();
            return (void*)1;
        }
        if (tsk->op != DbUpdTask::AddOrUpdate) {
            LOGFAT("DbMUpdWorker: op not AddOrUpdate: " << tsk->op << " !!\n");
            abort();
        }
        LOGDEB("DbMUpdWorker: got add/update task, ql " << qsz << "\n");

        Xapian::docid did = 0;
        std::string ermsg;
        bool status = false;
        XAPTRY(did = xwdb.add_document(*(tsk->doc.get())), xwdb, ermsg);
        if (!ermsg.empty()) {
            LOGERR("DbMupdWorker::add_document failed: " << ermsg << "\n");
        } else {
            XAPTRY(xwdb.set_metadata(ndbp->rawtextMetaKey(tsk->uniterm), tsk->rawztext),
                   xwdb, ermsg);
            if (!ermsg.empty()) {
                LOGERR("DbMUpdWorker: set_metadata failed: " << ermsg << "\n");
            } else {
                status = true;
                LOGINFO("Db::add: docid " << did << " added [" << tsk->udi << "]\n");
            }
        }

        delete tsk;

        if (!status) {
            LOGERR("DbMUpdWorker: index update failed\n");
            tqp->workerExit();
            return (void*)0;
        }

        // Flushing is triggered by the doc creation thread. This results into all indexes being
        // flushed at more or less the same time which is probably not optimal.
        bool needflush = false;
        {
            std::lock_guard<std::mutex> lock(ndbp->m_initidxmutex);
            if (ndbp->m_tmpdbflushflags[dbidx]) {
                ndbp->m_tmpdbflushflags[dbidx] = 0;
                needflush = true;
            }
        }
        if (needflush) {
            LOGDEB("DbMUpdWorker: flushing index " << dbidx << "\n");
            xwdb.commit();
        }
    }
}

void Db::Native::maybeStartThreads()
{
    m_havewriteq = false;
    const RclConfig *cnf = m_rcldb->m_config;
    int writeqlen = cnf->getThrConf(RclConfig::ThrDbWrite).first;
    int writethreads = cnf->getThrConf(RclConfig::ThrDbWrite).second;
    if (writethreads > 1) {
        LOGINFO("RclDb: write threads count was forced down to 1\n");
        writethreads = 1;
    }
    if (writeqlen >= 0 && writethreads > 0) {
        if (!m_wqueue.start(writethreads, DbUpdWorker, this)) {
            LOGERR("Db::Db: Worker start failed\n");
            return;
        }
        m_havewriteq = true;

        LOGINF("maybeStartThreads: tmpdbcnt is " << m_tmpdbcnt << "\n");
        if (m_tmpdbcnt > 0) {
            m_tmpdbflushflags.resize(m_tmpdbcnt);
            for (int i = 0; i < m_tmpdbcnt; i++) {
                m_tmpdbdirs.emplace_back(std::make_unique<TempDir>());
                LOGINF("Creating temporary database in " << m_tmpdbdirs.back()->dirname() << "\n");
                m_tmpdbs.push_back(
                    Xapian::WritableDatabase(
                        m_tmpdbdirs.back()->dirname(), Xapian::DB_CREATE_OR_OVERWRITE));
                m_tmpdbflushflags[i] = 0;
            }
            if (!m_mwqueue.start(m_tmpdbcnt, DbMUpdWorker, this)) {
                LOGERR("Db::Db: MWorker start failed\n");
                return;
            }
            LOGINF("Started MWQueue with " << m_tmpdbcnt << " threads\n");
        }
    }
    LOGDEB("RclDb:: threads: haveWriteQ " << m_havewriteq << ", wqlen " <<
           writeqlen << " wqts " << writethreads << "\n");
}

#endif // IDX_THREADS

void Db::Native::openWrite(const string& dir, Db::OpenMode mode, int flags)
{
    LOGINF("Db::Native::openWrite\n");
    int action = (mode == Db::DbUpd) ? Xapian::DB_CREATE_OR_OPEN : Xapian::DB_CREATE_OR_OVERWRITE;

#ifdef IDX_THREADS
    m_tmpdbcnt = 0;
    if (!(flags & Db::DbOFNoTmpDb))
        m_rcldb->m_config->getConfParam("thrTmpDbCnt", &m_tmpdbcnt);
#endif
    
#ifdef _WIN32
    // On Windows, Xapian is quite bad at erasing a partial db, which can occur because of open file
    // deletion errors.
    if (mode == DbTrunc) {
        if (path_exists(path_cat(dir, "iamglass")) || path_exists(path_cat(dir, "iamchert"))) {
            wipedir(dir);
            path_unlink(dir);
        }
    }
#endif
    
    if (path_exists(dir)) {
        // Existing index. 
        xwdb = Xapian::WritableDatabase(dir, action);
        if (action == Xapian::DB_CREATE_OR_OVERWRITE || xwdb.get_doccount() == 0) {
            // New or empty index. Set the "store text" option
            // according to configuration. The metadata record will be
            // written further down.
            m_storetext = o_index_storedoctext;
            LOGDEB("Db:: index " << (m_storetext?"stores":"does not store") << " document text\n");
        } else {
            // Existing non empty. Get the option from the index.
            storesDocText(xwdb);
        }
    } else {
        // Use the default index backend and let the user decide of the abstract generation
        // method. The configured default is to store the text.
        xwdb = Xapian::WritableDatabase(dir, action);
        m_storetext = o_index_storedoctext;
    }

    // If the index is empty, write the data format version, 
    // and the storetext option value inside the index descriptor (new
    // with recoll 1.24, maybe we'll have other stuff to store in
    // there in the future).
    if (xwdb.get_doccount() == 0) {
        string desc = string("storetext=") + (m_storetext ? "1" : "0") + "\n";
        xwdb.set_metadata(cstr_RCL_IDX_DESCRIPTOR_KEY, desc);
        xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, cstr_RCL_IDX_VERSION);
    }

    m_iswritable = true;

#ifdef IDX_THREADS
    maybeStartThreads();
#endif
}

void Db::Native::storesDocText(Xapian::Database& db)
{
    string desc = db.get_metadata(cstr_RCL_IDX_DESCRIPTOR_KEY);
    ConfSimple cf(desc, 1);
    string val;
    m_storetext = false;
    if (cf.get("storetext", val) && stringToBool(val)) {
        m_storetext = true;
    }
    LOGDEB("Db:: index " << (m_storetext?"stores":"does not store") << " document text\n");
}

void Db::Native::openRead(const string& dir)
{
    m_iswritable = false;
    xrdb = Xapian::Database(dir);
    storesDocText(xrdb);
}

/* See comment in class declaration: return all subdocuments of a
 * document given by its unique id. */
bool Db::Native::subDocs(const string &udi, int idxi, vector<Xapian::docid>& docids) 
{
    LOGDEB2("subDocs: [" << udi << "]\n");
    string pterm = make_parentterm(udi);
    vector<Xapian::docid> candidates;
    XAPTRY(docids.clear();
           candidates.insert(candidates.begin(), xrdb.postlist_begin(pterm),
                             xrdb.postlist_end(pterm)),
           xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Rcl::Db::subDocs: " << m_rcldb->m_reason << "\n");
        return false;
    } else {
        for (unsigned int i = 0; i < candidates.size(); i++) {
            if (whatDbIdx(candidates[i]) == (size_t)idxi) {
                docids.push_back(candidates[i]);
            }
        }
        LOGDEB0("Db::Native::subDocs: returning " << docids.size() << " ids\n");
        return true;
    }
}

bool Db::Native::docidToUdi(Xapian::docid xid, std::string& udi)
{
    Xapian::Document xdoc;
    XAPTRY(xdoc = xrdb.get_document(xid), xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::Native:docidToUdi: get_document error: " << m_rcldb->m_reason << "\n");
        return false;
    }
    return xdocToUdi(xdoc, udi);
}

bool Db::Native::xdocToUdi(Xapian::Document& xdoc, string &udi)
{
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin(); xit.skip_to(wrap_prefix(udi_prefix)),
           xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("xdocToUdi: xapian error: " << m_rcldb->m_reason << "\n");
        return false;
    }
    if (xit != xdoc.termlist_end()) {
        udi = *xit;
        if (!udi.empty()) {
            udi = udi.substr(wrap_prefix(udi_prefix).size());
            return true;
        }
    }
    return false;
}

// Clear term from document if its frequency is 0. This should
// probably be done by Xapian when the freq goes to 0 when removing a
// posting, but we have to do it ourselves
bool Db::Native::clearDocTermIfWdf0(Xapian::Document& xdoc, const string& term)
{
    LOGDEB1("Db::clearDocTermIfWdf0: [" << term << "]\n");

    // Find the term
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin(); xit.skip_to(term);,
           xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::clearDocTerm...: [" << term << "] skip failed: " << m_rcldb->m_reason << "\n");
        return false;
    }
    if (xit == xdoc.termlist_end() || term.compare(*xit)) {
        LOGDEB0("Db::clearDocTermIFWdf0: term [" << term << "] not found. xit: [" <<
                (xit == xdoc.termlist_end() ? "EOL": *xit) << "]\n");
        return false;
    }

    // Clear the term if its frequency is 0
    if (xit.get_wdf() == 0) {
        LOGDEB1("Db::clearDocTermIfWdf0: clearing [" << term << "]\n");
        XAPTRY(xdoc.remove_term(term), xwdb, m_rcldb->m_reason);
        if (!m_rcldb->m_reason.empty()) {
            LOGDEB0("Db::clearDocTermIfWdf0: failed [" << term << "]: "<<m_rcldb->m_reason << "\n");
        }
    }
    return true;
}

// Holder for term + pos
struct DocPosting {
    DocPosting(string t, Xapian::termpos ps)
        : term(t), pos(ps) {}
    string term;
    Xapian::termpos pos;
};

// Clear all terms for given field for given document.
// The terms to be cleared are all those with the appropriate
// prefix. We also remove the postings for the unprefixed terms (that
// is, we undo what we did when indexing).
bool Db::Native::clearField(Xapian::Document& xdoc, const string& pfx, Xapian::termcount wdfdec)
{
    LOGDEB1("Db::clearField: clearing prefix [" << pfx << "] for docid " << xdoc.get_docid()<<"\n");

    vector<DocPosting> eraselist;

    string wrapd = wrap_prefix(pfx);

    m_rcldb->m_reason.clear();
    for (int tries = 0; tries < 2; tries++) {
        try {
            Xapian::TermIterator xit;
            xit = xdoc.termlist_begin();
            xit.skip_to(wrapd);
            while (xit != xdoc.termlist_end() && !(*xit).compare(0, wrapd.size(), wrapd)) {
                LOGDEB1("Db::clearfield: erasing for [" << *xit << "]\n");
                Xapian::PositionIterator posit;
                for (posit = xit.positionlist_begin(); posit != xit.positionlist_end(); posit++) {
                    eraselist.push_back(DocPosting(*xit, *posit));
                    eraselist.push_back(DocPosting(strip_prefix(*xit), *posit));
                }
                xit++;
            }
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_rcldb->m_reason = e.get_msg();
            xrdb.reopen();
            continue;
        } XCATCHERROR(m_rcldb->m_reason);
        break;
    }
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::clearField: failed building erase list: " << m_rcldb->m_reason << "\n");
        return false;
    }

    // Now remove the found positions, and the terms if the wdf is 0
    for (const auto& er : eraselist) {
        LOGDEB1("Db::clearField: remove posting: [" << er.term << "] pos [" << er.pos << "]\n");
        XAPTRY(xdoc.remove_posting(er.term, er.pos, wdfdec);, xwdb, m_rcldb->m_reason);
        if (!m_rcldb->m_reason.empty()) {
            // Not that this normally fails for non-prefixed XXST and
            // ND, don't make a fuss
            LOGDEB1("Db::clearFiedl: remove_posting failed for [" << er.term <<
                    "]," << er.pos << ": " << m_rcldb->m_reason << "\n");
        }
        clearDocTermIfWdf0(xdoc, er.term);
    }
    return true;
}

// Check if doc given by udi is indexed by term
bool Db::Native::hasTerm(const string& udi, int idxi, const string& term)
{
    LOGDEB2("Native::hasTerm: udi [" << udi << "] term [" << term << "]\n");
    Xapian::Document xdoc;
    if (getDoc(udi, idxi, xdoc)) {
        Xapian::TermIterator xit;
        XAPTRY(xit = xdoc.termlist_begin(); xit.skip_to(term);, xrdb, m_rcldb->m_reason);
        if (!m_rcldb->m_reason.empty()) {
            LOGERR("Rcl::Native::hasTerm: " << m_rcldb->m_reason << "\n");
            return false;
        }
        if (xit != xdoc.termlist_end() && !term.compare(*xit)) {
            return true;
        }
    }
    return false;
}

// Retrieve Xapian document, given udi. There may be several identical udis
// if we are using multiple indexes. If idxi is -1 we don't care (looking for the path).
Xapian::docid Db::Native::getDoc(const string& udi, int idxi, Xapian::Document& xdoc)
{
    string uniterm = make_uniterm(udi);
    for (int tries = 0; tries < 2; tries++) {
        try {
            Xapian::PostingIterator docid;
            for (docid = xrdb.postlist_begin(uniterm);
                 docid != xrdb.postlist_end(uniterm); docid++) {
                xdoc = xrdb.get_document(*docid);
                if (idxi == -1 || whatDbIdx(*docid) == (size_t)idxi)
                    return *docid;
            }
            // Udi not in Db.
            return 0;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_rcldb->m_reason = e.get_msg();
            xrdb.reopen();
            continue;
        } XCATCHERROR(m_rcldb->m_reason);
        break;
    }
    LOGERR("Db::Native::getDoc: Xapian error: " << m_rcldb->m_reason << "\n");
    return 0;
}

// Turn data record from db into document fields
bool Db::Native::dbDataToRclDoc(Xapian::docid docid, Xapian::Document& xdoc,
                                std::string &data, Doc &doc, bool fetchtext)
{
    LOGDEB2("Db::dbDataToRclDoc: data:\n" << data << "\n");
    ConfSimple parms(data, 1, false, false);
    if (!parms.ok())
        return false;

    doc.xdocid = docid;
    doc.haspages = hasPages(docid);

    // Compute what index this comes from, and check for path translations
    string dbdir = m_rcldb->m_basedir;
    doc.idxi = 0;
    if (!m_rcldb->m_extraDbs.empty()) {
        int idxi = int(whatDbIdx(docid));

        // idxi is in [0, extraDbs.size()]. 0 is for the main index,
        // idxi-1 indexes into the additional dbs array.
        if (idxi) {
            dbdir = m_rcldb->m_extraDbs[idxi - 1];
            doc.idxi = idxi;
        }
    }
    parms.get(Doc::keyurl, doc.idxurl);
    doc.url = doc.idxurl;
    m_rcldb->m_config->urlrewrite(dbdir, doc.url);
    if (!doc.url.compare(doc.idxurl))
        doc.idxurl.clear();

    // Special cases:
    parms.get(Doc::keytp, doc.mimetype);
    parms.get(Doc::keyfmt, doc.fmtime);
    parms.get(Doc::keydmt, doc.dmtime);
    parms.get(Doc::keyoc, doc.origcharset);
    parms.get(cstr_caption, doc.meta[Doc::keytt]);

    parms.get(Doc::keyabs, doc.meta[Doc::keyabs]);
    // Possibly remove synthetic abstract indicator (if it's there, we
    // used to index the beginning of the text as abstract).
    doc.syntabs = false;
    if (doc.meta[Doc::keyabs].find(cstr_syntAbs) == 0) {
        doc.meta[Doc::keyabs] = doc.meta[Doc::keyabs].substr(cstr_syntAbs.length());
        doc.syntabs = true;
    }
    parms.get(Doc::keyipt, doc.ipath);
    parms.get(Doc::keypcs, doc.pcbytes);
    parms.get(Doc::keyfs, doc.fbytes);
    parms.get(Doc::keyds, doc.dbytes);
    parms.get(Doc::keysig, doc.sig);

    // Normal key/value pairs:
    for (const auto& key : parms.getNames(string())) {
        if (doc.meta.find(key) == doc.meta.end())
            parms.get(key, doc.meta[key]);
    }
    doc.meta[Doc::keyurl] = doc.url;
    doc.meta[Doc::keymt] = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    if (fetchtext) {
        std::string &udi = doc.meta[Doc::keyudi];
        if (udi.empty()) {
            xdocToUdi(xdoc, udi);
        }
        if (!udi.empty()) {
            getRawText(udi, docid, doc.text);
        }
    }
    return true;
}

bool Db::Native::hasPages(Xapian::docid docid)
{
    string ermsg;
    Xapian::PositionIterator pos;
    XAPTRY(pos = xrdb.positionlist_begin(docid, page_break_term); 
           if (pos != xrdb.positionlist_end(docid, page_break_term)) {
               return true;
           },
           xrdb, ermsg);
    if (!ermsg.empty()) {
        LOGERR("Db::Native::hasPages: xapian error: " << ermsg << "\n");
    }
    return false;
}

// Return the positions list for the page break term
bool Db::Native::getPagePositions(Xapian::docid docid, vector<int>& vpos)
{
    vpos.clear();
    // Need to retrieve the document record to check for multiple page breaks
    // that we store there for lack of better place
    map<int, int> mbreaksmap;
    try {
        Xapian::Document xdoc = xrdb.get_document(docid);
        string data = xdoc.get_data();
        Doc doc;
        string mbreaks;
        if (dbDataToRclDoc(docid, xdoc, data, doc) && 
            doc.getmeta(cstr_mbreaks, &mbreaks)) {
            vector<string> values;
            stringToTokens(mbreaks, values, ",");
            for (unsigned int i = 0; i < values.size() - 1; i += 2) {
                int pos  = atoi(values[i].c_str()) + baseTextPosition;
                int incr = atoi(values[i+1].c_str());
                mbreaksmap[pos] = incr;
            }
        }
    } catch (...) {
    }

    string qterm = page_break_term;
    Xapian::PositionIterator pos;
    try {
        for (pos = xrdb.positionlist_begin(docid, qterm);
             pos != xrdb.positionlist_end(docid, qterm); pos++) {
            int ipos = *pos;
            if (ipos < int(baseTextPosition)) {
                LOGDEB("getPagePositions: got page position " << ipos << " not in body\n");
                // Not in text body. Strange...
                continue;
            }
            map<int, int>::iterator it = mbreaksmap.find(ipos);
            if (it != mbreaksmap.end()) {
                LOGDEB1("getPagePositions: found multibreak at " << ipos <<
                        " incr " << it->second << "\n");
                for (int i = 0 ; i < it->second; i++) 
                    vpos.push_back(ipos);
            }
            vpos.push_back(ipos);
        } 
    } catch (...) {
        // Term does not occur. No problem.
    }
    return true;
}

int Db::Native::getPageNumberForPosition(const vector<int>& pbreaks, int pos)
{
    if (pos < int(baseTextPosition)) // Not in text body
        return -1;
    vector<int>::const_iterator it = upper_bound(pbreaks.begin(), pbreaks.end(), pos);
    return int(it - pbreaks.begin() + 1);
}

bool Db::Native::getRawText(const std::string& udi, Xapian::docid docid_combined, string& rawtext)
{
    if (!m_storetext) {
        LOGDEB("Db::Native::getRawText: document text not stored in index\n");
        return false;
    }
    auto uniterm = make_uniterm(udi);

    // When using multiple indexes, we need to open the right one (because of the old docid use).
    size_t dbidx = whatDbIdx(docid_combined);
    Xapian::docid docid = whatDbDocid(docid_combined);
    string reason;
    // We try with the uniterm key now in use, and try to fallback on the docid in case we have an
    // old index.
    if (dbidx != 0) {
        Xapian::Database db(m_rcldb->m_extraDbs[dbidx-1]);
        XAPTRY(rawtext = db.get_metadata(rawtextMetaKey(uniterm)), db, reason);
        if (!reason.empty() || rawtext.empty()) {
            reason.clear();
            XAPTRY(rawtext = db.get_metadata(rawtextMetaKey(docid)), db, reason);
        }
    } else {
        XAPTRY(rawtext = xrdb.get_metadata(rawtextMetaKey(uniterm)), xrdb, reason);
        if (!reason.empty() || rawtext.empty()) {
            reason.clear();
            XAPTRY(rawtext = xrdb.get_metadata(rawtextMetaKey(docid)), xrdb, reason);
        }
    }
    if (!reason.empty()) {
        LOGERR("Rcl::Db::getRawText: could not get value: " << reason << "\n");
        return false;
    }
    if (rawtext.empty()) {
        return true;
    }
    ZLibUtBuf cbuf;
    inflateToBuf(rawtext.c_str(), rawtext.size(), cbuf);
    rawtext.assign(cbuf.getBuf(), cbuf.getCnt());
    return true;
}

bool Db::Native::fsFull()
{
    // Check file system full every mbyte of indexed text. It's a bit wasteful
    // to do this after having prepared the document, but it needs to be in
    // the single-threaded section.
    if (m_rcldb->m_maxFsOccupPc > 0 && 
        (m_rcldb->m_occFirstCheck || (m_rcldb->m_curtxtsz - m_rcldb->m_occtxtsz) / MB >= 1)) {
        LOGDEB0("Db::add: checking file system usage\n");
        int pc;
        m_rcldb->m_occFirstCheck = 0;
        if (fsocc(m_rcldb->m_basedir, &pc) && pc >= m_rcldb->m_maxFsOccupPc) {
            LOGERR("Db::add: stop indexing: file system " << pc << " %" <<
                   " full > max " << m_rcldb->m_maxFsOccupPc << " %" << "\n");
            return true;
        }
        m_rcldb->m_occtxtsz = m_rcldb->m_curtxtsz;
    }
    return false;
}

// Note: we're passed a Xapian::Document* because Xapian reference-counting is not mt-safe. We take
// ownership and need to delete it before returning.
bool Db::Native::addOrUpdateWrite(
    const string& udi, const string& uniterm, std::unique_ptr<Xapian::Document> newdocument_ptr, 
    size_t textlen, std::string& rawztext)
{
    // Does its own locking, call before our own.
    bool docexists = m_rcldb->docExists(uniterm);
    PRETEND_USE(docexists);
    
#ifdef IDX_THREADS
    Chrono chron;
    std::unique_lock<std::mutex> lock(m_mutex);
#endif

    if (fsFull()) {
        return false;
    }
    
#ifdef IDX_THREADS
    if (!docexists && m_tmpdbinitidx > 0) {
        // New doc and we are using temporary indexes to speed things up, send it to the temp db
        // queue
        DbUpdTask *tp = new DbUpdTask(
            DbUpdTask::AddOrUpdate, udi, uniterm, std::move(newdocument_ptr), textlen, rawztext);
        if (!m_mwqueue.put(tp)) {
            LOGERR("Db::addOrUpdate:Cant mqueue task\n");
            return false;
        }
        auto ret = m_rcldb->maybeflush(textlen);
        return ret;
    }
#endif
    
    string ermsg;
    // Add db entry or update existing entry:
    Xapian::docid did = 0;
    try {
        did = xwdb.replace_document(uniterm, *(newdocument_ptr.get()));
        if (did < m_rcldb->updated.size()) {
            // This is necessary because only the file-level docs are tested by needUpdate(), so the
            // subdocs existence flags are only set here.
            m_rcldb->updated[did] = true;
            LOGINFO("Db::add: docid " << did << " updated [" << udi << "]\n");
        } else {
            LOGINFO("Db::add: docid " << did << " added [" << udi << "]\n");
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("Db::add: replace_document failed: " << ermsg << "\n");
        return false;
    }

    XAPTRY(xwdb.set_metadata(rawtextMetaKey(uniterm), rawztext), xwdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::addOrUpdate: set_metadata error: " << m_rcldb->m_reason << "\n");
        // This only affects snippets, so let's say not fatal
    }
    
    // Test if we're over the flush threshold (limit memory usage):
    bool ret = m_rcldb->maybeflush(textlen);
#ifdef IDX_THREADS
    m_totalworkns += chron.nanos();
#endif
    return ret;
}

// Delete the data for a given document.
// This is either called from the dbUpdate queue worker, or directly from the top level purge() if
// we are not using threads.
bool Db::Native::purgeFileWrite(bool orphansOnly, const string& udi, const string& uniterm)
{
#if defined(IDX_THREADS) 
    // We need a mutex even if we have a write queue (so we can only
    // be called by a single thread) to protect about multiple acces
    // to xrdb from subDocs() which is also called from needupdate()
    // (called from outside the write thread !
    std::unique_lock<std::mutex> lock(m_mutex);
#endif // IDX_THREADS

    string ermsg;
    try {
        Xapian::PostingIterator docid = xwdb.postlist_begin(uniterm);
        if (docid == xwdb.postlist_end(uniterm)) {
            return true;
        }
        if (m_rcldb->m_flushMb > 0) {
            Xapian::termcount trms = xwdb.get_doclength(*docid);
            m_rcldb->maybeflush(trms * 5);
        }
        string sig;
        if (orphansOnly) {
            Xapian::Document doc = xwdb.get_document(*docid);
            sig = doc.get_value(VALUE_SIG);
            if (sig.empty()) {
                LOGINFO("purgeFileWrite: got empty sig\n");
                return false;
            }
        } else {
            LOGDEB("purgeFile: delete docid " << *docid << "\n");
            deleteDocument(*docid);
        }
        vector<Xapian::docid> docids;
        subDocs(udi, 0, docids);
        LOGDEB("purgeFile: subdocs cnt " << docids.size() << "\n");
        for (const auto docid : docids) {
            if (m_rcldb->m_flushMb > 0) {
                Xapian::termcount trms = xwdb.get_doclength(docid);
                m_rcldb->maybeflush(trms * 5);
            }
            string subdocsig;
            if (orphansOnly) {
                Xapian::Document doc = xwdb.get_document(docid);
                subdocsig = doc.get_value(VALUE_SIG);
                if (subdocsig.empty()) {
                    LOGINFO("purgeFileWrite: got empty sig for subdoc??\n");
                    continue;
                }
            }
        
            if (!orphansOnly || sig != subdocsig) {
                LOGDEB("Db::purgeFile: delete subdoc " << docid << "\n");
                deleteDocument(docid);
            }
        }
        return true;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("Db::purgeFileWrite: " << ermsg << "\n");
    }
    return false;
}

size_t Db::Native::whatDbIdx(Xapian::docid id)
{
    LOGDEB1("Db::whatDbIdx: xdocid " << id << ", " << m_rcldb->m_extraDbs.size() << " extraDbs\n");
    if (id == 0) 
        return (size_t)-1;
    if (m_rcldb->m_extraDbs.size() == 0)
        return 0;
    return (id - 1) % (m_rcldb->m_extraDbs.size() + 1);
}

// Return the docid inside the non-combined index
Xapian::docid Db::Native::whatDbDocid(Xapian::docid docid_combined)
{
    if (m_rcldb->m_extraDbs.size() == 0)
        return docid_combined;
    return (docid_combined - 1) / (static_cast<int>(m_rcldb->m_extraDbs.size()) + 1) + 1;
}

static void addDateTerms(Xapian::Document& newdocument, time_t thetime,
                         const string& dayprefix, const string& monprefix, const string& yearprefix)
{
    struct tm tmb;
    localtime_r(&thetime, &tmb);
    char buf[50]; // It's actually 9, but use 50 to suppress warnings.
    snprintf(buf, 50, "%04d%02d%02d", tmb.tm_year+1900, tmb.tm_mon + 1, tmb.tm_mday);
            
    // Date (YYYYMMDD)
    newdocument.add_boolean_term(wrap_prefix(dayprefix) + string(buf)); 
    // Month (YYYYMM)
    buf[6] = '\0';
    newdocument.add_boolean_term(wrap_prefix(monprefix) + string(buf));
    // Year (YYYY)
    buf[4] = '\0';
    newdocument.add_boolean_term(wrap_prefix(yearprefix) + string(buf)); 
}

bool Db::Native::docToXdoc(
    TextSplitDb *splitter, const string& parent_udi, const string& uniterm, Doc& doc,
    Xapian::Document& newdocument, std::string& rawztext,
    std::vector <std::pair<int, int>>& pageincrvec)
{

    if (m_rcldb->m_idxTextTruncateLen > 0) {
        doc.text = truncate_to_word(doc.text, m_rcldb->m_idxTextTruncateLen);
    }
        
    // If the ipath is like a path, index the last element. This is for compound documents like
    // zip and chm for which the filter uses the file path as ipath.
    if (!doc.ipath.empty() && 
        doc.ipath.find_first_not_of("0123456789") != string::npos) {
        string utf8ipathlast;
        // We have no idea of the charset here, so let's hope it's ascii or utf-8. We call
        // transcode to strip the bad chars and pray
        if (transcode(path_getsimple(doc.ipath), utf8ipathlast, cstr_utf8, cstr_utf8)) {
            splitter->text_to_words(utf8ipathlast);
        }
    }

    // Split and index the path from the url for path-based filtering
    const FieldTraits *ftp{nullptr};
    m_rcldb->fieldToTraits(cstr_dir, &ftp);
    if (ftp && !ftp->pfx.empty()) {
        string path = url_gpathS(doc.url);
#ifdef _WIN32
        // Windows file names are case-insensitive, and read as UTF-8
        path = unactolower(path);
#endif
        vector<string> vpath;
        stringToTokens(path, vpath, "/");
        // If vpath is not /, the last elt is the file/dir name, not a part of the path.
        if (vpath.size())
            vpath.resize(vpath.size()-1);
        splitter->curpos = 0;
        newdocument.add_posting(wrap_prefix(pathelt_prefix), splitter->basepos + splitter->curpos++);
        for (auto& elt : vpath) {
            if (elt.length() > 230) {
                // Just truncate it. May still be useful because of wildcards
                elt = elt.substr(0, 230);
            }
            newdocument.add_posting(wrap_prefix(pathelt_prefix) + elt, 
                                    splitter->basepos + splitter->curpos++);
        }
        splitter->basepos += splitter->curpos + 100;
    }

    // Index textual metadata.  These are all indexed as text with positions, as we may want to do
    // phrase searches with them (this makes no sense for keywords by the way).
    //
    // The order has no importance, and we set a position gap of 100 between fields to avoid false
    // proximity matches.
    for (const auto& [fld, mdata]: doc.meta) {
        if (mdata.empty()) {
            continue;
        }
        const FieldTraits *ftp{nullptr};
        m_rcldb->fieldToTraits(fld, &ftp);
        if (ftp && ftp->valueslot) {
            LOGDEB("Adding value: for field " << fld << " slot " << ftp->valueslot << "\n");
            add_field_value(newdocument, *ftp, mdata);
        }

        // There was an old comment here about not testing for empty prefix, and we indeed did not
        // test. I don't think that it makes sense any more (and was in disagreement with the LOG
        // message). Really now: no prefix: no indexing.
        if (ftp && !ftp->pfx.empty()) {
            LOGDEB0("Db::add: field [" << fld << "] pfx [" <<
                    ftp->pfx << "] inc " << ftp->wdfinc << ": [" << mdata << "]\n");
            splitter->setTraits(*ftp);
            if (!splitter->text_to_words(mdata)) {
                LOGDEB("Db::addOrUpdate: split failed for " << fld << "\n");
            }
        } else {
            LOGDEB0("Db::add: no prefix for field [" << fld << "], no indexing\n");
        }
    }

    // Reset to no prefix and default params
    splitter->setTraits(FieldTraits());

    if (splitter->curpos < baseTextPosition)
        splitter->basepos = baseTextPosition;

    // Split and index the body text
    LOGDEB2("Db::add: split body: [" << doc.text << "]\n");
#ifdef TEXTSPLIT_STATS
    splitter->resetStats();
#endif
    if (!splitter->text_to_words(doc.text)) {
        LOGDEB("Db::addOrUpdate: split failed for main text\n");
    } else {
        if (m_storetext) {
            ZLibUtBuf buf;
            deflateToBuf(doc.text.c_str(), doc.text.size(), buf);
            rawztext.assign(buf.getBuf(), buf.getCnt());
        }
    }

#ifdef TEXTSPLIT_STATS
    // Reject bad data. unrecognized base64 text is characterized by high avg word length and high
    // variation (because there are word-splitters like +/ inside the data).
    TextSplit::Stats::Values v = splitter->getStats();
    // v.avglen > 15 && v.sigma > 12 
    if (v.count > 200 && (v.avglen > 10 && v.sigma / v.avglen > 0.8)) {
        LOGINFO("RclDb::addOrUpdate: rejecting doc for bad stats count " <<
                v.count << " avglen " << v.avglen << " sigma " << v.sigma << " url [" << doc.url <<
                "] ipath [" << doc.ipath << "] text " << doc.text << "\n");
        return true;
    }
#endif

    ////// Special terms for other metadata. No positions for these.

    // Mime type. We check the traits in case the user has disabled mimetype indexing by setting
    // the prefixes entry to empty. Otherwise, can't change the prefix, it's 'T'
    m_rcldb->fieldToTraits(cstr_mimetype, &ftp);
    if (ftp && !ftp->pfx.empty()) {
        newdocument.add_boolean_term(wrap_prefix(mimetype_prefix) + doc.mimetype);
    }

    // Simple file name indexed unsplit for specific "file name" searches. This is not the same as a
    // filename: clause inside the query language.
    // We also add a term for the filename extension if any.
    m_rcldb->fieldToTraits(unsplitFilenameFieldName, &ftp);
    if (ftp && !ftp->pfx.empty()) {
        string utf8fn;
        if (doc.getmeta(Doc::keyfn, &utf8fn) && !utf8fn.empty()) {
            string fn;
            if (unacmaybefold(utf8fn, fn, UNACOP_UNACFOLD)) {
                // We should truncate after extracting the extension, but this is a pathological
                // case anyway
                if (fn.size() > 230)
                    utf8truncate(fn, 230);
                string::size_type pos = fn.rfind('.');
                if (pos != string::npos && pos != fn.length() - 1) {
                    newdocument.add_boolean_term(wrap_prefix(fileext_prefix) + fn.substr(pos + 1));
                }
                newdocument.add_term(wrap_prefix(unsplitfilename_prefix) + fn, 0);
            }
        }
    }
        
    newdocument.add_boolean_term(uniterm);
    // Parent term. This is used to find all descendents, mostly to delete them when the parent goes
    // away
    if (!parent_udi.empty()) {
        newdocument.add_boolean_term(make_parentterm(parent_udi));
    }

    // Fields used for selecting by date. Note that this only works for years AD 0-9999 (no
    // crash elsewhere, but things won't work).
    time_t mtime = atoll(doc.dmtime.empty() ? doc.fmtime.c_str() : doc.dmtime.c_str());
    addDateTerms(newdocument, mtime, xapday_prefix, xapmonth_prefix, xapyear_prefix);
#ifdef EXT4_BIRTH_TIME
    // Selecting by birth time.
    std::string sbirtime;
    doc.getmeta(Doc::keybrt, &sbirtime);
    if (!sbirtime.empty()) {
        mtime = atoll(sbirtime.c_str());
        addDateTerms(newdocument, mtime,
                     xapbriday_prefix, xapbrimonth_prefix, xapbriyear_prefix);
    }
#endif


    string record = rcldocToDbData(doc, newdocument, pageincrvec);
    newdocument.set_data(record);
    return true;
}

std::string Db::Native::rcldocToDbData(
    Doc& doc, Xapian::Document& newdocument, std::vector <std::pair<int, int>>& pageincrvec)
{
    //////////////////////////////////////////////////////////////////
    // Document data record. omindex has the following nl separated fields:
    // - url
    // - sample
    // - caption (title limited to 100 chars)
    // - mime type 
    //
    // The title, author, abstract and keywords fields are special,
    // they always get stored in the document data
    // record. Configurable other fields can be, too.
    //
    // We truncate stored fields abstract, title and keywords to
    // reasonable lengths and suppress newlines (so that the data
    // record can keep a simple syntax)

    string record;
    RECORD_APPEND(record, Doc::keyurl, doc.url);
    RECORD_APPEND(record, Doc::keytp, doc.mimetype);
    // We left-zero-pad the times so that they are lexico-sortable
    leftzeropad(doc.fmtime, 11);
    RECORD_APPEND(record, Doc::keyfmt, doc.fmtime);
#ifdef EXT4_BIRTH_TIME
    {
        std::string birtime;
        doc.getmeta(Doc::keybrt, &birtime);
        if (!birtime.empty()) {
            leftzeropad(birtime, 11);
            RECORD_APPEND(record, Doc::keybrt, birtime);
        }
    }
#endif
    if (!doc.dmtime.empty()) {
        leftzeropad(doc.dmtime, 11);
        RECORD_APPEND(record, Doc::keydmt, doc.dmtime);
    }
    RECORD_APPEND(record, Doc::keyoc, doc.origcharset);

    if (doc.fbytes.empty())
        doc.fbytes = doc.pcbytes;

    if (!doc.fbytes.empty()) {
        RECORD_APPEND(record, Doc::keyfs, doc.fbytes);
        leftzeropad(doc.fbytes, 12);
        newdocument.add_value(VALUE_SIZE, doc.fbytes);
    }
    if (doc.haschildren) {
        newdocument.add_boolean_term(has_children_term);
    }   
    if (!doc.pcbytes.empty())
        RECORD_APPEND(record, Doc::keypcs, doc.pcbytes);
    RECORD_APPEND(record, Doc::keyds, std::to_string(doc.text.length()));

    // Note that we add the signature both as a value and in the data record
    if (!doc.sig.empty()) {
        RECORD_APPEND(record, Doc::keysig, doc.sig);
        newdocument.add_value(VALUE_SIG, doc.sig);
    }

    if (!doc.ipath.empty())
        RECORD_APPEND(record, Doc::keyipt, doc.ipath);

    // Fields from the Meta array. Handle title specially because it has a 
    // different name inside the data record (history...)
    string& ttref = doc.meta[Doc::keytt];
    ttref = neutchars(truncate_to_word(ttref, m_rcldb->m_idxMetaStoredLen), cstr_nc);
    if (!ttref.empty()) {
        RECORD_APPEND(record, cstr_caption, ttref);
        ttref.clear();
    }

    // If abstract is empty, we make up one with the beginning of the
    // document. This is then not indexed, but part of the doc data so
    // that we can return it to a query without having to decode the
    // original file.
    // Note that the map accesses by operator[] create empty entries if they
    // don't exist yet.
    if (m_rcldb->m_idxAbsTruncLen > 0) {
        string& absref = doc.meta[Doc::keyabs];
        trimstring(absref, " \t\r\n");
        if (absref.empty()) {
            if (!doc.text.empty())
                absref = cstr_syntAbs +
                    neutchars(truncate_to_word(doc.text, m_rcldb->m_idxAbsTruncLen), cstr_nc);
        } else {
            absref = neutchars(truncate_to_word(absref, m_rcldb->m_idxAbsTruncLen), cstr_nc);
        }
        // Do the append here to avoid the different truncation done
        // in the regular "stored" loop
        if (!absref.empty()) {
            RECORD_APPEND(record, Doc::keyabs, absref);
            absref.clear();
        }
    }
        
    // Append all regular "stored" meta fields
    for (const auto& rnm : m_rcldb->m_config->getStoredFields()) {
        string nm = m_rcldb->m_config->fieldCanon(rnm);
        if (!doc.meta[nm].empty()) {
            string value =
                neutchars(truncate_to_word(doc.meta[nm], m_rcldb->m_idxMetaStoredLen), cstr_nc);
            RECORD_APPEND(record, nm, value);
        }
    }

    // At this point, if the document "filename" field was empty, try to store the "container file
    // name" value. This is done after indexing because we don't want search matches on this, but
    // the filename is often useful for display purposes.
    const string *fnp = nullptr;
    if (!doc.peekmeta(Rcl::Doc::keyfn, &fnp) || fnp->empty()) {
        if (doc.peekmeta(Rcl::Doc::keyctfn, &fnp) && !fnp->empty()) {
            string value = neutchars(truncate_to_word(*fnp, m_rcldb->m_idxMetaStoredLen), cstr_nc);
            RECORD_APPEND(record, Rcl::Doc::keyfn, value);
        }
    }

    // If empty pages (multiple break at same pos) were recorded, save them (this is because we have
    // no way to record them in the Xapian list). pageincrvec is a termproc member inside rcldb and
    // was updated by calling the splitter.
    if (!pageincrvec.empty()) {
        ostringstream multibreaks;
        for (unsigned int i = 0; i < pageincrvec.size(); i++) {
            if (i != 0)
                multibreaks << ",";
            multibreaks << pageincrvec[i].first << "," << pageincrvec[i].second;
        }
        RECORD_APPEND(record, string(cstr_mbreaks), multibreaks.str());
    }
    
    // If the file's md5 was computed, add value and term.  The value is optionally used for query
    // result duplicate elimination, and the term to find the duplicates (XM is the prefix for
    // rclmd5 in fields) We don't do this for empty docs.
    const string *md5;
    if (doc.peekmeta(Doc::keymd5, &md5) && !md5->empty() && md5->compare(cstr_md5empty)) {
        string digest;
        MD5HexScan(*md5, digest);
        newdocument.add_value(VALUE_MD5, digest);
        newdocument.add_boolean_term(wrap_prefix("XM") + *md5);
    }

    LOGDEB0("Rcl::Db::add: new doc record:\n" << record << "\n");
    return record;
}

bool Db::Native::docToXdocMetaOnly(TextSplitDb *splitter, const string &udi, 
                                    Doc &doc, Xapian::Document& xdoc)
{
    LOGDEB0("Db::docToXdocMetaOnly\n");
#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_mutex);
#endif

    // Read existing document and its data record
    if (getDoc(udi, 0, xdoc) == 0) {
        LOGERR("docToXdocMetaOnly: existing doc not found\n");
        return false;
    }
    string data;
    XAPTRY(data = xdoc.get_data(), xrdb, m_rcldb->m_reason);
    if (!m_rcldb->m_reason.empty()) {
        LOGERR("Db::xattrOnly: got error: " << m_rcldb->m_reason << "\n");
        return false;
    }

    // Clear the term lists for the incoming fields and index the new values
    map<string, string>::iterator meta_it;
    for (const auto& [key, data] : doc.meta) {
        const FieldTraits *ftp;
        if (!m_rcldb->fieldToTraits(key, &ftp) || ftp->pfx.empty()) {
            LOGDEB0("Db::xattrOnly: no prefix for field [" << key << "], skipped\n");
            continue;
        }
        // Clear the previous terms for the field
        clearField(xdoc, ftp->pfx, ftp->wdfinc);
        LOGDEB0("Db::xattrOnly: field [" << key << "] pfx [" <<
                ftp->pfx << "] inc " << ftp->wdfinc << ": [" << data << "]\n");
        splitter->setTraits(*ftp);
        if (!splitter->text_to_words(data)) {
            LOGDEB("Db::xattrOnly: split failed for " << key << "\n");
        }
    }
    xdoc.add_value(VALUE_SIG, doc.sig);

    // Parse current data record into a dict for ease of processing
    ConfSimple datadic(data);
    if (!datadic.ok()) {
        LOGERR("db::docToXdocMetaOnly: failed turning data rec to dict\n");
        return false;
    }

    // For each "stored" field, check if set in doc metadata and
    // update the value if it is
    for (const auto& rnm : m_rcldb->m_config->getStoredFields()) {
        string nm = m_rcldb->m_config->fieldCanon(rnm);
        if (doc.getmeta(nm, nullptr)) {
            string value = neutchars(
                truncate_to_word(doc.meta[nm], m_rcldb->m_idxMetaStoredLen), cstr_nc);
            datadic.set(nm, value, "");
        }
    }

    // Recreate the record. We want to do this with the local RECORD_APPEND
    // method for consistency in format, instead of using ConfSimple print
    data.clear();
    for (const auto& nm : datadic.getNames("")) {
        string value;
        datadic.get(nm, value, "");
        RECORD_APPEND(data, nm, value);
    }
    RECORD_APPEND(data, Doc::keysig, doc.sig);
    xdoc.set_data(data);
    return true;
}

} // End namespace Rcl
