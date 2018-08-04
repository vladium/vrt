
#line 1 "src/vr/util/parse/impl/expr_lexer.rl"
//
// NOTE: THIS CODE IS AUTO-GENERATED
//
#include "vr/util/parse/impl/expr_parser_impl.h"
#include "vr/util/parse/impl/parse_tree.h"

#include "vr/util/logging.h"
#include "vr/util/memory.h"

// no header file is generated for these:

extern void * expr_parser_implAlloc (void * (* mallocProc) (size_t));
extern void   expr_parser_implFree (void * parser, void (* freeProc)(void *));
extern void   expr_parser_impl (void * parser, int token_ID, vr::util::parse::expr_token token, vr::util::parse::impl::parse_tree * tree);

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace parse
{
//............................................................................
//............................................................................
namespace
{

#line 31 "src/vr/util/parse/impl/expr_lexer.c"
static const int LEXER_start = 1;
static const int LEXER_first_final = 1;
static const int LEXER_error = 0;

static const int LEXER_en_main = 1;


#line 48 "src/vr/util/parse/impl/expr_lexer.rl"

} // end of anonymous
//............................................................................
//............................................................................

struct parser_ptr final
{
    parser_ptr () :
        m_parser { expr_parser_implAlloc (std::malloc) }
    {
    }
    
    ~parser_ptr ()
    {
        expr_parser_implFree (m_parser, std::free);
    }
    
    addr_t get () const
    {
        return m_parser;
    }
    
    addr_t const m_parser;
    
}; // end of local class
//............................................................................

std::unique_ptr<expr_node>
parse_expr (char_const_ptr_t const start, std::size_t const size)
{    
    parser_ptr pp { };
    addr_t const parser = pp.get ();
    
    string_literal_t p    = start;
    string_literal_t pe   = p + size;
    string_literal_t eof { pe };
    
    int32_t cs { };
    int32_t act VR_UNUSED { };
    
    string_literal_t ts { };
    string_literal_t te { };
    
    int32_t t { };
    impl::parse_tree tree { };
        
    
#line 87 "src/vr/util/parse/impl/expr_lexer.c"
	{
	cs = LEXER_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 95 "src/vr/util/parse/impl/expr_lexer.rl"
    
#line 97 "src/vr/util/parse/impl/expr_lexer.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr0:
#line 43 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p+1;}
	goto st1;
tr8:
#line 33 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_L_PAREN; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
tr9:
#line 34 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_R_PAREN; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
tr10:
#line 32 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_IDENTIFIER; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
tr13:
#line 32 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_IDENTIFIER; }
#line 36 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_AND; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
tr16:
#line 32 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_IDENTIFIER; }
#line 35 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_NOT; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
tr18:
#line 32 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_IDENTIFIER; }
#line 37 "src/vr/util/parse/impl/expr_lexer.rl"
	{ t = vr_EXPR_TOKEN_OR; }
#line 42 "src/vr/util/parse/impl/expr_lexer.rl"
	{te = p;p--;{ expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); }}
	goto st1;
st1:
#line 1 "NONE"
	{ts = 0;}
	if ( ++p == pe )
		goto _test_eof1;
case 1:
#line 1 "NONE"
	{ts = p;}
#line 157 "src/vr/util/parse/impl/expr_lexer.c"
	switch( (*p) ) {
		case 32: goto tr0;
		case 40: goto st2;
		case 41: goto st3;
		case 46: goto st4;
		case 65: goto st5;
		case 78: goto st8;
		case 79: goto st11;
		case 95: goto st4;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr0;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st4;
		} else if ( (*p) >= 66 )
			goto st4;
	} else
		goto st4;
	goto st0;
st0:
cs = 0;
	goto _out;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	goto tr8;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	goto tr9;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	switch( (*p) ) {
		case 46: goto st4;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	switch( (*p) ) {
		case 46: goto st4;
		case 78: goto st6;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	switch( (*p) ) {
		case 46: goto st4;
		case 68: goto st7;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	switch( (*p) ) {
		case 46: goto st4;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr13;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	switch( (*p) ) {
		case 46: goto st4;
		case 79: goto st9;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	switch( (*p) ) {
		case 46: goto st4;
		case 84: goto st10;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	switch( (*p) ) {
		case 46: goto st4;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr16;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	switch( (*p) ) {
		case 46: goto st4;
		case 82: goto st12;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr10;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	switch( (*p) ) {
		case 46: goto st4;
		case 95: goto st4;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st4;
	} else
		goto st4;
	goto tr18;
	}
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 2: goto tr8;
	case 3: goto tr9;
	case 4: goto tr10;
	case 5: goto tr10;
	case 6: goto tr10;
	case 7: goto tr13;
	case 8: goto tr10;
	case 9: goto tr10;
	case 10: goto tr16;
	case 11: goto tr10;
	case 12: goto tr18;
	}
	}

	_out: {}
	}

#line 96 "src/vr/util/parse/impl/expr_lexer.rl"
    
    // clean up allocated tokens in all execution paths here:
            
    if (VR_UNLIKELY (cs < LEXER_first_final))
    {
        LOG_trace1 << "lexer error, cs = " << cs;
        
        std::string const show {(te == nullptr ? std::string { te } :  std::string { start, size }) }; // TODO better capture, trim/prettify
        
        delete tree.m_root;
        throw_x (parse_failure, "failed to tokenize '" + show + '\'');
    }
    
    expr_parser_impl (parser, 0, { nullptr, 0 }, & tree); // end of token stream
    
    LOG_trace1 << "parse rc = " << tree.m_parse_rc;
    
    switch (tree.m_parse_rc)
    {
        case 0: return std::unique_ptr<expr_node> { tree.m_root };
        
        case 1:
        {
            delete tree.m_root;
            throw_x (parse_failure, "failed to parse: a [recoverable] syntax error");
        };
        
        default:
        {
            delete tree.m_root;
            throw_x (parse_failure, "failed to parse: an unrecoverable syntax error");
        };
         
    } // end of switch
}

} // end of 'parse'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
