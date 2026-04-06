/* Copyright (C) 2005-2025 J.F.Dockes
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

// For choosing the preview draw engine
#include "autoconfig.h"

#include "preview_w.h"

#include <string>

#include "log.h"
#include "recoll.h"
#include "preview_plaintorich.h"
#include "guiutils.h"
#include "rclwebpage.h"
#include "chrono.h"

// Make an attempt at trimming wildcard exprs at both ends of string
static void trimwildcards(std::string& elt)
{
    if (elt.empty())
        return;
    std::string::size_type initsize;
    do {
        initsize = elt.size();
        // Trim wildcard chars. 
        trimstring(elt, " *?");
        // Trim wildcard char classes 
        if (elt.size() && elt.back() == ']') {
            std::string::size_type offs = elt.find_last_of("[");
            if (offs != std::string::npos) {
                elt = elt.substr(0, offs);
                if (elt.size() && elt.back() == '[') {
                    elt.erase(elt.end()-1);
                }
            }
        }
        if (elt.size() && elt.front() == '[') {
            std::string::size_type offs = elt.find_first_of("]");
            if (offs != std::string::npos) {
                elt.erase(0, offs+1);
            }
        }
    } while (elt.size() && elt.size() != initsize);
}

void Preview::initSTermWalk()
{
    for (const auto& ugroup : m_hData.ugroups) {
        QString s;
        for (auto elt : ugroup) {
            trimwildcards(elt);
            if (!elt.empty()) {
                if (!s.isEmpty()) {
                    s.append(" ");
                }
                s.append(u8s2qs(elt));
            }
        }
        s = s.trimmed();
        searchTextCMB->addItem(s);
    }
    searchTextCMB->setCompleter(0);
}


void Preview::searchTextChanged(const QString & text)
{
    LOGDEB("Preview::searchTextChanged:(" << qs2utf8s(text) << ") current: ("<<
           qs2utf8s(searchTextCMB->currentText()) << ") currentindex " <<
           searchTextCMB->currentIndex() << "\n");
    if (!searchTextCMB->itemText(searchTextCMB->currentIndex()).compare(text)) {
        // Then we assume that the text was set by selecting in the
        // combobox There does not seem to be another way to
        // discriminate select and hand edit. Note that the
        // activated() signal is called *after* the editTextChanged()
        // one, so it is useless.
        m_searchTextFromIndex = searchTextCMB->currentIndex();
        doSearch("", false, false);
    } else {
        m_searchTextFromIndex = -1;
        if (text.isEmpty()) {
            m_dynSearchActive = false;
            clearPB->setEnabled(false);
        } else {
            m_dynSearchActive = true;
            clearPB->setEnabled(true);
            doSearch(text, false, false);
        }
    }
}


void Preview::walkAnchors(PreviewTextEdit *edit, bool reverse)
{
    if (!edit->m_plaintorich->haveAnchors()) {
        LOGDEB("NO ANCHORS\n");
        return;
    }
    // The combobox indices are equal to the search ugroup indices in hldata, that's how we built
    // the list.
    int anchornum;
    if (reverse) {
        anchornum = edit->m_plaintorich->prevAnchorNum(m_searchTextFromIndex);
    } else {
        anchornum = edit->m_plaintorich->nextAnchorNum(m_searchTextFromIndex);
    }
    auto totanchors = edit->m_plaintorich->anchorCount(m_searchTextFromIndex);
    QString txt = QString("%1/%2").arg(anchornum).arg(totanchors);
    hitIndicatorLBL->setText(txt);
    QString aname = edit->m_plaintorich->curAnchorName();

#ifdef PREVIEW_TEXTBROWSER
    LOGDEB("Calling scrollToAnchor(" << qs2utf8s(aname) << ")\n");
    edit->scrollToAnchor(aname);
    // Position the cursor at the anchor (top of viewport) so that searches start from here
    QTextCursor cursor = edit->cursorForPosition(QPoint(0, 0));
    edit->setTextCursor(cursor);
#else
    LOGDEB1("Highlighting anchor name " << qs2utf8s(aname) << "\n");
    std::string sjs = R"-(
            var elements = document.getElementsByClassName('rclhighlight');
            for (let i = 0; i < elements.length; i++) {
                elements.item(i).classList.remove('rclhighlight');
            }
            var element  = document.getElementById('%1');
            if (element) {
                element.classList.add('rclhighlight');
                element.scrollIntoView();
                var style = window.getComputedStyle(element, null).getPropertyValue('font-size');
                var fontSize = parseFloat(style);
                window.scrollBy(0,-%2 * fontSize);
            }
        )-";
    QString js = QString::fromUtf8(sjs.c_str()).arg(aname).arg(prefs.previewLinesOverAnchor);

    LOGDEB2("Running JS: " << qs2utf8s(js) << "\n");
#if defined(PREVIEW_WEBKIT)
    edit->page()->mainFrame()->evaluateJavaScript(js);
#elif defined(PREVIEW_WEBENGINE)
    edit->page()->runJavaScript(js);
#endif

#endif // !TEXTBROWSER
}

// Perform text search. If next is true, we look for the next match of the current search, trying to
// advance and possibly wrapping around. If next is false, the search string has been modified, we
// search for the new string, starting from the current position
void Preview::doSearch(const QString &_text, bool next, bool reverse, bool wordOnly)
{
    LOGDEB0("Preview::doSearch: text [" << qs2utf8s(_text) << "] idx " << m_searchTextFromIndex <<
           " next " << next << " rev " << reverse << " word " << wordOnly << "\n");

    bool matchCase = casematchCB->isChecked();
    PreviewTextEdit *edit = currentEditor();
    if (edit == 0) {
        LOGERR("Preview::doSearch: no current editor\n");
        // ??
        return;
    }
    QString text = _text;

    // Are we walking hit lists ?
    if (text.isEmpty() || m_searchTextFromIndex != -1) {
        walkAnchors(edit, reverse);
        return;
    }

    // Performing incremental text search
    hitIndicatorLBL->clear();

#ifdef PREVIEW_TEXTBROWSER
    // If next is false, the user added characters to the current search string.  We need to reset
    // the cursor position to the start of the previous match, else incremental search is going to
    // look for the next occurrence instead of trying to lenghten the current match
    if (!next) {
        QTextCursor cursor = edit->textCursor();
        cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
        edit->setTextCursor(cursor);
    }
    Chrono chron;
    LOGDEB("Preview::doSearch: first find call\n");
    // FindFlags is a QFlags class with default constructor to empty.
    QTextDocument::FindFlags flags;
    if (reverse)
        flags |= QTextDocument::FindBackward;
    if (wordOnly)
        flags |= QTextDocument::FindWholeWords;
    if (matchCase)
        flags |= QTextDocument::FindCaseSensitively;
    bool found = edit->find(text, flags);
    LOGDEB("Preview::doSearch: first find call return: found " << found <<
           " " << chron.secs() << " S\n");
    // If not found, try to wrap around. 
    if (!found) { 
        LOGDEB("Preview::doSearch: wrapping around\n");
        if (reverse) {
            edit->moveCursor (QTextCursor::End);
        } else {
            edit->moveCursor (QTextCursor::Start);
        }
        LOGDEB("Preview::doSearch: 2nd find call\n");
        chron.restart();
        found = edit->find(text, flags);
        LOGDEB("Preview::doSearch: 2nd find call return found " << found <<
               " " << chron.secs() << " S\n");
    }

    if (found) {
        m_canBeep = true;
    } else {
        if (m_canBeep && !prefs.noBeeps)
            QApplication::beep();
        m_canBeep = false;
    }
#else
    WEBPAGE::FindFlags flags;
    if (reverse)
        flags |= WEBPAGE::FindBackward;
    if (matchCase)
        flags |= WEBPAGE::FindCaseSensitively;
    edit->findText(text, flags);
#endif // !TEXTBROWSER
    LOGDEB("Preview::doSearch: return\n");
}

void Preview::nextPressed()
{
    LOGDEB2("Preview::nextPressed\n");
    doSearch(searchTextCMB->currentText(), true, false);
}

void Preview::prevPressed()
{
    LOGDEB2("Preview::prevPressed\n");
    doSearch(searchTextCMB->currentText(), true, true);
}
