/* Copyright (C) 2025 J.F.Dockes
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

// A small boolean expression evaluator. Would have like to find a ready-made one, but they are
// either huge and overkills for the need, or don't support string matching.

#include <string>
#include <map>
#include <variant>

namespace BoolExp {

/** Evaluate a boolean expression given as a string.
 *
 *  Values are ints or quoted strings or name references to the input symbol table
 *  Operators are < > <= >= = && || parentheses and ~ (string regexp: str ~ regexp).
 *
 *  @param sexpr the input expression
 *  @param symtable an association of names to int or string values
 *  @return -1 for error, 0 or 1.
 */
int evaluate(const std::string& sexpr,
             const std::map<std::string, std::variant<int, std::string>>& symtable,
             std::string *errstr = nullptr
    );

}
