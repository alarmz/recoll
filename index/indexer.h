/* Copyright (C) 2004-2021 J.F.Dockes
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
#ifndef _INDEXER_H_INCLUDED_
#define _INDEXER_H_INCLUDED_

#include <string>
#include <list>
#include <vector>

#include "rclconfig.h"
#include "rcldb.h"
#include "rcldoc.h"
#include "idxstatus.h"

class FsIndexer;
class WebQueueIndexer;

/**
 * The top level batch indexing object. Processes the configuration,
 * then invokes file system walking or other to populate/update the 
 * database(s). 
 */
class ConfIndexer {
public:
    enum runStatus {IndexerOk, IndexerError};
    ConfIndexer(RclConfig *cnf);
    virtual ~ConfIndexer();
    ConfIndexer(const ConfIndexer&) = delete;
    ConfIndexer& operator=(const ConfIndexer&) = delete;

    // Indexer types. Maybe we'll have something more dynamic one day
    enum ixType {IxTNone, IxTFs=1, IxTWebQueue=2, 
                 IxTAll = IxTFs | IxTWebQueue};
    // Misc indexing flags
    enum IxFlag {IxFNone = 0, 
                 IxFIgnoreSkip = 1, // Ignore skipped lists
                 IxFNoWeb = 2, // Do not process the web queue.
                 // First pass: just do the top files so that the user can 
                 // try searching asap.
                 IxFQuickShallow = 4, 
                 // Do not retry files which previously failed ('+' sigs)
                 IxFNoRetryFailed = 8,
                 // Perform the purge pass (normally on).
                 IxFDoPurge = 16,
                 // Evict each indexed file from the page cache.
                 IxFCleanCache = 32,
                 // In place reset: pretend all documents need updating: allows updating all while
                 // keeping a usable index.
                 IxFInPlaceReset = 64,
                 // Do not use multiple temporary indexes (which are only useful when many new files
                 // are expected, e.g. not during monitoring)
                 IxFNoTmpDb = 128,
    };

    /** Run indexers */
    bool index(bool resetbefore, ixType typestorun, int f = IxFNone);

    const std::string &getReason() {return m_reason;}

    /** Stemming reset to config: create needed, delete unconfigured */
    bool createStemmingDatabases();

    /** Create stem database for given language */
    bool createStemDb(const std::string &lang);

    /** Create misspelling expansion dictionary if aspell i/f is available */
    bool createAspellDict();

    /** List possible stemmer names */
    static std::vector<std::string> getStemmerNames();

    /** Index a list of files. No db cleaning or stemdb updating */
    bool indexFiles(std::list<std::string> &files, int f = IxFNone);

    /** Purge a list of files. */
    bool purgeFiles(std::list<std::string> &files, int f = IxFNone);

private:
    RclConfig *m_config;
    Rcl::Db    m_db;
    FsIndexer *m_fsindexer{nullptr}; 
    bool       m_doweb{false};
    WebQueueIndexer *m_webindexer{nullptr}; 
    std::string     m_reason;

    // The first time we index, we do things a bit differently to
    // avoid user frustration (make at least some results available
    // fast by using several passes, the first ones to index common
    // interesting locations).
    bool runFirstIndexing();
    bool firstFsIndexingSequence();
};

#endif /* _INDEXER_H_INCLUDED_ */
