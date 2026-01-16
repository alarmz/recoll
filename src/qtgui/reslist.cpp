/* Copyright (C) 2005-2022 J.F.Dockes
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

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include <memory>
#include <utility>

#include <qapplication.h>
#include <qvariant.h>
#include <qevent.h>
#include <qmenu.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qimage.h>
#include <qscrollbar.h>
#include <QTextBlock>
#include <QShortcut>
#include <QProgressDialog>

#include "log.h"
#include "smallut.h"
#include "recoll.h"
#include "guiutils.h"
#include "pathut.h"
#include "docseq.h"
#include "pathut.h"
#include "mimehandler.h"
#include "plaintorich.h"
#include "internfile.h"
#include "indexer.h"
#include "snippets_w.h"
#include "listdialog.h"
#include "reslist.h"
#include "moc_reslist.cpp"
#include "rclhelp.h"
#include "appformime.h"
#include "respopup.h"
#include "reslistpager.h"
#include "rclwebpage.h"

using std::string;
using std::vector;
using std::map;
using std::list;

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

#if defined(USING_WEBKIT)
# include <QWebFrame>
# include <QWebElement>
# include <QWebSettings>
# define QWEBSETTINGS QWebSettings
#elif defined(USING_WEBENGINE)
// Notes for WebEngine:
// - It used to be that all links must begin with http:// for
//   acceptNavigationRequest to be called. Can't remember if file://
//   would work. Anyway not any more since we set baseURL. See comments
//   in linkClicked().
// - The links passed to acceptNav.. have the host part 
//   lowercased -> we change S0 to http://localhost/S0, not http://S0
# include <QWebEnginePage>
# include <QWebEngineSettings>
# include <QtWebEngineWidgets>
# define QWEBSETTINGS QWebEngineSettings
#endif

#ifdef USING_WEBENGINE
// This script saves the location details when a mouse button is
// clicked. This is for replacing data provided by Webkit QWebElement
// on a right-click as QT WebEngine does not have an equivalent service.
static const string locdetailscript(R"raw(
var locDetails = '';
function saveLoc(ev) 
{
    el = ev.target;
    locDetails = '';
    while (el && el.attributes && !el.attributes.getNamedItem("rcldocnum")) {
        el = el.parentNode;
    }
    rcldocnum = el.attributes.getNamedItem("rcldocnum");
    if (rcldocnum) {
        rcldocnumvalue = rcldocnum.value;
    } else {
        rcldocnumvalue = "";
    }
    if (el && el.attributes) {
        locDetails = 'rcldocnum = ' + rcldocnumvalue
    }
}
)raw");

#endif // WEBENGINE


class QtGuiResListPager : public ResListPager {
public:
    QtGuiResListPager(RclConfig *cnf, ResList *p, int ps, bool alwayssnip) 
        : ResListPager(cnf, ps, alwayssnip), m_reslist(p) {}
    virtual bool append(const string& data) override;
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc) override;
    virtual string trans(const string& in) override;
    virtual string detailsLink() override;
    virtual const string &parFormat() override;
    virtual const string &dateFormat() override;
    virtual string nextUrl() override;
    virtual string prevUrl() override;
    virtual string headerContent() override;
    virtual void suggest(const vector<string>uterms, map<string, vector<string> >& sugg) override;
    virtual string absSep() override {return (const char *)(prefs.abssep.toUtf8());}
    virtual bool useAll() override {return prefs.useDesktopOpen;} 
#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)
    // We now set baseURL to file:///, The relative links will be received as e.g. file:///E%N we
    // don't set the linkPrefix() anymore (it was "file:///recoll-links/" at some point but this
    // broke links added by users).
    virtual string bodyAttrs() override {
        return "onload=\"addEventListener('contextmenu', saveLoc)\"";
    }
#endif

private:
    ResList *m_reslist;
};

//////////////////////////////
// /// QtGuiResListPager methods:
bool QtGuiResListPager::append(const string& data)
{
    m_reslist->append(u8s2qs(data));
    return true;
}

bool QtGuiResListPager::append(const string& data, int docnum, const Rcl::Doc&)
{
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    m_reslist->append(QString("<div class=\"rclresult\" id=\"%1\" rcldocnum=\"%1\">").arg(docnum));
    m_reslist->append(u8s2qs(data));
    m_reslist->append("</div>");
#else
    int blkcnt0 = m_reslist->document()->blockCount();
    m_reslist->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    m_reslist->textCursor().insertBlock();
    m_reslist->insertHtml(QString::fromUtf8(data.c_str()));
    m_reslist->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
    m_reslist->ensureCursorVisible();
    int blkcnt1 = m_reslist->document()->blockCount();
    for (int block = blkcnt0; block < blkcnt1; block++) {
        m_reslist->m_pageParaToReldocnums[block] = docnum;
    }
#endif
    return true;
}

string QtGuiResListPager::trans(const string& in)
{
    return qs2utf8s(ResList::tr(in.c_str()));
}

string QtGuiResListPager::detailsLink()
{
    return href("H-1", trans("(show query)"));
}

const string& QtGuiResListPager::parFormat()
{
    return prefs.creslistformat;
}
const string& QtGuiResListPager::dateFormat()
{
    return prefs.reslistdateformat;
}

string QtGuiResListPager::nextUrl()
{
    return "n-1";
}

string QtGuiResListPager::prevUrl()
{
    return "p-1";
}

string QtGuiResListPager::headerContent() 
{
    string out;
#if defined(USING_WEBENGINE)
    out += "<script type=\"text/javascript\">\n";
    out += locdetailscript;
    out += "</script>\n";
#endif

    return out + prefs.htmlHeaderContents();
}

void QtGuiResListPager::suggest(const vector<string>uterms, map<string, vector<string>>& sugg)
{
    sugg.clear();
    bool issimple = m_reslist && m_reslist->m_rclmain && m_reslist->m_rclmain->lastSearchSimple();

    for (const auto& userterm : uterms) {
        vector<string> spellersuggs;

        // If the term is in the dictionary, by default, aspell won't list alternatives, but the
        // recoll utility based on python-aspell. In any case, we might want to check the term
        // frequencies and taylor our suggestions accordingly ? For example propose a replacement
        // for a valid term only if the replacement is much more frequent ? (as google seems to do)?
        // To mimick the old code, we could check for the user term presence in the suggestion list,
        // but it's not clear that giving no replacements in this case would be better ?
        //
        // Also we should check that the term stems differently from the base word (else it's not
        // useful to expand the search). Or is it ? This should depend if stemming is turned on or
        // not
        if (!rcldb->getSpellingSuggestions(userterm, spellersuggs)) {
            continue;
        }
        if (!spellersuggs.empty()) {
            sugg[userterm] = vector<string>();
            int cnt = 0;
            for (auto subst : spellersuggs) {
                if (subst == userterm)
                    continue;
                if (++cnt > 5)
                    break;
                if (issimple) {
                    // If this is a simple search, we set up links as a <href="Sold|new">, else we
                    // just list the replacements.
                    subst = href("S-1|" + userterm + "|" + subst, subst);
                }
                sugg[userterm].push_back(subst);
            }
        }
    }
}

/////// /////// End reslistpager methods

string PlainToRichQtReslist::startMatch(unsigned int idx)
{
    PRETEND_USE(idx);
#if 0
    if (m_hdata) {
        string s1, s2;
        stringsToString<vector<string> >(m_hdata->groups[idx], s1); 
        stringsToString<vector<string> >(m_hdata->ugroups[m_hdata->grpsugidx[idx]], s2);
        LOGDEB2("Reslist startmatch: group " << s1 << " user group " << s2 << "\n");
    }
#endif                
    return string("<span class='rclmatch' style='") + qs2utf8s(prefs.qtermstyle) + string("'>");
}

string PlainToRichQtReslist::endMatch()
{
    return string("</span>");
}

static PlainToRichQtReslist g_hiliter;

/////////////////////////////////////

ResList::ResList(QWidget* parent, const char* name)
    : RESLIST_PARENTCLASS(parent)
{
    if (!name)
        setObjectName("resList");
    else 
        setObjectName(name);
    
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    settings()->setAttribute(QWEBSETTINGS::JavascriptEnabled, true);
#ifdef USING_WEBKIT
    LOGDEB("Reslist: using Webkit\n");
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QUrl &)), this, SLOT(onLinkClicked(const QUrl &)));
#else
    LOGDEB("Reslist: using Webengine\n");
    setPage(new RclWebPage(this));
    connect(page(), SIGNAL(linkClicked(const QUrl &)), this, SLOT(onLinkClicked(const QUrl &)));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(runStoredJS(bool)));
    // These appear to get randomly disconnected or never connected.
    connect(page(), SIGNAL(scrollPositionChanged(const QPointF &)),
            this, SLOT(onPageScrollPositionChanged(const QPointF &)));
    connect(page(), SIGNAL(contentsSizeChanged(const QSizeF &)),
            this, SLOT(onPageContentsSizeChanged(const QSizeF &)));
#endif
#else
    LOGDEB("Reslist: using QTextBrowser\n");
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setOpenLinks(false);
    setTabChangesFocus(true);
    // signals and slots connections
    connect(this, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(onLinkClicked(const QUrl &)));
#endif

    languageChange();

    (void)new HelpClient(this);
    HelpClient::installMap(qs2utf8s(this->objectName()), "RCL.SEARCH.GUI.RESLIST");

#if 0
    // See comments in "highlighted
    connect(this, SIGNAL(highlighted(const QString &)), this, SLOT(highlighted(const QString &)));
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));

    m_pager = new QtGuiResListPager(theconfig, this, prefs.respagesize, prefs.alwaysSnippets);
    m_pager->setHighLighter(&g_hiliter);
    resetView();
}

ResList::~ResList()
{
    // These have to exist somewhere for translations to work
#ifdef __GNUC__
    __attribute__((unused))
#endif
        static const char* strings[] = {
        QT_TR_NOOP("<p><b>No results found</b><br>"),
        QT_TR_NOOP("Documents"),
        QT_TR_NOOP("out of at least"),
        QT_TR_NOOP("for"),
        QT_TR_NOOP("Previous"),
        QT_TR_NOOP("Next"),
        QT_TR_NOOP("Unavailable document"),
        QT_TR_NOOP("Preview"),
        QT_TR_NOOP("Open"),
        QT_TR_NOOP("Snippets"),
        QT_TR_NOOP("(show query)"),
        QT_TR_NOOP("<p><i>Alternate spellings (accents suppressed): </i>"),
        QT_TR_NOOP("<p><i>Alternate spellings: </i>"),
        QT_TR_NOOP("This spelling guess was added to the search:"),
        QT_TR_NOOP("These spelling guesses were added to the search:"),
    };
}

void ResList::setRclMain(RclMain *m, bool ismain) 
{
    m_rclmain = m;
    m_ismainres = ismain;
    if (!m_ismainres) {
        connect(new QShortcut(closeKeySeq, this), SIGNAL (activated()), 
                this, SLOT (close()));
        connect(new QShortcut(quitKeySeq, this), SIGNAL (activated()), 
                m_rclmain, SLOT (fileExit()));
        connect(this, SIGNAL(previewRequested(Rcl::Doc)), 
                m_rclmain, SLOT(startPreview(Rcl::Doc)));
        connect(this, SIGNAL(docSaveToFileClicked(Rcl::Doc)), 
                m_rclmain, SLOT(saveDocToFile(Rcl::Doc)));
        connect(this, SIGNAL(editRequested(Rcl::Doc)), 
                m_rclmain, SLOT(startNativeViewer(Rcl::Doc)));
    }
}

#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)

void ResList::runJS(const QString& js)
{
    LOGDEB("runJS: " << qs2utf8s(js) << "\n");
#if defined(USING_WEBKIT)
    page()->mainFrame()->evaluateJavaScript(js);
#elif defined(USING_WEBENGINE)
    page()->runJavaScript(js);
//    page()->runJavaScript(js, [](const QVariant &v) {qDebug() << v.toString();});
#endif
}

#if defined(USING_WEBENGINE)
void ResList::runStoredJS(bool res)
{
    if (m_js.isEmpty()) {
        return;
    }
    LOGDEB0("ResList::runStoredJS: res " << res << " cnt " << m_js_countdown <<
           " m_js [" << qs2utf8s(m_js) << "]\n");
    if (m_js_countdown > 0) {
        m_js_countdown--;
        return;
    }
    runJS(m_js);
    m_js.clear();
}

void ResList::onPageScrollPositionChanged(const QPointF &position)
{
    LOGDEB0("ResList::onPageScrollPositionChanged: y : " << position.y() << "\n");
    m_scrollpos = position;
}

void ResList::onPageContentsSizeChanged(const QSizeF &size)
{
    LOGDEB0("ResList::onPageContentsSizeChanged: y : " << size.height() << "\n");
    m_contentsize = size;
}
#endif // WEBENGINE

static void maybeDump(const QString& text)
{
    std::string dumpfile;
    if (!theconfig->getConfParam("reslisthtmldumpfile", dumpfile) || dumpfile.empty()) {
        return;
    }
    dumpfile = path_tildexpand(dumpfile);
    if (path_exists(dumpfile)) {
        return;
    }
    auto fp = fopen(dumpfile.c_str(), "w");
    if (fp) {
        auto s = qs2utf8s(text);
        fwrite(s.c_str(), 1, s.size(), fp);
        fclose(fp);
    }
}

#endif // WEBKIT or WEBENGINE

void ResList::onUiPrefsChanged()
{
    displayPage();
}

void ResList::setFont()
{
#if !defined(USING_WEBKIT) && !defined(USING_WEBENGINE)
    // Using QTextBrowser
    if (prefs.reslistfontfamily != "") {
        QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
        QTextBrowser::setFont(nfont);
    } else {
        QFont font;
        font.setPointSize(prefs.reslistfontsize);
        QTextBrowser::setFont(font);
    }
#endif
}

int ResList::newListId()
{
    static int id;
    return ++id;
}

void ResList::setDocSource(std::shared_ptr<DocSequence> nsource)
{
    LOGDEB("ResList::setDocSource()\n");
    m_source = std::shared_ptr<DocSequence>(new DocSource(theconfig, nsource));
    if (m_pager)
        m_pager->setDocSource(m_source);
}

// A query was executed, or the filtering/sorting parameters changed,
// re-read the results.
void ResList::readDocSource()
{
    LOGDEB("ResList::readDocSource()\n");
    m_curPvDoc = -1;
    if (!m_source)
        return;
    m_listId = newListId();

    // Reset the page size in case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->setDocSource(m_source);
    resultPageNext();
    emit hasResults(m_source->getResCnt());
}

void ResList::resetList() 
{
    LOGDEB("ResList::resetList()\n");
    setDocSource(std::shared_ptr<DocSequence>());
    resetView();
}

void ResList::resetView() 
{
    m_curPvDoc = -1;
    // There should be a progress bar for long searches but there isn't 
    // We really want the old result list to go away, otherwise, for a
    // slow search, the user will wonder if anything happened. The
    // following helps making sure that the textedit is really
    // blank. Else, there are often icons or text left around
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    m_text = "";
    QString html("<html><head>");
    html += u8s2qs(m_pager->headerContent()) + "</head>";
    html += "<body></body></html>";
    setHtml(html);
#else
    m_pageParaToReldocnums.clear();
    clear();
    QTextBrowser::append(".");
    clear();
#endif

}

bool ResList::displayingHistory()
{
    // We want to reset the displayed history if it is currently
    // shown. Using the title value is an ugly hack
    string htstring = string((const char *)tr("Document history").toUtf8());
    if (!m_source || m_source->title().empty())
        return false;
    return m_source->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setWindowTitle(tr("Result list"));
}

#if !defined(USING_WEBKIT) && !defined(USING_WEBENGINE)
// Get document number from text block number
int ResList::docnumfromparnum(int block)
{
    if (m_pager->pageNumber() < 0)
        return -1;

    // Try to find the first number < input and actually in the map
    // (result blocks can be made of several text blocks)
    std::map<int,int>::iterator it;
    do {
        it = m_pageParaToReldocnums.find(block);
        if (it != m_pageParaToReldocnums.end())
            return pageFirstDocNum() + it->second;
    } while (--block >= 0);
    return -1;
}

// Get range of paragraph numbers which make up the result for document number
std::pair<int,int> ResList::parnumfromdocnum(int docnum)
{
    LOGDEB("parnumfromdocnum: docnum " << docnum << "\n");
    if (m_pager->pageNumber() < 0) {
        LOGDEB("parnumfromdocnum: no page return -1,-1\n");
        return {-1, -1};
    }
    int winfirst = pageFirstDocNum();
    if (docnum - winfirst < 0) {
        LOGDEB("parnumfromdocnum: docnum " << docnum << " < winfirst " <<
               winfirst << " return -1,-1\n");
        return {-1, -1};
    }
    docnum -= winfirst;
    for (const auto& entry : m_pageParaToReldocnums) {
        if (docnum == entry.second) {
            int first = entry.first;
            int last = first+1;
            std::map<int,int>::iterator it1;
            while ((it1 = m_pageParaToReldocnums.find(last)) != 
                   m_pageParaToReldocnums.end() && it1->second == docnum) {
                last++;
            }
            LOGDEB("parnumfromdocnum: return " << first << "," << last << "\n");
            return {first, last};
        }
    }
    LOGDEB("parnumfromdocnum: not found return -1,-1\n");
    return {-1,-1};
}
#endif // TEXTBROWSER

// Return doc from current or adjacent result pages. We can get called
// for a document not in the current page if the user browses through
// results inside a result window (with shift-arrow). This can only
// result in a one-page change.
bool ResList::getDoc(int docnum, Rcl::Doc &doc)
{
    LOGDEB("ResList::getDoc: docnum " << docnum << " winfirst " <<
           pageFirstDocNum() << "\n");
    int winfirst = pageFirstDocNum();
    int winlast = m_pager->pageLastDocNum();
    if (docnum < 0 ||  winfirst < 0 || winlast < 0)
        return false;

    // Is docnum in current page ? Then all Ok
    if (docnum >= winfirst && docnum <= winlast) {
        return m_pager->getDoc(docnum, doc);
    }

    // Else we accept to page down or up but not further
    if (docnum < winfirst && docnum >= winfirst - prefs.respagesize) {
        resultPageBack();
    } else if (docnum < winlast + 1 + prefs.respagesize) {
        resultPageNext();
    }
    winfirst = pageFirstDocNum();
    winlast = m_pager->pageLastDocNum();
    if (docnum >= winfirst && docnum <= winlast) {
        return m_pager->getDoc(docnum, doc);
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if ((e->modifiers() & Qt::ShiftModifier)) {
        if (e->key() == Qt::Key_PageUp) {
            // Shift-PageUp -> first page of results
            resultPageFirst();
            return;
        } 
    } else {
        if (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_Backspace) {
            resPageUpOrBack();
            return;
        } else if (e->key() == Qt::Key_PageDown || e->key() == Qt::Key_Space) {
            resPageDownOrNext();
            return;
        }
    }
    RESLIST_PARENTCLASS::keyPressEvent(e);
}

void ResList::mouseReleaseEvent(QMouseEvent *e)
{
    m_lstClckMod = 0;
    if (e->modifiers() & Qt::ControlModifier) {
        m_lstClckMod |= Qt::ControlModifier;
    } 
    if (e->modifiers() & Qt::ShiftModifier) {
        m_lstClckMod |= Qt::ShiftModifier;
    }
    RESLIST_PARENTCLASS::mouseReleaseEvent(e);
}

void ResList::highlighted(const QString& )
{
    // This is supposedly called when a link is preactivated (hover or tab
    // traversal, but is not actually called for tabs. We would have liked to
    // give some kind of visual feedback for tab traversal
}

// Page Up/Down: we don't try to check if current paragraph is last or
// first. We just page up/down and check if viewport moved. If it did,
// fair enough, else we go to next/previous result page.
void ResList::resPageUpOrBack()
{
#if defined(USING_WEBKIT)
    if (scrollIsAtTop()) {
        resultPageBack();
        runJS("window.scrollBy(0,50000);");
    } else {
        page()->mainFrame()->scroll(0, -int(0.9*geometry().height()));
    }
    setupArrows();
#elif defined(USING_WEBENGINE)
    if (scrollIsAtTop()) {
        // Displaypage used to call resetview() which caused a page load event. We wanted to run the
        // js on the second event, with countdown = 1. Not needed any more, but kept around.
        m_js_countdown = 0;
        m_js = "window.scrollBy(0,50000);";
        resultPageBack();
    } else {
        QString js = QString("window.scrollBy(%1, %2);").arg(0).arg(-int(0.9*geometry().height()));
        runJS(js);
    }
    QTimer::singleShot(50, this, SLOT(setupArrows()));
#else
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    if (vpos == verticalScrollBar()->value())
        resultPageBack();
#endif
}

void ResList::resPageDownOrNext()
{
#if defined(USING_WEBKIT)
    if (scrollIsAtBottom()) {
        resultPageNext();
    } else {
        page()->mainFrame()->scroll(0, int(0.9*geometry().height()));
    }
    setupArrows();
#elif defined(USING_WEBENGINE)
    if (scrollIsAtBottom()) {
        LOGDEB0("downOrNext: at bottom: call resultPageNext\n");
        resultPageNext();
    } else {
        LOGDEB0("downOrNext: scroll\n");
        QString js = QString("window.scrollBy(%1, %2);").arg(0).arg(int(0.9*geometry().height()));
        runJS(js);
    }
    QTimer::singleShot(50, this, SLOT(setupArrows()));
#else
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    LOGDEB("ResList::resPageDownOrNext: vpos before " << vpos << ", after "
           << verticalScrollBar()->value() << "\n");
    if (vpos == verticalScrollBar()->value()) 
        resultPageNext();
#endif
}

bool ResList::scrollIsAtBottom()
{
#if defined(USING_WEBKIT)
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
        ret = true;
    } else {
        int max = frame->scrollBarMaximum(Qt::Vertical);
        int cur = frame->scrollBarValue(Qt::Vertical); 
        ret = (max != 0) && (cur == max);
        LOGDEB2("Scrollatbottom: cur " << cur << " max " << max << "\n");
    }
    LOGDEB2("scrollIsAtBottom: returning " << ret << "\n");
    return ret;
#elif defined(USING_WEBENGINE)
    // TLDR: Could not find any way whatsoever to reliably get the page contents size or position
    // with either signals or direct calls. No obvious way to get this from javascript either. This
    // used to work, with direct calls and broke down around qt 5.15

    QSize wss = size();

    //QSize css = page()->contentsSize().toSize();
    QSize css = m_contentsize.toSize();
    // Does not work with recent (2022) qt releases: always 0
    //auto spf = page()->scrollPosition();
    // Found no easy way to block waiting for the js output
    //runJS("document.body.scrollTop", [](const QVariant &v) {qDebug() << v.toInt();});
    QPoint sp = m_scrollpos.toPoint();
    LOGDEB0("atBottom: contents W " << css.width() << " H " << css.height() << 
            " widget W " << wss.width() << " Y " << wss.height() << 
            " scroll X " << sp.x() << " Y " << sp.y() << "\n");
    // This seems to work but it's mysterious as points and pixels
    // should not be the same
    return wss.height() + sp.y() >= css.height() - 10;
#else
    return false;
#endif
}


bool ResList::scrollIsAtTop()
{
#if defined(USING_WEBKIT)
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
        ret = true;
    } else {
        int cur = frame->scrollBarValue(Qt::Vertical);
        int min = frame->scrollBarMinimum(Qt::Vertical);
        LOGDEB("Scrollattop: cur " << cur << " min " << min << "\n");
        ret = (cur == min);
    }
    LOGDEB2("scrollIsAtTop: returning " << ret << "\n");
    return ret;
#elif defined(USING_WEBENGINE)
    //return page()->scrollPosition().toPoint().ry() == 0;
    return m_scrollpos.y() == 0;
#else
    return false;
#endif
}


void ResList::setupArrows()
{
    emit prevPageAvailable(m_pager->hasPrev() || !scrollIsAtTop());
    emit nextPageAvailable(m_pager->hasNext() || !scrollIsAtBottom());
}


// Show previous page of results. We just set the current number back
// 2 pages and show next page.
void ResList::resultPageBack()
{
    if (m_pager->hasPrev()) {
        m_pager->resultPageBack();
        displayPage();
#ifdef USING_WEBENGINE
        runJS("window.scrollTo(0,0);");
#endif
    }
}

// Go to the first page
void ResList::resultPageFirst()
{
    // In case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->resultPageFirst();
    displayPage();
#ifdef USING_WEBENGINE
    runJS("window.scrollTo(0,0);");
#endif
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    if (m_pager->hasNext()) {
        m_pager->resultPageNext();
        displayPage();
#ifdef USING_WEBENGINE
        runJS("window.scrollTo(0,0);");
#endif
    }
}

void ResList::resultPageFor(int docnum)
{
    m_pager->resultPageFor(docnum);
    displayPage();
}

void ResList::append(const QString &text)
{
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    m_text += text;
    if (m_progress && text.startsWith("<div class=\"rclresult\"")) {
        m_progress->setValue(m_residx++);
    }
#else
    QTextBrowser::append(text);
#endif
}

const static QUrl baseUrl("file:///");
const static std::string sBaseUrl{"file:///"};

void ResList::displayPage()
{
#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)

    QProgressDialog progress("Generating text snippets...", "", 0, prefs.respagesize, this);
    m_residx = 0;
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(2000);
    m_progress = &progress;
    m_text = "";
#else
    clear();
    setFont();
#endif
    
    m_pager->displayPage(theconfig);

#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)
    // webengine from qt 5.15 on won't load local images if the base URL is
    // not set (previous versions worked with an empty one). Can't hurt anyway.
    if (m_progress) {
        m_progress->close();
        m_progress = nullptr;
    }
    if (m_lasttext == m_text)
        return;
    maybeDump(m_text);
    setHtml(m_text, baseUrl);
    m_lasttext = m_text;
#endif

    LOGDEB0("ResList::displayPg: hasNext " << m_pager->hasNext() <<
            " atBot " << scrollIsAtBottom() << " hasPrev " <<
            m_pager->hasPrev() << " at Top " << scrollIsAtTop() << " \n");
    QTimer::singleShot(100, this, SLOT(setupArrows()));

    // Possibly color paragraph of current preview if any
    previewExposed(m_curPvDoc);
}

// Color paragraph (if any) of currently visible preview
void ResList::previewExposed(int docnum)
{
    LOGDEB("ResList::previewExposed: doc " << docnum << "\n");

    // Possibly erase old one to white
    if (m_curPvDoc > -1) {
#if defined(USING_WEBKIT)
        QString sel = 
            QString("div[rcldocnum=\"%1\"]").arg(m_curPvDoc - pageFirstDocNum());
        LOGDEB2("Searching for element, selector: [" << qs2utf8s(sel) << "]\n");
        QWebElement elt = page()->mainFrame()->findFirstElement(sel);
        if (!elt.isNull()) {
            LOGDEB2("Found\n");
            elt.removeAttribute("style");
        } else {
            LOGDEB2("Not Found\n");
        }
#elif defined(USING_WEBENGINE)
        QString js = QString(
            "elt=document.getElementById('%1');"
            "if (elt){elt.removeAttribute('style');}"
            ).arg(m_curPvDoc - pageFirstDocNum());
        runJS(js);
#else
        std::pair<int,int> blockrange = parnumfromdocnum(m_curPvDoc);
        if (blockrange.first != -1) {
            for (int blockn = blockrange.first;
                 blockn < blockrange.second; blockn++) {
                QTextBlock block = document()->findBlockByNumber(blockn);
                QTextCursor cursor(block);
                QTextBlockFormat format = cursor.blockFormat();
                format.clearBackground();
                cursor.setBlockFormat(format);
            }
        }
#endif
        m_curPvDoc = -1;
    }

    if ((m_curPvDoc = docnum) < 0) {
        return;
    }

    // Set background for active preview's doc entry
    
#if defined(USING_WEBKIT)
    QString sel = QString("div[rcldocnum=\"%1\"]").arg(docnum - pageFirstDocNum());
    LOGDEB2("Searching for element, selector: [" << qs2utf8s(sel) << "]\n");
    QWebElement elt = page()->mainFrame()->findFirstElement(sel);
    if (!elt.isNull()) {
        LOGDEB2("Found\n");
        elt.setAttribute("style", "background: LightBlue;}");
    } else {
        LOGDEB2("Not Found\n");
    }
#elif defined(USING_WEBENGINE)
    QString js = QString(
        "elt=document.getElementById('%1');"
        "if(elt){elt.setAttribute('style', 'background: LightBlue');}"
        ).arg(docnum - pageFirstDocNum());
    runJS(js);
#else
    std::pair<int,int>  blockrange = parnumfromdocnum(docnum);

    // Maybe docnum is -1 or not in this window, 
    if (blockrange.first < 0)
        return;
    // Color the new active paragraph
    QColor color("LightBlue");
    for (int blockn = blockrange.first+1;
         blockn < blockrange.second; blockn++) {
        QTextBlock block = document()->findBlockByNumber(blockn);
        QTextCursor cursor(block);
        QTextBlockFormat format;
        format.setBackground(QBrush(color));
        cursor.mergeBlockFormat(format);
        setTextCursor(cursor);
        ensureCursorVisible();
    }
#endif
}

// Double click in res list: add selection to simple search
void ResList::mouseDoubleClickEvent(QMouseEvent *event)
{
    RESLIST_PARENTCLASS::mouseDoubleClickEvent(event);
#if defined(USING_WEBKIT) 
    emit wordSelect(selectedText());
#elif defined(USING_WEBENGINE)
    // webengineview does not have such an event function, and
    // reimplementing event() itself is not useful (tried) as it does
    // not get mouse clicks. We'd need javascript to do this, but it's
    // not that useful, so left aside for now.
#else
    if (textCursor().hasSelection())
        emit wordSelect(textCursor().selectedText());
#endif
}

void ResList::showQueryDetails()
{
    if (!m_source)
        return;
    string oq = breakIntoLines(m_source->getDescription(), 100, 50);
    QString str;
    QString desc = tr("Result count (est.)") + ": " + 
        str.setNum(m_source->getResCnt()) + "<br>";
    desc += tr("Query details") + ": " + QString::fromUtf8(oq.c_str());
    QMessageBox::information(this, tr("Query details"), desc);
}

#if defined(USING_WEBENGINE)
void ResList::onLinkClicked(const QUrl &qurl)
{
    QTimer::singleShot(0, this, [this, qurl] () {onLinkClicked1(qurl);});
}
#endif

#if defined(USING_WEBENGINE)
void ResList::onLinkClicked1(const QUrl &qurl)
#else
void ResList::onLinkClicked(const QUrl &qurl)
#endif
{
    string strurl = pc_decode(qs2utf8s(qurl.toString()));

    // Link prefix remark: it used to be that webengine refused to acknowledge link clicks on links
    // like "%P1", it needed an absolute URL like file:///recoll-links/P1. So, for a time this is
    // what the links looked like. However this broke the function of the "edit paragraph format"
    // thing because the manual still specified relative links (E%N). We now set baseUrl to file:///
    // to fix icons display which had stopped working). So linkprefix() is now empty, but the code
    // has been kept around. We now receive absolute links baseUrl+relative (e.g. file:///E1), and
    // substract the baseUrl.
    
#if defined(USING_WEBENGINE) || defined(USING_WEBKIT)
    auto prefix = m_pager->linkPrefix().size() ? m_pager->linkPrefix() : sBaseUrl;
#else
    std::string prefix;
#endif
    
    if (prefix.size() && (strurl.size() <= prefix.size() || !beginswith(strurl, prefix))) {
        LOGINF("ResList::onLinkClicked: bad URL [" << strurl << "] (prefix [" << prefix << "]\n");
        return;
    }
    if (prefix.size()) 
        strurl = strurl.substr(prefix.size());
    LOGDEB("ResList::onLinkClicked: processed URL: [" << strurl << "]\n");
    auto [what, docnum, origorscript, replacement] = internal_link(strurl);
    if (what == 0) {
        return;
    }
    bool havedoc{false};
    Rcl::Doc doc;
    if (docnum >= 0) {
        if (getDoc(docnum, doc)) {
            havedoc = true;
        } else {
            LOGERR("ResList::onLinkClicked: can't get doc for "<< docnum << "\n");
        }
    }

    switch (what) {
    case 'A': // Open abstract/snippets window
    { 
        if (!havedoc)
            return;
        emit showSnippets(doc);
    }
    break;
    case 'D': // Show duplicates
    {
        if (!m_source || !havedoc) 
            return;
        vector<Rcl::Doc> dups;
        if (m_source->docDups(doc, dups) && m_rclmain) {
            m_rclmain->newDupsW(doc, dups);
        }
    }
    break;
    case 'F': // Open parent folder
    {
        if (!havedoc)
            return;
        emit editRequested(ResultPopup::getFolder(doc));
    }
    break;
    case 'h': // Show query details
    case 'H':
    {
        showQueryDetails();
        break;
    }
    case 'P': // Preview and edit
    case 'E': 
    {
        if (!havedoc)
            return;
        if (what == 'P') {
            if (m_ismainres) {
                emit docPreviewClicked(docnum, doc, m_lstClckMod);
            } else {
                emit previewRequested(doc);
            }
        } else {
            emit editRequested(doc);
        }
    }
    break;
    case 'n': // Next/prev page
        resultPageNext();
        break;
    case 'p':
        resultPageBack();
        break;
    case 'R':// Run script. Link format Rnn|Script Name
    {
        if (!havedoc)
            return;
        LOGERR("Run script: [" << origorscript << "]\n");
        DesktopDb ddb(path_cat(theconfig->getConfDir(), "scripts"));
        DesktopDb::AppDef app;
        if (ddb.appByName(origorscript, app)) {
            QAction act(QString::fromUtf8(app.name.c_str()), this);
            QVariant v(QString::fromUtf8(app.command.c_str()));
            act.setData(v);
            m_popDoc = docnum;
            menuOpenWith(&act);
        }
    }
    break;
    case 'S': // Spelling: replacement suggestion clicked. strurl is like:
        // S-1|orograpphic|orographic
    {
        if (origorscript.empty() || replacement.empty())
            return;
        LOGDEB2("Emitting wordreplace " << origorscript << " -> " << replacement << "\n");
        emit wordReplace(u8s2qs(origorscript), u8s2qs(replacement));
    }
    break;
    default: 
        LOGERR("ResList::onLinkClicked: bad link [" << strurl.substr(0,20) << "]\n");
        break;
    }
}

void ResList::onPopupJsDone(const QVariant &jr)
{
    QString qs(jr.toString());
    LOGDEB("onPopupJsDone: parameter: " << qs2utf8s(qs) << "\n");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    auto skipflags = Qt::SkipEmptyParts;
#else
    auto skipflags = QString::SkipEmptyParts;
#endif
    QStringList qsl = qs.split("\n", skipflags);
    for (int i = 0 ; i < qsl.size(); i++) {
        int eq = qsl[i].indexOf("=");
        if (eq > 0) {
            QString nm = qsl[i].left(eq).trimmed();
            QString value = qsl[i].right(qsl[i].size() - (eq+1)).trimmed();
            if (!nm.compare("rcldocnum")) {
                m_popDoc = pageFirstDocNum() + value.toInt();
            } else {
                LOGERR("onPopupJsDone: unknown key: " << qs2utf8s(nm) << "\n");
            }
        }
    }
    doCreatePopupMenu();
}

void ResList::createPopupMenu(const QPoint& pos)
{
    LOGDEB("ResList::createPopupMenu(" << pos.x() << ", " << pos.y() << ")\n");
    m_popDoc = -1;
    m_popPos = pos;
#if defined(USING_WEBKIT)
    QWebHitTestResult htr = page()->mainFrame()->hitTestContent(pos);
    if (htr.isNull())
        return;
    QWebElement el = htr.enclosingBlockElement();
    while (!el.isNull() && !el.hasAttribute("rcldocnum"))
        el = el.parent();
    if (el.isNull())
        return;
    QString snum = el.attribute("rcldocnum");
    m_popDoc = pageFirstDocNum() + snum.toInt();
#elif defined(USING_WEBENGINE)
    QString js("window.locDetails;");
    RclWebPage *mypage = dynamic_cast<RclWebPage*>(page());
    mypage->runJavaScript(js, [this](const QVariant &v) {onPopupJsDone(v);});
#else
    QTextCursor cursor = cursorForPosition(pos);
    int blocknum = cursor.blockNumber();
    LOGDEB("ResList::createPopupMenu(): block " << blocknum << "\n");
    m_popDoc = docnumfromparnum(blocknum);
#endif
    doCreatePopupMenu();
}

void ResList::doCreatePopupMenu()
{
    if (m_popDoc < 0) 
        return;
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc))
        return;

    int options =  ResultPopup::showSaveOne;
    if (m_ismainres)
        options |= ResultPopup::isMain;
    QMenu *popup = ResultPopup::create(this, options, m_source, doc);
    popup->popup(mapToGlobal(m_popPos));
}

void ResList::menuPreview()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
        if (m_ismainres) {
            emit docPreviewClicked(m_popDoc, doc, 0);
        } else {
            emit previewRequested(doc);
        }
    }
}

void ResList::menuSaveToFile()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit docSaveToFileClicked(doc);
}

void ResList::menuPreviewParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source)  {
        Rcl::Doc pdoc = ResultPopup::getParent(m_source, doc);
        if (pdoc.mimetype == "inode/directory") {
            emit editRequested(pdoc);
        } else {
            emit previewRequested(pdoc);
        }
    }
}

void ResList::menuOpenParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source)  {
        Rcl::Doc pdoc = ResultPopup::getParent(m_source, doc);
        if (!pdoc.url.empty()) {
            emit editRequested(pdoc);
        }
    }
}

void ResList::menuOpenFolder()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && m_source) {
        Rcl::Doc pdoc = ResultPopup::getFolder(doc);
        if (!pdoc.url.empty()) {
            emit editRequested(pdoc);
        }
    }
}

void ResList::menuShowSnippets()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit showSnippets(doc);
}

void ResList::menuShowSubDocs()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) 
        emit showSubDocs(doc);
}

void ResList::menuEdit()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit editRequested(doc);
}
void ResList::menuOpenWith(QAction *act)
{
    if (act == 0)
        return;
    string cmd = qs2utf8s(act->data().toString());
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit openWithRequested(doc, cmd);
}

void ResList::menuCopyFN()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyFN(doc);
}

void ResList::menuCopyPath()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyPath(doc);
}

void ResList::menuCopyText()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyText(doc, m_rclmain);
}

void ResList::menuCopyURL()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        ResultPopup::copyURL(doc);
}

void ResList::menuExpand()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
        emit docExpand(doc);
}

int ResList::pageFirstDocNum()
{
    return m_pager->pageFirstDocNum();
}
