/* Copyright (C) 2007 J.F.Dockes
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

#ifndef _rcldb_p_h_included_
#define _rcldb_p_h_included_

#include "autoconfig.h"

#include <mutex>
#include <functional>
#include <string>
#include <vector>
#include <memory>

#include <xapian.h>
#ifndef XAPIAN_AT_LEAST
// Added in Xapian 1.4.2. Define it here for older versions
#define XAPIAN_AT_LEAST(A,B,C)                                      \
    (XAPIAN_MAJOR_VERSION > (A) ||                                  \
     (XAPIAN_MAJOR_VERSION == (A) &&                                \
      (XAPIAN_MINOR_VERSION > (B) ||                                \
       (XAPIAN_MINOR_VERSION == (B) && XAPIAN_REVISION >= (C)))))
#endif

#ifdef IDX_THREADS
#include "workqueue.h"
#endif // IDX_THREADS

#include "xmacros.h"
#include "log.h"
#include "rclconfig.h"
#include "cstr.h"
#include "rclutil.h"

namespace Rcl {

class Query;

inline bool has_prefix(const std::string& trm)
{
    if (o_index_stripchars) {
        return !trm.empty() && 'A' <= trm[0] && trm[0] <= 'Z';
    } else {
        return !trm.empty() && trm[0] == ':';
    }
}

inline std::string strip_prefix(const std::string& trm)
{
    if (!has_prefix(trm))
        return trm;
    std::string::size_type st = 0;
    if (o_index_stripchars) {
        st = trm.find_first_not_of("ABCDEFIJKLMNOPQRSTUVWXYZ");
#ifdef _WIN32
        // We have a problem there because we forgot to lowercase the drive
        // name. So if the found character is a colon consider the drive name as
        // the first non capital even if it is uppercase
        if (st != std::string::npos && st >= 2 && trm[st] == ':') {
            st -= 1;
        }
#endif
    } else {
        st = trm.find_first_of(":", 1) + 1;
    }
    if (st == std::string::npos) {
        return std::string(); // ??
    }
    return trm.substr(st);
}

inline std::string get_prefix(const std::string& trm)
{
    if (!has_prefix(trm))
        return std::string();
    std::string::size_type st = 0;
    if (o_index_stripchars) {
        st = trm.find_first_not_of("ABCDEFIJKLMNOPQRSTUVWXYZ");
        if (st == std::string::npos) {
            return std::string(); // ??
        }
#ifdef _WIN32
        // We have a problem there because we forgot to lowercase the drive
        // name. So if the found character is a colon consider the drive name as
        // the first non capital even if it is uppercase
        if (st >= 2 && trm[st] == ':') {
            st -= 1;
        }
#endif
        return trm.substr(0, st);
    } else {
        st = trm.find_first_of(":", 1) + 1;
        if (st == std::string::npos) {
            return std::string(); // ??
        }
        return trm.substr(1, st-2);
    }
}

inline std::string wrap_prefix(const std::string& pfx) 
{
    if (o_index_stripchars) {
        return pfx;
    } else {
        return cstr_colon + pfx + cstr_colon;
    }
}

#ifdef IDX_THREADS
// Task for the index update thread. This can be:
//  - add/update for a new / update document
//  - delete for a deleted document
//  - purgeOrphans when a multidoc file is updated during a partial pass (no  general purge).
//    We want to remove the subDocs that possibly don't exist anymore. We find them by their
//    different sig.
// txtlen and doc are only valid for add/update else, len is (size_t)-1 and doc is empty
class DbUpdTask {
public:
    enum Op {AddOrUpdate, Delete, PurgeOrphans, Flush};
    // Note that udi and uniterm are strictly equivalent and are
    // passed both just to avoid recomputing uniterm which is
    // available on the caller site.
    // Take some care to avoid sharing string data (if string impl is cow)
    DbUpdTask(Op _op, const std::string& ud, const std::string& un, 
              std::unique_ptr<Xapian::Document> d, size_t tl, std::string& rztxt)
        : op(_op), udi(ud.begin(), ud.end()), uniterm(un.begin(), un.end()), 
          doc(std::move(d)), txtlen(tl) {
        rawztext.swap(rztxt);
    }
    // Udi and uniterm equivalently designate the doc
    Op op;
    std::string udi;
    std::string uniterm;
    std::unique_ptr<Xapian::Document> doc;
    // txtlen is used to update the flush interval. It's -1 for a
    // purge because we actually don't know it, and the code fakes a
    // text length based on the term count.
    size_t txtlen;
    std::string rawztext; // Compressed doc text
};
#endif // IDX_THREADS

class TextSplitDb;

// A class for data and methods that would have to expose Xapian-specific stuff if they were in
// Rcl::Db. There could actually be 2 different ones for indexing or query as there is not much in
// common.
class Db::Native {
 public:
    Db  *m_rcldb; // Parent
    bool m_isopen{false};
    bool m_iswritable{false};
    bool m_noversionwrite{false}; //Set if open failed because of version mismatch!
    bool m_storetext{false};
#ifdef IDX_THREADS
    WorkQueue<DbUpdTask*> m_wqueue;
    std::mutex m_mutex;
    long long  m_totalworkns{0};
    bool m_havewriteq{false};
    void maybeStartThreads();
    friend void *DbUpdWorker(void*);

    int m_tmpdbcnt{0};
    int m_tmpdbinitidx{0};
    std::mutex m_initidxmutex;
    WorkQueue<DbUpdTask*> m_mwqueue;
    std::vector<Xapian::WritableDatabase> m_tmpdbs;
    std::vector<std::unique_ptr<TempDir>> m_tmpdbdirs;
    std::vector<char> m_tmpdbflushflags;
    friend void *DbMUpdWorker(void*);
#endif // IDX_THREADS

    // Indexing 
    Xapian::WritableDatabase xwdb;
    // Querying (active even if the wdb is too)
    Xapian::Database xrdb;

    Native(Db *db);
    ~Native();
    Native(const Native &) = delete;
    Native& operator=(const Native &) = delete;


    void openWrite(const std::string& dir, Db::OpenMode mode, int flags);
    void openRead(const std::string& dir);

    // Determine if an existing index is of the full-text-storing kind
    // by looking at the index metadata. Stores the result in m_storetext
    void storesDocText(Xapian::Database&);
    
    // Final steps of doc update, part which need to be single-threaded
    bool addOrUpdateWrite(const std::string& udi, const std::string& uniterm, 
                          std::unique_ptr<Xapian::Document> doc, size_t txtlen,
                          std::string& rawztext);

    /** Delete all documents which are contained in the input document, which must be a file-level
     * one.
     * 
     * @param onlyOrphans if true, only delete documents which have not the same signature as the
     * input. This is used to delete docs which do not exist any more in the file after an update,
     * for example the tail messages after a folder truncation). If false, delete all.
     * @param udi the parent document identifier.
     * @param uniterm equivalent to udi, passed just to avoid recomputing.
     */
    bool purgeFileWrite(bool onlyOrphans, const std::string& udi, const std::string& uniterm);

    bool getPagePositions(Xapian::docid docid, std::vector<int>& vpos);
    int getPageNumberForPosition(const std::vector<int>& pbreaks, int pos);

    bool dbDataToRclDoc(Xapian::docid docid, std::string &data, Doc &doc, bool fetchtext = false);

    size_t whatDbIdx(Xapian::docid id);
    Xapian::docid whatDbDocid(Xapian::docid);

    /** Retrieve a Xapian::Document and Xapian::docid, given the external Unique Document
     * Identifier, using the posting list for the UDI-derived term.
     * 
     * @param udi the unique document identifier (e.g. for fsindexer, opaque hashed path+ipath).
     * @param idxi the database index, at query time, when using external databases.
     * @param[out] xdoc the xapian document.
     * @return 0 if not found
     */
    Xapian::docid getDoc(const std::string& udi, int idxi, Xapian::Document& xdoc);

    /** Retrieve the unique document identifier for a given Xapian document, using the document
     * termlist */
    bool xdocToUdi(Xapian::Document& xdoc, std::string &udi);

    /** Check if doc is indexed by term */
    bool hasTerm(const std::string& udi, int idxi, const std::string& term);

    /** Update existing Xapian document for pure extended attrs change */
    bool docToXdocMetaOnly(TextSplitDb *splitter, const std::string &udi, 
                           Doc &doc, Xapian::Document& xdoc);
    /** Remove all terms currently indexed for field defined by idx prefix */
    bool clearField(Xapian::Document& xdoc, const std::string& pfx, Xapian::termcount wdfdec);

    /** Check if term wdf is 0 and remove term if so */
    bool clearDocTermIfWdf0(Xapian::Document& xdoc, const std::string& term);

    /** Compute the list of subdocuments for a given udi. We look for documents indexed by a parent
     * term matching the udi, the posting list for the parentterm(udi) (As suggested by James
     * Aylett)
     *
     * Note that this is not currently recursive: all subdocs are supposed to be children of the
     * standalone file doc.
     * I.e.: in a mail folder, all messages, attachments, attachments of attached messages etc. must
     * have the folder file document as parent.
     *
     * Finer grain parent-child relationships are defined by the indexer (rcldb user), using the
     * ipath.
     */
    bool subDocs(const std::string &udi, int idxi, std::vector<Xapian::docid>& docids);

    /** Final matcher. All term transformations are done, we are just matching the input
     * expression against index stored terms. 
     * @param matchtyp match type: can be ET_NONE, ET_WILDCARD or ET_REGEXP.
     * @param expr prefix-less expression to be matched against.
     * @param client function to be called when a matching term is found. The term parameter 
     *        will have no prefix.
     * @return false for error (Xapian issue mostly).
     */
    bool idxTermMatch_p(int matchtyp, const std::string &expr, const std::string& prefix,
                        std::function<bool(const std::string& term, Xapian::termcount colfreq,
                                           Xapian::doccount termfreq)> client);

    /** Check if a page position list is defined */
    bool hasPages(Xapian::docid id);

    std::string rawtextMetaKey(Xapian::docid did) {
        // Xapian's Olly Betts advises to use a key which will sort the same as the docid (which we
        // do), and to use Xapian's pack.h:pack_uint_preserving_sort() which is efficient but hard
        // to read. I'd wager that this does not make much of a difference. 10 ascii bytes gives us
        // 10 billion docs, which is enough (says I).
        char buf[30];
        snprintf(buf, sizeof(buf), "%010d", did);
        return buf;
    }

    bool getRawText(Xapian::docid docid, std::string& rawtext);

    void deleteDocument(Xapian::docid docid) {
        std::string metareason;
        XAPTRY(xwdb.set_metadata(rawtextMetaKey(docid), std::string()), xwdb, metareason);
        if (!metareason.empty()) {
            LOGERR("deleteDocument: set_metadata error: " << metareason << "\n");
            // not fatal
        }
        xwdb.delete_document(docid);
    }
};

// This is the term position offset at which we index the body text. Abstract, keywords, etc.. are
// stored before this.
static const unsigned int baseTextPosition = 100000;

}
#endif /* _rcldb_p_h_included_ */
