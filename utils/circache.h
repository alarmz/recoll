/* Copyright (C) 2009 J.F.Dockes
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
#ifndef _circache_h_included_
#define _circache_h_included_

/**
 * A data cache implemented as a circularly managed file.
 *
 * A single file is used to serially store objects. The file grows to a specified maximum size, then
 * is rewritten from the start, overwriting older entries.
 *
 * The file begins with a descriptive block, containing both static attributes (e.g. maximum storage
 * size, store duplicate entries or not), and state information (e.g. position of the write pointer
 * for the next entry).
 *
 * The objects inside the cache each have two consecutive parts: an attribute (metadata) dictionary
 * and a data segment. They are named using an externally chosen unique identifier set when storing
 * them. Recoll uses the document UDI for this purpose.
 *
 * The unique identifiers are stored inside the entry's dictionary under the key "udi".
 *
 * It is assumed that the dictionaries are small (they are routinely read/parsed)
 */

#include <cstdint>
#include <string>

class ConfSimple;
class CirCacheInternal;

class CirCache {
public:
    /// Constructor: only performs initialization, it does not create or access the storage.
    CirCache(const std::string& dir);
    /// Destructor: memory cleanup, no file access except for closing the fd.
    virtual ~CirCache();

    CirCache(const CirCache&) = delete;
    CirCache& operator=(const CirCache&) = delete;

    /// Get an explanatory message for the last error.
    virtual std::string getReason();

    enum CreateFlags {CC_CRNONE = 0,
                      // Unique entries: erase older instances when same udi is stored.
                      CC_CRUNIQUE = 1,
                      // Truncate file (restart from scratch).
                      CC_CRTRUNCATE = 2
                     };
    virtual bool create(int64_t maxsize, int flags);

    /// Open the storage file in the prescribed mode, and read the first (attributes) block.
    enum OpMode {CC_OPREAD, CC_OPWRITE};
    virtual bool open(OpMode mode);

    /// Return the current file size (as from the storage file st_size)
    virtual int64_t size() const;
    /// Return the set maximum storage size in bytes.
    virtual int64_t maxsize() const;
    /// Return the position of the start of the newest entry (nheadoffs).
    virtual int64_t nheadpos() const;
    /// Return the current write position. This is the EOF if the file is not full, or the start of
    /// the oldest entry (or possibly EOF) else.
    virtual int64_t writepos() const;
    /// Return the 'unique' attribute, determining if we store duplicate UDIs.
    virtual bool uniquentries() const;
    /// Return the storage file path.
    virtual std::string getpath() const;

    /// Get data and attributes for a given udi.
    /// @param udi the object identifier.
    /// @param dic the std::string where to store the metadata dictionary.
    /// @param data std::string pointer where to store the data. Set to nullptr if you just want
    ///   the header.
    /// @param instance Specific instance to retrieve (if storing duplicates). Set to -1 to get
    ///   latest. Otherwise specify from 1+
    virtual bool get(const std::string& udi, std::string& dic,
                     std::string *data = nullptr, int instance = -1);

    /// Store a data object.
    /// Note: the dicp MUST have an udi entry.
    enum PutFlags {
        NoCompHint = 1, // Do not attempt compression.
    };
    virtual bool put(const std::string& udi, const ConfSimple *dicp,
                     const std::string& data, unsigned int flags = 0);

    /// Erase all instances for given udi. If reallyclear is set, actually overwrite the contents,
    /// else just logically erase.
    virtual bool erase(const std::string& udi, bool reallyclear = false);

    /** Walk the archive.
     *
     * Maybe we'll have separate iterators one day, but this is good
     * enough for now. No put() operations should be performed while
     * using these.
     */
    /** Back to oldest */
    virtual bool rewind(bool& eof);
    /** Get entry under cursor */
    virtual bool getCurrent(std::string& udi, std::string& dic, std::string *data = nullptr);
    /** Get current entry udi only. Udi can be empty (erased empty), caller
     * should call again */
    virtual bool getCurrentUdi(std::string& udi);
    /** Skip to next. (false && !eof) -> error, (false&&eof)->EOF. */
    virtual bool next(bool& eof);

    /** Write global header and all entry headers to stdout. Debug. */
    virtual bool dump();

    /** Utility: append all entries from sdir to ddir. 
     * 
     * ddir must already exist. It will be appropriately resized if needed to avoid recycling while
     * writing the new entries.
     *
     * ** Note: if dest is not currently growing, this action will recycle old dest entries
     *    between the current write point and EOF (or up to wherever we need to write to store 
     *    the source data) **
     *
     * Also note that if the objective is just to compact (reuse the erased entries space) you
     * should first create the new circache with the same maxsize as the old one, else the new
     * maxsize will be the current file size (current erased+active entries, with available space
     * corresponding to the old erased entries).
     *
     * @param ddir destination circache (must exist)
     * @param sdir source circache
     * @return number of entries copied or -1
     */
    static int appendCC(const std::string& ddir, const std::string& sdir,
                        std::string *reason = nullptr);

    /** Utility: rewrite the cache so that the space wasted to erased entries is recovered. 
     * This may need to temporarily use twice the current cache disk space. */
    static bool compact(const std::string& dir, std::string *reason = nullptr);

    /** Utility: extract all entries as metadata/data file pairs */
    static bool burst(
        const std::string& ccdir, const std::string destdir, std::string *reason = nullptr);
    
protected:
    CirCacheInternal *m_d{nullptr};
    std::string m_dir;
};

#endif /* _circache_h_included_ */
