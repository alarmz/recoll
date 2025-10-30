/* This file was copied from xapian-omega-1.0.1 and modified */

/* htmlparse.h: simple HTML parser for omega indexer
 *
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002,2006,2008,2009,2011,2016 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef INCLUDED_HTMLPARSE_H
#define INCLUDED_HTMLPARSE_H

#include <string>
#include <map>


class HtmlParser {
    std::map<std::string, std::string> parameters;
protected:
    virtual void decode_entities(std::string &s);
    bool in_script;
    std::string charset;
    static std::map<std::string, unsigned int> named_ents;

    bool get_parameter(const std::string & param, std::string & value) const;
public:
    virtual void process_text(const std::string &/*text*/) { }
    virtual bool opening_tag(const std::string &/*tag*/) { return true; }
    virtual bool closing_tag(const std::string &/*tag*/) { return true; }
    virtual void parse_html(const std::string &text);
    HtmlParser() = default;
    virtual ~HtmlParser() = default;
};

#endif
