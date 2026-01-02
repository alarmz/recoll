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

#include "docseqsem.h"

#ifdef ENABLE_SEMANTIC
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

#include <json/json.h>

#include "log.h"
#include "cmdtalk.h"
#include "rclconfig.h"
#include "rcldb.h"
#include "pathut.h"

static CmdTalk cmd(10);

// Start our worker process if it is not already up. The process is kept up and reused for further
// queries.
static bool maybeStartCmd(const RclConfig *conf)
{
    if (cmd.running())
        return true;

    std::string venvdir;
    if (!conf->getConfParam("sem_venv", venvdir)) {
        LOGERR("maybeStartCmd: 'sem_venv' not set in configuration\n");
        return false;
    }

    // We run the script with an explicit python command so that it runs in the venv without having
    // to deal with the activate script.
    auto cmdname = path_cat(venvdir, {"bin", "python3"});
    std::vector<std::string> args{path_cat(venvdir, "rclsem_talk.py")};
    if (!cmd.startCmd(cmdname, args)) {
        LOGERR("startCmd failed \n");
        return false;
    }
    return cmd.running();
}

// Holder for one result
struct Result {
    std::string rcludi;
    int phridx;
    std::string ctxbefore;
    std::string segment;
    std::string ctxafter;
};

class DocSequenceSem::Internal {
public:
    std::shared_ptr<Rcl::Db> db;
    // This is set to false after we call runquery from the first getDoc() or getResCnt()
    bool needquery{true};
    std::string question;
    std::vector<Result> results;

    bool mayberunquery();
};

bool DocSequenceSem::Internal::mayberunquery()
{
    if (!needquery)
        return true;
    needquery = false;

    if (!maybeStartCmd(db->getConf())) {
        LOGERR("DocSequenceSem:: could not start command\n");
        return false;
    }

    // Call the worker "query" method with the expected args
    std::unordered_map<std::string, std::string> response;
    std::unordered_map<std::string, std::string> args {
        {"confdir", db->getConf()->getConfDir()},
        {"nres", std::to_string(10)},
        {"question", question}};
    if (!cmd.callproc("query", args, response)) {
        LOGERR("getDoc: callproc 'query' failed\n");
        return false;
    }
    // We get the answer as json in the "results" key.
    auto it = response.find("results");
    if (it == response.end()) {
        LOGERR("getDoc: no 'results' in command response\n");
        return false;
    }
    // Do the jsoncpp thing and store the results
    Json::Value decoded;
    std::istringstream input(it->second);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::String errs;
    if (!Json::parseFromStream(builder, input, &decoded, &errs)) {
        LOGERR("Could not parse JSON: " << it->second << "\n");
        return false;
    }
    // The data is a sequence of tuples (rcludi, phridx, ctxbefore, segment, ctxafter)
    for (unsigned int i = 0; i < decoded.size(); i++) {
        Json::Value& decod_i = decoded[i];
        results.push_back({decod_i[0].asString(),
                             atoi(decod_i[1].asString().c_str()),
                             decod_i[2].asString(),
                             decod_i[3].asString(),
                             decod_i[4].asString()});
        // TBD FIX THIS !
        if (results.back().rcludi.find('|') == std::string::npos)
            results.back().rcludi += '|';
    }

    return true;
}

DocSequenceSem::DocSequenceSem(const std::string& t, std::shared_ptr<Rcl::Db> db,
                               const std::string& question)
    : DocSequence(t)
{
    LOGDEB0("DocSequenceSem:: title " << t << " question " << question << '\n');
    m = new Internal;
    m->db = db;
    m->question = question;
}

DocSequenceSem::~DocSequenceSem()
{
    delete m;
}

std::shared_ptr<Rcl::Db> DocSequenceSem::getDb()
{
    return m->db;
}

bool DocSequenceSem::getDoc(int num, Rcl::Doc &doc, std::string *)
{
    LOGDEB0("DocSequenceSem::getDoc: idx " << num << '\n');
    if (!m->mayberunquery()) {
        LOGERR("DocSequenceSem::getDoc: could not run query\n");
        return false;
    }
    if (num < 0 || num >= (int)m->results.size()) {
        LOGERR("DocSequenceSem::getDoc: bad idx " << num << " results size " <<
               m->results.size() << '\n');
        return false;
    }

    if (!m->db->getDoc(m->results[num].rcludi, 0, doc, false)) {
        LOGERR("DocSequenceSem::getDoc: db->getDoc failed for " <<
               m->results[num].rcludi << "\n");
        return false;
    }
    // Set the segment and its context as document abstract
    doc.meta[Rcl::Doc::keyabs] = m->results[num].ctxbefore +
        "<br/><em>" + m->results[num].segment + "</em></br>" + m->results[num].ctxafter;
    //doc.dump();
    return true;
}


int DocSequenceSem::getResCnt()
{
    if (!m->mayberunquery()) {
        LOGERR("DocSequenceSem::getResCnt: could not run query\n");
        return false;
    }
    LOGDEB0("DocSequenceSem::getResCnt(): return " << m->results.size() << '\n');
    return (int)m->results.size();
}

bool DocSequenceSem::getAbstract(Rcl::Doc &doc, PlainToRich *, std::vector<std::string>& out, bool)
{
    LOGDEB0("DocSequenceSem::getAbstract:\n");
    std::string abs;
    doc.getmeta(Rcl::Doc::keyabs, &abs);
    out.push_back(abs);
    return true;
}

#endif // ENABLE_SEMANTIC 
