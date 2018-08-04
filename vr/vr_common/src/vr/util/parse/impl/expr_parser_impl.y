%include
{
//
// NOTE: THIS CODE IS AUTO-GENERATED
//

#include "vr/util/parse/impl/parse_tree.h"

VR_DIAGNOSTIC_IGNORE ("-Wunused-variable")

using namespace vr::util::parse;
}
//............................................................................

%name           expr_parser_impl

%token_prefix   vr_EXPR_TOKEN_
%token_type     { expr_token }  // tokens passed by value, don't need to be destructed

%extra_argument { impl::parse_tree * tree }

%type       expr    { expr_node * }
%destructor expr    { delete $$; }

//............................................................................
// operators, in increasing precedence order:

%left   OR.
%left   AND.

%right  NOT.

//............................................................................
// grammar:

start       ::= expr(E).                    { tree->m_root = E; } 

expr(E)     ::= expr(LHS) AND expr(RHS).    { E = new binary_logical_expr { LHS, vr::lop::AND, RHS }; }
expr(E)     ::= expr(LHS) OR  expr(RHS).    { E = new binary_logical_expr { LHS, vr::lop::OR,  RHS }; }

expr(E)     ::= NOT expr(RHS).              { E = new unary_logical_expr { vr::lop::NOT, RHS }; }

expr(E)     ::= IDENTIFIER(ID).             { E = new identifier { ID }; }

expr(E)     ::= L_PAREN expr(RHS) R_PAREN.  { E = RHS; }

//............................................................................

%parse_accept
{
    if (tree->m_parse_rc < 0) tree->m_parse_rc = 0;
}

%syntax_error
{
    tree->m_parse_rc = 1;    
}

%parse_failure
{
    tree->m_parse_rc = 2;
}
//----------------------------------------------------------------------------
