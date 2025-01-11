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
#include "internfile.h"
#include "wipedir.h"
#include "zlibut.h"
#include "idxstatus.h"
#include "rcldoc.h"
#include "stoplist.h"
#include "daterange.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif



namespace Rcl {

// Recoll index format version is stored in user metadata. When this change,
// we can't open the db and will have to reindex.
const string cstr_RCL_IDX_VERSION_KEY("RCL_IDX_VERSION_KEY");
const string cstr_RCL_IDX_VERSION("1");
const string cstr_RCL_IDX_DESCRIPTOR_KEY("RCL_IDX_DESCRIPTOR_KEY");

const string cstr_mbreaks("rclmbreaks");
// Some prefixes that we could get from the fields file, but are not going to ever change.
const string fileext_prefix = "XE";
const string mimetype_prefix = "T";
const string pathelt_prefix = "XP";
const string udi_prefix("Q");
const string parent_prefix("F");
// Special terms to mark begin/end of field (for anchored searches).
string start_of_field_term;
string end_of_field_term;
// Special term for page breaks. Note that we use a complicated mechanism for multiple page
// breaks at the same position, when it would have been probably simpler to use XXPG/n terms
// instead (did not try to implement though). A change would force users to reindex.
const string page_break_term = "XXPG/";
// Special term to mark documents with children.
const string has_children_term("XXC/");
// Field name for the unsplit file name. Has to exist in the field file 
// because of usage in termmatch()
const string unsplitFilenameFieldName = "rclUnsplitFN";
const string unsplitfilename_prefix = "XSFS";
// Synthetic abstract marker (to discriminate from abstract actually found in document)
const string cstr_syntAbs("?!#@");

string version_string(){
    return string("Recoll ") + string(PACKAGE_VERSION) + string(" + Xapian ") +
        string(Xapian::version_string());
}

/* Rcl::Db methods ///////////////////////////////// */

bool Db::o_nospell_chars[256];

Db::Db(const RclConfig *cfp)
{
    m_config = new RclConfig(*cfp);
    m_config->getConfParam("maxfsoccuppc", &m_maxFsOccupPc);
    m_config->getConfParam("idxflushmb", &m_flushMb);
    m_config->getConfParam("idxmetastoredlen", &m_idxMetaStoredLen);
    m_config->getConfParam("idxtexttruncatelen", &m_idxTextTruncateLen);
    m_config->getConfParam("autoSpellRarityThreshold", &m_autoSpellRarityThreshold);
    m_config->getConfParam("autoSpellSelectionThreshold", &m_autoSpellSelectionThreshold);
    if (start_of_field_term.empty()) {
        if (o_index_stripchars) {
            start_of_field_term = "XXST";
            end_of_field_term = "XXND";
        } else {
            start_of_field_term = "XXST/";
            end_of_field_term = "XXND/";
        }
        memset(o_nospell_chars, 0, sizeof(o_nospell_chars));
        for (unsigned char c : " !\"#$%&()*+,-./0123456789:;<=>?@[\\]^_`{|}~") {
            o_nospell_chars[(unsigned int)c] = 1;
        }
    }
    m_ndb = new Native(this);
    m_syngroups = std::make_unique<SynGroups>();
    m_stops = std::make_unique<StopList>();
}

Db::~Db()
{
    LOGDEB2("Db::~Db\n");
    if (nullptr == m_ndb)
        return;
    LOGDEB("Db::~Db: isopen " << m_ndb->m_isopen << " m_iswritable " << m_ndb->m_iswritable << "\n");
    this->close();
    delete m_ndb;
#ifdef RCL_USE_ASPELL
    delete m_aspell;
#endif
    delete m_config;
}

vector<string> Db::getStemmerNames()
{
    vector<string> res;
    stringToStrings(Xapian::Stem::get_available_languages(), res);
    return res;
}

bool Db::open(OpenMode mode, OpenError *error, int flags)
{
    if (error)
        *error = DbOpenMainDb;

    if (nullptr == m_ndb || m_config == nullptr) {
        m_reason = "Null configuration or Xapian Db";
        return false;
    }
    LOGDEB("Db::open: m_isopen " << m_ndb->m_isopen << " m_iswritable " <<
           m_ndb->m_iswritable << " mode " << mode << "\n");

    m_inPlaceReset = false;
    if (m_ndb->m_isopen) {
        // We used to return an error here but I see no reason to
        if (!close())
            return false;
    }
    if (!m_config->getStopfile().empty())
        m_stops->setFile(m_config->getStopfile());

    if (isWriteMode(mode)) {
        // Check for an index-time synonyms file. We use this to
        // generate multiword terms for multiword synonyms
        string synfile = m_config->getIdxSynGroupsFile();
        if (path_exists(synfile)) {
            setSynGroupsFile(synfile);
        }
    }
    
    string dir = m_config->getDbDir();
    string ermsg;
    try {
        if (isWriteMode(mode)) {
            m_ndb->openWrite(dir, mode, flags);
            updated = vector<bool>(m_ndb->xwdb.get_lastdocid() + 1, false);
            // We used to open a readonly object in addition to the r/w one because some operations
            // were faster when performed through a Database: no forced flushes on allterms_begin(),
            // used in subDocs(). This issue has been gone for a long time (now: Xapian 1.2) and the
            // separate objects seem to trigger other Xapian issues, so the query db is now a clone
            // of the update one.
            m_ndb->xrdb = m_ndb->xwdb;
            LOGDEB("Db::open: lastdocid: " << m_ndb->xwdb.get_lastdocid() << "\n");
        } else {
            m_ndb->openRead(dir);
            for (auto& db : m_extraDbs) {
                if (error)
                    *error = DbOpenExtraDb;
                LOGDEB("Db::Open: adding query db [" << &db << "]\n");
                // An error here used to be non-fatal (1.13 and older) but I can't see why
                m_ndb->xrdb.add_database(Xapian::Database(db));
            }
        }
        if (error)
            *error = DbOpenMainDb;

        // Check index format version. Must not try to check a just created or truncated db
        if (mode != DbTrunc && m_ndb->xrdb.get_doccount() > 0) {
            string version = m_ndb->xrdb.get_metadata(cstr_RCL_IDX_VERSION_KEY);
            if (version.compare(cstr_RCL_IDX_VERSION)) {
                m_ndb->m_noversionwrite = true;
                LOGERR("Rcl::Db::open: file index [" << version <<
                       "], software [" << cstr_RCL_IDX_VERSION << "]\n");
                throw Xapian::DatabaseError("Recoll index version mismatch", "", "");
            }
        }
        m_mode = mode;
        m_ndb->m_isopen = true;
        m_basedir = dir;
        if (error)
            *error = DbOpenNoError;
        return true;
    } XCATCHERROR(ermsg);

    m_reason = ermsg;
    LOGERR("Db::open: exception while opening [" << dir << "]: " << ermsg << "\n");
    return false;
}

bool Db::storesDocText()
{
    if (!m_ndb || !m_ndb->m_isopen) {
        LOGERR("Db::storesDocText: called on non-opened db\n");
        return false;
    }
    return m_ndb->m_storetext;
}

// Note: xapian has no close call, we delete and recreate the db
bool Db::close()
{
    if (nullptr == m_ndb)
        return false;
    LOGDEB("Db::close: isopen " << m_ndb->m_isopen << " iswritable " << m_ndb->m_iswritable << "\n");
    if (m_ndb->m_isopen == false) {
        return true;
    }

    string ermsg;
    try {
        bool w = m_ndb->m_iswritable;
        if (w) {
            LOGDEB("Rcl::Db:close: xapian will close. May take some time\n");
#ifdef IDX_THREADS
            m_ndb->m_wqueue.closeShop();
            if (m_ndb->m_tmpdbinitidx > 0) {
                m_ndb->m_mwqueue.closeShop();
            }
            waitUpdIdle();
#endif
            if (!m_ndb->m_noversionwrite) {
                m_ndb->xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, cstr_RCL_IDX_VERSION);
                m_ndb->xwdb.commit();
            }

#ifdef IDX_THREADS
            if (m_ndb->m_tmpdbinitidx > 0) {
                mergeAndCompact();
            }
#endif // IDX_THREADS
        }
        LOGDEB("Rcl::Db:close() xapian close done.\n");

        deleteZ(m_ndb);
        m_ndb = new Native(this);
        return true;
    } XCATCHERROR(ermsg);
    LOGERR("Db:close: exception while deleting/recreating db object: " << ermsg << "\n");
    return false;
}

void Db::mergeAndCompact()
{
#ifdef IDX_THREADS
    if (m_ndb->m_tmpdbinitidx <= 0)
        return;
    
    // Note: the commits() have been called by waitUpdIdle() above.
    LOGINF("Rcl::Db::close: starting merge of " << m_ndb->m_tmpdbinitidx << " temporary indexes\n");
    for (int i = 0; i < m_ndb->m_tmpdbinitidx; i++) {
        m_ndb->xwdb.add_database(m_ndb->m_tmpdbs[i]);
    }
    string dbdir = m_config->getDbDir();
    std::set<std::string> oldfiles;
    std::string errs;
    if (!listdir(dbdir, errs, oldfiles)) {
        LOGERR("Db::close: failed listing existing db files in " << dbdir << ": " << errs << "\n");
        throw Xapian::DatabaseError("Failed listing db files", errs);
    }

    std::string tmpdir = path_cat(dbdir, "tmp");
    m_ndb->xwdb.compact(tmpdir);

    // Get rid of the temporary indexes and their directories
    deleteZ(m_ndb);

    // Back up the current index by moving it aside.
    auto backupdir = path_cat(dbdir, "backup");
    path_makepath(backupdir, 0700);
    for (const auto& fn : oldfiles) {
        auto ofn = path_cat(dbdir, fn);
        auto nfn = path_cat(backupdir, fn);
        if (path_isfile(ofn) && !path_rename(ofn, nfn)) {
            LOGERR("Db::close: failed renaming " << ofn << " to " << nfn << "\n");
            throw Xapian::DatabaseError("Failed renaming db file for backup", ofn + "->" + nfn);
        }
    }

    // Move the new index in place
    std::set<std::string> nfiles;
    if (!listdir(tmpdir, errs, nfiles)) {
        LOGERR("Db::close: failed listing newdb files in " << tmpdir << ": " << errs << "\n");
        throw Xapian::DatabaseError("Failed listing new db files", errs);
    }
    for (const auto& fn : nfiles) {
        auto ofn = path_cat(tmpdir, fn);
        auto nfn = path_cat(dbdir, fn);
        if (path_isfile(ofn) && !path_rename(ofn, nfn)) {
            LOGERR("Db::close: failed renaming " << ofn << " to " << nfn << "\n");
            throw Xapian::DatabaseError("Failed renaming new db file to dbdir", ofn + "->" + nfn);
        }
    }

    // Get rid of the old index
    wipedir(backupdir, true, true);
    LOGINF("Rcl::Db::close: merge and compact done\n");
#endif // IDX_THREADS
}

int Db::docCnt()
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    XAPTRY(res = m_ndb->xrdb.get_doccount(), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::docCnt: got error: " << m_reason << "\n");
        return -1;
    }
    return res;
}

int Db::termDocCnt(const string& _term)
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    string term = _term;
    if (o_index_stripchars)
        if (!unacmaybefold(_term, term, UNACOP_UNACFOLD)) {
            LOGINFO("Db::termDocCnt: unac failed for [" << _term << "]\n");
            return 0;
        }

    if (m_stops->isStop(term)) {
        LOGDEB1("Db::termDocCnt [" << term << "] in stop list\n");
        return 0;
    }

    XAPTRY(res = m_ndb->xrdb.get_termfreq(term), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR("Db::termDocCnt: got error: " << m_reason << "\n");
        return -1;
    }
    return res;
}

// Reopen the db with a changed list of additional dbs
bool Db::adjustdbs()
{
    if (m_mode != DbRO) {
        LOGERR("Db::adjustdbs: mode not RO\n");
        return false;
    }
    if (m_ndb && m_ndb->m_isopen) {
        if (!close())
            return false;
        if (!open(m_mode)) {
            return false;
        }
    }
    return true;
}

// Set the extra indexes to the input list.
bool Db::setExtraQueryDbs(const std::vector<std::string>& dbs)
{
    LOGDEB0("Db::setExtraQueryDbs: ndb " << m_ndb << " iswritable " <<
            ((m_ndb)?m_ndb->m_iswritable:0) << " dbs [" << stringsToString(dbs) << "]\n");
    if (!m_ndb) {
        return false;
    }
    if (m_ndb->m_iswritable) {
        return false;
    }
    m_extraDbs.clear();
    for (const auto& dir : dbs) {
        m_extraDbs.push_back(path_canon(dir));
    }
    return adjustdbs();
}

bool Db::addQueryDb(const string &_dir) 
{
    string dir = _dir;
    LOGDEB0("Db::addQueryDb: ndb " << m_ndb << " iswritable " <<
            ((m_ndb)?m_ndb->m_iswritable:0) << " db [" << dir << "]\n");
    if (!m_ndb)
        return false;
    if (m_ndb->m_iswritable)
        return false;
    dir = path_canon(dir);
    if (find(m_extraDbs.begin(), m_extraDbs.end(), dir) == m_extraDbs.end()) {
        m_extraDbs.push_back(dir);
    }
    return adjustdbs();
}

bool Db::rmQueryDb(const string &dir)
{
    if (!m_ndb)
        return false;
    if (m_ndb->m_iswritable)
        return false;
    if (dir.empty()) {
        m_extraDbs.clear();
    } else {
        auto it = find(m_extraDbs.begin(), m_extraDbs.end(), dir);
        if (it != m_extraDbs.end()) {
            m_extraDbs.erase(it);
        }
    }
    return adjustdbs();
}

// Determining what index a doc result comes from is based on the
// modulo of the docid against the db count. Ref:
// http://trac.xapian.org/wiki/FAQ/MultiDatabaseDocumentID
bool Db::fromMainIndex(const Doc& doc)
{
    return m_ndb->whatDbIdx(doc.xdocid) == 0;
}

std::string Db::whatIndexForResultDoc(const Doc& doc)
{
    size_t idx = m_ndb->whatDbIdx(doc.xdocid);
    if (idx == (size_t)-1) {
        LOGERR("whatIndexForResultDoc: whatDbIdx returned -1 for " << doc.xdocid << "\n");
        return string();
    }
    // idx is [0..m_extraDbs.size()] 0 is for the main index, else
    // idx-1 indexes into m_extraDbs
    if (idx == 0) {
        return m_basedir;
    } else {
        return m_extraDbs[idx-1];
    }
}

bool Db::testDbDir(const string &dir, bool *stripped_p)
{
    string aerr;
    bool mstripped = true;
    LOGDEB("Db::testDbDir: [" << dir << "]\n");
    try {
        Xapian::Database db(dir);
        // If the prefix for udi is wrapped, it's an unstripped index. Q is guaranteed to exist if
        // there is at least one doc in the index.
        Xapian::TermIterator term = db.allterms_begin(":Q:");
        if (term == db.allterms_end()) {
            mstripped = true;
        } else {
            mstripped = false;
        }
        LOGDEB("testDbDir: " << dir << " is a " << (mstripped ? "stripped" : "raw") << " index\n");
    } XCATCHERROR(aerr);
    if (!aerr.empty()) {
        LOGERR("Db::Open: error while trying to open database from [" << dir << "]: " << aerr<<"\n");
        return false;
    }
    if (stripped_p) 
        *stripped_p = mstripped;

    return true;
}

bool Db::isopen()
{
    if (nullptr == m_ndb)
        return false;
    return m_ndb->m_isopen;
}

// Try to translate field specification into field prefix. 
bool Db::fieldToTraits(const string& fld, const FieldTraits **ftpp, bool isquery)
{
    if (m_config && m_config->getFieldTraits(fld, ftpp, isquery))
        return true;

    *ftpp = nullptr;
    return false;
}


class TermProcIdx : public TermProc {
public:
    TermProcIdx() : TermProc(nullptr), m_ts(nullptr), m_lastpagepos(0), m_pageincr(0) {}
    void setTSD(TextSplitDb *ts) {m_ts = ts;}

    bool takeword(const std::string &term, size_t pos, size_t, size_t) override {
        // Compute absolute position (pos is relative to current segment), and remember relative.
        m_ts->curpos = static_cast<Xapian::termpos>(pos);
        pos += m_ts->basepos;
        // Don't try to add empty term Xapian doesnt like it... Safety check
        // this should not happen.
        if (term.empty())
            return true;
        string ermsg;
        try {
            // Index without prefix, using the field-specific weighting
            LOGDEB1("Emitting term at " << pos << " : [" << term << "]\n");
            if (!m_ts->ft.pfxonly) {
                if (!o_no_term_positions) {
                    m_ts->doc.add_posting(term, static_cast<Xapian::termpos>(pos), m_ts->ft.wdfinc);
                } else {
                    m_ts->doc.add_term(term, m_ts->ft.wdfinc);
                }
            }

#ifdef TESTING_XAPIAN_SPELL
            if (Db::isSpellingCandidate(term, false)) {
                m_ts->wdb.add_spelling(term);
            }
#endif
            // Index the prefixed term.
            if (!m_ts->ft.pfx.empty()) {
                if (!o_no_term_positions) {
                    m_ts->doc.add_posting(m_ts->ft.pfx + term, static_cast<Xapian::termpos>(pos),
                                          m_ts->ft.wdfinc);
                } else {
                    m_ts->doc.add_term(m_ts->ft.pfx + term, m_ts->ft.wdfinc);
                }
            }
            return true;
        } XCATCHERROR(ermsg);
        LOGERR("Db: xapian add_posting error " << ermsg << "\n");
        return false;
    }
    void newpage(size_t pos) override
    {
        pos += m_ts->basepos;
        if (pos < baseTextPosition) {
            LOGDEB("newpage: not in body: " << pos << "\n");
            return;
        }

        if (!o_no_term_positions) 
            m_ts->doc.add_posting(m_ts->ft.pfx + page_break_term, static_cast<Xapian::termpos>(pos));
        if (pos == static_cast<unsigned int>(m_lastpagepos)) {
            m_pageincr++;
            LOGDEB2("newpage: same pos, pageincr " << m_pageincr <<
                    " lastpagepos " << m_lastpagepos << "\n");
        } else {
            LOGDEB2("newpage: pos change, pageincr " << m_pageincr <<
                    " lastpagepos " << m_lastpagepos << "\n");
            if (m_pageincr > 0) {
                // Remember the multiple page break at this position
                unsigned int relpos = m_lastpagepos - baseTextPosition;
                LOGDEB2("Remembering multiple page break. Relpos " << relpos <<
                        " cnt " << m_pageincr << "\n");
                m_pageincrvec.push_back({relpos, m_pageincr});
            }
            m_pageincr = 0;
        }
        m_lastpagepos = static_cast<int>(pos);
    }

    virtual bool flush() override
    {
        if (m_pageincr > 0) {
            unsigned int relpos = m_lastpagepos - baseTextPosition;
            LOGDEB2("Remembering multiple page break. Position " << relpos <<
                    " cnt " << m_pageincr << "\n");
            m_pageincrvec.push_back(pair<int, int>(relpos, m_pageincr));
            m_pageincr = 0;
        }
        return TermProc::flush();
    }

    TextSplitDb *m_ts;
    // Auxiliary page breaks data for positions with multiple page breaks.
    int m_lastpagepos;
    // increment of page breaks at same pos. Normally 0, 1.. when several
    // breaks at the same pos
    int m_pageincr; 
    vector <pair<int, int> > m_pageincrvec;
};


// Let our user set the parameters for abstract processing
void Db::setAbstractParams(int idxtrunc, int syntlen, int syntctxlen)
{
    LOGDEB1("Db::setAbstractParams: trunc " << idxtrunc << " syntlen " <<
            syntlen << " ctxlen " << syntctxlen << "\n");
    if (idxtrunc >= 0)
        m_idxAbsTruncLen = idxtrunc;
    if (syntlen > 0)
        m_synthAbsLen = syntlen;
    if (syntctxlen > 0)
        m_synthAbsWordCtxLen = syntctxlen;
}

bool Db::setSynGroupsFile(const string& fn)
{
    return m_syngroups->setfile(fn);
}


// Add document in internal form to the database: index the terms in the title abstract and body and
// add special terms for file name, date, mime type etc., create the document data record (more
// metadata), and update database.
bool Db::addOrUpdate(const string &udi, const string &parent_udi, Doc &doc)
{
    LOGDEB("Db::add: udi [" << udi << "] parent [" << parent_udi << "]\n");
    if (nullptr == m_ndb)
        return false;

    // This document is potentially going to be passed to the index update thread. The reference
    // counters are not mt-safe, so we need to do this through a pointer. The reference is just
    // there to avoid changing too much code (the previous version passed a copy).
    std::unique_ptr<Xapian::Document> newdocument_ptr = std::make_unique<Xapian::Document>();
    Xapian::Document& newdocument(*newdocument_ptr.get());
    
    // The term processing pipeline:
    TermProcIdx tpidx;
    TermProc *nxt = &tpidx;
    TermProcStop tpstop(nxt, *m_stops);nxt = &tpstop;
    //TermProcCommongrams tpcommon(nxt, m_stops); nxt = &tpcommon;

    TermProcMulti tpmulti(nxt, *m_syngroups);
    if (m_syngroups->getmultiwordsmaxlength() > 1) {
        nxt = &tpmulti;
    }

    TermProcPrep tpprep(nxt);
    if (o_index_stripchars)
        nxt = &tpprep;
    
    TextSplitDb splitter(m_ndb->xwdb, newdocument, nxt);
    tpidx.setTSD(&splitter);

    // Udi unique term: this is used for file existence/uptodate
    // checks, and unique id for the replace_document() call.
    string uniterm = Native::make_uniterm(udi);
    string rawztext; // Doc compressed text

    if (doc.metaonly) {
        // Only updating an existing doc with new extended attributes data.  Need to read the old
        // doc and its data record first. This is so different from the normal processing that it
        // uses a fully separate code path (with some duplication unfortunately)
        if (!m_ndb->docToXdocMetaOnly(&splitter, udi, doc, newdocument)) {
            return false;
        }
    } else {
        if (!m_ndb->docToXdoc(&splitter, parent_udi, uniterm, doc, newdocument,
                              rawztext, tpidx.m_pageincrvec)) {
            return false;
        }
    }
    
#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
        DbUpdTask *tp = new DbUpdTask(DbUpdTask::AddOrUpdate, udi, uniterm,
                                      std::move(newdocument_ptr), doc.text.length(), rawztext);
        if (!m_ndb->m_wqueue.put(tp)) {
            LOGERR("Db::addOrUpdate:Cant queue task\n");
            return false;
        }
        return true;
    }
#endif

    return m_ndb->addOrUpdateWrite(udi, uniterm,
                                   std::move(newdocument_ptr), doc.text.length(), rawztext);
}


#ifdef IDX_THREADS
void Db::closeQueue()
{
    if (m_ndb->m_iswritable && m_ndb->m_havewriteq) {
        m_ndb->m_wqueue.closeShop();
    }
}

void Db::waitUpdIdle()
{
    if (m_ndb->m_iswritable && m_ndb->m_havewriteq) {
        Chrono chron;
        m_ndb->m_wqueue.waitIdle();
        if (m_ndb->m_tmpdbinitidx > 0) {
            m_ndb->m_mwqueue.waitIdle();
        }
        // We flush here just for correct measurement of the thread work time
        string ermsg;
        try {
            LOGINF("DbMUpdWorker: flushing main index\n");
            m_ndb->xwdb.commit();
            for (int i = 0; i < m_ndb->m_tmpdbinitidx; i++) {
                LOGDEB("DbMUpdWorker: flushing index " << i << "\n");
                m_ndb->m_tmpdbs[i].commit();
            }
        } XCATCHERROR(ermsg);
        if (!ermsg.empty()) {
            LOGERR("Db::waitUpdIdle: flush() failed: " << ermsg << "\n");
        }
        m_ndb->m_totalworkns += chron.nanos();
        LOGINFO("Db::waitUpdIdle: total xapian work " <<
                lltodecstr(m_ndb->m_totalworkns/1000000) << " mS\n");
    }
}
#endif

// Flush when idxflushmbs is reached
bool Db::maybeflush(int64_t moretext)
{
    if (m_flushMb > 0) {
        m_curtxtsz += moretext;
        if ((m_curtxtsz - m_flushtxtsz) / MB >= m_flushMb) {
            LOGINF("Db::add/delete: txt size >= " << m_flushMb << " Mb, flushing\n");
            return doFlush();
        }
    }
    return true;
}

bool Db::doFlush()
{
    if (!m_ndb) {
        LOGERR("Db::doFLush: no ndb??\n");
        return false;
    }
#ifdef IDX_THREADS
    if (m_ndb->m_tmpdbinitidx > 0) {
        std::lock_guard<std::mutex> lock(m_ndb->m_initidxmutex);
        for (int i = 0; i < m_ndb->m_tmpdbinitidx; i++) {
            m_ndb->m_tmpdbflushflags[i] = 1;
        }
    }
#endif
    string ermsg;
    try {
        statusUpdater()->update(DbIxStatus::DBIXS_FLUSH, "");
        LOGINF("DbMUpdWorker: flushing main index\n");
        m_ndb->xwdb.commit();
    } XCATCHERROR(ermsg);
    statusUpdater()->update(DbIxStatus::DBIXS_NONE, "");
    if (!ermsg.empty()) {
        LOGERR("Db::doFlush: flush() failed: " << ermsg << "\n");
        return false;
    }
    m_flushtxtsz = m_curtxtsz;
    return true;
}

void Db::setExistingFlags(const string& udi, unsigned int docid)
{
    if (m_mode == DbRO)
        return;
    if (docid == (unsigned int)-1) {
        LOGERR("Db::setExistingFlags: called with bogus docid !!\n");
        return;
    }
#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif
    i_setExistingFlags(udi, docid);
}

void Db::i_setExistingFlags(const string& udi, unsigned int docid)
{
    // Set the up to date flag for the document and its subdocs. needUpdate() can also be called at
    // query time (for preview up to date check), so no error if the updated bitmap is of size 0,
    // and also this now happens when fsIndexer() calls udiTreeMarkExisting() after an error, so the
    // message level is now debug
    if (docid >= updated.size()) {
        if (updated.size()) {
            LOGDEB("needUpdate: existing docid beyond updated.size() "
                   "(probably ok). Udi [" << udi << "], docid " << docid <<
                   ", updated.size() " << updated.size() << "\n");
        }
        return;
    } else {
        updated[docid] = true;
    }

    // Set the existence flag for all the subdocs (if any)
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(udi, 0, docids)) {
        LOGERR("Rcl::Db::needUpdate: can't get subdocs\n");
        return;
    }
    for (const auto docid : docids) {
        if (docid < updated.size()) {
            LOGDEB2("Db::needUpdate: docid " << docid << " set\n");
            updated[docid] = true;
        }
    }
}

// Before running indexing for a specific backend: set the existence bits for all backends except
// the specified one, so that the corresponding documents are not purged after the update.  The "FS"
// backend is special because the documents usually have no rclbes field (but we prepare for the
// time when they might have one).
//  - If this is the FS backend: unset all the bits, set them for all backends but FS (docs without
//    a backend field will stay off).
//  - Else set all the bits, unset them for the specified backend (docs without a backend field
//    will stay on).
bool Db::preparePurge(const std::string& _backend)
{
    auto backend = stringtolower(_backend);
    TermMatchResult result;
    if (!idxTermMatch(ET_WILD, "*", result, -1, Doc::keybcknd)) {
        LOGERR("Rcl::Db:preparePurge: termMatch failed\n");
        return false;
    }
    if ("fs" == backend) {
        updated = vector<bool>(m_ndb->xwdb.get_lastdocid() + 1, false);
        for (const auto& entry : result.entries) {
            auto stripped = strip_prefix(entry.term);
            if (stripped.empty() || "fs" == stripped)
                continue;
            Xapian::PostingIterator docid = m_ndb->xrdb.postlist_begin(entry.term);
            while (docid != m_ndb->xrdb.postlist_end(entry.term)) {
                if (*docid < updated.size()) {
                    updated[*docid] = true;
                }
                docid++;
            }
        }
    } else {
        updated = vector<bool>(m_ndb->xwdb.get_lastdocid() + 1, true);
        for (const auto& entry : result.entries) {
            auto stripped = strip_prefix(entry.term);
            if (stripped.empty() || backend != strip_prefix(entry.term))
                continue;
            Xapian::PostingIterator docid = m_ndb->xrdb.postlist_begin(entry.term);
            while (docid != m_ndb->xrdb.postlist_end(entry.term)) {
                if (*docid < updated.size()) {
                    updated[*docid] = false;
                }
                docid++;
            }
        }
    }
    return true;
}

// Test if doc given by udi has changed since last indexed (test sigs)
bool Db::needUpdate(const string &udi, const string& sig, unsigned int *docidp, string *osigp)
{
    if (nullptr == m_ndb)
        return false;

    if (osigp)
        osigp->clear();
    if (docidp)
        *docidp = 0;

    // If we are doing an in place or full reset, no need to test.
    if (m_inPlaceReset || m_mode == DbTrunc) {
        // For in place reset, pretend the doc existed, to enable
        // subdoc purge. The value is only used as a boolean in this case.
        if (docidp && m_inPlaceReset) {
            *docidp = -1;
        }
        return true;
    }

    string uniterm = Native::make_uniterm(udi);
    string ermsg;

#ifdef IDX_THREADS
    // Need to protect against interaction with the doc update/insert
    // thread which also updates the existence map, and even multiple
    // accesses to the readonly Xapian::Database are not allowed
    // anyway
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    // Try to find the document indexed by the uniterm. 
    Xapian::PostingIterator docid;
    XAPTRY(docid = m_ndb->xrdb.postlist_begin(uniterm), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: xapian::postlist_begin failed: " << m_reason << "\n");
        return false;
    }
    if (docid == m_ndb->xrdb.postlist_end(uniterm)) {
        // No document exists with this path: we do need update
        LOGDEB("Db::needUpdate:yes (new): [" << uniterm << "]\n");
        return true;
    }
    Xapian::Document xdoc;
    XAPTRY(xdoc = m_ndb->xrdb.get_document(*docid), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: get_document error: " << m_reason << "\n");
        return true;
    }

    if (docidp) {
        *docidp = *docid;
    }

    // Retrieve old file/doc signature from value
    string osig;
    XAPTRY(osig = xdoc.get_value(VALUE_SIG), m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::needUpdate: get_value error: " << m_reason << "\n");
        return true;
    }
    LOGDEB2("Db::needUpdate: oldsig [" << osig << "] new [" << sig << "]\n");

    if (osigp) {
        *osigp = osig;
    }

    // Compare new/old sig
    if (sig != osig) {
        LOGDEB("Db::needUpdate:yes: olsig [" << osig << "] new [" << sig << "] [" << uniterm<<"]\n");
        // Db is not up to date. Let's index the file
        return true;
    }

    // Up to date. Set the existance flags in the map for the doc and
    // its subdocs.
    LOGDEB("Db::needUpdate:no: [" << uniterm << "]\n");
    i_setExistingFlags(udi, *docid);
    return false;
}

// Return existing stem db languages
vector<string> Db::getStemLangs()
{
    LOGDEB("Db::getStemLang\n");
    vector<string> langs;
    if (nullptr == m_ndb || m_ndb->m_isopen == false)
        return langs;
    StemDb db(m_ndb->xrdb);
    db.getMembers(langs);
    return langs;
}

/**
 * Delete stem db for given language
 */
bool Db::deleteStemDb(const string& lang)
{
    LOGDEB("Db::deleteStemDb(" << lang << ")\n");
    if (nullptr == m_ndb || m_ndb->m_isopen == false || !m_ndb->m_iswritable)
        return false;
    XapWritableSynFamily db(m_ndb->xwdb, synFamStem);
    return db.deleteMember(lang);
}

/**
 * Create database of stem to parents associations for a given language.
 * We walk the list of all terms, stem them, and create another Xapian db
 * with documents indexed by a single term (the stem), and with the list of
 * parent terms in the document data.
 */
bool Db::createStemDbs(const vector<string>& langs)
{
    LOGDEB("Db::createStemDbs\n");
    if (nullptr == m_ndb || m_ndb->m_isopen == false || !m_ndb->m_iswritable) {
        LOGERR("createStemDb: db not open or not writable\n");
        return false;
    }

    return createExpansionDbs(m_ndb->xwdb, langs);
}

/**
 * This is called at the end of an indexing session, to delete the documents for files that are no
 * longer there. It depends on the state of the 'updated' bitmap and will delete all documents 
 * for which the existence bit is off.
 * This can ONLY be called after an appropriate call to unsetExistbits() and a full backend document
 * set walk, else the file existence flags will be wrong.
 */
bool Db::purge()
{
    if (nullptr == m_ndb) {
        LOGERR("Db::purge: null m_ndb??\n");
        return false;
    }
    LOGDEB("Db::purge: m_isopen "<<m_ndb->m_isopen<<" m_iswritable "<<m_ndb->m_iswritable<<"\n");
    if (m_ndb->m_isopen == false || m_ndb->m_iswritable == false) 
        return false;

#ifdef IDX_THREADS
    waitUpdIdle();
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif // IDX_THREADS

    // For Xapian versions up to 1.0.1, deleting a non-existant document would trigger an exception
    // that would discard any pending update. This could lose both previous added documents or
    // deletions. Adding the flush before the delete pass ensured that any added document would go
    // to the index. Kept here because it doesn't really hurt.
    m_reason.clear();
    try {
        statusUpdater()->update(DbIxStatus::DBIXS_FLUSH, "");
        m_ndb->xwdb.commit();
    } XCATCHERROR(m_reason);
    statusUpdater()->update(DbIxStatus::DBIXS_NONE, "");
    if (!m_reason.empty()) {
        LOGERR("Db::purge: 1st flush failed: " << m_reason << "\n");
        return false;
    }

    // Walk the 'updated' bitmap and delete any Xapian document whose flag is not set (we did not
    // see its source during indexing).
    int purgecount = 0;
    for (Xapian::docid docid = 1; docid < updated.size(); ++docid) {
        if (!updated[docid]) {
            if ((purgecount+1) % 100 == 0) {
                try {
                    CancelCheck::instance().checkCancel();
                } catch(CancelExcept) {
                    LOGINFO("Db::purge: partially cancelled\n");
                    break;
                }
            }

            try {
                if (m_flushMb > 0) {
                    // We use an average term length of 5 for estimating the doc sizes which is
                    // probably not accurate but gives rough consistency with what we do for
                    // add/update. I should fetch the doc size from the data record, but this would
                    // be bad for performance.
                    Xapian::termcount trms = m_ndb->xwdb.get_doclength(docid);
                    maybeflush(trms * 5);
                }
                m_ndb->deleteDocument(docid);
                LOGDEB("Db::purge: deleted document #" << docid << "\n");
            } catch (const Xapian::DocNotFoundError &) {
                LOGDEB0("Db::purge: document #" << docid << " not found\n");
            } catch (const Xapian::Error &e) {
                LOGERR("Db::purge: document #" << docid << ": " << e.get_msg() << "\n");
            } catch (...) {
                LOGERR("Db::purge: document #" << docid << ": unknown error\n");
            }
            purgecount++;
        }
    }

    m_reason.clear();
    try {
        statusUpdater()->update(DbIxStatus::DBIXS_FLUSH, "");
        m_ndb->xwdb.commit();
    } XCATCHERROR(m_reason);
    statusUpdater()->update(DbIxStatus::DBIXS_NONE, "");
    if (!m_reason.empty()) {
        LOGERR("Db::purge: 2nd flush failed: " << m_reason << "\n");
        return false;
    }
    return true;
}

// Test for doc existence.
bool Db::docExists(const string& uniterm)
{
#ifdef IDX_THREADS
    // Need to protect read db against multiaccess. 
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    string ermsg;
    try {
        Xapian::PostingIterator docid = m_ndb->xrdb.postlist_begin(uniterm);
        if (docid == m_ndb->xrdb.postlist_end(uniterm)) {
            return false;
        } else {
            return true;
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("Db::docExists(" << uniterm << ") " << ermsg << "\n");
    }
    return false;
}

/* Delete document(s) for given unique identifier (doc and descendents) */
bool Db::purgeFile(const string &udi, bool *existed)
{
    LOGDEB("Db:purgeFile: [" << udi << "]\n");
    if (nullptr == m_ndb || !m_ndb->m_iswritable)
        return false;

    string uniterm = Native::make_uniterm(udi);
    bool exists = docExists(uniterm);
    if (existed)
        *existed = exists;
    if (!exists)
        return true;

#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
        string rztxt;
        DbUpdTask *tp = new DbUpdTask(DbUpdTask::Delete, udi, uniterm, nullptr, (size_t)-1, rztxt);
        if (!m_ndb->m_wqueue.put(tp)) {
            LOGERR("Db::purgeFile:Cant queue task\n");
            return false;
        }
        return true;
    }
#endif
    /* We get there is IDX_THREADS is not defined or there is no queue */
    return m_ndb->purgeFileWrite(false, udi, uniterm);
}

// Delete subdocs with an out of date sig. We do this to purge obsolete subdocs during a partial
// update where no general purge will be done.
bool Db::purgeOrphans(const string &udi)
{
    LOGDEB("Db:purgeOrphans: [" << udi << "]\n");
    if (nullptr == m_ndb || !m_ndb->m_iswritable)
        return false;

    string uniterm = Native::make_uniterm(udi);

#ifdef IDX_THREADS
    if (m_ndb->m_havewriteq) {
        string rztxt;
        DbUpdTask *tp = new DbUpdTask(DbUpdTask::PurgeOrphans, udi, uniterm,
                                      nullptr, (size_t)-1, rztxt);
        if (!m_ndb->m_wqueue.put(tp)) {
            LOGERR("Db::purgeFile:Cant queue task\n");
            return false;
        }
        return true;
    }
#endif
    /* We get there is IDX_THREADS is not defined or there is no queue */
    return m_ndb->purgeFileWrite(true, udi, uniterm);
}

bool Db::dbStats(DbStats& res, bool listfailed)
{
    if (!m_ndb || !m_ndb->m_isopen)
        return false;
    Xapian::Database xdb = m_ndb->xrdb;

    XAPTRY(res.dbdoccount = xdb.get_doccount();
           res.dbavgdoclen = xdb.get_avlength();
           res.mindoclen = xdb.get_doclength_lower_bound();
           res.maxdoclen = xdb.get_doclength_upper_bound();
           , xdb, m_reason);
    if (!m_reason.empty())
        return false;
    if (!listfailed) {
        return true;
    }

    // listfailed is set : look for failed docs
    string ermsg;
    try {
        for (unsigned int docid = 1; docid < xdb.get_lastdocid(); docid++) {
            try {
                Xapian::Document doc = xdb.get_document(docid);
                string sig = doc.get_value(VALUE_SIG);
                if (sig.empty() || sig.back() != '+') {
                    continue;
                }
                string data = doc.get_data();
                ConfSimple parms(data);
                if (!parms.ok()) {
                } else {
                    string url, ipath;
                    parms.get(Doc::keyipt, ipath);
                    parms.get(Doc::keyurl, url);
                    // Turn to local url or not? It seems to make more
                    // sense to keep the original urls as seen by the
                    // indexer.
                    // m_config->urlrewrite(dbdir, url);
                    if (!ipath.empty()) {
                        url += " | " + ipath;
                    }
                    res.failedurls.push_back(url);
                }
            } catch (Xapian::DocNotFoundError) {
                continue;
            }
        }
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
        LOGERR("Db::dbStats: " << ermsg << "\n");
        return false;
    }
    return true;
}

// Retrieve document defined by Unique doc identifier. This is used
// by the GUI history feature and by open parent/getenclosing
// ! The return value is always true except for fatal errors. Document
//  existence should be tested by looking at doc.pc
bool Db::getDoc(const string &udi, const Doc& idxdoc, Doc &doc)
{
    LOGDEB1("Db:getDoc: [" << udi << "]\n");
    int idxi = idxdoc.idxi;
    return getDoc(udi, idxi, doc);
}

bool Db::getDoc(const string &udi, const std::string& dbdir, Doc &doc, bool fetchtext)
{
    LOGDEB1("Db::getDoc(udi, dbdir): (" << udi << ", " << dbdir << ")\n");
    int idxi = -1;
    if (dbdir.empty() || dbdir == m_basedir) {
        idxi = 0;
    } else {
        for (unsigned int i = 0; i < m_extraDbs.size(); i++) {
            if (dbdir == m_extraDbs[i]) {
                idxi = int(i + 1);
                break;
            }
        }
    }
    LOGDEB1("Db::getDoc(udi, dbdir): idxi: " << idxi << "\n");
    if (idxi < 0) {
        LOGERR("Db::getDoc(udi, dbdir): dbdir not in current extra dbs\n");
        return false;
    }
    return getDoc(udi, idxi, doc, fetchtext);
}

bool Db::getDoc(const string& udi, int idxi, Doc& doc, bool fetchtext)
{
    // Initialize what we can in any case. If this is history, caller
    // will make partial display in case of error
    if (nullptr == m_ndb)
        return false;
    doc.meta[Rcl::Doc::keyrr] = "100%";
    doc.pc = 100;
    Xapian::Document xdoc;
    Xapian::docid docid = m_ndb->getDoc(udi, idxi, xdoc);
    if (docid) {
        string data = xdoc.get_data();
        doc.meta[Doc::keyudi] = udi;
        return m_ndb->dbDataToRclDoc(docid, xdoc, data, doc, fetchtext);
    } else {
        // Document found in history no longer in the
        // database.  We return true (because their might be
        // other ok docs further) but indicate the error with
        // pc = -1
        doc.pc = -1;
        LOGINFO("Db:getDoc: no such doc in current index: [" << udi << "]\n");
        return true;
    }
}

// Retrieve UDI for query result document. We do this lazily because it's actually rarely needed and
// takes about 20% of the document fetch time because of the need to access the term list.
std::string Db::fetchUdi(Doc& doc)
{
    std::string& udi = doc.meta[Doc::keyudi];
    if (!udi.empty()) {
        return udi;
    }
    if (!doc.xdocid) {
        return udi;
    }
    if (!m_ndb || !m_ndb->m_isopen) {
        m_reason = "Db::fetchUdi: called on non-opened db\n";
        return udi;
    }
    m_ndb->docidToUdi(doc.xdocid, udi);
    return udi;
}

bool Db::getDocRawText(Doc& doc)
{
    if (!m_ndb || !m_ndb->m_isopen) {
        LOGERR("Db::getDocRawText: called on non-opened db\n");
        return false;
    }
    return m_ndb->getRawText(fetchUdi(doc), doc.xdocid, doc.text);
}

bool Db::hasSubDocs(Doc &idoc)
{
    if (nullptr == m_ndb)
        return false;
    string inudi = fetchUdi(idoc);
    if (inudi.empty()) {
        LOGERR("Db::hasSubDocs: no input udi or empty\n");
        return false;
    }
    LOGDEB1("Db::hasSubDocs: idxi " << idoc.idxi << " inudi [" << inudi << "]\n");

    // Not sure why we perform both the subDocs() call and the test on
    // has_children. The former will return docs if the input is a
    // file-level document, but the latter should be true both in this
    // case and if the input is already a subdoc, so the first test
    // should be redundant. Does not hurt much in any case, to be
    // checked one day.
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(inudi, idoc.idxi, docids)) {
        LOGDEB("Db::hasSubDocs: lower level subdocs failed\n");
        return false;
    }
    if (!docids.empty())
        return true;

    // Check if doc has an "has_children" term
    if (m_ndb->hasTerm(inudi, idoc.idxi, has_children_term))
        return true;
    return false;
}

// Retrieve all subdocuments of a given one, which may not be a file-level
// one (in which case, we have to retrieve this first, then filter the ipaths)
bool Db::getSubDocs(Doc &idoc, vector<Doc>& subdocs)
{
    if (nullptr == m_ndb)
        return false;

    string inudi = fetchUdi(idoc);
    if (inudi.empty()) {
        LOGERR("Db::getSubDocs: no input udi or empty\n");
        return false;
    }

    string rootudi;
    string ipath = idoc.ipath;
    LOGDEB0("Db::getSubDocs: idxi " << idoc.idxi << " inudi [" << inudi <<
            "] ipath [" << ipath << "]\n");
    if (ipath.empty()) {
        // File-level doc. Use it as root
        rootudi = inudi;
    } else {
        // See if we have a parent term
        Xapian::Document xdoc;
        if (!m_ndb->getDoc(inudi, idoc.idxi, xdoc)) {
            LOGERR("Db::getSubDocs: can't get Xapian document\n");
            return false;
        }
        Xapian::TermIterator xit;
        XAPTRY(xit = xdoc.termlist_begin();
               xit.skip_to(wrap_prefix(parent_prefix)),
               m_ndb->xrdb, m_reason);
        if (!m_reason.empty()) {
            LOGERR("Db::getSubDocs: xapian error: " << m_reason << "\n");
            return false;
        }
        if (xit == xdoc.termlist_end() || get_prefix(*xit) != parent_prefix) {
            LOGERR("Db::getSubDocs: parent term not found\n");
            return false;
        }
        rootudi = strip_prefix(*xit);
    }

    LOGDEB("Db::getSubDocs: root: [" << rootudi << "]\n");

    // Retrieve all subdoc xapian ids for the root
    vector<Xapian::docid> docids;
    if (!m_ndb->subDocs(rootudi, idoc.idxi, docids)) {
        LOGDEB("Db::getSubDocs: lower level subdocs failed\n");
        return false;
    }

    // Retrieve doc, filter, and build output list
    for (int tries = 0; tries < 2; tries++) {
        try {
            for (const auto docid : docids) {
                Xapian::Document xdoc = m_ndb->xrdb.get_document(docid);
                string data = xdoc.get_data();
                Doc doc;
                doc.meta[Doc::keyrr] = "100%";
                doc.pc = 100;
                if (!m_ndb->dbDataToRclDoc(docid, xdoc, data, doc)) {
                    LOGERR("Db::getSubDocs: doc conversion error\n");
                    return false;
                }
                if (ipath.empty() ||
                    FileInterner::ipathContains(ipath, doc.ipath)) {
                    subdocs.push_back(doc);
                }
            }
            return true;
        } catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();
            m_ndb->xrdb.reopen();
            continue;
        } XCATCHERROR(m_reason);
        break;
    }

    LOGERR("Db::getSubDocs: Xapian error: " << m_reason << "\n");
    return false;
}

bool Db::getContainerDoc(Doc &idoc, Doc& ctdoc)
{
    if (nullptr == m_ndb)
        return false;

    string inudi = fetchUdi(idoc);
    if (!inudi.empty()) {
        LOGERR("Db::getContainerDoc: no input udi or empty\n");
        return false;
    }

    string rootudi;
    string ipath = idoc.ipath;
    LOGDEB0("Db::getContainerDoc: idxi " << idoc.idxi << " inudi [" << inudi <<
            "] ipath [" << ipath << "]\n");
    if (ipath.empty()) {
        // File-level doc ??
        ctdoc = idoc;
        return true;
    } 
    // See if we have a parent term
    Xapian::Document xdoc;
    if (!m_ndb->getDoc(inudi, idoc.idxi, xdoc)) {
        LOGERR("Db::getContainerDoc: can't get Xapian document\n");
        return false;
    }
    Xapian::TermIterator xit;
    XAPTRY(xit = xdoc.termlist_begin();
           xit.skip_to(wrap_prefix(parent_prefix)),
           m_ndb->xrdb, m_reason);
    if (!m_reason.empty()) {
        LOGERR("Db::getContainerDoc: xapian error: " << m_reason << "\n");
        return false;
    }
    if (xit == xdoc.termlist_end() || get_prefix(*xit) != parent_prefix) {
        LOGERR("Db::getContainerDoc: parent term not found\n");
        return false;
    }
    rootudi = strip_prefix(*xit);

    if (!getDoc(rootudi, idoc.idxi, ctdoc)) {
        LOGERR("Db::getContainerDoc: can't get container document\n");
        return false;
    }
    return true;
}

// Walk an UDI section (all UDIs beginning with input prefix), and mark all docs and subdocs as
// existing. Caller beware: Makes sense or not depending on the UDI structure for the data store. In
// practise, used for absent FS mountable volumes.
bool Db::udiTreeMarkExisting(const string& udi)
{
    LOGDEB("Db::udiTreeMarkExisting: " << udi << "\n");
    string prefix = wrap_prefix(udi_prefix);
    string expr = udi + "*";

#ifdef IDX_THREADS
    std::unique_lock<std::mutex> lock(m_ndb->m_mutex);
#endif

    bool ret = m_ndb->idxTermMatch_p(
        int(ET_WILD), expr, prefix,
        [this, &udi](const string& term, Xapian::termcount, Xapian::doccount) {
            Xapian::PostingIterator docid;
            XAPTRY(docid = m_ndb->xrdb.postlist_begin(term), m_ndb->xrdb, m_reason);
            if (!m_reason.empty()) {
                LOGERR("Db::udiTreeWalk: xapian::postlist_begin failed: " << m_reason << "\n");
                return false;
            }
            if (docid == m_ndb->xrdb.postlist_end(term)) {
                LOGDEB("Db::udiTreeWalk:no doc for " << term << " ??\n");
                return false;
            }
            i_setExistingFlags(udi, *docid);
            LOGDEB0("Db::udiTreeWalk: uniterm: " << term << "\n");
            return true;
        });
    return ret;
}

} // End namespace Rcl
