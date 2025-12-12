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

#include "boolexp.h"

#include <iostream>
#include <stack>
#include <regex.h>

namespace BoolExp {
// specialinchars are special everywhere (break a word) except inside a quoted string
static const std::string specialinchars{"=<>()~"};

class Lexer {
public:
    Lexer(const std::string& in) : m_input(in) {}

    enum token {
        ERROR = -1,
        LEXEOF = 0,
        // Operators
        FIRST_TOKEN = 256,
        AND, OR, EQUAL, NOTEQUAL, MATCHES, SMALLEREQ, SMALLER, GREATER, GREATEREQ, 
        LAST_OPER,
        // Parentheses
        PARENTOPEN, PARENTCLOSE,
        // Values
        WORD, INT, QUOTED,
        LAST_TOKEN,
    };

    std::pair<Lexer::token, std::string> next() {
        static const std::pair<token, std::string> error_ret{ERROR, ""};
        static const std::pair<token, std::string> eof_ret{LEXEOF, ""};
        int c;
        /* Skip white space.  */
        while ((c = GETCHAR()) && isspace(c))
            continue;
        if (c == 0)
            return eof_ret;

        switch (c) {
        case '=': {
            int c1 = GETCHAR();
            if (c1 == '=') {
                return {EQUAL, ""};
            } else {
                return error_ret;
            }
        }
        case '~': return {MATCHES, ""};
        case '!': {
            int c1 = GETCHAR();
            if (c1 == '=') {
                return {NOTEQUAL, ""};
            } else {
                return error_ret;
            }
        }
        case '<': {
            int c1 = GETCHAR();
            if (c1 == '=') {
                return {SMALLEREQ, ""};
            } else {
                UNGETCHAR();
                return {SMALLER, ""};
            }
        }
        case '>': {
            int c1 = GETCHAR();
            if (c1 == '=') {
                return {GREATEREQ, ""};
            } else {
                UNGETCHAR();
                return {GREATER, ""};
            }
        }
        case '(':
            return {PARENTOPEN, ""};
        case ')':
            return {PARENTCLOSE, ""};
        }
        
        if (c == '"')
            return parseString();

        UNGETCHAR();

        // Other chars start a term or field name or reserved word
        std::string word;
        while ((c = GETCHAR())) {
            if (isspace(c)) {
                //std::cerr << "Word broken by whitespace" << '\n';
                break;
            } else if (specialinchars.find(c) != std::string::npos) {
                //std::cerr << "Word broken by special char" << '\n';
                UNGETCHAR();
                break;
            } else if (c == 0) {
                //std::cerr << "Word broken by EOF" << '\n';
                break;
            } else {
                word += c;
            }
        }
        if (word == "AND" || word == "&&") {
            return {AND, ""};
        } else if (word == "OR" || word == "||") {
            return {OR, ""};
        }

        if (word.size() && isdigit(word[0]))
            return {INT, word};
        else
            return {WORD, word};
    }

private:
    int GETCHAR() {
        if (m_pos < m_input.size())
            return m_input[m_pos++];
        return 0;
    }
    void UNGETCHAR() {
        if (m_pos > 0)
            m_pos--;
    }
    // Called with the first dquote already read
    std::pair<token, std::string> parseString() {
        std::string value;
        int c;
        while ((c = GETCHAR())) {
            switch (c) {
            case '\\':
                /* Escape: get next char */
                c = GETCHAR();
                if (c == 0) {
                    value += c;
                    goto out;
                }
                value += c;
                break;
            case '"':
                goto out;
            default:
                value += c;
            }
        }
        // If we get there, we got to the end of input without finding the closing quote
        return {ERROR, ""};
    out:
        return {QUOTED, value};
    }
    const std::string &m_input;
    std::string::size_type m_pos = 0;
};

static std::map<Lexer::token, const char *> token_names {
    {Lexer::EQUAL, "EQUAL"}, {Lexer::NOTEQUAL, "NOTEQUAL"}, {Lexer::MATCHES, "MATCHES"},
    {Lexer::SMALLEREQ, "SMALLEREQ"}, {Lexer::SMALLER, "SMALLER"}, {Lexer::GREATER, "GREATER"},
    {Lexer::GREATEREQ, "GREATEREQ"}, {Lexer::AND, "AND"}, {Lexer::OR, "OR"},
    {Lexer::PARENTOPEN, "PARENTOPEN"}, {Lexer::PARENTCLOSE, "PARENTCLOSE"}, {Lexer::WORD, "WORD"},
    {Lexer::INT, "INT"}, {Lexer::QUOTED, "QUOTED"},
};

#if 0
// Debug
void print_value(const std::variant<int, std::string>& value, std::ostream& os = std::cerr)
{
    switch (value.index()) {
    case 0: os << std::get<int>(value); break;
    case 1: os << std::get<std::string>(value); break;
    }
}
void print_token(const std::pair<Lexer::token, std::variant<int, std::string>>& tok,
    std::ostream& os = std::cerr)
{
    auto& [kind, value] = tok;
    if (kind <= Lexer::FIRST_TOKEN || kind >= Lexer::LAST_TOKEN) {
        os << kind;
    } else {
        os << token_names[kind];
    }
    if (kind > Lexer::LAST_OPER) {
        print_value(value, os);
    }
    os << '\n';
}
#endif


// Evaluate the expression. We use the Shunting Yard algorithm:
//   https://en.wikipedia.org/wiki/Shunting_yard_algorithm
int evaluate(const std::string& sexpr,
             const std::map<std::string, std::variant<int, std::string>>& symtable,
             std::string *errstr)
{
    std::stack<std::pair<Lexer::token, std::variant<int, std::string>>> output;
    std::stack<Lexer::token> operstack;
    Lexer lex(sexpr);
    while (true) {
        auto [kind, value] = lex.next();
        if (kind == Lexer::LEXEOF) {
            break;
        } else if (kind == Lexer::ERROR) {
            return -1;
        }

        //std::cerr << "kind [" << token_names[kind] << "]\n";

        if (kind > Lexer::PARENTCLOSE) {
            // It's a value
            if (kind == Lexer::INT) {
                output.push({kind, {static_cast<int>(atoi(value.c_str()))}});
            } else if (kind == Lexer::QUOTED) {
                output.push({kind, {value}});
            } else if (kind == Lexer::WORD) {
                const auto it = symtable.find(value);
                if (it == symtable.end()) {
                    if (errstr) *errstr = std::string("Bad variable name") + value;
                    return -1;
                }
                switch (it->second.index()) {
                case 0: {
                    output.push({kind, {std::get<int>(it->second)}});
                    break;
                }
                case 1: {
                    output.push({kind, {std::get<std::string>(it->second)}});
                    break;
                }
                }
            }
            // Done with the value
            continue;
        }

        // Operators
        if (kind < Lexer::LAST_OPER) {
            while (!operstack.empty() && operstack.top() != Lexer::PARENTOPEN) {
                auto& o2 = operstack.top();
                if (o2 >= kind) {
                    operstack.pop();
                    output.push({o2, {0}});
                } else {
                    break;
                }
            }
            operstack.push(kind);
            continue;
        }

        if (kind == Lexer::PARENTOPEN) {
            operstack.push(kind);
            continue;
        } else if (kind == Lexer::PARENTCLOSE) {
            while (true) {
                if (operstack.empty()) {
                    if (errstr) *errstr = "Empty operstack while looking for opening parenthese";
                    return -1;
                }
                if (operstack.top() == Lexer::PARENTOPEN) {
                    operstack.pop();
                    break;
                }
                output.push({operstack.top(), {0}});
                operstack.pop();
            }
            continue;
        }
    } // While true, token loop

    // Move remaining operators from stack to output
    while (!operstack.empty()) {
        if (operstack.top() == Lexer::PARENTOPEN || operstack.top() == Lexer::PARENTCLOSE) {
            if (errstr) *errstr = "Mismatched parentheses";
            return -1;
        }
        output.push({operstack.top(), {0}});
        operstack.pop();
    }

    // Evaluate
    std::stack<std::pair<Lexer::token, std::variant<int, std::string>>> backoutput;
    while (!output.empty()) {
        backoutput.push(output.top());
        output.pop();
    }
    while (!backoutput.empty()) {
        //print_token(backoutput.top());
        auto& [kind, value] = backoutput.top();

        if (kind > Lexer::LAST_OPER) {
            output.push({kind, value});
        } else {
            if (output.empty()) {
                if (errstr) *errstr = "Stack underflow";
                return -1;
            }
            auto [kright, right] = output.top();
            output.pop();
            if (output.empty()) {
                if (errstr) *errstr = "Stack underflow";
                return -1;
            }
            auto [kleft, left] = output.top();
            output.pop();

            if (left.index() != right.index()) {
                return -1;
            }

            //std::cerr << "EVALUATE "; print_value(left);
            //std::cerr << " "<< token_names[kind] << " "; print_value(right); std::cerr << '\n';

            try {
                switch(kind) {
                case Lexer::EQUAL:
                    output.push({Lexer::INT, int(left == right)});
                    break;
                case Lexer::NOTEQUAL:
                    output.push({Lexer::INT, int(left != right)});
                    break;
                case Lexer::SMALLEREQ:
                    output.push({Lexer::INT, int(left <= right)});
                    break;
                case Lexer::SMALLER:
                    output.push({Lexer::INT, int(left < right)});
                    break;
                case Lexer::GREATER:
                    output.push({Lexer::INT, int(left > right)});
                    break;
                case Lexer::GREATEREQ:
                    output.push({Lexer::INT, int(left >= right)});
                    break;

                case Lexer::AND:
                    output.push({Lexer::INT, {int(std::get<int>(left) && std::get<int>(right))}});
                    break;
                case Lexer::OR:
                    output.push({Lexer::INT, {int(std::get<int>(left) || std::get<int>(right))}});
                    break;

                case Lexer::MATCHES: {
                    static regex_t reg;
                    if (regcomp(&reg, std::get<std::string>(right).c_str(), REG_EXTENDED|REG_NOSUB)){
                        if (errstr) *errstr = "Bad regular expression";
                        return -1;
                    }
                    auto res = regexec(&reg, std::get<std::string>(left).c_str(), 0, 0, 0);
                    output.push({Lexer::INT, {int(!res)}});
                    regfree(&reg);
                    break;
                }
                default:
                    if (errstr) *errstr = std::string("Bad oper during eval: ") + token_names[kind];
                    return -1;
                }
            } catch(...) {
                if (errstr) *errstr = "Type error";
                return -1;
            }
        }

        backoutput.pop();
    }

    if (output.empty()) {
        if (errstr) *errstr = "No result ??";
        return -1;
    }
    auto& [lkind, lvalue] = output.top();
    if (lkind < Lexer::LAST_OPER) {
        if (errstr) *errstr = " Operator ??";
        return -1;
    }
    return std::get<int>(lvalue);
}

}//namespace BoolExp
