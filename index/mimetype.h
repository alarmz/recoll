/* Copyright (C) 2004-2026 J.F.Dockes
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
#ifndef _MIMETYPE_H_INCLUDED_
#define _MIMETYPE_H_INCLUDED_

#include <string>
#include "pathut.h"

class RclConfig;

/**
 * Try to determine a MIME type for file or data.
 *
 * If @param stp is valid, @param filename must be usable to actually access file data.
 *
 * Whether we use suffixes only or also try to identify from data is controlled by the
 * "usesystemfilecommand" configuration parameter (on by default), with the exception that,
 * if data is accessible, and no suffix match was found, we always check for email data.
 * 
 * @param cfg recoll config
 * @param filename file/path name. This references an actual file iff stp is set,
 * else it's just a string.
 * @param stp if not null use st_mode bits for directories etc. Also confirms that the path
 * references an actual file.
 * @param data file contents. This is only set by mh_execm when identifying subdocs (we always have
 * a memory copy of the data then). stp is nullptr in this case
 * @param forcemagic act as if usefilesystemcommand was true.
 */
std::string mimetype(RclConfig *cfg, const std::string &filename,
                     const struct PathStat *stp = nullptr, const std::string& data = std::string(),
                     bool forcemagic = false);


#endif /* _MIMETYPE_H_INCLUDED_ */
