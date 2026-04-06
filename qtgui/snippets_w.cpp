/* Copyright (C) 2012-2021 J.F.Dockes
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

#include <string>
#include <vector>
#include <sstream>

#if defined(USING_WEBKIT)
#  include <QWebSettings>
#  include <QWebFrame>
#  include <QUrl>
#  define QWEBSETTINGS QWebSettings
#  define QWEBPAGE QWebPage
#elif defined(USING_WEBENGINE)
// Notes for WebEngine
// - All links must begin with http:// for acceptNavigationRequest to be
//   called. 
// - The links passed to acceptNav.. have the host part 
//   lowercased -> we change S0 to http://h/S0, not http://S0
#  include <QWebEnginePage>
#  include <QWebEngineSettings>
#  include <QtWebEngineWidgets>
#  define QWEBSETTINGS QWebEngineSettings
#  define QWEBPAGE QWebEnginePage
#else
#include <QTextBrowser>
#endif

#include <QShortcut>

#include "log.h"
#include "recoll.h"
#include "snippets_w.h"
#include "guiutils.h"
#include "rcldb.h"
#include "rclhelp.h"
#include "plaintorich.h"
#include "scbase.h"
#include "readfile.h"

using namespace std;

#if defined(USING_WEBKIT)
#define browser ((QWebView*)browserw)
#elif defined(USING_WEBENGINE)
#define browser ((QWebEngineView*)browserw)
#else
#define browser ((QTextBrowser*)browserw)
#endif

class PlainToRichQtSnippets : public PlainToRich {
public:
    virtual string startMatch(unsigned int) {
        return string("<span class='rclmatch' style='") + qs2utf8s(prefs.qtermstyle) + string("'>");
    }
    virtual string endMatch() {
        return string("</span>");
    }
};
static PlainToRichQtSnippets g_hiliter;

void SnippetsW::init()
{
    m_sortingByPage = prefs.snipwSortByPage;
    QPushButton *searchButton = new QPushButton(tr("Search"));
    searchButton->setAutoDefault(false);
    buttonBox->addButton(searchButton, QDialogButtonBox::ActionRole);
    searchFM->hide();

    onNewShortcuts();
    connect(&SCBase::scBase(), SIGNAL(shortcutsChanged()), this, SLOT(onNewShortcuts()));

    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    if (closeButton)
        connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(searchButton, SIGNAL(clicked()), this, SLOT(slotEditFind()));
    connect(searchLE, SIGNAL(textChanged(const QString&)), 
            this, SLOT(slotSearchTextChanged(const QString&)));
    connect(nextPB, SIGNAL(clicked()), this, SLOT(slotEditFindNext()));
    connect(prevPB, SIGNAL(clicked()), this, SLOT(slotEditFindPrevious()));


    // Get rid of the placeholder widget created from the .ui
    delete browserw;
#if defined(USING_WEBKIT)
    browserw = new QWebView(this);
    verticalLayout->insertWidget(0, browserw);
    browser->setUrl(QUrl(QString::fromUtf8("about:blank")));
    connect(browser, SIGNAL(linkClicked(const QUrl &)), this, SLOT(onLinkClicked(const QUrl &)));
    browser->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    browser->page()->currentFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    browserw->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(browserw, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
#elif defined(USING_WEBENGINE)
    browserw = new QWebEngineView(this);
    verticalLayout->insertWidget(0, browserw);
    browser->setPage(new SnipWebPage(this));
    // Stylesheet TBD
    browserw->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(browserw, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(createPopupMenu(const QPoint&)));
#else
    browserw = new QTextBrowser(this);
    verticalLayout->insertWidget(0, browserw);
    connect(browser, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(onLinkClicked(const QUrl &)));
    browser->setReadOnly(true);
    browser->setUndoRedoEnabled(false);
    browser->setOpenLinks(false);
    browser->setTabChangesFocus(true);
    if (prefs.reslistfontfamily != "") {
        QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
        browser->setFont(nfont);
    } else {
        browser->setFont(QFont());
    }
#endif
}

void SnippetsW::onNewShortcuts()
{
    SETSHORTCUT(this, "snippets:156", tr("Snippets Window"), tr("Find"),
                "Ctrl+F", m_find1sc, slotEditFind);
    SETSHORTCUT(this, "snippets:158", tr("Snippets Window"), tr("Find (alt)"),
                "/", m_find2sc, slotEditFind);
    SETSHORTCUT(this, "snippets:160", tr("Snippets Window"), tr("Find next"),
                "F3", m_findnextsc, slotEditFindNext);
    SETSHORTCUT(this, "snippets:162", tr("Snippets Window"), tr("Find previous"),
                "Shift+F3", m_findprevsc, slotEditFindPrevious);
    SETSHORTCUT(this, "snippets:164", tr("Snippets Window"), tr("Close window"),
                "Esc", m_hidesc, hide);
    auto sseq = QKeySequence(QKeySequence::ZoomIn).toString();
    SETSHORTCUT(this, "snippets:166", tr("Snippets Window"), tr("Increase font size"),
                sseq, m_zisc, slotZoomIn);
    sseq = QKeySequence(QKeySequence::ZoomOut).toString();
    SETSHORTCUT(this, "snippets:168", tr("Snippets Window"), tr("Decrease font size"),
                sseq, m_zosc, slotZoomOut);
}

void SnippetsW::listShortcuts()
{
    LISTSHORTCUT(this, "snippets:156", tr("Snippets Window"), tr("Find"),
                 "Ctrl+F", m_find1sc, slotEditFind);
    LISTSHORTCUT(this, "snippets:158", tr("Snippets Window"), tr("Find (alt)"),
                 "/", m_find2sc, slotEditFind);
    LISTSHORTCUT(this, "snippets:160",tr("Snippets Window"), tr("Find next"),
                 "F3", m_find2sc, slotEditFindNext);
    LISTSHORTCUT(this, "snippets:162",tr("Snippets Window"), tr("Find previous"),
                 "Shift+F3", m_find2sc, slotEditFindPrevious);
    LISTSHORTCUT(this, "snippets:164", tr("Snippets Window"), tr("Close window"),
                 "Esc", m_hidesc, hide);
    auto sseq = QKeySequence(QKeySequence::ZoomIn).toString();
    LISTSHORTCUT(this, "snippets:166", tr("Snippets Window"), tr("Increase font size"),
                sseq, m_zisc, slotZoomIn);
    sseq = QKeySequence(QKeySequence::ZoomOut).toString();
    LISTSHORTCUT(this, "snippets:168", tr("Snippets Window"), tr("Decrease font size"),
                sseq, m_zosc, slotZoomOut);
}

void SnippetsW::createPopupMenu(const QPoint& pos)
{
    QMenu *popup = new QMenu(this);
    if (m_sortingByPage) {
        popup->addAction(tr("Sort By Relevance"), this, SLOT(reloadByRelevance()));
    } else {
        popup->addAction(tr("Sort By Page"), this, SLOT(reloadByPage()));
    }
    popup->popup(mapToGlobal(pos));
}

void SnippetsW::reloadByRelevance()
{
    m_sortingByPage = false;
    onSetDoc(m_doc, m_source);
}
void SnippetsW::reloadByPage()
{
    m_sortingByPage = true;
    onSetDoc(m_doc, m_source);
}

void SnippetsW::onSetDoc(Rcl::Doc doc, std::shared_ptr<DocSequence> source)
{
    m_doc = doc;
    m_source = source;
    if (!source)
        return;

    // Make title out of file name if none yet
    string titleOrFilename;
    string utf8fn;
    m_doc.getmeta(Rcl::Doc::keytt, &titleOrFilename);
    m_doc.getmeta(Rcl::Doc::keyfn, &utf8fn);
    if (titleOrFilename.empty()) {
        titleOrFilename = utf8fn;
    }
    QString title("Recoll - Snippets");
    if (!titleOrFilename.empty()) {
        title += QString(" : ") + QString::fromUtf8(titleOrFilename.c_str());
    }
    setWindowTitle(title);

    vector<Rcl::Snippet> vpabs;
    source->getAbstract(m_doc, &g_hiliter, vpabs, prefs.snipwMaxLength, m_sortingByPage);

    std::string snipcss = prefs.snipCSS();
    ostringstream oss;
    oss << "<html><head>\n"
        "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\n";
    oss << prefs.htmlHeaderContents() << snipcss;
    oss << "\n</head>\n<body>\n<table class=\"snippets\">";

    bool nomatch = true;

    for (const auto& snippet : vpabs) {
        if (snippet.page == -1) {
            oss << "<tr><td colspan=\"2\">" << snippet.snippet << "</td></tr>" << "\n";
            continue;
        }
        nomatch = false;
        oss << "<tr><td>";
        if (snippet.page > 0) {
            oss << "<a href=\"http://h/P" << snippet.page << "T" << snippet.term << "\">" <<
                "P.&nbsp;" << snippet.page << "</a>";
        } else if (snippet.line > 0) {
            oss << "<a href=\"http://h/L" << snippet.line << "T" << snippet.term << "\">" <<
                "L.&nbsp;" << snippet.line << "</a>";
        }
        oss << "</td><td>" << snippet.snippet << "</td></tr>" << "\n";
    }
    oss << "</table>" << "\n";
    if (nomatch) {
        oss.str("<html><head></head><body>\n");
        oss << qs2utf8s(tr("<p>Sorry, no exact match was found within limits. "
                           "Probably the document is very big and the snippets "
                           "generator got lost in a maze...</p>"));
    }
    oss << "\n</body></html>";
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->setHtml(u8s2qs(oss.str()));
#else
    browser->clear();
    browser->append(".");
    browser->clear();
    browser->insertHtml(u8s2qs(oss.str()));
    browser->moveCursor (QTextCursor::Start);
    browser->ensureCursorVisible();
#endif
    raise();
}

void SnippetsW::slotEditFind()
{
    searchFM->show();
    searchLE->selectAll();
    searchLE->setFocus();
}

void SnippetsW::slotEditFindNext()
{
    if (!searchFM->isVisible())
        slotEditFind();

#if defined(USING_WEBKIT)  || defined(USING_WEBENGINE)
    browser->findText(searchLE->text());
#else
    browser->find(searchLE->text());
#endif

}

void SnippetsW::slotEditFindPrevious()
{
    if (!searchFM->isVisible())
        slotEditFind();

#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->findText(searchLE->text(), QWEBPAGE::FindBackward);
#else
    browser->find(searchLE->text(), QTextDocument::FindBackward);
#endif
}

void SnippetsW::onUiPrefsChanged()
{
    if (m_sortingByPage) {
        reloadByPage();
    } else {
        reloadByRelevance();
    }
}

void SnippetsW::slotSearchTextChanged(const QString& txt)
{
#if defined(USING_WEBKIT) || defined(USING_WEBENGINE)
    browser->findText(txt);
#else
    // Cursor thing is so that we don't go to the next occurrence with
    // each character, but rather try to extend the current match
    QTextCursor cursor = browser->textCursor();
    cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
    browser->setTextCursor(cursor);
    browser->find(txt);
#endif
}

void SnippetsW::slotZoomIn()
{
    emit zoomIn();
}
void SnippetsW::slotZoomOut()
{
    emit zoomOut();
}

void SnippetsW::onLinkClicked(const QUrl &url)
{
    string ascurl = qs2u8s(url.toString()).substr(9);
    LOGDEB("Snippets::onLinkClicked: [" << ascurl << "]\n");

    if (ascurl.size() > 3) {
        int what = ascurl[0];
        switch (what) {
        case 'P':
        case 'L':
        {
            string::size_type numpos = ascurl.find_first_of("0123456789");
            if (numpos == string::npos)
                return;
            int page = -1, line = -1;
            if (what == 'P') {
                page = atoi(ascurl.c_str() + numpos);
            } else {
                line = atoi(ascurl.c_str() + numpos);
            }
            string::size_type termpos = ascurl.find_first_of("T");
            string term;
            if (termpos != string::npos)
                term = ascurl.substr(termpos+1);
            emit startNativeViewer(m_doc, page, u8s2qs(term), line);
            return;
        }
        }
    }
    LOGERR("Snippets::onLinkClicked: bad link [" << ascurl << "]\n");
}
