/* Copyright (C) 2006-2022 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _CONFTREE_H_
#define  _CONFTREE_H_

/**
 * Classes for managing ini-type data, with 'name = value' lines, and supporting subsections
 * with lines like '[subkey]'
 *
 * Lines like '[subkey]' in the file define subsections, with independant
 * configuration namespaces. Only subsections holding at least one variable are
 * significant (empty subsections may be deleted during an update, or not).
 *
 * Whitespace around name and value is ignored (trailing white space may be optionally preserved).
 *
 * The names are case-sensitive by default, but there are separate options for ignoring case in
 * names and subkeys.
 *
 * Values can be queried for, or set.
 *
 * Any line without a '=', or beginning with '[ \t]*#' is a comment.
 *
 * A configuration object can be created empty or by reading from a file or a string.
 *
 * All 'set' calls normally cause an immediate rewrite of the backing
 * object if any (file or string). This can be prevented with holdWrites().
 *
 * The ConfTree derived class interprets the subkeys as file paths and
 * lets subdir keys hierarchically inherit the properties from parents.
 *
 * The ConfStack class stacks several Con(Simple/Tree) objects so that
 * parameters from the top of the stack override the values from lower
 * (useful to have central/personal config files).
 */

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>

#include "pathut.h"

/** Internal class used for storing presentation information */
class ConfLine {
public:
    enum Kind {CFL_COMMENT, CFL_SK, CFL_VAR, CFL_VARCOMMENT};
    Kind m_kind;
    // For a comment or varcomment line, m_data is the full line
    // For an SK or a VAR, m_data is the name (sk or var name)
    std::string m_data;
    // For a VAR m_value is the original value. Unset for other types. Unchanged
    // if the variable is updated or erased. Only (barely) used with the XML comments stuff.
    // For normal rewriting, the value is extracted with a regular get()
    std::string m_value;
    // Only used for VARCOMMENT lines (commented out variable assignation). Holds the variable name
    // in this case. Used if the variable is set, to group the comment and actual assignment.
    std::string m_aux;
    ConfLine(Kind k, const std::string& d, std::string a = std::string())
        : m_kind(k), m_data(d), m_aux(a) {
    }
    bool operator==(const ConfLine& o) {
        return o.m_kind == m_kind && o.m_data == m_data;
    }
};

/**
 * Virtual base class used to define the interface, and a few helper methods.
 */
class ConfNull {
public:
    enum StatusCode {STATUS_ERROR = 0, STATUS_RO = 1, STATUS_RW = 2};
    ConfNull() = default;
    virtual ~ConfNull() = default;
    ConfNull(const ConfNull&) = delete;
    ConfNull &operator=(const ConfNull &) = delete;
    virtual int get(const std::string& name, std::string& value,
                    const std::string& sk = std::string()) const = 0;
    virtual int set(const std::string& nm, const std::string& val,
                    const std::string& sk = std::string()) = 0;
    virtual long long getInt(const std::string& name, long long dflt,
                             const std::string& sk = std::string());
    virtual double getFloat(const std::string& name, double dflt,
                            const std::string& sk = std::string());
    virtual bool getBool(const std::string& name, bool dflt, const std::string& sk = std::string());
    virtual bool ok() const = 0;
    virtual std::vector<std::string> getNames(const std::string& sk,
                                              const char* = nullptr) const = 0;
    virtual bool hasNameAnywhere(const std::string& nm) const = 0;
    virtual int erase(const std::string&, const std::string&) = 0;
    virtual int eraseKey(const std::string&) = 0;
    virtual std::vector<std::string> getSubKeys() const = 0;
    virtual std::vector<std::string> getSubKeys(bool) const = 0;
    virtual bool holdWrites(bool) = 0;
    virtual bool sourceChanged() const = 0;
    virtual bool write(std::ostream&) const {return true;};
};

struct CaseComparator {
    CaseComparator(bool nocase = false)
        : m_nocase(nocase) {}
    bool operator()(const std::string& a, const std::string& b) const {
        if (!m_nocase)
            return a < b;
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char ch1, char ch2) {
                return std::tolower(ch1) < std::tolower(ch2);
            });
    }
    bool m_nocase;
};

/**
 * Manage simple configuration data with subsections.
 */
class ConfSimple : public ConfNull {
public:

    /**
     * Build the object by reading content from file.
     * @param filename file to open
     * @param readonly if true open readonly, else rw
     * @param tildexp  try tilde (home dir) expansion for subkey values
     * @param trimvalues remove trailing white space from values
     */
    ConfSimple(const char *fname, int readonly = 0, bool tildexp = false, bool trimvalues = true);

    /**
     * Build the object by reading content from a string
     * @param data points to the data to parse. This is used as input only. Use write() to 
     *  get the data back after updating the object.
     * @param readonly if true open readonly, else rw
     * @param tildexp  try tilde (home dir) expansion for subsection names
     * @param trimvalues remove trailing white space from values
     */
    ConfSimple(const std::string& data, int readonly = 0, bool tildexp = false,
               bool trimvalues = true);

    /**
     * Build an empty object. This will be memory only, with no backing store.
     * @param readonly if true open read only, else rw
     * @param tildexp  try tilde (home dir) expansion for subsection names
     * @param trimvalues remove trailing white space from values
     */
    ConfSimple(int readonly = 0, bool tildexp = false, bool trimvalues = true);

    /**
     * Build any kind of ConfSimple, depending on flags.
     * @param flags a bitfield where the following flags can be set:
     *     CFSF_RO: if set, the object can't be modified after initialisation
     *     CFSF_FROMSTRING: if set, @param dataorfn is the data to parse, else it is a file name.
     *     CFSF_TILDEXP: perform tilde expansion on subsection names
     *     CFSF_NOTRIMVALUES: do not trim white space at the end of values.
     *     CFSF_SUBMAPNOCASE: use case-insensitive comparisons for submap names.
     *     CFSF_KEYNOCASE: use case-insensitive comparisons for keys.
     *     CFSF_NOCASE: both keys and sections are case-insensitive.
     * @param dataorfn  input data or file name depending on flags.
     */
    enum Flag {CFSF_NONE = 0, CFSF_RO = 1, CFSF_TILDEXP = 2, CFSF_NOTRIMVALUES = 4,
        CFSF_SUBMAPNOCASE = 8, CFSF_KEYNOCASE = 0x10, CFSF_FROMSTRING = 0x20,
        CFSF_NOCASE = CFSF_SUBMAPNOCASE | CFSF_KEYNOCASE,
    };
    ConfSimple(int flags, const std::string& dataorfn);

    virtual ~ConfSimple() = default;

    /** Origin file changed. Only makes sense if we read the data from a file */
    virtual bool sourceChanged() const override;

    /**
     * Decide if we actually rewrite the backing-store after modifying the
     * tree.
     */
    virtual bool holdWrites(bool on) override {
        m_holdWrites = on;
        if (on == false) {
            return write();
        } else {
            return true;
        }
    }

    /** Clear, then reparse from string */
    void reparse(const std::string& in);

    /**
     * Get string value for named parameter, from specified subsection (looks 
     * in global space if sk is empty).
     * @return 0 if name not found, 1 else
     */
    virtual int get(const std::string& name, std::string& value,
                    const std::string& sk = std::string()) const override;

    /**
     * Set value for named string parameter in specified subsection (or global)
     * @return 0 for error, 1 else
     */
    virtual int set(const std::string& nm, const std::string& val,
                    const std::string& sk = std::string()) override;
    /**
     * Set value for named integer parameter in specified subsection (or global)
     * @return 0 for error, 1 else
     */
    virtual int set(const std::string& nm, long long val, const std::string& sk = std::string());

    /**
     * Remove name and value from config
     */
    virtual int erase(const std::string& name, const std::string& sk) override;

    /**
     * Erase all names under given subkey (and subkey itself)
     */
    virtual int eraseKey(const std::string& sk) override;

    /** Clear all content */
    virtual int clear();

    virtual StatusCode getStatus() const;
    virtual bool ok() const override {
        return getStatus() != STATUS_ERROR;
    }

    /**
     * Walk the configuration values, calling function for each.
     * The function is called with a null nm when changing subsections (the
     * value is then the new subsection name)
     * @return WALK_STOP when/if the callback returns WALK_STOP,
     *         WALK_CONTINUE else (got to end of config)
     */
    enum WalkerCode {WALK_STOP, WALK_CONTINUE};
    virtual WalkerCode sortwalk(
        WalkerCode (*wlkr)(void *cldata, const std::string& nm, const std::string& val),
        void *clidata) const;

    /** Return all names in given submap. On win32, the pattern thing
        only works in recoll builds */
    virtual std::vector<std::string> getNames(
        const std::string& sk, const char *pattern = nullptr) const override;

    /** Check if name is present in any submap. This is relatively expensive
     * but useful for saving further processing sometimes */
    virtual bool hasNameAnywhere(const std::string& nm) const override;

    /**
     * Return all subkeys
     */
    virtual std::vector<std::string> getSubKeys(bool) const override {
        return getSubKeys();
    }
    virtual std::vector<std::string> getSubKeys() const override;
    
    /** Return subkeys in file order. BEWARE: only for the original from the 
     * file: the data is not duplicated to further copies */
    virtual std::vector<std::string> getSubKeys_unsorted(bool = false) const {
        return m_subkeys_unsorted;
    }

    /** Test for subkey existence */
    virtual bool hasSubKey(const std::string& sk) const {
        return m_submaps.find(sk) != m_submaps.end();
    }

    virtual std::string getFilename() const {
        return m_filename;
    }

    /** Used with config files with specially formatted, xml-like comments.
     * Extract the comments as text */
    virtual bool commentsAsXML(std::ostream& out);

    /** !! Note that assignment and copy constructor do not copy the
        auxiliary data (m_order and subkeys_unsorted). */
    
    /**
     * Copy constructor. Expensive but less so than a full rebuild
     */
    ConfSimple(const ConfSimple& rhs)
        : ConfNull() {
        if ((status = rhs.status) == STATUS_ERROR) {
            return;
        }
        dotildexpand = rhs.dotildexpand;
        trimvalues = rhs.trimvalues;
        m_flags = rhs.m_flags;
        m_filename = rhs.m_filename;
        m_submaps = rhs.m_submaps;
    }

    /**
     * Assignement. This is expensive
     */
    ConfSimple& operator=(const ConfSimple& rhs) {
        if (this != &rhs && (status = rhs.status) != STATUS_ERROR) {
            dotildexpand = rhs.dotildexpand;
            trimvalues = rhs.trimvalues;
            m_flags = rhs.m_flags;
            m_filename = rhs.m_filename;
            m_submaps = rhs.m_submaps;
        }
        return *this;
    }

    /**
     * Write in file format to out. If the object was normally constructed from a file or string,
     * this will respect the original comments and declarations order. If this object was copied
     * from another, this will just output the significant content (submaps and parameters values).
     */
    bool write(std::ostream& out) const override;

    /** Give access to semi-parsed file contents */
    const std::vector<ConfLine>& getlines() const {
        return m_order;
    }
    
protected:
    bool dotildexpand;
    bool trimvalues;
    StatusCode status;
private:
    int m_flags{0};
    // Set if we're working with a file
    std::string m_filename;
    int64_t     m_fmtime{0};
    // Configuration data submaps (one per subkey, the main data has a
    // null subkey)
    std::map<std::string, std::map<std::string, std::string, CaseComparator>,
             CaseComparator> m_submaps;
    // Presentation data. We keep the comments, empty lines and variable and subkey ordering
    // information in there (for rewriting the file while keeping hand-edited information).
    // ** This is not copied by the copy constructor or assignment operator, and is only valid
    //    for an object directly constructed from data. **
    std::vector<ConfLine>    m_order;
    std::vector<std::string> m_subkeys_unsorted;
    // Control if we're writing to the backing store after each set()
    bool m_holdWrites{false};
    // Comparators to use for names and keys comparisons. One is passed to the maps constructors,
    // depending on our construction flags
    CaseComparator m_casecomp;
    CaseComparator m_nocasecomp{true};

    void parseinput(std::istream& input);
    bool write();
    bool content_write(std::ostream& out) const;

    // Internal version of set: no RW checking
    virtual int i_set(const std::string& nm, const std::string& val,
                      const std::string& sk, bool init = false);
    bool i_changed(bool upd);
    void openfile(int readonly, std::fstream& input);
};

/**
 * This is a configuration class which attaches tree-like signification to the
 * submap names.
 *
 * If a given variable is not found in the specified section, it will be
 * looked up the tree of section names, and in the global space.
 *
 * submap names should be '/' separated paths (ie: /sub1/sub2). No checking
 * is done, but else the class adds no functionality to ConfSimple.
 *
 * NOTE: contrary to common behaviour, the global or root space is NOT
 * designated by '/' but by '' (empty subkey). A '/' subkey will not
 * be searched at all.
 *
 * Note: getNames() : uses ConfSimple method, this does *not* inherit
 *     names from englobing submaps.
 */
class ConfTree : public ConfSimple {

public:
    /* The constructors just call ConfSimple's, asking for key tilde expansion */
    ConfTree(const char *fname, int readonly = 0, bool trimvalues=true)
        : ConfSimple(fname, readonly, true, trimvalues) {}
    ConfTree(const std::string& data, int readonly = 0, bool trimvalues=true)
        : ConfSimple(data, readonly, true, trimvalues) {}
    ConfTree(int readonly = 0, bool trimvalues=true)
        : ConfSimple(readonly, true, trimvalues) {}
    ConfTree(int flags, const std::string& dataorfn)
        : ConfSimple(flags|ConfSimple::CFSF_TILDEXP, dataorfn) {}
    virtual ~ConfTree() = default;
    ConfTree(const ConfTree& r) : ConfSimple(r) {};
    ConfTree& operator=(const ConfTree& r) {
        ConfSimple::operator=(r);
        return *this;
    }

    /**
     * Get value for named parameter, from specified subsection, or its
     * parents.
     * @return 0 if name not found, 1 else
     */
    virtual int get(const std::string& name, std::string& value,
                    const std::string& sk) const override;
};

/**
 * Use several config files, trying to get values from each in order. 
 *
 * Enables having a central/default config, with possible overrides
 * from more specific (e.g. personal) ones.
 *
 * Notes: it's ok for some of the files not to exist, but the last
 * (bottom) one must or we generate an error. We open all trees
 * readonly, except the topmost one if requested. All writes go to the
 * topmost file. Note that erase() won't work except for parameters
 * only defined in the topmost file (it erases only from there).
 */
template <class T> class ConfStack : public ConfNull {
public:
    /// Construct from configuration file names. The earlier files in have priority when fetching
    /// values.
    /// Only the first file will be updated by set().
    ConfStack(int flags, const std::vector<std::string>& fns) {
        ConfStack::construct(flags, fns);
    }

    // Old call, compat.
    ConfStack(const std::vector<std::string>& fns, bool ro)
        : ConfStack((ro ? ConfSimple::CFSF_RO : ConfSimple::CFSF_NONE), fns) {}

    /// Construct out of single file name and multiple directories
    ConfStack(const std::string& nm, const std::vector<std::string>& dirs, bool ro) {
        std::vector<std::string> fns;
        for (const auto& dir : dirs) {
            fns.push_back(path_cat(dir, nm));
        }
        ConfStack::construct((ro ? ConfSimple::CFSF_RO : ConfSimple::CFSF_NONE), fns);
    }

    /// Construct out of single file name and multiple directories
    ConfStack(int flags, const std::string& nm, const std::vector<std::string>& dirs) {
        std::vector<std::string> fns;
        for (const auto& dir : dirs) {
            fns.push_back(path_cat(dir, nm));
        }
        ConfStack::construct(flags, fns);
    }

    ConfStack(const ConfStack& rhs)
        : ConfNull() {
        init_from(rhs);
    }

    virtual ~ConfStack() {
        clear();
        m_ok = false;
    }

    ConfStack& operator=(const ConfStack& rhs) {
        if (this != &rhs) {
            clear();
            m_ok = rhs.m_ok;
            if (m_ok) {
                init_from(rhs);
            }
        }
        return *this;
    }

    virtual bool sourceChanged() const override {
        for (const auto& conf : m_confs) {
            if (conf->sourceChanged()) {
                return true;
            }
        }
        return false;
    }

    virtual int get(const std::string& name, std::string& value,
                    const std::string& sk, bool shallow) const {
        for (const auto& conf : m_confs) {
            if (conf->get(name, value, sk)) {
                return true;
            }
            if (shallow) {
                break;
            }
        }
        return false;
    }

    virtual int get(const std::string& name, std::string& value,
                    const std::string& sk) const override {
        return get(name, value, sk, false);
    }

    virtual bool hasNameAnywhere(const std::string& nm) const override {
        for (const auto& conf : m_confs) {
            if (conf->hasNameAnywhere(nm)) {
                return true;
            }
        }
        return false;
    }

    virtual int set(const std::string& nm, const std::string& val,
                    const std::string& sk = std::string()) override {
        if (!m_ok) {
            return 0;
        }
        //LOGDEB2(("ConfStack::set [%s]:[%s] -> [%s]\n", sk.c_str(),
        //nm.c_str(), val.c_str()));
        // Avoid adding unneeded entries: if the new value matches the
        // one out from the deeper configs, erase or dont add it
        // from/to the topmost file
        auto it = m_confs.begin();
        it++;
        while (it != m_confs.end()) {
            std::string value;
            if ((*it)->get(nm, value, sk)) {
                // This file has value for nm/sk. If it is the same as the new
                // one, no need for an entry in the topmost file. Else, stop
                // looking and add the new entry
                if (value == val) {
                    m_confs.front()->erase(nm, sk);
                    return true;
                } else {
                    break;
                }
            }
            it++;
        }

        return m_confs.front()->set(nm, val, sk);
    }

    virtual int erase(const std::string& nm, const std::string& sk) override {
        return m_confs.front()->erase(nm, sk);
    }
    virtual int eraseKey(const std::string& sk) override {
        return m_confs.front()->eraseKey(sk);
    }
    virtual bool holdWrites(bool on) override {
        return m_confs.front()->holdWrites(on);
    }

    /** Return all names in given submap. On win32, the pattern thing
        only works in recoll builds */
    virtual std::vector<std::string> getNames(
        const std::string& sk, const char *pattern = 0) const override {
        return getNames1(sk, pattern, false);
    }
    virtual std::vector<std::string> getNamesShallow(
        const std::string& sk, const char *patt = 0) const {
        return getNames1(sk, patt, true);
    }

    virtual std::vector<std::string> getNames1(
        const std::string& sk, const char *pattern, bool shallow) const {
        std::vector<std::string> nms;
        bool skfound = false;
        for (const auto& conf : m_confs) {
            if (conf->hasSubKey(sk)) {
                skfound = true;
                std::vector<std::string> lst = conf->getNames(sk, pattern);
                nms.insert(nms.end(), lst.begin(), lst.end());
            }
            if (shallow && skfound) {
                break;
            }
        }
        sort(nms.begin(), nms.end());
        std::vector<std::string>::iterator uit = unique(nms.begin(), nms.end());
        nms.resize(uit - nms.begin());
        return nms;
    }

    virtual std::vector<std::string> getSubKeys() const override {
        return getSubKeys(false);
    }
    virtual std::vector<std::string> getSubKeys(bool shallow) const override {
        std::vector<std::string> sks;
        for (const auto& conf : m_confs) {
            std::vector<std::string> lst;
            lst = conf->getSubKeys();
            sks.insert(sks.end(), lst.begin(), lst.end());
            if (shallow) {
                break;
            }
        }
        sort(sks.begin(), sks.end());
        std::vector<std::string>::iterator uit = unique(sks.begin(), sks.end());
        sks.resize(uit - sks.begin());
        return sks;
    }

    virtual bool ok() const override {
        return m_ok;
    }

private:
    bool     m_ok;
    std::vector<T*> m_confs;

    /// Reset to pristine
    void clear() {
        for (auto& conf : m_confs) {
            delete(conf);
        }
        m_confs.clear();
    }

    /// Common code to initialize from existing object
    void init_from(const ConfStack& rhs) {
        if ((m_ok = rhs.m_ok)) {
            for (const auto& conf : rhs.m_confs) {
                m_confs.push_back(new T(*conf));
            }
        }
    }

    /// Common construct from file names.
    /// Fail if any fails, except for missing files in all but the bottom location, or the
    /// top one in rw mode.
    void construct(int flags, const std::vector<std::string>& fns) {
        bool ok{true};
        for (unsigned int i = 0; i < fns.size(); i++) {
            const auto& fn = fns[i];
            T* p = new T(flags, fn);
            if (p && p->ok()) {
                m_confs.push_back(p);
            } else {
                delete p;
                // We accept missing files in all but the bottom/ directory.
                // In rw mode, the topmost file must be present.
                if (!path_exists(fn)) {
                    // !ro can only be true for i==0
                    if (!(flags & ConfSimple::CFSF_RO) || (i == fns.size() - 1)) {
                        ok = false;
                        break;
                    }
                }
            }
            // Only the first file is opened rw
            flags |= ConfSimple::CFSF_RO;
        }
        m_ok = ok;
    }
};

#endif /*_CONFTREE_H_ */
