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

#include "autoconfig.h"

#include <ctype.h>

#include <string>
#include <list>

#ifdef ENABLE_LIBMAGIC
#include <magic.h>
#endif

#include "mimetype.h"
#include "log.h"
#include "execmd.h"
#include "rclconfig.h"
#include "smallut.h"
#include "idfile.h"
#include "pxattr.h"
#include "rclutil.h"

using namespace std;

// Some document types have losely defined MIME types, about which xdg-mime, file, and libmagic may
// differ. Map them to our own choice.
static std::map<std::string, std::string> mimealiases{
    {"text/xml", "application/xml"}, // libmagic wrong. We right: rfc7303
};

/// Identification of file from contents. This is called for files with
/// unrecognized extensions.
///
/// The system 'file' utility does not always work for us. For example
/// it will mistake mail folders for simple text files if there is no
/// 'Received' header, which would be the case, for example in a
/// 'Sent' folder. Also "file -i" does not exist on all systems, and
/// is quite costly to execute.
/// So we first call the internal file identifier, which currently
/// only knows about mail, but in which we can add the more
/// current/interesting file types.
/// As a last resort we execute 'file' or its configured replacement
/// (except if forbidden by config)
static string mimetypefromdata(RclConfig *cfg, const string &fn, bool usfc)
{
    LOGDEB1("mimetypefromdata: fn [" << fn << "]\n");
    PRETEND_USE(cfg);

    // First try the internal identifying routine
    string mime = idFile(fn.c_str());
    if (!mime.empty()) {
        return mime;
    }
    if (!usfc) {
        LOGDEB1("mimetypefromdata: usfc unset: not using libmagic or MIME guess command\n");
        return string();
    }

    // Last resort: use "file -i", or its configured replacement, or libmagic (the latter on
    // Windows mostly at the moment)

#ifdef ENABLE_LIBMAGIC

    // Caching the open mgtoken would slightly improve performance but we'd need locking because
    // libmagic is not thread-safe
    auto mgtoken = magic_open(MAGIC_MIME_TYPE);
    if (mgtoken) {
#ifdef _WIN32
        string magicfile = path_cat(path_rclpkgdatadir(), "magic.mgc");
        int ret = magic_load(mgtoken, magicfile.c_str());
#else
        int ret = magic_load(mgtoken, nullptr);
#endif
        if (ret == 0) {
            mime = magic_file(mgtoken, fn.c_str());
            magic_close(mgtoken);
        }
    }

#else /* Not using libmagic, use command -> */

    // 'file' fallback if the configured command (default: xdg-mime) is not found
    static const vector<string> tradfilecmd = {{FILE_PROG},
#ifdef __APPLE__
        // On the Mac, /usr/bin/file wants a -I to print the MIME type, but we may be using
        // a MacPorts or Homebrew version, needing -i. Fortunately both accept --mime-type
                                               {"--mime-type"}
#else
                                               {"-i"}
#endif
    };

    vector<string> cmd;
    string scommand;
    if (cfg->getConfParam("systemfilecommand", scommand)) {
        LOGDEB2("mimetype: syscmd from config: " << scommand << "\n");
        stringToStrings(scommand, cmd);
        string exe;
        if (cmd.empty()) {
            cmd = tradfilecmd;
        } else if (!ExecCmd::which(cmd[0], exe)) {
            cmd = tradfilecmd;
        } else {
            cmd[0] = exe;
        }
        cmd.push_back(fn);
    } else {
        LOGDEB("mimetype:systemfilecommand not found, using " <<
               stringsToString(tradfilecmd) << "\n");
        cmd = tradfilecmd;
    }

    string result;
    LOGDEB2("mimetype: executing: [" << stringsToString(cmd) << "]\n");
    if (!ExecCmd::backtick(cmd, result)) {
        LOGERR("mimetypefromdata: exec " << stringsToString(cmd) << " failed\n");
        return string();
    }
    trimstring(result, " \t\n\r");
    LOGDEB2("mimetype: systemfilecommand output [" << result << "]\n");
    
    // The normal output from "file -i" looks like the following:
    //   thefilename.xxx: text/plain; charset=us-ascii
    // Sometimes the semi-colon is missing like in:
    //     mimetype.cpp: text/x-c charset=us-ascii
    // And sometimes we only get the mime type. This apparently happens
    // when 'file' believes that the file name is binary
    // xdg-mime only outputs the MIME type.

    // If there is no colon and there is a slash, this is hopefully the MIME type
    if (result.find(":") == string::npos && result.find("/") != string::npos) {
        return result;
    }

    // Else the result should begin with the file name. Get rid of it:
    if (result.find(fn) != 0) {
        // Garbage "file" output. Maybe the result of a charset conversion attempt?
        LOGERR("mimetype: can't interpret output from [" <<
               stringsToString(cmd) << "] : [" << result << "]\n");
        return string();
    }
    result = result.substr(fn.size());

    // Now should look like ": text/plain; charset=us-ascii".
    mime = growmimearoundslash(mime);

#endif // Not libmagic

    auto it = mimealiases.find(mime);
    if (it != mimealiases.end())
        return it->second;
    return mime;
}

// Guess mime type, first from suffix, then from file data. We also have a list of suffixes that we
// don't touch at all.
string mimetype(const string &fn, RclConfig *cfg, bool usfc, const struct PathStat& stp)
{
    // Use stat data if available to check for non regular files
    if (stp.pst_type != PathStat::PST_INVALID) {
        if (stp.pst_type == PathStat::PST_DIR)
            return "inode/directory";
        if (stp.pst_type == PathStat::PST_SYMLINK)
            return "inode/symlink";
        if (stp.pst_type != PathStat::PST_REGULAR)
            return "inode/x-fsspecial";
        // Empty files are just this: avoid further errors with actual filters.
        if (stp.pst_size == 0) 
            return "inode/x-empty";
    }

    string mtype;
    if (cfg && cfg->inStopSuffixes(fn)) {
        LOGDEB("mimetype: fn [" << fn << "] in stopsuffixes\n");
        return mtype;
    }

    // Extended attribute has priority on everything, as per:
    // http://freedesktop.org/wiki/CommonExtendedAttributes
    if (pxattr::get(fn, "mime_type", &mtype)) {
        LOGDEB0("Mimetype: 'mime_type' xattr : [" << mtype << "]\n");
        if (mtype.empty()) {
            LOGDEB0("Mimetype: getxattr() returned empty mime type !\n");
        } else {
            return mtype;
        }
    }

    if (nullptr == cfg)  {
        LOGERR("Mimetype: null config ??\n");
        return mtype;
    }

    // Compute file name suffix and search the mimetype map
    string::size_type dot = fn.find_first_of(".");
    while (dot != string::npos) {
        string suff = stringtolower(fn.substr(dot));
        mtype = cfg->getMimeTypeFromSuffix(suff);
        if (!mtype.empty() || dot >= fn.size() - 1)
            break;
        dot = fn.find_first_of(".", dot + 1);
    }

    // If the type was not determined from the suffix, examine file data. Can only do this if we
    // have an actual file (as opposed to a pure name).
    if (mtype.empty() && stp.pst_type != PathStat::PST_INVALID)
        mtype = mimetypefromdata(cfg, fn, usfc);

    return mtype;
}
