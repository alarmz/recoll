/*
 *   Copyright 2004 J.F.Dockes
 *
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

#include <errno.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <mutex>
#include <algorithm>

#include "cstr.h"
#include "mimehandler.h"
#include "log.h"
#include "rclconfig.h"
#include "smallut.h"
#include "md5ut.h"
#include "mh_exec.h"
#include "mh_execm.h"
#include "mh_html.h"
#include "mh_mail.h"
#include "mh_mbox.h"
#include "mh_text.h"
#ifndef _WIN32
#include "mh_symlink.h"
#endif // _WIN32
#include "mh_unknown.h"
#include "mh_null.h"
#include "mh_xslt.h"
#include "rcldoc.h"
#include "rclutil.h"
#include "conftree.h"
#include "boolexp.h"

using namespace std;

// Performance help: we use a pool of already known and created
// handlers. There can be several instances for a given mime type
// (think email attachment in email message: 2 rfc822 handlers are
// needed simulteanously)
static multimap<string, RecollFilter*>  o_handlers;
static list<multimap<string, RecollFilter*>::iterator> o_hlru;
typedef list<multimap<string, RecollFilter*>::iterator>::iterator hlruit_tp;

static std::mutex o_handlers_mutex;

static const unsigned int max_handlers_cache_size = 100;

/* Look for mime handler in pool */
static RecollFilter *getMimeHandlerFromCache(const string& key)
{
    std::unique_lock<std::mutex> locker(o_handlers_mutex);
    string xdigest;
    MD5HexPrint(key, xdigest);
    LOGDEB("getMimeHandlerFromCache: " << xdigest << " cache size " <<
           o_handlers.size() << "\n");

    multimap<string, RecollFilter *>::iterator it = o_handlers.find(key);
    if (it != o_handlers.end()) {
        RecollFilter *h = it->second;
        hlruit_tp it1 = find(o_hlru.begin(), o_hlru.end(), it);
        if (it1 != o_hlru.end()) {
            o_hlru.erase(it1);
        } else {
            LOGERR("getMimeHandlerFromCache: lru position not found\n");
        }
        o_handlers.erase(it);
        LOGDEB("getMimeHandlerFromCache: " << xdigest << " found size " <<
               o_handlers.size() << "\n");
        return h;
    }
    LOGDEB("getMimeHandlerFromCache: " << xdigest << " not found\n");
    return 0;
}

/* Return mime handler to pool */
void returnMimeHandler(RecollFilter *handler)
{
    typedef multimap<string, RecollFilter*>::value_type value_type;

    if (handler == 0) {
        LOGERR("returnMimeHandler: bad parameter\n");
        return;
    }
    handler->clear();

    std::unique_lock<std::mutex> locker(o_handlers_mutex);

    LOGDEB("returnMimeHandler: returning filter for " <<
           handler->get_mime_type() << " cache size " << o_handlers.size() <<
           "\n");

    // Limit pool size. The pool can grow quite big because there are
    // many filter types, each of which can be used in several copies
    // at the same time either because it occurs several times in a
    // stack (ie mail attachment to mail), or because several threads
    // are processing the same mime type at the same time.
    multimap<string, RecollFilter *>::iterator it;
    if (o_handlers.size() >= max_handlers_cache_size) {
        static int once = 1;
        if (once) {
            once = 0;
            for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
                LOGDEB1("Cache full. key: " << it->first << "\n");
            }
            LOGDEB1("Cache LRU size: " << o_hlru.size() << "\n");
        }
        if (o_hlru.size() > 0) {
            it = o_hlru.back();
            o_hlru.pop_back();
            delete it->second;
            o_handlers.erase(it);
        }
    }
    it = o_handlers.insert(value_type(handler->get_id(), handler));
    o_hlru.push_front(it);
}

void clearMimeHandlerCache()
{
    LOGDEB("clearMimeHandlerCache()\n");
    multimap<string, RecollFilter *>::iterator it;
    std::unique_lock<std::mutex> locker(o_handlers_mutex);
    for (it = o_handlers.begin(); it != o_handlers.end(); it++) {
        delete it->second;
    }
    o_handlers.clear();
    TempFile::tryRemoveAgain();
}

/** For mime types set as "internal" in mimeconf: 
 * create appropriate handler object. */
static RecollFilter *mhFactory(RclConfig *config, const string &mimeOrParams,
                               bool nobuild, string& id)
{
    LOGDEB1("mhFactory(" << mimeOrParams << ")\n");
    vector<string> lparams;
    stringToStrings(mimeOrParams, lparams);
    if (lparams.empty()) {
        // ??
        return nullptr;
    }
    string lmime(lparams[0]);
    stringtolower(lmime);
    if (cstr_textplain == lmime) {
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerText\n");
        MD5String("MimeHandlerText", id);
        return nobuild ? 0 : new MimeHandlerText(config, id);
    } else if (cstr_texthtml == lmime) {
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerHtml\n");
        MD5String("MimeHandlerHtml", id);
        return nobuild ? 0 : new MimeHandlerHtml(config, id);
    } else if ("text/x-mail" == lmime) {
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerMbox\n");
        MD5String("MimeHandlerMbox", id);
        return nobuild ? 0 : new MimeHandlerMbox(config, id);
    } else if ("message/rfc822" == lmime) {
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerMail\n");
        MD5String("MimeHandlerMail", id);
        return nobuild ? 0 : new MimeHandlerMail(config, id);
#ifndef _WIN32
    } else if ("inode/symlink" == lmime) {
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerSymlink\n");
        MD5String("MimeHandlerSymlink", id);
        return nobuild ? 0 : new MimeHandlerSymlink(config, id);
#endif // _WIN32
    } else if ("application/x-zerosize" == lmime) {
        LOGDEB("mhFactory(" << lmime << "): returning MimeHandlerNull\n");
        MD5String("MimeHandlerNull", id);
        return nobuild ? 0 : new MimeHandlerNull(config, id);
    } else if (lmime.find("text/") == 0) {
        // Try to handle unknown text/xx as text/plain. This only happen if the text/xx was defined
        // as "internal" in mimeconf.
        // This is historic, because the natural way with the current code would be to specify
        // "internal text/plain" instead of just "internal"...
        // Also see rclconfig for the textunknownisplain configuration variable which allows
        // processing all unknown text/* as text/plain.
        LOGDEB2("mhFactory(" << lmime << "): returning MimeHandlerText(x)\n");
        MD5String("MimeHandlerText", id);
        return nobuild ? 0 : new MimeHandlerText(config, id);
    } else if ("xsltproc" == lmime) {
        // XML Types processed with one or several xslt style sheets.
        MD5String(mimeOrParams, id);
        return nobuild ? 0 : new MimeHandlerXslt(config, id, lparams);
    } else {
        // We should not get there. It means that "internal" was set
        // as a handler in mimeconf for a mime type we actually can't
        // handle.
        LOGERR("mhFactory: mime type [" << lmime <<
               "] set as internal but unknown\n");
        MD5String("MimeHandlerUnknown", id);
        return nobuild ? 0 : new MimeHandlerUnknown(config, id);
    }
}

static const string cstr_mh_charset("charset");
static const string cstr_mh_maxseconds("maxseconds");
/**
 * Create a filter that executes an external program or script
 * A filter def can look like:
 *      someprog -v -t " h i j";charset= xx; mimetype=yy
 * A semi-colon list of attr=value pairs may come after the exec spec.
 * This list is treated by replacing semi-colons with newlines and building
 * a confsimple. This is done quite brutally and we don't support having
 * a ';' inside a quoted string for now. Can't see a use for it.
 */
MimeHandlerExec *mhExecFactory(RclConfig *cfg, const string& mtype, string& hs,
                               bool multiple, const string& id)
{
    ConfSimple attrs;
    string cmdstr;

    if (!cfg->valueSplitAttributes(hs, cmdstr, attrs)) {
        LOGERR("mhExecFactory: bad config line for [" << mtype << "]: [" << hs << "]\n");
        return 0;
    }

    // Split command name and args, and build exec object
    vector<string> cmdtoks;
    stringToStrings(cmdstr, cmdtoks);
    if (cmdtoks.empty()) {
        LOGERR("mhExecFactory: bad config line for [" << mtype << "]: [" << hs << "]\n");
        return 0;
    }
    if (!cfg->processFilterCmd(cmdtoks)) {
        return nullptr;
    }
    MimeHandlerExec *h = multiple ? new MimeHandlerExecMultiple(cfg, id) :
        new MimeHandlerExec(cfg, id);
    h->params = cmdtoks;

    // Handle additional attributes. We substitute the semi-colons
    // with newlines and use a ConfSimple
    string value;
    if (attrs.get(cstr_mh_charset, value)) 
        h->cfgFilterOutputCharset = stringtolower((const string&)value);
    if (attrs.get(cstr_dj_keymt, value))
        h->cfgFilterOutputMtype = stringtolower((const string&)value);
    if (attrs.get(cstr_mh_maxseconds, value)) {
        h->setmaxseconds(atoi(value.c_str()));
    }
    LOGDEB2("mhExecFactory:mt [" << mtype << "] cfgmt [" <<
            h->cfgFilterOutputMtype << "] cfgcs ["<<h->cfgFilterOutputCharset <<
            "] cmd: [" << stringsToString(h->params) << "]\n");
    return h;
}

// Separate e.g. [execm rcldoc.py whatever;some other] into 'execm' and the rest.
static inline void xtract_tp_cmd(std::string& hs, std::string& handlertype, std::string& cmdstr)
{
    trimstring(hs);
    auto  b1 = hs.find_first_of(" \t");
    handlertype = hs.substr(0, b1);
    stringtolower(handlertype);
    if (b1 != string::npos) {
        cmdstr = hs.substr(b1);
        trimstring(cmdstr);
    }
}

/* Get handler/filter object for given mime type: */
RecollFilter *getMimeHandler(const string &mtype, RclConfig *cfg,
                             bool filtertypes, const std::string& fn, const struct PathStat *stp)
{
    LOGDEB("getMimeHandler: mtype [" << mtype << "] filtertypes " << filtertypes << "\n");
    RecollFilter *h = 0;

    // Get handler definition for mime type. We do this even if an appropriate handler object may be
    // in the cache.
    // This is fast, and necessary to conform to the configuration, (ie: text/html might be filtered
    // out by indexedmimetypes but an html handler could still be in the cache because it was needed
    // by some other interning stack).
    std::string hs;
    hs = cfg->getMimeHandlerDef(mtype, filtertypes, fn);

    std::string id;
    if (!hs.empty()) { 
        // Got a handler definition line
        std::string handlertype;
        std::string cmdstr;

        // Break definition into type (internal/exec/execm) and name/command string. Repeat if type
        // is "as" (points to other definition).
        int i = 0;
        do { 
            xtract_tp_cmd(hs, handlertype, cmdstr);
            if (handlertype == "as") {
                hs = cfg->getMimeHandlerDef(cmdstr, filtertypes, fn);
                if (hs.empty()) {
                    LOGERR("getMimeHandler: 'as' leads to empty def for " << mtype << '\n');
                    goto out;
                }
            }
            if (++i > 10) {
                LOGERR("getMimeHandler: got \"as\" loop for " << mtype << '\n');
                goto out;
            }
        } while (handlertype == "as");

        // Process possible conditional treatment: [decide condition ? def1 : def2]
        // This is used to decide kind of processing, e.g. based on size
        if (handlertype == "decide") {
            auto questionpos = cmdstr.find('?');
            auto colonpos = cmdstr.find(':');
            if (questionpos == std::string::npos || colonpos == std::string::npos) {
                LOGERR("Bad definition for decide line : no question mark or no colon\n");
                goto out;
            }
            auto condition = cmdstr.substr(0, questionpos);
            auto cmdstr1 = cmdstr.substr(questionpos+1, colonpos-questionpos-1);
            auto cmdstr2 = cmdstr.substr(colonpos+1);
            LOGDEB0("  CONDITION [" << condition << "] cmd1 [" << cmdstr1 <<
                    "] cmd2 [" << cmdstr2 << "]\n");
            // This can only work if we have an actual file name (and actual stat data if size is
            // involved
            if (!fn.empty()) {
                auto sfn = path_getsimple(fn);
                int size = stp ? static_cast<int>(stp->pst_size/1024) : 0; 
                std::map<std::string, std::variant<int, std::string>> symtable {
                    {"sizekbs", size},
                    {"filename", sfn},
                };
                std::string errstr;
                auto result = BoolExp::evaluate(condition, symtable, &errstr);
                if (!errstr.empty()) {
                    LOGERR("Error evaluating [" << condition << "] : " << errstr << '\n');
                }
                if (result) {
                    hs = cmdstr1;
                } else {
                    hs = cmdstr2;
                }
            } else {
                // No data: use the first choice.
                hs = cmdstr1;
            }
            xtract_tp_cmd(hs, handlertype, cmdstr);
        }

        LOGDEB1(" HANDLERTYPE [" << handlertype << "] cmdstr [" << cmdstr << "]\n");
        
        bool internal = (handlertype == "internal");
        if (internal) {
            // For internal types let the factory compute the cache id
            mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, true, id);
        } else {
            // exec/execm: use the md5 of the def line
            MD5String(hs, id);
        }

        // Do we already have a handler object in the cache ?
        h = getMimeHandlerFromCache(id);
        if (h != 0)
            goto out;

        LOGDEB2("getMimeHandler: " << mtype << " not in cache\n");
        if (internal) {
            // A MIME type and other values can be specified as additional text after the handler
            // type, e.g "internal text/plain"
            // This is so that we can have specific MIME types like text/x-purple-html-log (allowing
            // a specific icon and viewer), and process them as text/plain or text/html for indexing.
            // This is partly redundant with the localfields/rclaptg approach and the newer "as"
            // indirection.
            // Also, special processing is performed by mhFactory if the first word is "xsltproc"
            // because what follows are the intruction for maybe processing multiple XML docs into
            // main text and metadata.
            LOGDEB2("handlertype internal, cmdstr [" << cmdstr << "]\n");
            h = mhFactory(cfg, cmdstr.empty() ? mtype : cmdstr, false, id);
            goto out;
        } else if ("dll" == handlertype) {
            // Never actually used :)
        } else {
            // Executing an external program. Which must be set...
            if (cmdstr.empty()) {
                LOGERR("getMimeHandler: bad line for " << mtype << ": " << hs << "\n");
                goto out;
            }
            if ("exec" == handlertype) {
                h = mhExecFactory(cfg, mtype, cmdstr, false, id);
                goto out;
            } else if ("execm" == handlertype) {
                h = mhExecFactory(cfg, mtype, cmdstr, true, id);
                goto out;
            } else {
                LOGERR("getMimeHandler: bad line for " << mtype << ": " << hs << "\n");
                goto out;
            }
        }
    } else {
        // No identified MIME type, or no handler associated.
        // Such files are either ignored or their name and generic metadata is indexed, depending on
        // configuration
        bool indexunknown = false;
        cfg->getConfParam("indexallfilenames", &indexunknown);
        if (indexunknown) {
            MD5String("MimeHandlerUnknown", id);
            if ((h = getMimeHandlerFromCache(id)) == 0)
                h = new MimeHandlerUnknown(cfg, id);
        }
        goto out;
    }

out:
    if (h) {
        h->set_property(RecollFilter::DEFAULT_CHARSET, cfg->getDefCharset());
        // In multithread context, and in case this handler is out
        // from the cache, it may have a config pointer belonging to
        // another thread. Fix it.
        h->setConfig(cfg);
    }
    return h;
}

/// Can this mime type be interned (according to config) ?
bool canIntern(const std::string mtype, RclConfig *cfg)
{
    if (mtype.empty())
        return false;
    string hs = cfg->getMimeHandlerDef(mtype);
    if (hs.empty())
        return false;
    return true;
}
/// Same, getting MIME from doc
bool canIntern(Rcl::Doc *doc, RclConfig *cfg)
{
    if (doc) {
        return canIntern(doc->mimetype, cfg);
    }
    return false;
}

/// Can this MIME type be opened (has viewer def) ?
bool canOpen(Rcl::Doc *doc, RclConfig *cfg, bool useall)
{
    if (!doc) {
        return false;
    }
    string apptag;
    doc->getmeta(Rcl::Doc::keyapptg, &apptag);
    return !cfg->getMimeViewerDef(doc->mimetype, apptag, useall).empty();
}

string RecollFilter::metadataAsString()
{
    string s;
    for (const auto& ent : m_metaData) {
        if (ent.first == "content")
            continue;
        s += ent.first + "->" + ent.second + "\n";
    }
    return s;
}
