/* Copyright (C) 2004-2023 J.F.Dockes
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
#ifndef _RCLCONFIG_H_INCLUDED_
#define _RCLCONFIG_H_INCLUDED_

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <map>
#include <unordered_set>
#include <memory>

class ParamStale;
class ConfSimple;
class ConfNull;

// Hold the description for an external metadata-gathering command
struct MDReaper {
    std::string fieldname;
    std::vector<std::string> cmdv;
};

// Data associated to a indexed field name: 
struct FieldTraits {
    std::string pfx; // indexing prefix,
    uint32_t valueslot{0};
    enum ValueType {STR, INT};
    ValueType valuetype{STR};
    // INT type fields are stored as Xapian values, and padded so that they can be compared as
    // strings. valuelen is the padding width.
    int    valuelen{0};
    int    wdfinc{1}; // Index time term frequency increment (default 1)
    double boost{1.0}; // Query time boost (default 1.0)
    bool   pfxonly{false}; // Suppress prefix-less indexing
    bool   noterms{false}; // Don't add term to highlight data (e.g.: rclbes)
};

class RclConfig {
public:

    /** Constructor: we will look for the configuration location, except if this was specified on the
     * command line and passed through argcnf */
    RclConfig(const std::string *argcnf = nullptr);
    RclConfig(const RclConfig &r);

    ~RclConfig();

    RclConfig& operator=(const RclConfig &r);

    /** Return a writable clone of the main config. This belongs to the caller (must delete it when
     * done) */
    ConfNull *cloneMainConfig();

    /** (re)Read recoll.conf */
    bool updateMainConfig();

    bool ok() const;
    const std::string &getReason() const;

    /** Return the directory where this configuration is stored. This was possibly silently created
     *  by the rclconfig constructor it it is the default one (~/.recoll) and it did not exist
     *  yet. */
    std::string getConfDir() const;
    std::string getCacheDir() const;

    /** Check if the config files were modified since we read them */
    bool sourceChanged() const;

    /** Returns true if this is ~/.recoll */
    bool isDefaultConfig() const;
    /** Get the local value for /usr/local/share/recoll/ */
    const std::string& getDatadir() const;

    /** Set current directory reference, and fetch automatic parameters. */
    void setKeyDir(const std::string &dir);
    std::string getKeyDir() const;

    /** Get generic configuration parameter according to current keydir */
    bool getConfParam(const std::string& name, std::string& value, bool shallow=false) const;
    /** Variant with autoconversion to int */
    bool getConfParam(const std::string &name, int *value, bool shallow=false) const;
    /** Variant with autoconversion to double */
    bool getConfParam(const std::string &name, double *value, bool shallow=false) const;
    /** Variant with autoconversion to bool */
    bool getConfParam(const std::string &name, bool *value, bool shallow=false) const;
    /** Variant with conversion to vector<string>
     *  (stringToStrings). Can fail if the string is malformed. */
    bool getConfParam(const std::string& name, std::vector<std::string> *value,
                      bool shallow=false) const;
    /** Variant with conversion to unordered_set<string>
     *  (stringToStrings). Can fail if the string is malformed. */
    bool getConfParam(const std::string &name, std::unordered_set<std::string> *v, 
                      bool shallow=false) const;
    /** Variant with conversion to vector<int> */
    bool getConfParam(const std::string &name, std::vector<int> *value, bool shallow=false) const;

    enum ThrStage {ThrIntern=0, ThrSplit=1, ThrDbWrite=2};
    std::pair<int, int> getThrConf(ThrStage who) const;

    /** 
     * Get list of config names under current sk, with possible wildcard filtering 
     */
    std::vector<std::string> getConfNames(const char *pattern = nullptr) const;

    /** Check if name exists anywhere in config */
    bool hasNameAnywhere(const std::string& nm) const;

    /** Get default charset for current keydir (was set during setKeydir) 
     * filenames are handled differently */
    const std::string &getDefCharset(bool filename = false) const;

    /** Get list of top directories. This is needed from a number of places
     * and needs some cleaning-up code. An empty list is always an error, no
     * need for other status 
     * @param formonitor if set retrieve the list for real time monitoring 
     *         (if the monitor list does not exist we return the normal one).
     */
    std::vector<std::string> getTopdirs(bool formonitor = false) const;

    std::string getConfdirPath(const char *varname, const char *dflt) const;
    std::string getCachedirPath(const char *varname, const char *dflt) const;
    /** Get database and other directories */
    std::string getDbDir() const;
    std::string getWebcacheDir() const;
    std::string getMboxcacheDir() const;
    std::string getAspellcacheDir() const;
    /** Get stoplist file name */
    std::string getStopfile() const;
    /** Get synonym groups file name */
    std::string getIdxSynGroupsFile() const;
    /** Get indexing pid file name */
    std::string getPidfile() const;
    /** Get indexing status file name */
    std::string getIdxStatusFile() const;
    std::string getIdxStopFile() const;
    /** Do path translation according to the ptrans table */
    void urlrewrite(const std::string& dbdir, std::string& url) const;
    ConfSimple *getPTrans();
    /** Get Web Queue directory name */
    std::string getWebQueueDir() const;

    /** Get list of skipped file names for current keydir */
    std::vector<std::string>& getSkippedNames();
    /** Get list of file name filters for current keydir (only those names indexed) */
    std::vector<std::string>& getOnlyNames();

    /** Get list of skipped paths patterns. Doesn't depend on the keydir */
    std::vector<std::string> getSkippedPaths() const;
    /** Get list of skipped paths patterns, daemon version (may add some)
        Doesn't depend on the keydir */
    std::vector<std::string> getDaemSkippedPaths() const;

    /** Return list of no content suffixes. Used by confgui, indexing uses
        inStopSuffixes() for testing suffixes */
    std::vector<std::string>& getStopSuffixes();

    /** 
     * mimemap: Check if file name should be ignored because of suffix
     *
     * The list of ignored suffixes is initialized on first call, and
     * not changed for subsequent setKeydirs.
     */
    bool inStopSuffixes(const std::string& fn);

    /** 
     * Check in mimeconf if input mime type is a compressed one, and
     * return command to uncompress if it is.
     *
     * The returned command has substitutable places for input file name 
     * and temp dir name, and will return output name
     */
    bool getUncompressor(const std::string &mtpe, std::vector<std::string>& cmd) const;

    /** mimemap: compute mimetype */
    std::string getMimeTypeFromSuffix(const std::string &suffix) const;
    /** mimemap: get a list of all indexable mime types defined */
    std::vector<std::string> getAllMimeTypes() const;
    /** mimemap: Get appropriate suffix for mime type. This is inefficient */
    std::string getSuffixFromMimeType(const std::string &mt) const;

    /** mimeconf: get input filter for mimetype */
    std::string getMimeHandlerDef(const std::string &mimetype, bool filtertypes=false,
                             const std::string& fn = std::string());

    /** For lines like: [name = some value; attr1 = value1; attr2 = val2]
     * Separate the value and store the attributes in a ConfSimple 
     *
     * In the value part, semi-colons inside double quotes are ignored, and double quotes are
     * conserved. In the common case where the string is then processed by stringToStrings() to
     * build a command line, this allows having semi-colons inside arguments. However, no backslash
     * escaping is possible, so that, for example "bla\"1;2\"" would not work (the value part
     * would stop at the semi-colon).
     *
     * @param whole the raw value.
     */
    static bool valueSplitAttributes(const std::string& whole, std::string& value,
                                     ConfSimple& attrs) ;

    /** Compute difference between 'base' and 'changed', as elements to be
     * added and substracted from base. Input and output strings are in
     * stringToStrings() format. */
    static void setPlusMinus(const std::string& base, const std::set<std::string>& changed,
                             std::string& plus, std::string& minus);

    /** Return the locale's character set */
    static const std::string& getLocaleCharset();
    
    /** Return icon path for mime type and tag */
    std::string getMimeIconPath(const std::string &mt, const std::string& apptag) const;

    /** mimeconf: get list of file categories */
    bool getMimeCategories(std::vector<std::string>&) const;
    /** mimeconf: is parameter one of the categories ? */
    bool isMimeCategory(const std::string&) const;
    /** mimeconf: get list of mime types for category */
    bool getMimeCatTypes(const std::string& cat, std::vector<std::string>&) const;

    /** mimeconf: get list of gui filters (doc cats by default */
    bool getGuiFilterNames(std::vector<std::string>&) const;
    /** mimeconf: get query lang frag for named filter */
    bool getGuiFilter(const std::string& filtername, std::string& frag) const;

    /** fields: get field prefix from field name. Use additional query
        aliases if isquery is set */
    bool getFieldTraits(const std::string& fldname, const FieldTraits **,
                        bool isquery = false) const;

    const std::set<std::string>& getStoredFields() const;

    std::set<std::string> getIndexedFields() const;

    /** Get canonic name for possible alias */
    std::string fieldCanon(const std::string& fld) const;

    /** Get canonic name for possible alias, including query-only aliases */
    std::string fieldQCanon(const std::string& fld) const;

    /** Get xattr name to field names translations */
    const std::map<std::string, std::string>& getXattrToField() const;

    /** Get value of a parameter inside the "fields" file. Only some filters 
     * use this (ie: mh_mail). The information specific to a given filter
     * is typically stored in a separate section(ie: [mail]) 
     */
    std::vector<std::string> getFieldSectNames(const std::string &sk, const char* = nullptr) const;
    bool getFieldConfParam(const std::string &name, const std::string &sk, std::string &value)
        const;

    /** mimeview: get/set external viewer exec string(s) for mimetype(s) */
    std::string getMimeViewerDef(const std::string &mimetype, const std::string& apptag, 
                            bool useall) const;
    std::set<std::string> getMimeViewerAllEx() const;
    bool setMimeViewerAllEx(const std::set<std::string>& allex);
    bool getMimeViewerDefs(std::vector<std::pair<std::string, std::string> >&) const;
    bool setMimeViewerDef(const std::string& mimetype, const std::string& cmd);
    /** Check if mime type is designated as needing no uncompress before view
     * (if a file of this type is found compressed). Default is true,
     *  exceptions are found in the nouncompforviewmts mimeview list */
    bool mimeViewerNeedsUncomp(const std::string &mimetype) const;

    /** Retrieve extra metadata-gathering commands */
    const std::vector<MDReaper>& getMDReapers();

    /** Store/retrieve missing helpers description string */
    bool getMissingHelperDesc(std::string&) const;
    void storeMissingHelperDesc(const std::string &s);

    /** Replace simple command name(s) inside vector with full
     * paths. May have to replace two if the first entry is an
     * interpreter name */
    bool processFilterCmd(std::vector<std::string>& cmd) const;

    /** Find exec file for external filter. 
     *
     * If the input is an absolute path, we just return it. Else We
     * look in $RECOLL_FILTERSDIR, "filtersdir" from the config file,
     * $RECOLL_CONFDIR/. If nothing is found, we return the input with
     * the assumption that this will be used with a PATH-searching
     * exec.
     *
     * @param cmd is normally the command name from the command string 
     *    returned by getMimeHandlerDef(), but this could be used for any 
     *    command. If cmd begins with a /, we return cmd without
     *    further processing.
     */
    std::string findFilter(const std::string& cmd) const;

    /** Thread config init is not done automatically because not all
        programs need it and it uses the debug log so that it's better to
        call it after primary init */
    void initThrConf();

    const std::string& getOrigCwd() const;

    class Internal;
    friend class ParamStale;
private:
    std::unique_ptr<Internal> m;
};

// This global variable defines if we are running with an index
// stripped of accents and case or a raw one. Ideally, it should be
// constant, but it needs to be initialized from the configuration, so
// there is no way to do this. It never changes after initialization
// of course. Changing the value on a given index imposes a
// reset. When using multiple indexes, all must have the same value
extern bool o_index_stripchars;

// Do we generate term positions. The default is yes, to enable phrase and proximity searches, but
// it is possible to disable it for index size or performance reasons if they are not needed.
extern bool o_no_term_positions;

// Store document text in index. Allows extracting snippets from text
// instead of building them from index position data. Has become
// necessary for versions of Xapian 1.6, which have dropped support
// for the chert index format, and adopted a setup which renders our
// use of positions list unacceptably slow in cases. The text just
// translated from its original format to UTF-8 plain text, and is not
// stripped of upper-case, diacritics, or punctuation signs. Defaults to true.
extern bool o_index_storedoctext;

// This global variable defines if we use mtime instead of ctime for
// up-to-date tests. This is mostly incompatible with xattr indexing,
// in addition to other issues. See recoll.conf comments. 
extern bool o_uptodate_test_use_mtime;

// Up to version 1.33.x recoll never stem-expanded terms inside phrase searches. There was actually
// no obvious reasons for this. The default was not changed, but a modifier was added (x) to allow
// for on-request phrase expansion. Setting the following to true will change the default behaviour
// (use 'l' to disable for a specific instance)
extern bool o_expand_phrases;

#endif /* _RCLCONFIG_H_INCLUDED_ */
