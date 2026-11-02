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





#include "ModelParser.hpp"


// Unqualified %code blocks.
#line 44 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"

    //Declared only when ModelParser class is already defined
    YY_DECL;
    void save_location(shared_ptr<ModelAST> m, location location);

#line 52 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"


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

#line 10 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
namespace ModelParserGen {
#line 145 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"

  /// Build a parser object.
  ModelParser::ModelParser (shared_ptr<ModelAST> *result_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      result (result_yyarg)
  {}

  ModelParser::~ModelParser ()
  {}

  ModelParser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  ModelParser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  ModelParser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  ModelParser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  ModelParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  ModelParser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  ModelParser::symbol_kind_type
  ModelParser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  ModelParser::stack_symbol_type::stack_symbol_type ()
  {}

  ModelParser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state), YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_qualifier: // qualifier
        value.YY_MOVE_OR_COPY< DeclQualifier > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_type: // type
        value.YY_MOVE_OR_COPY< Type > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOATL: // "float_literal"
        value.YY_MOVE_OR_COPY< float > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INTL: // "int_literal"
        value.YY_MOVE_OR_COPY< int > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_decl: // decl
      case symbol_kind::S_decl_body: // decl_body
        value.YY_MOVE_OR_COPY< shared_ptr<Decl> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_dist: // dist
        value.YY_MOVE_OR_COPY< shared_ptr<Dist> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_guard: // guard
      case symbol_kind::S_exp: // exp
        value.YY_MOVE_OR_COPY< shared_ptr<Exp> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_location: // location
        value.YY_MOVE_OR_COPY< shared_ptr<Location> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_model: // model
        value.YY_MOVE_OR_COPY< shared_ptr<Model> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_module: // module
        value.YY_MOVE_OR_COPY< shared_ptr<ModuleAST> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_prop: // prop
        value.YY_MOVE_OR_COPY< shared_ptr<Prop> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_transition: // transition
        value.YY_MOVE_OR_COPY< shared_ptr<TransitionAST> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_effects: // effects
        value.YY_MOVE_OR_COPY< shared_vector<Effect> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_exp_seq: // exp_seq
        value.YY_MOVE_OR_COPY< shared_vector<Exp> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pbranches: // pbranches
        value.YY_MOVE_OR_COPY< shared_vector<PBranch> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_proplist: // proplist
        value.YY_MOVE_OR_COPY< shared_vector<Prop> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_ID: // "id"
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_qualifierlist: // qualifierlist
        value.YY_MOVE_OR_COPY< vector<DeclQualifier> > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  ModelParser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s, YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_qualifier: // qualifier
        value.move< DeclQualifier > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_type: // type
        value.move< Type > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOATL: // "float_literal"
        value.move< float > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INTL: // "int_literal"
        value.move< int > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_decl: // decl
      case symbol_kind::S_decl_body: // decl_body
        value.move< shared_ptr<Decl> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_dist: // dist
        value.move< shared_ptr<Dist> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_guard: // guard
      case symbol_kind::S_exp: // exp
        value.move< shared_ptr<Exp> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_location: // location
        value.move< shared_ptr<Location> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_model: // model
        value.move< shared_ptr<Model> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_module: // module
        value.move< shared_ptr<ModuleAST> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_prop: // prop
        value.move< shared_ptr<Prop> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_transition: // transition
        value.move< shared_ptr<TransitionAST> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_effects: // effects
        value.move< shared_vector<Effect> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_exp_seq: // exp_seq
        value.move< shared_vector<Exp> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pbranches: // pbranches
        value.move< shared_vector<PBranch> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_proplist: // proplist
        value.move< shared_vector<Prop> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_ID: // "id"
        value.move< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_qualifierlist: // qualifierlist
        value.move< vector<DeclQualifier> > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  ModelParser::stack_symbol_type&
  ModelParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_qualifier: // qualifier
        value.copy< DeclQualifier > (that.value);
        break;

      case symbol_kind::S_type: // type
        value.copy< Type > (that.value);
        break;

      case symbol_kind::S_FLOATL: // "float_literal"
        value.copy< float > (that.value);
        break;

      case symbol_kind::S_INTL: // "int_literal"
        value.copy< int > (that.value);
        break;

      case symbol_kind::S_decl: // decl
      case symbol_kind::S_decl_body: // decl_body
        value.copy< shared_ptr<Decl> > (that.value);
        break;

      case symbol_kind::S_dist: // dist
        value.copy< shared_ptr<Dist> > (that.value);
        break;

      case symbol_kind::S_guard: // guard
      case symbol_kind::S_exp: // exp
        value.copy< shared_ptr<Exp> > (that.value);
        break;

      case symbol_kind::S_location: // location
        value.copy< shared_ptr<Location> > (that.value);
        break;

      case symbol_kind::S_model: // model
        value.copy< shared_ptr<Model> > (that.value);
        break;

      case symbol_kind::S_module: // module
        value.copy< shared_ptr<ModuleAST> > (that.value);
        break;

      case symbol_kind::S_prop: // prop
        value.copy< shared_ptr<Prop> > (that.value);
        break;

      case symbol_kind::S_transition: // transition
        value.copy< shared_ptr<TransitionAST> > (that.value);
        break;

      case symbol_kind::S_effects: // effects
        value.copy< shared_vector<Effect> > (that.value);
        break;

      case symbol_kind::S_exp_seq: // exp_seq
        value.copy< shared_vector<Exp> > (that.value);
        break;

      case symbol_kind::S_pbranches: // pbranches
        value.copy< shared_vector<PBranch> > (that.value);
        break;

      case symbol_kind::S_proplist: // proplist
        value.copy< shared_vector<Prop> > (that.value);
        break;

      case symbol_kind::S_ID: // "id"
        value.copy< std::string > (that.value);
        break;

      case symbol_kind::S_qualifierlist: // qualifierlist
        value.copy< vector<DeclQualifier> > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }

  ModelParser::stack_symbol_type&
  ModelParser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_qualifier: // qualifier
        value.move< DeclQualifier > (that.value);
        break;

      case symbol_kind::S_type: // type
        value.move< Type > (that.value);
        break;

      case symbol_kind::S_FLOATL: // "float_literal"
        value.move< float > (that.value);
        break;

      case symbol_kind::S_INTL: // "int_literal"
        value.move< int > (that.value);
        break;

      case symbol_kind::S_decl: // decl
      case symbol_kind::S_decl_body: // decl_body
        value.move< shared_ptr<Decl> > (that.value);
        break;

      case symbol_kind::S_dist: // dist
        value.move< shared_ptr<Dist> > (that.value);
        break;

      case symbol_kind::S_guard: // guard
      case symbol_kind::S_exp: // exp
        value.move< shared_ptr<Exp> > (that.value);
        break;

      case symbol_kind::S_location: // location
        value.move< shared_ptr<Location> > (that.value);
        break;

      case symbol_kind::S_model: // model
        value.move< shared_ptr<Model> > (that.value);
        break;

      case symbol_kind::S_module: // module
        value.move< shared_ptr<ModuleAST> > (that.value);
        break;

      case symbol_kind::S_prop: // prop
        value.move< shared_ptr<Prop> > (that.value);
        break;

      case symbol_kind::S_transition: // transition
        value.move< shared_ptr<TransitionAST> > (that.value);
        break;

      case symbol_kind::S_effects: // effects
        value.move< shared_vector<Effect> > (that.value);
        break;

      case symbol_kind::S_exp_seq: // exp_seq
        value.move< shared_vector<Exp> > (that.value);
        break;

      case symbol_kind::S_pbranches: // pbranches
        value.move< shared_vector<PBranch> > (that.value);
        break;

      case symbol_kind::S_proplist: // proplist
        value.move< shared_vector<Prop> > (that.value);
        break;

      case symbol_kind::S_ID: // "id"
        value.move< std::string > (that.value);
        break;

      case symbol_kind::S_qualifierlist: // qualifierlist
        value.move< vector<DeclQualifier> > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  ModelParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  ModelParser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
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
  ModelParser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  ModelParser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  ModelParser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  ModelParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  ModelParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  ModelParser::debug_level_type
  ModelParser::debug_level () const
  {
    return yydebug_;
  }

  void
  ModelParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  ModelParser::state_type
  ModelParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  ModelParser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  ModelParser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  ModelParser::operator() ()
  {
    return parse ();
  }

  int
  ModelParser::parse ()
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
            symbol_type yylookahead (yylex (result));
            yyla.move (yylookahead);
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
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_qualifier: // qualifier
        yylhs.value.emplace< DeclQualifier > ();
        break;

      case symbol_kind::S_type: // type
        yylhs.value.emplace< Type > ();
        break;

      case symbol_kind::S_FLOATL: // "float_literal"
        yylhs.value.emplace< float > ();
        break;

      case symbol_kind::S_INTL: // "int_literal"
        yylhs.value.emplace< int > ();
        break;

      case symbol_kind::S_decl: // decl
      case symbol_kind::S_decl_body: // decl_body
        yylhs.value.emplace< shared_ptr<Decl> > ();
        break;

      case symbol_kind::S_dist: // dist
        yylhs.value.emplace< shared_ptr<Dist> > ();
        break;

      case symbol_kind::S_guard: // guard
      case symbol_kind::S_exp: // exp
        yylhs.value.emplace< shared_ptr<Exp> > ();
        break;

      case symbol_kind::S_location: // location
        yylhs.value.emplace< shared_ptr<Location> > ();
        break;

      case symbol_kind::S_model: // model
        yylhs.value.emplace< shared_ptr<Model> > ();
        break;

      case symbol_kind::S_module: // module
        yylhs.value.emplace< shared_ptr<ModuleAST> > ();
        break;

      case symbol_kind::S_prop: // prop
        yylhs.value.emplace< shared_ptr<Prop> > ();
        break;

      case symbol_kind::S_transition: // transition
        yylhs.value.emplace< shared_ptr<TransitionAST> > ();
        break;

      case symbol_kind::S_effects: // effects
        yylhs.value.emplace< shared_vector<Effect> > ();
        break;

      case symbol_kind::S_exp_seq: // exp_seq
        yylhs.value.emplace< shared_vector<Exp> > ();
        break;

      case symbol_kind::S_pbranches: // pbranches
        yylhs.value.emplace< shared_vector<PBranch> > ();
        break;

      case symbol_kind::S_proplist: // proplist
        yylhs.value.emplace< shared_vector<Prop> > ();
        break;

      case symbol_kind::S_ID: // "id"
        yylhs.value.emplace< std::string > ();
        break;

      case symbol_kind::S_qualifierlist: // qualifierlist
        yylhs.value.emplace< vector<DeclQualifier> > ();
        break;

      default:
        break;
    }


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
  case 2: // begin: model
#line 166 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
                {*result = yystack_[0].value.as < shared_ptr<Model> > (); save_location(*result, yystack_[0].location);}
#line 909 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 3: // begin: model "properties" proplist "endproperties"
#line 168 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{auto &res = yystack_[3].value.as < shared_ptr<Model> > (); res->add_props(yystack_[1].value.as < shared_vector<Prop> > ()); *result = res; save_location(*result, yystack_[3].location);}
#line 915 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 4: // begin: proplist
#line 170 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{//assert model is already created
    assert(*result != nullptr);
    shared_ptr<Model> model = static_pointer_cast<Model>(*result);
    model->add_props(yystack_[0].value.as < shared_vector<Prop> > ());
    save_location(model, yystack_[0].location);
}
#line 926 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 5: // model: decl ";"
#line 179 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Model> > () = make_shared<Model>(yystack_[1].value.as < shared_ptr<Decl> > ()); save_location(yylhs.value.as < shared_ptr<Model> > (), yylhs.location);}
#line 932 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 6: // model: model decl ";"
#line 181 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yystack_[2].value.as < shared_ptr<Model> > ()->add_decl(yystack_[1].value.as < shared_ptr<Decl> > ()); yylhs.value.as < shared_ptr<Model> > () = yystack_[2].value.as < shared_ptr<Model> > (); save_location(yylhs.value.as < shared_ptr<Model> > (), yylhs.location);}
#line 938 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 7: // model: "module" "id" module "endmodule"
#line 183 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yystack_[1].value.as < shared_ptr<ModuleAST> > ()->set_name(yystack_[2].value.as < std::string > ());
    yylhs.value.as < shared_ptr<Model> > () = make_shared<Model>(yystack_[1].value.as < shared_ptr<ModuleAST> > ()); save_location(yylhs.value.as < shared_ptr<Model> > (), yylhs.location);}
#line 945 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 8: // model: model "module" "id" module "endmodule"
#line 186 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    string &id = yystack_[2].value.as < std::string > ();
    shared_ptr<Model> model = yystack_[4].value.as < shared_ptr<Model> > ();
    if (model->has_module(id)) {
	std::cerr << "Two modules with the same name " + id << std::endl;
	exit(1);
    } else {
        yystack_[1].value.as < shared_ptr<ModuleAST> > ()->set_name(id);
        yystack_[4].value.as < shared_ptr<Model> > ()->add_module(yystack_[1].value.as < shared_ptr<ModuleAST> > ());
	yylhs.value.as < shared_ptr<Model> > () = std::move(yystack_[4].value.as < shared_ptr<Model> > ());
    }
    save_location(yylhs.value.as < shared_ptr<Model> > (), yylhs.location);
}
#line 963 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 9: // prop: "P" "(" exp "U" exp ")"
#line 202 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{ yylhs.value.as < shared_ptr<Prop> > () = make_shared<TransientProp>(yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ()); save_location(yylhs.value.as < shared_ptr<Prop> > (), yylhs.location);}
#line 969 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 10: // prop: "S" "(" exp ")"
#line 204 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{ yylhs.value.as < shared_ptr<Prop> > () = make_shared<RateProp>(yystack_[1].value.as < shared_ptr<Exp> > ()); save_location(yylhs.value.as < shared_ptr<Prop> > (), yylhs.location);}
#line 975 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 11: // prop: "S" "[" exp ":" exp "]" "(" exp ")"
#line 206 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{ yylhs.value.as < shared_ptr<Prop> > () = make_shared<TBoundSSProp>(yystack_[6].value.as < shared_ptr<Exp> > (),yystack_[4].value.as < shared_ptr<Exp> > (),yystack_[1].value.as < shared_ptr<Exp> > ()); save_location(yylhs.value.as < shared_ptr<Prop> > (), yylhs.location);}
#line 981 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 12: // proplist: prop
#line 210 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{ yylhs.value.as < shared_vector<Prop> > () = vector<shared_ptr<Prop>>{yystack_[0].value.as < shared_ptr<Prop> > ()};}
#line 987 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 13: // proplist: proplist prop
#line 212 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{ yylhs.value.as < shared_vector<Prop> > () = yystack_[1].value.as < shared_vector<Prop> > () ; yylhs.value.as < shared_vector<Prop> > ().push_back(yystack_[0].value.as < shared_ptr<Prop> > ());}
#line 993 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 14: // type: "int"
#line 216 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < Type > () = Type::tint;}
#line 999 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 15: // type: "bool"
#line 218 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < Type > () = Type::tbool;}
#line 1005 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 16: // type: "float"
#line 220 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < Type > () = Type::tfloat;}
#line 1011 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 17: // module: decl ";"
#line 224 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<ModuleAST> > () = make_shared<ModuleAST>(yystack_[1].value.as < shared_ptr<Decl> > ()); save_location(yylhs.value.as < shared_ptr<ModuleAST> > (), yylhs.location);}
#line 1017 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 18: // module: transition ";"
#line 226 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<ModuleAST> > () = make_shared<ModuleAST>(yystack_[1].value.as < shared_ptr<TransitionAST> > ());}
#line 1023 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 19: // module: module decl ";"
#line 228 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yystack_[2].value.as < shared_ptr<ModuleAST> > ()->add_decl(yystack_[1].value.as < shared_ptr<Decl> > ()); yylhs.value.as < shared_ptr<ModuleAST> > () = yystack_[2].value.as < shared_ptr<ModuleAST> > (); save_location(yylhs.value.as < shared_ptr<ModuleAST> > (), yylhs.location);}
#line 1029 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 20: // module: module transition ";"
#line 230 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yystack_[2].value.as < shared_ptr<ModuleAST> > ()->add_transition(yystack_[1].value.as < shared_ptr<TransitionAST> > ()); yylhs.value.as < shared_ptr<ModuleAST> > () = yystack_[2].value.as < shared_ptr<ModuleAST> > (); save_location(yylhs.value.as < shared_ptr<ModuleAST> > (), yylhs.location);}
#line 1035 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 21: // qualifier: "const"
#line 233 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
                    {yylhs.value.as < DeclQualifier > () = DeclQualifier::constant;}
#line 1041 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 22: // qualifierlist: qualifierlist qualifier
#line 236 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < vector<DeclQualifier> > () = yystack_[1].value.as < vector<DeclQualifier> > (); yylhs.value.as < vector<DeclQualifier> > ().push_back(yystack_[0].value.as < DeclQualifier > ());}
#line 1047 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 23: // qualifierlist: %empty
#line 238 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < vector<DeclQualifier> > () = std::vector<DeclQualifier>();}
#line 1053 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 24: // decl: qualifierlist decl_body
#line 240 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
                                     {
    yylhs.value.as < shared_ptr<Decl> > () = yystack_[0].value.as < shared_ptr<Decl> > ();
    for (DeclQualifier q : yystack_[1].value.as < vector<DeclQualifier> > ()) {
        yylhs.value.as < shared_ptr<Decl> > ()->add_qualifier(q);
    }
}
#line 1064 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 25: // decl_body: "id" ":" "[" exp ".." exp "]"
#line 250 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<RangedDecl>(yystack_[6].value.as < std::string > (), yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Decl> > (), yylhs.location);}
#line 1071 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 26: // decl_body: "id" ":" "[" exp ".." exp "]" "init" exp
#line 253 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<RangedDecl>(yystack_[8].value.as < std::string > (), yystack_[5].value.as < shared_ptr<Exp> > (), yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Decl> > (), yylhs.location);}
#line 1078 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 27: // decl_body: "id" ":" type "init" exp
#line 256 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<InitializedDecl>(yystack_[2].value.as < Type > (), yystack_[4].value.as < std::string > (), yystack_[0].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Decl> > (), yylhs.location);}
#line 1085 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 28: // decl_body: type "id" "=" exp
#line 259 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<InitializedDecl>(yystack_[3].value.as < Type > (), yystack_[2].value.as < std::string > (), yystack_[0].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Decl> > (), yylhs.location);}
#line 1092 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 29: // decl_body: "id" ":" "clock"
#line 262 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<ClockDecl>(yystack_[2].value.as < std::string > ());
	save_location(yylhs.value.as < shared_ptr<Decl> > (), yylhs.location);}
#line 1099 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 30: // decl_body: "id" "[" exp "]" ":" "[" exp ".." exp "]" "init" exp
#line 266 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<RangedInitializedArray>(yystack_[11].value.as < std::string > (), yystack_[9].value.as < shared_ptr<Exp> > (), yystack_[5].value.as < shared_ptr<Exp> > (), yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());}
#line 1105 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 31: // decl_body: "id" "[" exp "]" ":" "[" exp ".." exp "]" "init" "{" exp_seq "}"
#line 269 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<RangedMultipleInitializedArray>
            (yystack_[13].value.as < std::string > (), yystack_[11].value.as < shared_ptr<Exp> > (), yystack_[7].value.as < shared_ptr<Exp> > (), yystack_[5].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_vector<Exp> > ());}
#line 1112 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 32: // decl_body: "id" "[" exp "]" ":" type "init" exp
#line 272 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<InitializedArray>(yystack_[2].value.as < Type > (), yystack_[7].value.as < std::string > (), yystack_[5].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());}
#line 1118 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 33: // decl_body: "id" "[" exp "]" ":" type "init" "{" exp_seq "}"
#line 274 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Decl> > () = make_shared<MultipleInitializedArray>(yystack_[4].value.as < Type > (), yystack_[9].value.as < std::string > (), yystack_[7].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_vector<Exp> > ());}
#line 1124 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 34: // exp_seq: exp_seq "," exp
#line 278 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Exp> > () = concat(yystack_[2].value.as < shared_vector<Exp> > (), shared_vector<Exp>{yystack_[0].value.as < shared_ptr<Exp> > ()});}
#line 1130 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 35: // exp_seq: exp
#line 280 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Exp> > () = shared_vector<Exp>{yystack_[0].value.as < shared_ptr<Exp> > ()};}
#line 1136 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 36: // transition: "[" "id" "?" "]" guard "->" pbranches
#line 284 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<InputTransition>(yystack_[5].value.as < std::string > (), yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > ());
	save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1143 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 37: // transition: "[" "id" "!" "]" guard "@" location "->" pbranches
#line 287 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<OutputTransition>(yystack_[7].value.as < std::string > (), yystack_[4].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > (), yystack_[2].value.as < shared_ptr<Location> > ());
    save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1150 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 38: // transition: "[" "]" guard "@" location "->" pbranches
#line 290 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<TauTransition>(yystack_[4].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > (), yystack_[2].value.as < shared_ptr<Location> > ());
    save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1157 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 39: // transition: "[" "_" "?" "]" guard "->" pbranches
#line 293 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<WildcardInputTransition>(yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > ());
    save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1164 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 40: // transition: "[" "id" "!" "!" "]" guard "->" pbranches
#line 296 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<OutputCommittedTransition>(yystack_[6].value.as < std::string > (), yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > ());
    save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1171 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 41: // transition: "[" "id" "?" "?" "]" guard "->" pbranches
#line 299 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<TransitionAST> > () = make_shared<InputCommittedTransition>(yystack_[6].value.as < std::string > (), yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<PBranch> > ());
	save_location(yylhs.value.as < shared_ptr<TransitionAST> > (), yylhs.location);}
#line 1178 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 42: // location: "id"
#line 304 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Location> > () = make_shared<Location>(yystack_[0].value.as < std::string > ());
	save_location(yylhs.value.as < shared_ptr<Location> > (), yylhs.location);}
#line 1185 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 43: // location: "id" "[" exp "]"
#line 307 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Location> > () = make_shared<ArrayPosition>(yystack_[3].value.as < std::string > (), yystack_[1].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Location> > (), yylhs.location);}
#line 1192 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 44: // guard: %empty
#line 312 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<BConst>(true);
	save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1199 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 45: // guard: exp
#line 315 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = yystack_[0].value.as < shared_ptr<Exp> > ();
	save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1206 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 46: // pbranches: %empty
#line 320 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<PBranch> > () = shared_vector<PBranch>{make_shared<PBranch>(make_shared<FConst>(1.0), shared_vector<Effect>())};}
#line 1212 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 47: // pbranches: effects
#line 322 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<PBranch> > () = shared_vector<PBranch>{make_shared<PBranch>(make_shared<FConst>(1.0), yystack_[0].value.as < shared_vector<Effect> > ())};}
#line 1218 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 48: // pbranches: exp ":" effects
#line 324 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<PBranch> > () = shared_vector<PBranch>{make_shared<PBranch>(yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_vector<Effect> > ())};}
#line 1224 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 49: // pbranches: pbranches "+" pbranches
#line 326 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<PBranch> > () = concat(yystack_[2].value.as < shared_vector<PBranch> > (), yystack_[0].value.as < shared_vector<PBranch> > ());}
#line 1230 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 50: // effects: %empty
#line 330 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Effect> > () = shared_vector<Effect>(); }
#line 1236 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 51: // effects: "(" location "'" "=" exp ")"
#line 332 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Effect> > () = shared_vector<Effect>{make_shared<Assignment>(yystack_[4].value.as < shared_ptr<Location> > (), yystack_[1].value.as < shared_ptr<Exp> > ())};}
#line 1242 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 52: // effects: "(" location "'" "=" dist ")"
#line 334 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Effect> > () = shared_vector<Effect>{make_shared<ClockReset>(yystack_[4].value.as < shared_ptr<Location> > (), yystack_[1].value.as < shared_ptr<Dist> > ())};}
#line 1248 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 53: // effects: effects "&" effects
#line 336 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_vector<Effect> > () = concat(yystack_[2].value.as < shared_vector<Effect> > (), yystack_[0].value.as < shared_vector<Effect> > ());}
#line 1254 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 54: // dist: "erlang" "(" exp "," exp ")"
#line 340 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::erlang, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1261 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 55: // dist: "normal" "(" exp "," exp ")"
#line 343 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::normal, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1268 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 56: // dist: "uniform" "(" exp "," exp ")"
#line 346 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::uniform, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1275 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 57: // dist: "exponential" "(" exp ")"
#line 349 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<SingleParameterDist>(DistType::exponential, yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1282 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 58: // dist: "hyperexponential2" "(" exp "," exp "," exp ")"
#line 352 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::hyperexponential2, yystack_[5].value.as < shared_ptr<Exp> > (), yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1289 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 59: // dist: "lognormal" "(" exp "," exp ")"
#line 355 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::lognormal, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1296 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 60: // dist: "weibull" "(" exp "," exp ")"
#line 358 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::weibull, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1303 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 61: // dist: "gamma" "(" exp "," exp ")"
#line 361 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<MultipleParameterDist>(DistType::gamma, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1310 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 62: // dist: "rayleigh" "(" exp ")"
#line 364 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<SingleParameterDist>(DistType::rayleigh, yystack_[1].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1317 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 63: // dist: "dirac" "(" exp ")"
#line 367 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Dist> > () = make_shared<SingleParameterDist>(DistType::dirac, yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Dist> > (), yylhs.location);}
#line 1324 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 64: // exp: location
#line 381 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<LocExp>(yystack_[0].value.as < shared_ptr<Location> > ()); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1330 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 65: // exp: "int_literal"
#line 383 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<IConst>(yystack_[0].value.as < int > ()); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1336 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 66: // exp: "float_literal"
#line 385 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<FConst>(yystack_[0].value.as < float > ()); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1342 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 67: // exp: "true"
#line 387 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<BConst>(true); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1348 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 68: // exp: "false"
#line 389 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<BConst>(false); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1354 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 69: // exp: exp "+" exp
#line 391 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::plus, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1363 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 70: // exp: exp "-" exp
#line 396 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::minus, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1372 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 71: // exp: exp "/" exp
#line 401 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::div, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1381 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 72: // exp: exp "*" exp
#line 406 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::times, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1390 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 73: // exp: exp "%" exp
#line 411 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::mod, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1399 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 74: // exp: exp "==" exp
#line 416 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::eq, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1408 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 75: // exp: exp "!=" exp
#line 421 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::neq, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1417 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 76: // exp: exp "<" exp
#line 426 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::lt, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1426 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 77: // exp: exp ">" exp
#line 431 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::gt, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1435 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 78: // exp: exp "<=" exp
#line 436 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::le, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1444 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 79: // exp: exp ">=" exp
#line 441 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () =  make_shared<BinOpExp>(ExpOp::ge, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1453 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 80: // exp: exp "&" exp
#line 446 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::andd, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1462 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 81: // exp: exp "=>" exp
#line 451 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::implies, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1471 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 82: // exp: exp "|" exp
#line 456 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::orr, yystack_[2].value.as < shared_ptr<Exp> > (), yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1480 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 83: // exp: "log" "(" exp "," exp ")"
#line 461 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::log, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1487 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 84: // exp: "pow" "(" exp "," exp ")"
#line 464 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::pow, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1496 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 85: // exp: "min" "(" exp "," exp ")"
#line 469 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::min, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1505 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 86: // exp: "max" "(" exp "," exp ")"
#line 474 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::max, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1514 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 87: // exp: "-" exp
#line 479 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::minus, yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1523 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 88: // exp: "!" exp
#line 484 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::nott, yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1532 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 89: // exp: "floor" exp
#line 489 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::floor, yystack_[0].value.as < shared_ptr<Exp> > ());
        save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1539 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 90: // exp: "ceil" exp
#line 492 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::ceil, yystack_[0].value.as < shared_ptr<Exp> > ());
        save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1546 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 91: // exp: "abs" exp
#line 495 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::abs, yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1555 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 92: // exp: "sgn" exp
#line 500 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<UnOpExp>(ExpOp::sgn, yystack_[0].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1564 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 93: // exp: "fsteq" "(" exp "," exp ")"
#line 505 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::fsteq, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1573 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 94: // exp: "lsteq" "(" exp "," exp ")"
#line 510 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::lsteq, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1582 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 95: // exp: "rndeq" "(" exp "," exp ")"
#line 515 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::rndeq, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1591 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 96: // exp: "minfrom" "(" exp "," exp ")"
#line 520 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::minfrom, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1600 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 97: // exp: "maxfrom" "(" exp "," exp ")"
#line 525 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::maxfrom, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1609 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 98: // exp: "sumfrom" "(" exp "," exp ")"
#line 530 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
	yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::sumfrom, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
	save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1618 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 99: // exp: "sumkmax" "(" exp "," exp ")"
#line 535 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
	yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::sumkmax, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1627 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 100: // exp: "consec" "(" exp "," exp ")"
#line 540 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::consec, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1636 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 101: // exp: "broken" "(" exp "," exp ")"
#line 545 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::broken, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1645 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 102: // exp: "fstexclude" "(" exp "," exp ")"
#line 550 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{
    yylhs.value.as < shared_ptr<Exp> > () = make_shared<BinOpExp>(ExpOp::fstexclude, yystack_[3].value.as < shared_ptr<Exp> > (), yystack_[1].value.as < shared_ptr<Exp> > ());
    save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);
}
#line 1654 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;

  case 103: // exp: "(" exp ")"
#line 555 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
{yylhs.value.as < shared_ptr<Exp> > () = yystack_[1].value.as < shared_ptr<Exp> > (); save_location(yylhs.value.as < shared_ptr<Exp> > (), yylhs.location);}
#line 1660 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"
    break;


#line 1664 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"

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
  ModelParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what ());
  }

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  ModelParser::yytnamerr_ (const char *yystr)
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
  ModelParser::symbol_name (symbol_kind_type yysymbol)
  {
    return yytnamerr_ (yytname_[yysymbol]);
  }



  // ModelParser::context.
  ModelParser::context::context (const ModelParser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  ModelParser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
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
  ModelParser::yy_syntax_error_arguments_ (const context& yyctx,
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
  ModelParser::yysyntax_error_ (const context& yyctx) const
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


  const short ModelParser::yypact_ninf_ = -205;

  const signed char ModelParser::yytable_ninf_ = -51;

  const short
  ModelParser::yypact_[] =
  {
       7,   -33,    92,    55,    68,    14,  -205,   -24,    26,   111,
     101,   412,   412,   412,  -205,    36,   -24,   115,  -205,  -205,
    -205,  -205,  -205,     3,    40,  -205,  -205,  -205,    -9,    11,
     120,   121,   412,   412,   412,  -205,  -205,   412,   412,   412,
     412,   113,   116,   117,   118,   133,   135,   136,   139,   140,
     142,   143,   146,   148,   163,   162,  -205,  -205,  -205,  1786,
     167,  1088,   101,     0,  -205,   412,   103,   108,   412,   127,
       9,  -205,   175,   178,  -205,  -205,   207,  -205,  -205,  -205,
    -205,  -205,  -205,   412,   412,   412,   412,   412,   412,   412,
     412,   412,   412,   412,   412,   412,   412,   412,   412,   412,
     412,   412,   412,   412,   412,   412,   412,   412,   412,   412,
     412,   412,   412,  -205,   412,    25,  -205,   961,   412,  -205,
     147,   412,   159,   151,   170,    -5,    -7,  -205,  -205,  -205,
    1130,  1158,  1186,  1214,  1242,  1270,  1298,  1326,  1354,  1382,
    1410,  1438,  1466,  1494,   996,   185,   185,   228,   228,   228,
     228,   151,   981,    95,     2,     2,  -205,  -205,  -205,   466,
    1010,  -205,   196,  1745,   412,   151,   138,   412,   412,   204,
     412,   213,   412,   412,   412,   412,   412,   412,   412,   412,
     412,   412,   412,   412,   412,   412,  -205,  -205,   223,    56,
     412,   151,   184,   186,   189,   412,   233,   412,   482,   503,
     519,   540,   556,   577,   593,   614,   630,   651,   667,   688,
     704,   725,   412,   412,   202,  1045,   308,   308,   308,   206,
     138,   208,  -205,  -205,  -205,  -205,  -205,  -205,  -205,  -205,
    -205,  -205,  -205,  -205,  -205,  -205,   741,  1772,   340,   224,
     412,   216,   221,  1102,   216,   216,   308,   218,   308,  -205,
     412,   412,   151,   412,   251,   308,   260,   260,   216,   308,
     216,  1059,    21,   151,   151,   259,  -205,   138,  -205,   221,
     216,   256,  -205,   412,   268,   251,   380,   151,   280,   291,
     292,   297,   301,   303,   304,   305,   306,   311,   312,   762,
     412,   151,   412,   412,   412,   412,   412,   412,   412,   412,
     412,   412,  -205,  -205,    24,  1522,  1550,  1578,  1606,   778,
    1634,  1662,  1690,   799,   815,  -205,   412,   412,   412,   412,
    -205,   412,   412,   412,  -205,  -205,   836,   852,   873,   889,
    1718,   910,   926,  -205,  -205,  -205,  -205,   412,  -205,  -205,
     947,  -205
  };

  const signed char
  ModelParser::yydefact_[] =
  {
      23,     0,     0,     0,     0,    23,    12,     4,     0,     0,
      23,     0,     0,     0,     1,     0,     0,     0,    13,    14,
      15,    16,    21,     0,     0,    22,    24,     5,     0,    23,
       0,     0,     0,     0,     0,    67,    68,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    42,    65,    66,    64,     0,
       0,     0,    23,     0,     6,     0,     0,     0,    44,     0,
       0,     7,     0,     0,    17,    18,     0,    87,    88,    89,
      90,    92,    91,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    10,     0,    23,     3,     0,     0,    29,
       0,     0,     0,    45,     0,     0,     0,    19,    20,   103,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    74,    75,    76,    77,    78,
      79,    81,    80,    82,    69,    70,    72,    73,    71,     0,
       0,     8,     0,     0,     0,    28,     0,    44,    44,     0,
      44,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    43,     9,     0,     0,
       0,    27,     0,     0,     0,    44,     0,    44,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,    46,    46,     0,
       0,     0,    85,    86,    83,    84,    93,    94,    95,    97,
      96,    98,    99,   100,   101,   102,     0,     0,     0,    25,
       0,    38,    47,     0,    39,    36,    46,     0,    46,    11,
       0,     0,    32,     0,    64,    46,    50,    50,    41,    46,
      40,     0,     0,    35,    26,     0,    49,     0,    53,    48,
      37,     0,    33,     0,     0,     0,     0,    34,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    30,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    52,    51,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    31,     0,     0,     0,     0,
      57,     0,     0,     0,    62,    63,     0,     0,     0,     0,
       0,     0,     0,    54,    55,    59,    56,     0,    60,    61,
       0,    58
  };

  const short
  ModelParser::yypgoto_[] =
  {
    -205,  -205,  -205,    -2,   319,   -63,   262,  -205,  -205,     4,
    -205,    57,   -10,  -118,  -155,  -109,  -204,  -205,   -11
  };

  const short
  ModelParser::yydefgoto_[] =
  {
       0,     4,     5,     6,     7,    24,    29,    25,     8,    30,
      26,   262,    31,    58,   122,   241,   242,   288,   243
  };

  const short
  ModelParser::yytable_[] =
  {
      59,    60,    61,   120,     9,    18,    68,   116,   170,    17,
     168,     1,   193,   194,    -2,   196,    71,    65,    15,    73,
      16,    76,    77,    78,    66,    28,    79,    80,    81,    82,
     161,     2,     3,    72,    19,    20,    21,    22,   272,    28,
     219,   315,   221,   273,   171,   169,   273,    10,   192,   109,
     110,   111,   268,   269,   117,     2,     3,   123,    69,   125,
     126,    18,     2,     3,    19,    20,    21,    12,    14,    13,
     213,    70,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   247,   160,    11,    73,    23,   163,   244,   245,
     165,    19,    20,    21,    27,    28,    62,   118,    64,    72,
      67,   119,   254,    74,    75,    83,   214,   121,    84,    85,
      86,    98,    99,   100,   101,   102,   103,   258,   105,   260,
     107,   108,   109,   110,   111,    87,   266,    88,    89,   275,
     270,    90,    91,   191,    92,    93,   123,   123,    94,   123,
      95,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,    96,    97,   124,   127,   215,
     113,   128,   164,   166,   123,   167,   123,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   236,   237,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   189,    55,   195,
     129,   -51,   -51,   100,   101,   102,   103,   252,   197,    76,
     107,   108,   109,   110,   111,   212,   216,   238,   217,   261,
     263,   218,   264,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   220,   246,   253,
     248,   255,   277,   289,   256,   291,   -51,   -51,   -51,   -51,
     259,   265,   267,   107,   108,   109,   110,   111,   274,   263,
      32,   305,   306,   307,   308,   309,   310,   311,   312,   313,
     314,   276,   292,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   293,   294,   326,   327,   328,   329,   295,
     330,   331,   332,   296,    33,   297,   298,   299,   300,    34,
     240,    35,    36,   301,   115,   302,   340,    37,    38,    39,
      40,    41,    42,    43,    44,    63,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,     0,   304,    55,    56,
      57,   -50,    32,     0,    33,     0,   251,     0,     0,    34,
       0,    35,    36,     0,     0,     0,     0,    37,    38,    39,
      40,    41,    42,    43,    44,     0,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    33,     0,    55,    56,
      57,    34,    32,    35,    36,     0,   290,     0,     0,    37,
      38,    39,    40,    41,    42,    43,    44,     0,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,     0,     0,
      55,    56,    57,     0,    32,     0,    33,     0,     0,     0,
       0,    34,     0,    35,    36,     0,     0,     0,     0,    37,
      38,    39,    40,    41,    42,    43,    44,     0,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    33,     0,
      55,    56,    57,    34,     0,    35,    36,     0,     0,     0,
       0,    37,    38,    39,    40,    41,    42,    43,    44,   187,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
       0,     0,    55,    56,    57,   222,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   223,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   224,     0,     0,     0,     0,     0,     0,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   225,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   226,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     227,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   228,     0,     0,     0,
       0,     0,     0,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   229,     0,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   230,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   231,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     232,     0,     0,     0,     0,     0,     0,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   233,     0,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   234,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   235,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   249,     0,     0,     0,     0,     0,
       0,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   303,     0,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   320,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   324,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   325,     0,
       0,     0,     0,     0,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   333,
       0,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   334,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   335,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   336,     0,     0,     0,     0,     0,     0,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   338,     0,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   339,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     341,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   162,     0,     0,     0,
       0,     0,     0,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   186,     0,     0,     0,     0,     0,    98,    99,   100,
     101,   102,   103,     0,     0,   188,   107,   108,   109,   110,
     111,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     239,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   271,     0,     0,     0,     0,     0,
       0,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   114,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   257,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   172,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   174,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   175,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   176,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   177,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     178,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   179,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   180,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   181,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   182,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     183,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   184,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   185,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   316,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   317,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     318,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   319,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   321,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   322,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   323,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     337,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   190,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   250,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,     0,     0,     0,     0,
       0,     0,     0,     0,   112
  };

  const short
  ModelParser::yycheck_[] =
  {
      11,    12,    13,    66,     0,     7,    15,     7,    15,     5,
      15,     4,   167,   168,     0,   170,     5,    14,     4,    29,
       6,    32,    33,    34,    21,    14,    37,    38,    39,    40,
       5,    55,    56,    29,     8,     9,    10,    11,    17,    14,
     195,    17,   197,    22,    51,    50,    22,    80,   166,    47,
      48,    49,   256,   257,    65,    55,    56,    68,    67,    50,
      51,    63,    55,    56,     8,     9,    10,    12,     0,    14,
      14,    80,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   220,   114,    12,   115,    80,   118,   217,   218,
     121,     8,     9,    10,     3,    14,    80,    14,     3,   115,
      80,    18,   240,     3,     3,    12,   189,    19,    12,    12,
      12,    36,    37,    38,    39,    40,    41,   246,    43,   248,
      45,    46,    47,    48,    49,    12,   255,    12,    12,   267,
     259,    12,    12,   164,    12,    12,   167,   168,    12,   170,
      12,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   185,    12,    14,    50,     3,   190,
      13,     3,    35,    24,   195,    15,   197,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,   212,   213,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    21,    80,    15,
      13,    36,    37,    38,    39,    40,    41,   238,    15,   240,
      45,    46,    47,    48,    49,    12,    52,    35,    52,   250,
     251,    52,   253,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    24,    52,    35,
      52,    45,   273,   274,    43,   276,    38,    39,    40,    41,
      52,    20,    12,    45,    46,    47,    48,    49,    19,   290,
      12,   292,   293,   294,   295,   296,   297,   298,   299,   300,
     301,    35,    12,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    12,    12,   316,   317,   318,   319,    12,
     321,   322,   323,    12,    46,    12,    12,    12,    12,    51,
      12,    53,    54,    12,    62,    13,   337,    59,    60,    61,
      62,    63,    64,    65,    66,    16,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    -1,   290,    80,    81,
      82,    43,    12,    -1,    46,    -1,    16,    -1,    -1,    51,
      -1,    53,    54,    -1,    -1,    -1,    -1,    59,    60,    61,
      62,    63,    64,    65,    66,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    46,    -1,    80,    81,
      82,    51,    12,    53,    54,    -1,    16,    -1,    -1,    59,
      60,    61,    62,    63,    64,    65,    66,    -1,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    -1,    -1,
      80,    81,    82,    -1,    12,    -1,    46,    -1,    -1,    -1,
      -1,    51,    -1,    53,    54,    -1,    -1,    -1,    -1,    59,
      60,    61,    62,    63,    64,    65,    66,    -1,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    46,    -1,
      80,    81,    82,    51,    -1,    53,    54,    -1,    -1,    -1,
      -1,    59,    60,    61,    62,    63,    64,    65,    66,    13,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      -1,    -1,    80,    81,    82,    13,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    13,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    13,    -1,    -1,    -1,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    13,    -1,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    13,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      13,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    13,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    13,    -1,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    13,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    13,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      13,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    13,    -1,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    13,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    13,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    13,    -1,    -1,    -1,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    13,    -1,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    13,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    13,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    13,    -1,
      -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    13,
      -1,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    13,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    13,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    13,    -1,    -1,    -1,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    13,    -1,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    13,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      13,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    15,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,    -1,    -1,    15,    45,    46,    47,    48,
      49,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    21,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    21,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    22,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    22,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    22,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    22,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    22,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    23,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    23,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    58
  };

  const signed char
  ModelParser::yystos_[] =
  {
       0,     4,    55,    56,    84,    85,    86,    87,    91,    92,
      80,    12,    12,    14,     0,     4,     6,    92,    86,     8,
       9,    10,    11,    80,    88,    90,    93,     3,    14,    89,
      92,    95,    12,    46,    51,    53,    54,    59,    60,    61,
      62,    63,    64,    65,    66,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    80,    81,    82,    96,   101,
     101,   101,    80,    87,     3,    14,    21,    80,    15,    67,
      80,     5,    92,    95,     3,     3,   101,   101,   101,   101,
     101,   101,   101,    12,    12,    12,    12,    12,    12,    12,
      12,    12,    12,    12,    12,    12,    12,    14,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    58,    13,    21,    89,     7,   101,    14,    18,
      88,    19,    97,   101,    50,    50,    51,     3,     3,    13,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,     5,    15,   101,    35,   101,    24,    15,    15,    50,
      15,    51,    22,    22,    22,    22,    22,    22,    22,    22,
      22,    22,    22,    22,    22,    22,    15,    13,    15,    21,
      23,   101,    96,    97,    97,    15,    97,    15,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,    12,    14,    88,   101,    52,    52,    52,    97,
      24,    97,    13,    13,    13,    13,    13,    13,    13,    13,
      13,    13,    13,    13,    13,    13,   101,   101,    35,    15,
      12,    98,    99,   101,    98,    98,    52,    96,    52,    13,
      23,    16,   101,    35,    96,    45,    43,    21,    98,    52,
      98,   101,    94,   101,   101,    20,    98,    12,    99,    99,
      98,    15,    17,    22,    19,    96,    35,   101,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,   100,   101,
      16,   101,    12,    12,    12,    12,    12,    12,    12,    12,
      12,    12,    13,    13,    94,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,    17,    22,    22,    22,    22,
      13,    22,    22,    22,    13,    13,   101,   101,   101,   101,
     101,   101,   101,    13,    13,    13,    13,    22,    13,    13,
     101,    13
  };

  const signed char
  ModelParser::yyr1_[] =
  {
       0,    83,    84,    84,    84,    85,    85,    85,    85,    86,
      86,    86,    87,    87,    88,    88,    88,    89,    89,    89,
      89,    90,    91,    91,    92,    93,    93,    93,    93,    93,
      93,    93,    93,    93,    94,    94,    95,    95,    95,    95,
      95,    95,    96,    96,    97,    97,    98,    98,    98,    98,
      99,    99,    99,    99,   100,   100,   100,   100,   100,   100,
     100,   100,   100,   100,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101
  };

  const signed char
  ModelParser::yyr2_[] =
  {
       0,     2,     1,     4,     1,     2,     3,     4,     5,     6,
       4,     9,     1,     2,     1,     1,     1,     2,     2,     3,
       3,     1,     2,     0,     2,     7,     9,     5,     4,     3,
      12,    14,     8,    10,     3,     1,     7,     9,     7,     7,
       8,     8,     1,     4,     0,     1,     0,     1,     3,     3,
       0,     6,     6,     3,     6,     6,     6,     4,     8,     6,
       6,     6,     4,     4,     1,     1,     1,     1,     1,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     6,     6,     6,     6,     2,     2,     2,
       2,     2,     2,     6,     6,     6,     6,     6,     6,     6,
       6,     6,     6,     3
  };


#if YYDEBUG || 1
  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
  const char*
  const ModelParser::yytname_[] =
  {
  "\"end of file\"", "error", "\"invalid token\"", "\";\"", "\"module\"",
  "\"endmodule\"", "\"properties\"", "\"endproperties\"", "\"int\"",
  "\"bool\"", "\"float\"", "\"const\"", "\"(\"", "\")\"", "\"[\"", "\"]\"",
  "\"{\"", "\"}\"", "\"clock\"", "\"=\"", "\"'\"", "\":\"", "\",\"",
  "\"..\"", "\"@\"", "\"erlang\"", "\"normal\"", "\"lognormal\"",
  "\"uniform\"", "\"exponential\"", "\"hyperexponential2\"", "\"weibull\"",
  "\"gamma\"", "\"rayleigh\"", "\"dirac\"", "\"init\"", "\"==\"", "\"!=\"",
  "\"<\"", "\">\"", "\"<=\"", "\">=\"", "\"=>\"", "\"&\"", "\"|\"",
  "\"+\"", "\"-\"", "\"*\"", "\"%\"", "\"/\"", "\"?\"", "\"!\"", "\"->\"",
  "\"true\"", "\"false\"", "\"P\"", "\"S\"", "\"B\"", "\"U\"", "\"floor\"",
  "\"ceil\"", "\"sgn\"", "\"abs\"", "\"min\"", "\"max\"", "\"log\"",
  "\"pow\"", "\"_\"", "\"fsteq\"", "\"lsteq\"", "\"rndeq\"", "\"maxfrom\"",
  "\"minfrom\"", "\"sumfrom\"", "\"sumkmax\"", "\"consec\"", "\"broken\"",
  "\"fstexclude\"", "UMINUS", "NEG", "\"id\"", "\"int_literal\"",
  "\"float_literal\"", "$accept", "begin", "model", "prop", "proplist",
  "type", "module", "qualifier", "qualifierlist", "decl", "decl_body",
  "exp_seq", "transition", "location", "guard", "pbranches", "effects",
  "dist", "exp", YY_NULLPTR
  };
#endif


#if YYDEBUG
  const short
  ModelParser::yyrline_[] =
  {
       0,   166,   166,   167,   169,   178,   180,   182,   185,   201,
     203,   205,   209,   211,   215,   217,   219,   223,   225,   227,
     229,   233,   235,   237,   240,   249,   252,   255,   258,   261,
     264,   267,   271,   273,   277,   279,   283,   286,   289,   292,
     295,   298,   303,   306,   311,   314,   319,   321,   323,   325,
     329,   331,   333,   335,   339,   342,   345,   348,   351,   354,
     357,   360,   363,   366,   380,   382,   384,   386,   388,   390,
     395,   400,   405,   410,   415,   420,   425,   430,   435,   440,
     445,   450,   455,   460,   463,   468,   473,   478,   483,   488,
     491,   494,   499,   504,   509,   514,   519,   524,   529,   534,
     539,   544,   549,   554
  };

  void
  ModelParser::yy_stack_print_ () const
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
  ModelParser::yy_reduce_print_ (int yyrule) const
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


#line 10 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"
} // ModelParserGen
#line 2642 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParser.cpp"

#line 557 "/mnt/disk-2/ass/2026/single_wolf/260225CPP算法改进/artifact/setup/fig/src/ModelParserGen.yy"

      
void
ModelParserGen::ModelParser::error(const ModelParserGen::location& l,
                                   const std::string& m) {
    std::stringstream ss;
    ss << "at " << l << " " << m;
    ModelAST::on_scanner_error(ss.str());
}

inline void save_location(shared_ptr<ModelAST> m, location loc) {
    shared_ptr<location> ploc = make_shared<location>(loc);
    m->set_file_location(ploc);
}
