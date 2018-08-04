
#line 1 "src/vr/io/parse/JSON_tokenizer.rl"
//
// NOTE: THIS CODE IS AUTO-GENERATED
//
// ragel -C -G2 -I data/grammar/  src/vr/io/parse/JSON_tokenizer.rl && rename .c .cpp src/vr/io/parse/*.c
//
#include "vr/io/parse/JSON_tokenizer.h"

#include "vr/asserts.h"

#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace parse
{
//............................................................................
//............................................................................
namespace
{

#line 27 "src/vr/io/parse/JSON_tokenizer.c"
static const int PARSER_start = 20;
static const int PARSER_first_final = 20;
static const int PARSER_error = 0;

static const int PARSER_en_main = 20;


#line 48 "src/vr/io/parse/JSON_tokenizer.rl"

} // end of anonymous
//............................................................................
//............................................................................

VR_ASSUME_HOT int32_t
JSON_tokenize (string_literal_t const start, std::size_t const size, std::vector<JSON_token> & tokens)
{
    assert_empty (tokens);
    {
        string_literal_t p    = start;
        string_literal_t pe   = p + size;
        string_literal_t eof { pe };
        
        int32_t cs { };
        int32_t act VR_UNUSED { };
        
        string_literal_t ts { };
        string_literal_t te { };
        
        JSON_token::enum_t t { JSON_token::size };
            
        
#line 59 "src/vr/io/parse/JSON_tokenizer.c"
	{
	cs = PARSER_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 71 "src/vr/io/parse/JSON_tokenizer.rl"
        
#line 69 "src/vr/io/parse/JSON_tokenizer.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr10:
#line 1 "NONE"
	{	switch( act ) {
	case 0:
	{{goto st0;}}
	break;
	case 1:
	{{p = ((te))-1;} tokens.emplace_back (ts, te - ts, t); }
	break;
	}
	}
	goto st20;
tr12:
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{{p = ((te))-1;}{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr24:
#line 43 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p+1;}
	goto st20;
tr29:
#line 34 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::string; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr30:
#line 32 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::num_fp; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr32:
#line 32 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::num_fp; }
#line 33 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::num_int; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr33:
#line 35 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::eoa; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr34:
#line 31 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::boolean; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
tr35:
#line 30 "src/vr/io/parse/JSON_tokenizer.rl"
	{ t = JSON_token::null; }
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st20;
st20:
#line 1 "NONE"
	{ts = 0;}
#line 1 "NONE"
	{act = 0;}
	if ( ++p == pe )
		goto _test_eof20;
case 20:
#line 1 "NONE"
	{ts = p;}
#line 143 "src/vr/io/parse/JSON_tokenizer.c"
	switch( (*p) ) {
		case 34: goto st1;
		case 44: goto tr24;
		case 46: goto st8;
		case 58: goto tr24;
		case 91: goto tr24;
		case 93: goto st25;
		case 102: goto st11;
		case 110: goto st15;
		case 116: goto st18;
		case 123: goto tr24;
		case 125: goto tr24;
	}
	if ( (*p) > 45 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr9;
	} else if ( (*p) >= 43 )
		goto st7;
	goto st0;
st0:
cs = 0;
	goto _out;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	switch( (*p) ) {
		case 34: goto st21;
		case 92: goto st2;
	}
	goto st1;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	goto tr29;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 34: goto st1;
		case 47: goto st1;
		case 92: goto st1;
		case 98: goto st1;
		case 102: goto st1;
		case 110: goto st1;
		case 114: goto st1;
		case 116: goto st1;
		case 117: goto st3;
	}
	goto st0;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st4;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st4;
	} else
		goto st4;
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st5;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st5;
	} else
		goto st5;
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st6;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st6;
	} else
		goto st6;
	goto st0;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st1;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st1;
	} else
		goto st1;
	goto st0;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	if ( (*p) == 46 )
		goto st8;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr9;
	goto st0;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr11;
	goto tr10;
tr11:
#line 1 "NONE"
	{te = p+1;}
	goto st22;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
#line 272 "src/vr/io/parse/JSON_tokenizer.c"
	switch( (*p) ) {
		case 69: goto st9;
		case 101: goto st9;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr11;
	goto tr30;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	switch( (*p) ) {
		case 43: goto st10;
		case 45: goto st10;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st23;
	goto tr12;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st23;
	goto tr12;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st23;
	goto tr30;
tr9:
#line 1 "NONE"
	{te = p+1;}
#line 42 "src/vr/io/parse/JSON_tokenizer.rl"
	{act = 1;}
	goto st24;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
#line 315 "src/vr/io/parse/JSON_tokenizer.c"
	switch( (*p) ) {
		case 46: goto st8;
		case 69: goto st9;
		case 101: goto st9;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr9;
	goto tr32;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	goto tr33;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	if ( (*p) == 97 )
		goto st12;
	goto st0;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	if ( (*p) == 108 )
		goto st13;
	goto st0;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( (*p) == 115 )
		goto st14;
	goto st0;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( (*p) == 101 )
		goto st26;
	goto st0;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	goto tr34;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	if ( (*p) == 117 )
		goto st16;
	goto st0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) == 108 )
		goto st17;
	goto st0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( (*p) == 108 )
		goto st27;
	goto st0;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	goto tr35;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 114 )
		goto st19;
	goto st0;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	if ( (*p) == 117 )
		goto st14;
	goto st0;
	}
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 21: goto tr29;
	case 8: goto tr10;
	case 22: goto tr30;
	case 9: goto tr12;
	case 10: goto tr12;
	case 23: goto tr30;
	case 24: goto tr32;
	case 25: goto tr33;
	case 26: goto tr34;
	case 27: goto tr35;
	}
	}

	_out: {}
	}

#line 72 "src/vr/io/parse/JSON_tokenizer.rl"
                
        if (VR_UNLIKELY (cs < PARSER_first_final))
        {
            if (te == nullptr)
                throw_x (parse_failure, "failed to " + ((cs == PARSER_error ? "tokenize '" : "parse '") + std::string { start, size }) + '\'');  // TODO better capture, trim/prettify
            else
            {    
                std::string const remainder { te }; // TODO better capture, trim/prettify
                
                throw_x (parse_failure, "failed to " + ((cs == PARSER_error ? "tokenize '" : "parse '") + remainder) + '\''); 
            }
        }
    }
    
    return tokens.size ();
}

} // end of 'parse'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
