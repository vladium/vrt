
#line 1 "src/vr/io/parse/CSV_tokenizer.rl"
//
// NOTE: THIS CODE IS AUTO-GENERATED
//
// ragel -C -G2 -I data/grammar/  src/vr/io/parse/CSV_tokenizer.rl && rename .c .cpp src/vr/io/parse/*.c
//
#include "vr/io/parse/CSV_tokenizer.h"

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

#line 27 "src/vr/io/parse/CSV_tokenizer.c"
static const int PARSER_start = 42;
static const int PARSER_first_final = 42;
static const int PARSER_error = 0;

static const int PARSER_en_main = 42;


#line 43 "src/vr/io/parse/CSV_tokenizer.rl"

} // end of anonymous
//............................................................................
//............................................................................

VR_ASSUME_HOT int32_t
CSV_tokenize (string_literal_t const start, std::size_t const size, std::vector<CSV_token> & tokens)
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
        
        CSV_token::enum_t t { CSV_token::size };
            
        
#line 59 "src/vr/io/parse/CSV_tokenizer.c"
	{
	cs = PARSER_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 66 "src/vr/io/parse/CSV_tokenizer.rl"
        
#line 69 "src/vr/io/parse/CSV_tokenizer.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr6:
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
	goto st42;
tr8:
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{{p = ((te))-1;}{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
tr47:
#line 38 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p+1;}
	goto st42;
tr50:
#line 31 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::quoted_name; }
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
tr51:
#line 29 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::num_fp; }
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
tr53:
#line 29 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::num_fp; }
#line 30 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::num_int; }
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
tr58:
#line 28 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::datetime; }
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
tr68:
#line 32 "src/vr/io/parse/CSV_tokenizer.rl"
	{ t = CSV_token::NA_token; }
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{te = p;p--;{ tokens.emplace_back (ts, te - ts, t); }}
	goto st42;
st42:
#line 1 "NONE"
	{ts = 0;}
#line 1 "NONE"
	{act = 0;}
	if ( ++p == pe )
		goto _test_eof42;
case 42:
#line 1 "NONE"
	{ts = p;}
#line 137 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 34: goto st1;
		case 39: goto st3;
		case 44: goto tr47;
		case 46: goto st6;
		case 48: goto tr5;
		case 78: goto st41;
	}
	if ( (*p) < 49 ) {
		if ( 43 <= (*p) && (*p) <= 45 )
			goto st5;
	} else if ( (*p) > 50 ) {
		if ( 51 <= (*p) && (*p) <= 57 )
			goto tr5;
	} else
		goto tr48;
	goto st0;
st0:
cs = 0;
	goto _out;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	switch( (*p) ) {
		case 46: goto st2;
		case 95: goto st2;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st2;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st2;
	} else
		goto st2;
	goto st0;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 34: goto st43;
		case 46: goto st2;
		case 95: goto st2;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st2;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto st2;
	} else
		goto st2;
	goto st0;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
	goto tr50;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
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
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	switch( (*p) ) {
		case 39: goto st43;
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
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	if ( (*p) == 46 )
		goto st6;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr5;
	goto st0;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr7;
	goto tr6;
tr7:
#line 1 "NONE"
	{te = p+1;}
	goto st44;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
#line 257 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr7;
	goto tr51;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	switch( (*p) ) {
		case 43: goto st8;
		case 45: goto st8;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st45;
	goto tr8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st45;
	goto tr8;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st45;
	goto tr51;
tr5:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{act = 1;}
	goto st46;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
#line 300 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 46: goto st6;
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr5;
	goto tr53;
tr48:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{act = 1;}
	goto st47;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
#line 319 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 46: goto st6;
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr54;
	goto tr53;
tr54:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{act = 1;}
	goto st48;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
#line 338 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 46: goto st6;
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr55;
	goto tr53;
tr55:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{act = 1;}
	goto st49;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
#line 357 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 46: goto st6;
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr56;
	goto tr53;
tr56:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/io/parse/CSV_tokenizer.rl"
	{act = 1;}
	goto st50;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
#line 376 "src/vr/io/parse/CSV_tokenizer.c"
	switch( (*p) ) {
		case 45: goto st9;
		case 46: goto st6;
		case 69: goto st7;
		case 101: goto st7;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr5;
	goto tr53;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	switch( (*p) ) {
		case 65: goto st10;
		case 68: goto st26;
		case 70: goto st28;
		case 74: goto st30;
		case 77: goto st33;
		case 78: goto st35;
		case 79: goto st37;
		case 83: goto st39;
	}
	goto tr8;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	switch( (*p) ) {
		case 112: goto st11;
		case 117: goto st25;
	}
	goto tr8;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	if ( (*p) == 114 )
		goto st12;
	goto tr8;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	if ( (*p) == 45 )
		goto st13;
	goto tr8;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( 48 <= (*p) && (*p) <= 51 )
		goto st14;
	goto tr8;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st15;
	goto tr8;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	if ( (*p) == 32 )
		goto st16;
	goto tr8;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( 48 <= (*p) && (*p) <= 50 )
		goto st17;
	goto tr8;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st18;
	goto tr8;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 58 )
		goto st19;
	goto tr8;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	if ( 48 <= (*p) && (*p) <= 53 )
		goto st20;
	goto tr8;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st21;
	goto tr8;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	if ( (*p) == 58 )
		goto st22;
	goto tr8;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( 48 <= (*p) && (*p) <= 53 )
		goto st23;
	goto tr8;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr33;
	goto tr8;
tr33:
#line 1 "NONE"
	{te = p+1;}
	goto st51;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
#line 509 "src/vr/io/parse/CSV_tokenizer.c"
	if ( (*p) == 46 )
		goto st24;
	goto tr58;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st52;
	goto tr8;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st53;
	goto tr58;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st54;
	goto tr58;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st55;
	goto tr58;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st56;
	goto tr58;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st57;
	goto tr58;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st58;
	goto tr58;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st59;
	goto tr58;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st60;
	goto tr58;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	goto tr58;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 103 )
		goto st12;
	goto tr8;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	if ( (*p) == 101 )
		goto st27;
	goto tr8;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	if ( (*p) == 99 )
		goto st12;
	goto tr8;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
	if ( (*p) == 101 )
		goto st29;
	goto tr8;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( (*p) == 98 )
		goto st12;
	goto tr8;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	switch( (*p) ) {
		case 97: goto st31;
		case 117: goto st32;
	}
	goto tr8;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	if ( (*p) == 110 )
		goto st12;
	goto tr8;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
	switch( (*p) ) {
		case 108: goto st12;
		case 110: goto st12;
	}
	goto tr8;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
	if ( (*p) == 97 )
		goto st34;
	goto tr8;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
	switch( (*p) ) {
		case 114: goto st12;
		case 121: goto st12;
	}
	goto tr8;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
	if ( (*p) == 111 )
		goto st36;
	goto tr8;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	if ( (*p) == 118 )
		goto st12;
	goto tr8;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	if ( (*p) == 99 )
		goto st38;
	goto tr8;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	if ( (*p) == 116 )
		goto st12;
	goto tr8;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
	if ( (*p) == 101 )
		goto st40;
	goto tr8;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
	if ( (*p) == 112 )
		goto st12;
	goto tr8;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
	if ( (*p) == 65 )
		goto st61;
	goto st0;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	goto tr68;
	}
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 43: goto tr50;
	case 6: goto tr6;
	case 44: goto tr51;
	case 7: goto tr8;
	case 8: goto tr8;
	case 45: goto tr51;
	case 46: goto tr53;
	case 47: goto tr53;
	case 48: goto tr53;
	case 49: goto tr53;
	case 50: goto tr53;
	case 9: goto tr8;
	case 10: goto tr8;
	case 11: goto tr8;
	case 12: goto tr8;
	case 13: goto tr8;
	case 14: goto tr8;
	case 15: goto tr8;
	case 16: goto tr8;
	case 17: goto tr8;
	case 18: goto tr8;
	case 19: goto tr8;
	case 20: goto tr8;
	case 21: goto tr8;
	case 22: goto tr8;
	case 23: goto tr8;
	case 51: goto tr58;
	case 24: goto tr8;
	case 52: goto tr58;
	case 53: goto tr58;
	case 54: goto tr58;
	case 55: goto tr58;
	case 56: goto tr58;
	case 57: goto tr58;
	case 58: goto tr58;
	case 59: goto tr58;
	case 60: goto tr58;
	case 25: goto tr8;
	case 26: goto tr8;
	case 27: goto tr8;
	case 28: goto tr8;
	case 29: goto tr8;
	case 30: goto tr8;
	case 31: goto tr8;
	case 32: goto tr8;
	case 33: goto tr8;
	case 34: goto tr8;
	case 35: goto tr8;
	case 36: goto tr8;
	case 37: goto tr8;
	case 38: goto tr8;
	case 39: goto tr8;
	case 40: goto tr8;
	case 61: goto tr68;
	}
	}

	_out: {}
	}

#line 67 "src/vr/io/parse/CSV_tokenizer.rl"
                
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
