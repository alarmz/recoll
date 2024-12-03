// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.



// First part of user prologue.
#line 1 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"

#define YYDEBUG 1
#include "autoconfig.h"

#include <stdio.h>

#include <iostream>
#include <string>

#include "searchdata.h"
#include "wasaparserdriver.h"
#include "wasaparse.hpp"

using namespace std;

//#define LOG_PARSER
#ifdef LOG_PARSER
#define LOGP(X) {cerr << X;}
#else
#define LOGP(X)
#endif

int yylex(yy::parser::semantic_type *, yy::parser::location_type *, 
          WasaParserDriver *);
void yyerror(char const *);
static void qualify(Rcl::SearchDataClauseDist *, const string &);

static void addSubQuery(WasaParserDriver *,
                        Rcl::SearchData *sd, Rcl::SearchData *sq)
{
    if (sd && sq)
        sd->addClause(
            new Rcl::SearchDataClauseSub(std::shared_ptr<Rcl::SearchData>(sq)));
}


#line 78 "wasaparse.tab.cpp"







#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (false)
# endif


// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

namespace yy {
#line 175 "wasaparse.tab.cpp"

  /// Build a parser object.
  parser::parser (WasaParserDriver* d_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      d (d_yyarg)
  {}

  parser::~parser ()
  {}

  parser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/

  // basic_symbol.
  template <typename Base>
  parser::basic_symbol<Base>::basic_symbol (const basic_symbol& that)
    : Base (that)
    , value (that.value)
    , location (that.location)
  {}


  /// Constructor for valueless symbols.
  template <typename Base>
  parser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, YY_MOVE_REF (location_type) l)
    : Base (t)
    , value ()
    , location (l)
  {}

  template <typename Base>
  parser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, YY_RVREF (value_type) v, YY_RVREF (location_type) l)
    : Base (t)
    , value (YY_MOVE (v))
    , location (YY_MOVE (l))
  {}


  template <typename Base>
  parser::symbol_kind_type
  parser::basic_symbol<Base>::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }


  template <typename Base>
  bool
  parser::basic_symbol<Base>::empty () const YY_NOEXCEPT
  {
    return this->kind () == symbol_kind::S_YYEMPTY;
  }

  template <typename Base>
  void
  parser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move (s);
    value = YY_MOVE (s.value);
    location = YY_MOVE (s.location);
  }

  // by_kind.
  parser::by_kind::by_kind () YY_NOEXCEPT
    : kind_ (symbol_kind::S_YYEMPTY)
  {}

#if 201103L <= YY_CPLUSPLUS
  parser::by_kind::by_kind (by_kind&& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {
    that.clear ();
  }
#endif

  parser::by_kind::by_kind (const by_kind& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {}

  parser::by_kind::by_kind (token_kind_type t) YY_NOEXCEPT
    : kind_ (yytranslate_ (t))
  {}



  void
  parser::by_kind::clear () YY_NOEXCEPT
  {
    kind_ = symbol_kind::S_YYEMPTY;
  }

  void
  parser::by_kind::move (by_kind& that)
  {
    kind_ = that.kind_;
    that.clear ();
  }

  parser::symbol_kind_type
  parser::by_kind::kind () const YY_NOEXCEPT
  {
    return kind_;
  }


  parser::symbol_kind_type
  parser::by_kind::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }



  // by_state.
  parser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  parser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  parser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  parser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  parser::symbol_kind_type
  parser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  parser::stack_symbol_type::stack_symbol_type ()
  {}

  parser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state), YY_MOVE (that.value), YY_MOVE (that.location))
  {
#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  parser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s, YY_MOVE (that.value), YY_MOVE (that.location))
  {
    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    value = that.value;
    location = that.location;
    return *this;
  }

  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    value = that.value;
    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);

    // User destructor.
    switch (yysym.kind ())
    {
      case symbol_kind::S_WORD: // WORD
#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
                    {delete (yysym.value.str);}
#line 387 "wasaparse.tab.cpp"
        break;

      case symbol_kind::S_QUOTED: // QUOTED
#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
                    {delete (yysym.value.str);}
#line 393 "wasaparse.tab.cpp"
        break;

      case symbol_kind::S_QUALIFIERS: // QUALIFIERS
#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
                    {delete (yysym.value.str);}
#line 399 "wasaparse.tab.cpp"
        break;

      case symbol_kind::S_complexfieldname: // complexfieldname
#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
                    {delete (yysym.value.str);}
#line 405 "wasaparse.tab.cpp"
        break;

      default:
        break;
    }
  }

#if YYDEBUG
  template <typename Base>
  void
  parser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " ("
            << yysym.location << ": ";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  parser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  parser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  parser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  parser::state_type
  parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  parser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  parser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  parser::operator() ()
  {
    return parse ();
  }

  int
  parser::parse ()
  {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            yyla.kind_ = yytranslate_ (yylex (&yyla.value, &yyla.location, d));
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        goto yydefault;
      }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* If YYLEN is nonzero, implement the default value of the
         action: '$$ = $1'.  Otherwise, use the top of the stack.

         Otherwise, the following line sets YYLHS.VALUE to garbage.
         This behavior is undocumented and Bison users should not rely
         upon it.  */
      if (yylen)
        yylhs.value = yystack_[yylen - 1].value;
      else
        yylhs.value = yystack_[0].value;

      // Default location.
      {
        stack_type::slice range (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, range, yylen);
        yyerror_range[1].location = yylhs.location;
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 2: // topquery: query
#line 74 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    // It's possible that we end up with no query (e.g.: because just a
    // date filter was set, no terms). Allocate an empty query so that we
    // have something to set the global criteria on (this will yield a
    // Xapian search like <alldocuments> FILTER xxx
    if ((yystack_[0].value.sd) == nullptr)
        d->m_result = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
    else
        d->m_result = (yystack_[0].value.sd);
}
#line 685 "wasaparse.tab.cpp"
    break;

  case 3: // query: query query
#line 87 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("q: query query\n");
    Rcl::SearchData *sd = nullptr;
    if ((yystack_[1].value.sd) || (yystack_[0].value.sd)) {
        sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
        addSubQuery(d, sd, (yystack_[1].value.sd));
        addSubQuery(d, sd, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = sd;
}
#line 700 "wasaparse.tab.cpp"
    break;

  case 4: // query: query AND query
#line 98 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("q: query AND query\n");
    Rcl::SearchData *sd = nullptr;
    if ((yystack_[2].value.sd) || (yystack_[0].value.sd)) {
        sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
        addSubQuery(d, sd, (yystack_[2].value.sd));
        addSubQuery(d, sd, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = sd;
}
#line 715 "wasaparse.tab.cpp"
    break;

  case 5: // query: query OR query
#line 109 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("query: query OR query\n");
    Rcl::SearchData *top = nullptr;
    if ((yystack_[2].value.sd) || (yystack_[0].value.sd)) {
       top = new Rcl::SearchData(Rcl::SCLT_OR, d->m_stemlang);
       addSubQuery(d, top, (yystack_[2].value.sd));
       addSubQuery(d, top, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = top;
}
#line 730 "wasaparse.tab.cpp"
    break;

  case 6: // query: '(' query ')'
#line 120 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("q: ( query )\n");
    (yylhs.value.sd) = (yystack_[1].value.sd);
}
#line 739 "wasaparse.tab.cpp"
    break;

  case 7: // query: fieldexpr
#line 126 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("q: fieldexpr\n");
    Rcl::SearchData *sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
    if (d->addClause(sd, (yystack_[0].value.cl))) {
        (yylhs.value.sd) = sd;
    } else {
        delete sd;
        (yylhs.value.sd) = nullptr;
    }
}
#line 754 "wasaparse.tab.cpp"
    break;

  case 8: // fieldexpr: term
#line 139 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: simple fieldexpr: " << (yystack_[0].value.cl)->gettext() << endl);
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 763 "wasaparse.tab.cpp"
    break;

  case 9: // fieldexpr: complexfieldname EQUALS term
#line 144 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " = " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_EQUALS);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 775 "wasaparse.tab.cpp"
    break;

  case 10: // fieldexpr: complexfieldname CONTAINS term
#line 152 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " : " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_CONTAINS);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 787 "wasaparse.tab.cpp"
    break;

  case 11: // fieldexpr: complexfieldname CONTAINS range
#line 160 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " : " << (yystack_[0].value.rg)->gettext() << endl);
    (yystack_[0].value.rg)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.rg)->setrel(Rcl::SearchDataClause::REL_CONTAINS);
    (yylhs.value.cl) = (yystack_[0].value.rg);
    delete (yystack_[2].value.str);
}
#line 799 "wasaparse.tab.cpp"
    break;

  case 12: // fieldexpr: complexfieldname SMALLER term
#line 168 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " < " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_LT);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 811 "wasaparse.tab.cpp"
    break;

  case 13: // fieldexpr: complexfieldname SMALLEREQ term
#line 176 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " <= " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_LTE);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 823 "wasaparse.tab.cpp"
    break;

  case 14: // fieldexpr: complexfieldname GREATER term
#line 184 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: "  << *(yystack_[2].value.str) << " > " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_GT);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 835 "wasaparse.tab.cpp"
    break;

  case 15: // fieldexpr: complexfieldname GREATEREQ term
#line 192 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: " << *(yystack_[2].value.str) << " >= " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_GTE);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 847 "wasaparse.tab.cpp"
    break;

  case 16: // fieldexpr: '-' fieldexpr
#line 200 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("fe: - fieldexpr[" << (yystack_[0].value.cl)->gettext() << "]" << endl);
    (yystack_[0].value.cl)->setexclude(true);
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 857 "wasaparse.tab.cpp"
    break;

  case 17: // complexfieldname: WORD
#line 210 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("cfn: WORD" << endl);
    (yylhs.value.str) = (yystack_[0].value.str);
}
#line 866 "wasaparse.tab.cpp"
    break;

  case 18: // complexfieldname: complexfieldname CONTAINS WORD
#line 216 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("cfn: complexfieldname ':' WORD" << endl);
    (yylhs.value.str) = new string(*(yystack_[2].value.str) + string(":") + *(yystack_[0].value.str));
    delete (yystack_[2].value.str);
    delete (yystack_[0].value.str);
}
#line 877 "wasaparse.tab.cpp"
    break;

  case 19: // range: WORD RANGE WORD
#line 225 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("Range: " << *(yystack_[2].value.str) << string(" .. ") << *(yystack_[0].value.str) << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange(*(yystack_[2].value.str), *(yystack_[0].value.str));
    delete (yystack_[2].value.str);
    delete (yystack_[0].value.str);
}
#line 888 "wasaparse.tab.cpp"
    break;

  case 20: // range: RANGE WORD
#line 233 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("Range: " << "" << string(" .. ") << *(yystack_[0].value.str) << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange("", *(yystack_[0].value.str));
    delete (yystack_[0].value.str);
}
#line 898 "wasaparse.tab.cpp"
    break;

  case 21: // range: WORD RANGE
#line 240 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("Range: " << *(yystack_[1].value.str) << string(" .. ") << "" << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange(*(yystack_[1].value.str), "");
    delete (yystack_[1].value.str);
}
#line 908 "wasaparse.tab.cpp"
    break;

  case 22: // term: WORD
#line 249 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("term[" << *(yystack_[0].value.str) << "]" << endl);
    (yylhs.value.cl) = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, *(yystack_[0].value.str));
    delete (yystack_[0].value.str);
}
#line 918 "wasaparse.tab.cpp"
    break;

  case 23: // term: qualquote
#line 255 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 926 "wasaparse.tab.cpp"
    break;

  case 24: // qualquote: QUOTED
#line 261 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("QUOTED[" << *(yystack_[0].value.str) << "]" << endl);
    (yylhs.value.cl) = new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, *(yystack_[0].value.str), 0);
    delete (yystack_[0].value.str);
}
#line 936 "wasaparse.tab.cpp"
    break;

  case 25: // qualquote: QUOTED QUALIFIERS
#line 267 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"
{
    LOGP("QUOTED[" << *(yystack_[1].value.str) << "] QUALIFIERS[" << *(yystack_[0].value.str) << "]" << endl);
    Rcl::SearchDataClauseDist *cl = 
        new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, *(yystack_[1].value.str), 0);
    qualify(cl, *(yystack_[0].value.str));
    (yylhs.value.cl) = cl;
    delete (yystack_[1].value.str);
    delete (yystack_[0].value.str);
}
#line 950 "wasaparse.tab.cpp"
    break;


#line 954 "wasaparse.tab.cpp"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        context yyctx (*this, yyla);
        std::string msg = yysyntax_error_ (yyctx);
        error (yyla.location, YY_MOVE (msg));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
              {
                yyn = yytable_[yyn];
                if (0 < yyn)
                  break;
              }
          }

        // Pop the current state because it cannot handle the error token.
        if (yystack_.size () == 1)
          YYABORT;

        yyerror_range[1].location = yystack_[0].location;
        yy_destroy_ ("Error: popping", yystack_[0]);
        yypop_ ();
        YY_STACK_PRINT ();
      }
    {
      stack_symbol_type error_token;

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  parser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what ());
  }

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  parser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr;
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              else
                goto append;

            append:
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }

  std::string
  parser::symbol_name (symbol_kind_type yysymbol)
  {
    return yytnamerr_ (yytname_[yysymbol]);
  }



  // parser::context.
  parser::context::context (const parser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  parser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
  {
    // Actual number of expected tokens
    int yycount = 0;

    const int yyn = yypact_[+yyparser_.yystack_[0].state];
    if (!yy_pact_value_is_default_ (yyn))
      {
        /* Start YYX at -YYN if negative to avoid negative indexes in
           YYCHECK.  In other words, skip the first -YYN actions for
           this state because they are default actions.  */
        const int yyxbegin = yyn < 0 ? -yyn : 0;
        // Stay within bounds of both yycheck and yytname.
        const int yychecklim = yylast_ - yyn + 1;
        const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
        for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
          if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror
              && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
            {
              if (!yyarg)
                ++yycount;
              else if (yycount == yyargn)
                return 0;
              else
                yyarg[yycount++] = YY_CAST (symbol_kind_type, yyx);
            }
      }

    if (yyarg && yycount == 0 && 0 < yyargn)
      yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
  }






  int
  parser::yy_syntax_error_arguments_ (const context& yyctx,
                                                 symbol_kind_type yyarg[], int yyargn) const
  {
    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */

    if (!yyctx.lookahead ().empty ())
      {
        if (yyarg)
          yyarg[0] = yyctx.token ();
        int yyn = yyctx.expected_tokens (yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
      }
    return 0;
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (const context& yyctx) const
  {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_ (yyctx, yyarg, YYARGS_MAX);

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
      default: // Avoid compiler warnings.
        YYCASE_ (0, YY_("syntax error"));
        YYCASE_ (1, YY_("syntax error, unexpected %s"));
        YYCASE_ (2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_ (3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_ (4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_ (5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += symbol_name (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char parser::yypact_ninf_ = -3;

  const signed char parser::yytable_ninf_ = -19;

  const signed char
  parser::yypact_[] =
  {
      31,    32,     3,    31,    33,     6,    14,    -3,    38,    -3,
      -3,    -3,     1,    -3,    -3,    31,    31,     4,    -2,     9,
      -2,    -2,    -2,    -2,    -3,     4,    -3,    -3,    -3,    16,
      18,    -3,    -3,    -3,    -3,    -3,    -3,    22,    -3,    -3
  };

  const signed char
  parser::yydefact_[] =
  {
       0,    22,    24,     0,     0,     0,     2,     7,     0,     8,
      23,    25,     0,    16,     1,     0,     0,     3,     0,     0,
       0,     0,     0,     0,     6,     4,     5,    22,     9,    22,
       0,    11,    10,    13,    12,    15,    14,    21,    20,    19
  };

  const signed char
  parser::yypgoto_[] =
  {
      -3,    -3,     0,    34,    -3,    -3,    37,    -3
  };

  const signed char
  parser::yydefgoto_[] =
  {
       0,     5,    17,     7,     8,    31,     9,    10
  };

  const signed char
  parser::yytable_[] =
  {
       6,    27,     2,    12,     1,     2,    14,    15,    11,     3,
       4,    16,    29,     2,    16,    25,    26,     1,     2,    24,
      15,    38,     3,     4,    16,    39,    30,   -18,   -18,   -18,
     -18,   -18,   -18,    37,     1,     2,     1,     2,    13,     3,
       4,     0,     4,   -17,   -17,   -17,   -17,   -17,   -17,    18,
      19,    20,    21,    22,    23,    28,    32,    33,    34,    35,
      36
  };

  const signed char
  parser::yycheck_[] =
  {
       0,     3,     4,     3,     3,     4,     0,     6,     5,     8,
       9,    10,     3,     4,    10,    15,    16,     3,     4,    18,
       6,     3,     8,     9,    10,     3,    17,    11,    12,    13,
      14,    15,    16,    17,     3,     4,     3,     4,     4,     8,
       9,    -1,     9,    11,    12,    13,    14,    15,    16,    11,
      12,    13,    14,    15,    16,    18,    19,    20,    21,    22,
      23
  };

  const signed char
  parser::yystos_[] =
  {
       0,     3,     4,     8,     9,    20,    21,    22,    23,    25,
      26,     5,    21,    22,     0,     6,    10,    21,    11,    12,
      13,    14,    15,    16,    18,    21,    21,     3,    25,     3,
      17,    24,    25,    25,    25,    25,    25,    17,     3,     3
  };

  const signed char
  parser::yyr1_[] =
  {
       0,    19,    20,    21,    21,    21,    21,    21,    22,    22,
      22,    22,    22,    22,    22,    22,    22,    23,    23,    24,
      24,    24,    25,    25,    26,    26
  };

  const signed char
  parser::yyr2_[] =
  {
       0,     2,     1,     2,     3,     3,     3,     1,     1,     3,
       3,     3,     3,     3,     3,     3,     2,     1,     3,     3,
       2,     2,     1,     1,     1,     2
  };


#if YYDEBUG || 1
  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
  const char*
  const parser::yytname_[] =
  {
  "\"end of file\"", "error", "\"invalid token\"", "WORD", "QUOTED",
  "QUALIFIERS", "AND", "UCONCAT", "'('", "'-'", "OR", "EQUALS", "CONTAINS",
  "SMALLEREQ", "SMALLER", "GREATEREQ", "GREATER", "RANGE", "')'",
  "$accept", "topquery", "query", "fieldexpr", "complexfieldname", "range",
  "term", "qualquote", YY_NULLPTR
  };
#endif


#if YYDEBUG
  const short
  parser::yyrline_[] =
  {
       0,    73,    73,    86,    97,   108,   119,   125,   138,   143,
     151,   159,   167,   175,   183,   191,   199,   209,   215,   224,
     232,   239,   248,   254,   260,   266
  };

  void
  parser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  parser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG

  parser::symbol_kind_type
  parser::yytranslate_ (int t) YY_NOEXCEPT
  {
    // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
    // TOKEN-NUM as returned by yylex.
    static
    const signed char
    translate_table[] =
    {
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       8,    18,     2,     2,     2,     9,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,    10,    11,    12,    13,    14,    15,    16,
      17
    };
    // Last valid token kind.
    const int code_max = 270;

    if (t <= 0)
      return symbol_kind::S_YYEOF;
    else if (t <= code_max)
      return static_cast <symbol_kind_type> (translate_table[t]);
    else
      return symbol_kind::S_YYUNDEF;
  }

} // yy
#line 1491 "wasaparse.tab.cpp"

#line 278 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp"


#include <ctype.h>

// Look for int at index, skip and return new index found? value.
static unsigned int qualGetInt(const string& q, unsigned int cur, int *pval)
{
    unsigned int ncur = cur;
    if (cur < q.size() - 1) {
        char *endptr;
        int val = strtol(&q[cur + 1], &endptr, 10);
        if (endptr != &q[cur + 1]) {
            ncur += static_cast<unsigned int>(endptr - &q[cur + 1]);
            *pval = val;
        }
    }
    return ncur;
}

static void qualify(Rcl::SearchDataClauseDist *cl, const string& quals)
{
    // cerr << "qualify(" << cl << ", " << quals << ")" << endl;
    for (unsigned int i = 0; i < quals.length(); i++) {
        //fprintf(stderr, "qual char %c\n", quals[i]);
        switch (quals[i]) {
        case 'b': 
            cl->setWeight(10.0);
            break;
        case 'c': break;
        case 'C': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
            break;
        case 'd': break;
        case 'D':  
            cl->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
            break;
        case 'e': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
            cl->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
            break;
        case 'l': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
            break;
        case 'L': break;
        case 'o':  
        {
            int slack = 10;
            i = qualGetInt(quals, i, &slack);
            cl->setslack(slack);
            //cerr << "set slack " << cl->getslack() << " done" << endl;
        }
        break;
        case 'p': 
            cl->setTp(Rcl::SCLT_NEAR);
            if (cl->getslack() == 0) {
                cl->setslack(10);
                //cerr << "set slack " << cl->getslack() << " done" << endl;
            }
            break;
        case 's': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSYNS);
            break;
	case 'S':
            break;
        case 'x': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_EXPANDPHRASE);
            break;
        case 'w': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOWILDEXP);
            break;
        case '.':case '0':case '1':case '2':case '3':case '4':
        case '5':case '6':case '7':case '8':case '9':
        {
            int n = 0;
            float factor = 1.0;
            if (sscanf(&(quals[i]), "%f %n", &factor, &n)) {
                if (factor != 1.0) {
                    cl->setWeight(factor);
                }
            }
            if (n > 0)
                i += n - 1;
        }
        default:
            break;
        }
    }
}


// specialstartchars are special only at the beginning of a token
// (e.g. doctor-who is a term, not 2 terms separated by '-')
static const string specialstartchars("-");
// specialinchars are special everywhere except inside a quoted string
static const string specialinchars(":=<>()");

// Called with the first dquote already read
static int parseString(WasaParserDriver *d, yy::parser::semantic_type *yylval)
{
    string* value = new string();
    d->qualifiers().clear();
    int c;
    while ((c = d->GETCHAR())) {
        switch (c) {
        case '\\':
            /* Escape: get next char */
            c = d->GETCHAR();
            if (c == 0) {
                value->push_back(c);
                goto out;
            }
            value->push_back(c);
            break;
        case '"':
            /* End of string. Look for qualifiers */
            while ((c = d->GETCHAR()) && (isalnum(c) || c == '.'))
                d->qualifiers().push_back(c);
            d->UNGETCHAR(c);
            goto out;
        default:
            value->push_back(c);
        }
    }
out:
    //cerr << "GOT QUOTED ["<<value<<"] quals [" << d->qualifiers() << "]" << endl;
    yylval->str = value;
    return yy::parser::token::QUOTED;
}


int yylex(yy::parser::semantic_type *yylval, yy::parser::location_type *, 
		  WasaParserDriver *d)
{
    if (!d->qualifiers().empty()) {
        yylval->str = new string();
        yylval->str->swap(d->qualifiers());
        return yy::parser::token::QUALIFIERS;
    }

    int c;

    /* Skip white space.  */
    while ((c = d->GETCHAR()) && isspace(c))
        continue;

    if (c == 0)
        return 0;

    if (specialstartchars.find(c) != string::npos) {
        //cerr << "yylex: return " << c << endl;
        return c;
    }

    // field-term relations, and ranges
    switch (c) {
    case '=': return yy::parser::token::EQUALS;
    case ':': return yy::parser::token::CONTAINS;
    case '<': {
        int c1 = d->GETCHAR();
        if (c1 == '=') {
            return yy::parser::token::SMALLEREQ;
        } else {
            d->UNGETCHAR(c1);
            return yy::parser::token::SMALLER;
        }
    }
    case '.': {
        int c1 = d->GETCHAR();
        if (c1 == '.') {
            return yy::parser::token::RANGE;
        } else {
            d->UNGETCHAR(c1);
            break;
        }
    }
    case '>': {
        int c1 = d->GETCHAR();
        if (c1 == '=') {
            return yy::parser::token::GREATEREQ;
        } else {
            d->UNGETCHAR(c1);
            return yy::parser::token::GREATER;
        }
    }
    case '(': case ')':
        return c;
    }
        
    if (c == '"')
        return parseString(d, yylval);

    d->UNGETCHAR(c);

    // Other chars start a term or field name or reserved word
    string* word = new string();
    while ((c = d->GETCHAR())) {
        if (isspace(c)) {
            //cerr << "Word broken by whitespace" << endl;
            break;
        } else if (specialinchars.find(c) != string::npos) {
            //cerr << "Word broken by special char" << endl;
            d->UNGETCHAR(c);
            break;
        } else if (c == '.') {
            int c1 = d->GETCHAR();
            if (c1 == '.') {
                d->UNGETCHAR(c1);
                d->UNGETCHAR(c);
                break;
            } else {
                d->UNGETCHAR(c1);
                word->push_back(c);
            }
        } else if (c == 0) {
            //cerr << "Word broken by EOF" << endl;
            break;
        } else {
            word->push_back(c);
        }
    }
    
    if (!word->compare("AND") || !word->compare("&&")) {
        delete word;
        return yy::parser::token::AND;
    } else if (!word->compare("OR") || !word->compare("||")) {
        delete word;
        return yy::parser::token::OR;
    }

//    cerr << "Got word [" << word << "]" << endl;
    yylval->str = word;
    return yy::parser::token::WORD;
}
