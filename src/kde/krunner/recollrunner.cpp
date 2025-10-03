/* Copyright (C) 2022 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "recollrunner.h"

#include <string>
#include <iostream>

#include <KIO/OpenUrlJob>
#include <klocalizedstring.h>
#include <QIcon>
#include <QMimeDatabase>
#if KRUNNER_VERSION_MAJOR >= 6
#include <KConfigGroup>
#endif

#include "rclconfig.h"
#include "rclinit.h"
#include "rclquery.h"
#include "wasatorcl.h"
#include "rcldb.h"
#include "rcldoc.h"
#include "searchdata.h"
#include "log.h"

QMimeDatabase mimeDb;

inline std::string qs2utf8s(const QString& qs)
{
    return std::string(qs.toUtf8().constData());
}
inline QString u8s2qs(const std::string& us)
{
    return QString::fromUtf8(us.c_str());
}
inline QString path2qs(const std::string& us)
{
    return QString::fromLocal8Bit(us.c_str());
}
inline std::string qs2path(const QString& qs)
{
    return qs.toLocal8Bit().constData();
}

#if KRUNNER_VERSION_MAJOR >= 6
RecollRunner::RecollRunner(QObject *parent, const KPluginMetaData &data)
    : AbstractRunner(parent, data)
#else
RecollRunner::RecollRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : AbstractRunner(parent, data, args)
#endif
{
#if KRUNNER_VERSION_MAJOR < 6
    setPriority(LowPriority);
#endif
}

void RecollRunner::init()
{
    reloadConfiguration();
    connect(this, &KRUNNS::AbstractRunner::prepare, this, [this]() {
        // Initialize data for the match session. This gets called from the main thread
        RclConfig *m_rclconfig = recollinit(0, nullptr, nullptr, m_reason);
        if (nullptr == m_rclconfig) {
            LOGERR("RecollRunner: Could not open recoll configuration\n");
            return;
        }
        m_rclconfig->getConfParam("kioshowsubdocs", &m_showSubdocs);
        m_rcldb = new Rcl::Db(m_rclconfig);
        if (nullptr == m_rcldb) {
            LOGERR("RecollRunner: Could not build database object. (out of memory ?)");
            return;
        }
        if (!m_rcldb->open(Rcl::Db::DbRO)) {
            LOGERR("RecollRunner: Could not open index in " + m_rclconfig->getDbDir() << "\n");
            return;
        }
        char *cp = getenv("RECOLL_KIO_STEMLANG");
        if (cp) {
            m_stemlang = cp;
        } else {
            m_stemlang = "english";
        }
        //Logger::getTheLog("")->setLogLevel(Logger::LLDEB);
        m_initok = true;
    });
    connect(this, &KRUNNS::AbstractRunner::teardown, this, [this]() {
        // Cleanup data from the match session. This gets called from the main thread
        delete m_rcldb;
        delete m_rclconfig;
    });
}

void RecollRunner::match(KRUNNS::RunnerContext &context)
{
    std::unique_lock<std::mutex> lockit(m_mutex);
    
    QString query = context.query();
    LOGDEB0("RecollRunner::match: input query: " << qs2utf8s(query) << "\n");
    if (query == QLatin1Char('.') || query == QLatin1String("..")) {
        return;
    }
    // This should not get in the way of the Help-Runner which gets triggered by
    // queries starting with '?'
    if (query.startsWith(QLatin1Char('?'))) {
        return;
    }

    if (!m_triggerWord.isEmpty()) {
        if (!query.startsWith(m_triggerWord)) {
            return;
        }
        query.remove(0, m_triggerWord.length());
    }

//    if (query.length() > 3) {
//        query.append(QLatin1Char('*'));
//    }
   
    if (!context.isValid()) {
        LOGERR("RecollRunner::match: context not valid\n");
        return;
    }

    std::string qs = qs2utf8s(query);
    LOGDEB0("RecollRunner::match: recoll query: " << qs2utf8s(query) << "\n");
    std::shared_ptr<Rcl::SearchData> sdata = wasaStringToRcl(m_rclconfig, m_stemlang, qs, m_reason);
    if (!sdata) {
        LOGINF("RecollRunner::match: wasaStringToRcl failed for [" << qs << "]\n");
        return;
    }
    sdata->setSubSpec(m_showSubdocs ? Rcl::SearchData::SUBDOC_ANY: Rcl::SearchData::SUBDOC_NO);
    std::unique_ptr<Rcl::Query> rclq = std::make_unique<Rcl::Query>(m_rcldb);
    if (!rclq->setQuery(sdata)) {
        LOGERR("RecollRunner::match: setquery failed\n");
        m_reason = "Query execute failed. Invalid query or syntax error?";
        return;
    }
    // int cnt = rclq->getResCnt();std::cerr << "RecollRunner::match: got " << cnt << " results\n";
    QList<KRUNNS::QueryMatch> matches;
    int i = 0;
    for (;i < 50;i++) {
        Rcl::Doc doc;
        if (!rclq->getDoc(i, doc, false)) {
            break;
        }
        KRUNNS::QueryMatch match(this);
        std::string title;
        if (!doc.getmeta(Rcl::Doc::keytt, &title) || title.empty()) {
            doc.getmeta(Rcl::Doc::keyfn, &title);
        }
        match.setText(u8s2qs(title));
        match.setData(path2qs(fileurltolocalpath(doc.url)));
        match.setId(path2qs(doc.url));
        QIcon icon = QIcon::fromTheme(mimeDb.mimeTypeForFile(match.data().toString()).iconName());
        match.setIcon(icon);

        match.setRelevance(doc.pc/100.0);

        // Could also call context.addMatch() for each match instead of buffering
        matches.append(match);
    }
    context.addMatches(matches);
}

void RecollRunner::run(const KRUNNS::RunnerContext & /*context*/, const KRUNNS::QueryMatch &match)
{
    // KIO::OpenUrlJob autodeletes itself, so we can just create it and forget it!
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(match.data().toString()));
//    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    job->setRunExecutables(false);
    job->start();
}

void RecollRunner::reloadConfiguration()
{
    KConfigGroup c = config();
    m_triggerWord = c.readEntry("trigger", QString());
    if (!m_triggerWord.isEmpty()) {
        m_triggerWord.append(QLatin1Char(' '));
    }
    QString m_path;
#ifdef theEXAMPLE
    m_path = c.readPathEntry("path", QDir::homePath());
    QFileInfo pathInfo(m_path);
    if (!pathInfo.isDir()) {
        m_path = QDir::homePath();
    }
#endif
    
    QList<KRUNNS::RunnerSyntax> syntaxes;
    KRUNNS::RunnerSyntax syntax(QStringLiteral("%1:q:").arg(m_triggerWord),
                                i18n("Finds files matching :q: in the %1 folder", m_path));
    syntaxes.append(syntax);
    setSyntaxes(syntaxes);
}

K_PLUGIN_CLASS_WITH_JSON(RecollRunner, "recollrunner.json")
#include "recollrunner.moc"
