/* Copyright (C) 2008 J.F.Dockes
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
#ifndef _rclquery_h_included_
#define _rclquery_h_included_
#include <string>
#include <vector>

#include <memory>

class PlainToRich;

namespace Rcl {

class Db;
class Doc;
class SearchData;

enum abstract_result {
    ABSRES_ERROR = 0,
    ABSRES_OK = 1,
    ABSRES_TRUNC = 2,
    ABSRES_TERMMISS = 4
};

// Snippet data out of makeDocAbstract
class Snippet {
public:
    Snippet(int page, const std::string& snip, int ln = 0) 
        : page(page), snippet(snip), line(ln) {}
    Snippet& setTerm(const std::string& trm) {
        term = trm;
        return *this;
    }
    int page{0};
    std::string snippet;
    int line{0};
    // This is the "best term" in the fragment, as determined by the qualityTerms() coef. It's only
    // used as a search term when launching an external app. We don't even try to use NEAR/PHRASE
    // groups for this, there are many cases where this would fail.
    std::string term;
};

        
/**
 * An Rcl::Query is a question (SearchData) applied to a
 * database. Handles access to the results. Somewhat equivalent to a
 * cursor in an rdb.
 *
 */
class Query {
public:
    Query(Db *db);
    ~Query();

    Query(const Query &) = delete;
    Query& operator=(const Query &) = delete;

    /** Get explanation about last error */
    std::string getReason() const {
        return m_reason;
    }

    /** Choose sort order. Must be called before setQuery */
    void setSortBy(const std::string& fld, bool ascending = true);
    const std::string& getSortBy() const {
        return m_sortField;
    }
    bool getSortAscending() const {
        return m_sortAscending;
    }

    /** Return or filter results with identical content checksum */
    void setCollapseDuplicates(bool on) {
        m_collapseDuplicates = on;
    }

    /** Accept data describing the search and query the index. This can
     * be called repeatedly on the same object which gets reinitialized each
     * time.
     */
    bool setQuery(std::shared_ptr<SearchData> q);


    /** Set the query fetch slice size. Set to high for bulk fetches */
    void setqquantum(int q) {
        m_qquantum = q;
    }

    /**  Get results count for current query.
     *
     * @param useestimate Use get_matches_estimated() if true, else 
     *     get_matches_lower_bound()
     * @param checkatleast checkatleast parameter to get_mset(). Use -1 for 
     *     full scan.
     */
    int getResCnt(int checkatleast=1000, bool useestimate=false);

    /** Get document at rank i in current query results. */
    bool getDoc(int i, Doc &doc, bool fetchtext = false);

    /** Get possibly expanded list of query terms */
    bool getQueryTerms(std::vector<std::string>& terms);

    bool getDocTerms(const Doc& doc, std::vector<std::vector<std::string>>& terms);
    
    /** Build synthetic abstract for document, extracting chunks relevant for
     * the input query. This uses index data only (no access to the file) 
     */
    // Returned as a vector of Snippet objects
    int makeDocAbstract(const Doc &doc, PlainToRich *plaintorich, std::vector<Snippet>& abstract, 
                        int maxoccs= -1, int ctxwords= -1,bool sortbypage=false);
    // Returned as a vector of text snippets. This just calls the above with default parameters and
    // does a bit of formatting (page/line numbers if applicable).
    bool makeDocAbstract(const Doc &doc, PlainToRich *plaintorich,
                         std::vector<std::string>& abstract);

    /** Choose most interesting term and return the page number for its first match
     *  @param term returns the chosen term 
     *  @return page number or -1 if term not found or other issue
     * could be done and saved while we compute the abstracts, quite a lot of waste here. */
    int getFirstMatchPage(const Doc &doc, std::string& term);
    
    /** Retrieve a reference to the searchData we are using */
    std::shared_ptr<SearchData> getSD() {
        return m_sd;
    }

    /** Expand query to look for documents like the one passed in */
    std::vector<std::string> expand(const Doc &doc);

    /** Return the Db we're set for */
    Db *whatDb() const {
        return m_db;
    }

    /* make this public for access from embedded Db::Native */
    class Native;
    Native *m_nq;

private:
    std::string m_reason; // Error explanation
    Db    *m_db;
    void  *m_sorter{nullptr};
    std::string m_sortField;
    bool   m_sortAscending{true};
    bool   m_collapseDuplicates{false};     
    int    m_resCnt{-1};
    std::shared_ptr<SearchData> m_sd;
    int    m_snipMaxPosWalk{1000000};

    // Mset size. When a document is requested, we actually fetch a slice. Bigger slice: slower for
    // a single doc and more memory use, but faster for bulk retrievals. The default of 100 is a
    // reasonable compromise.
    // Note: times for retrieving (multiple times)all docs from a sample
    // 25k docs db (q: mime:*)
    // qquantum: 10 50  100 150 200 1000
    // Seconds:  21 8.5 6.7 6.4 5.8 6.0
    int    m_qquantum{100};
};

}

#endif /* _rclquery_h_included_ */
