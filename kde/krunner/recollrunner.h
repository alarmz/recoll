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

#pragma once

#include <memory>
#include <mutex>

#include <krunner_version.h>
#include <KRunner/AbstractRunner>
#if KRUNNER_VERSION_MAJOR >= 6
#define KRUNNS KRunner
#else
#define KRUNNS Plasma
#endif

class RclConfig;
namespace Rcl {
class Db;
}

class RecollRunner : public KRUNNS::AbstractRunner
{
    Q_OBJECT

public:
#if KRUNNER_VERSION_MAJOR >= 6
    RecollRunner(QObject *parent, const KPluginMetaData &data);
#else
    RecollRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
#endif

    void match(KRUNNS::RunnerContext &context) override;
    void run(const KRUNNS::RunnerContext &context, const KRUNNS::QueryMatch &match) override;
    void reloadConfiguration() override;

protected:
    void init() override;

private:
    std::mutex m_mutex;
    QString m_triggerWord;
    RclConfig *m_rclconfig{nullptr};
    Rcl::Db *m_rcldb{nullptr};
    std::string m_reason;
    // english by default else env[RECOLL_KIO_STEMLANG]
    std::string m_stemlang{"english"};
    bool m_showSubdocs{false};
    bool m_initok{false};
};
