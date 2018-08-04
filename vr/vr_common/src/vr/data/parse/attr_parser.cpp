
#line 1 "src/vr/data/parse/attr_parser.rl"
//
// NOTE: THIS CODE IS AUTO-GENERATED
//
// ragel -C -G2 -I data/grammar/  src/vr/data/parse/attr_parser.rl && rename .c .cpp src/vr/data/parse/*.c
//
#include "vr/data/parse/attr_parser.h"

#include "vr/data/attributes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
namespace parse
{
//............................................................................
//............................................................................
namespace
{

#line 25 "src/vr/data/parse/attr_parser.c"
static const int PARSER_start = 29;
static const int PARSER_first_final = 29;
static const int PARSER_error = 0;

static const int PARSER_en_main = 29;


#line 53 "src/vr/data/parse/attr_parser.rl"

} // end of anonymous
//............................................................................
//............................................................................

std::vector<attribute>
attr_parser::parse (std::string const & s)
{
    std::vector<attribute> attrs;
    {
        string_literal_t p    = s.c_str ();
        string_literal_t pe   = p + s.size ();
        string_literal_t eof { pe };
        
        int32_t cs { };
        int32_t act VR_UNUSED { };
        
        string_literal_t ts { };
        string_literal_t te { };
        
        string_vector labels;
        string_vector attr_names;
        attr_type at { atype::timestamp };
    
        
#line 59 "src/vr/data/parse/attr_parser.c"
	{
	cs = PARSER_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 78 "src/vr/data/parse/attr_parser.rl"
        
#line 69 "src/vr/data/parse/attr_parser.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr15:
#line 46 "src/vr/data/parse/attr_parser.rl"
	{{p = ((te))-1;}}
	goto st29;
tr52:
#line 47 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr53:
#line 35 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr56:
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr59:
#line 36 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr62:
#line 33 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr65:
#line 34 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr68:
#line 32 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::timestamp }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
tr72:
#line 37 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { label_array::create_and_intern (labels) }; labels.clear (); }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
#line 46 "src/vr/data/parse/attr_parser.rl"
	{te = p;p--;}
	goto st29;
st29:
#line 1 "NONE"
	{ts = 0;}
	if ( ++p == pe )
		goto _test_eof29;
case 29:
#line 1 "NONE"
	{ts = p;}
#line 143 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st30;
		case 46: goto tr8;
		case 95: goto tr8;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st30;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr8;
		} else if ( (*p) >= 65 )
			goto tr8;
	} else
		goto tr8;
	goto st0;
st0:
cs = 0;
	goto _out;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	if ( (*p) == 32 )
		goto st30;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st30;
	goto tr52;
tr8:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ ts = p; }
	goto st1;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
#line 181 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr0;
		case 44: goto tr2;
		case 46: goto st1;
		case 58: goto tr4;
		case 95: goto st1;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr0;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st1;
		} else if ( (*p) >= 65 )
			goto st1;
	} else
		goto st1;
	goto st0;
tr0:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
#line 209 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st2;
		case 44: goto st3;
		case 58: goto st4;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st2;
	goto st0;
tr2:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 226 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st3;
		case 46: goto tr8;
		case 95: goto tr8;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st3;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr8;
		} else if ( (*p) >= 65 )
			goto tr8;
	} else
		goto tr8;
	goto st0;
tr4:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st4;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
#line 252 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st4;
		case 102: goto st5;
		case 105: goto st22;
		case 116: goto st23;
		case 123: goto st26;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st4;
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	switch( (*p) ) {
		case 52: goto st31;
		case 56: goto st34;
	}
	goto st0;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	switch( (*p) ) {
		case 32: goto tr54;
		case 59: goto tr55;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr54;
	goto tr53;
tr54:
#line 35 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
tr60:
#line 36 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
tr63:
#line 33 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
tr66:
#line 34 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
tr69:
#line 32 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::timestamp }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
tr73:
#line 37 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { label_array::create_and_intern (labels) }; labels.clear (); }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st32;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
#line 323 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st32;
		case 59: goto tr58;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st32;
	goto tr56;
tr58:
#line 1 "NONE"
	{te = p+1;}
	goto st33;
tr55:
#line 1 "NONE"
	{te = p+1;}
#line 35 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
tr61:
#line 1 "NONE"
	{te = p+1;}
#line 36 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::f8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
tr64:
#line 1 "NONE"
	{te = p+1;}
#line 33 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i4 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
tr67:
#line 1 "NONE"
	{te = p+1;}
#line 34 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::i8 }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
tr70:
#line 1 "NONE"
	{te = p+1;}
#line 32 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { atype::timestamp }; }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
tr74:
#line 1 "NONE"
	{te = p+1;}
#line 37 "src/vr/data/parse/attr_parser.rl"
	{ new (& at) attr_type { label_array::create_and_intern (labels) }; labels.clear (); }
#line 40 "src/vr/data/parse/attr_parser.rl"
	{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); }
	goto st33;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
#line 387 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st6;
		case 46: goto tr17;
		case 95: goto tr17;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st6;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr17;
		} else if ( (*p) >= 65 )
			goto tr17;
	} else
		goto tr17;
	goto tr56;
tr19:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st6;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
#line 413 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st6;
		case 46: goto tr17;
		case 95: goto tr17;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st6;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr17;
		} else if ( (*p) >= 65 )
			goto tr17;
	} else
		goto tr17;
	goto tr15;
tr17:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ ts = p; }
	goto st7;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
#line 439 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr18;
		case 44: goto tr19;
		case 46: goto st7;
		case 58: goto tr21;
		case 95: goto st7;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr18;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st7;
		} else if ( (*p) >= 65 )
			goto st7;
	} else
		goto st7;
	goto tr15;
tr18:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
#line 467 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st8;
		case 44: goto st6;
		case 58: goto st9;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st8;
	goto tr15;
tr21:
#line 25 "src/vr/data/parse/attr_parser.rl"
	{ attr_names.emplace_back (ts, p - ts);  }
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 484 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st9;
		case 102: goto st10;
		case 105: goto st11;
		case 116: goto st12;
		case 123: goto st19;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st9;
	goto tr15;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	switch( (*p) ) {
		case 52: goto st31;
		case 56: goto st34;
	}
	goto tr15;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
	switch( (*p) ) {
		case 32: goto tr60;
		case 59: goto tr61;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr60;
	goto tr59;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	switch( (*p) ) {
		case 52: goto st35;
		case 56: goto st36;
	}
	goto tr15;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
	switch( (*p) ) {
		case 32: goto tr63;
		case 59: goto tr64;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr63;
	goto tr62;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	switch( (*p) ) {
		case 32: goto tr66;
		case 59: goto tr67;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr66;
	goto tr65;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	switch( (*p) ) {
		case 105: goto st13;
		case 115: goto st38;
	}
	goto tr15;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( (*p) == 109 )
		goto st14;
	goto tr15;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( (*p) == 101 )
		goto tr33;
	goto tr15;
tr33:
#line 1 "NONE"
	{te = p+1;}
	goto st37;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
#line 577 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr69;
		case 59: goto tr70;
		case 115: goto st15;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr69;
	goto tr68;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	if ( (*p) == 116 )
		goto st16;
	goto tr15;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) == 97 )
		goto st17;
	goto tr15;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( (*p) == 109 )
		goto st18;
	goto tr15;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 112 )
		goto st38;
	goto tr15;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	switch( (*p) ) {
		case 32: goto tr69;
		case 59: goto tr70;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr69;
	goto tr68;
tr39:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ labels.emplace_back (ts, p - ts);  }
	goto st19;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
#line 633 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st19;
		case 46: goto tr37;
		case 95: goto tr37;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st19;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr37;
		} else if ( (*p) >= 65 )
			goto tr37;
	} else
		goto tr37;
	goto tr15;
tr37:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ ts = p; }
	goto st20;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
#line 659 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr38;
		case 44: goto tr39;
		case 46: goto st20;
		case 95: goto st20;
		case 125: goto tr41;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr38;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st20;
		} else if ( (*p) >= 65 )
			goto st20;
	} else
		goto st20;
	goto tr15;
tr38:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ labels.emplace_back (ts, p - ts);  }
	goto st21;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
#line 687 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st21;
		case 44: goto st19;
		case 125: goto st39;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st21;
	goto tr15;
tr41:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ labels.emplace_back (ts, p - ts);  }
	goto st39;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
#line 704 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr73;
		case 59: goto tr74;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr73;
	goto tr72;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	switch( (*p) ) {
		case 52: goto st35;
		case 56: goto st36;
	}
	goto st0;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	switch( (*p) ) {
		case 105: goto st24;
		case 115: goto st38;
	}
	goto st0;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( (*p) == 109 )
		goto st25;
	goto st0;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 101 )
		goto tr33;
	goto st0;
tr48:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ labels.emplace_back (ts, p - ts);  }
	goto st26;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
#line 752 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st26;
		case 46: goto tr46;
		case 95: goto tr46;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto st26;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto tr46;
		} else if ( (*p) >= 65 )
			goto tr46;
	} else
		goto tr46;
	goto st0;
tr46:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ ts = p; }
	goto st27;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
#line 778 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto tr47;
		case 44: goto tr48;
		case 46: goto st27;
		case 95: goto st27;
		case 125: goto tr41;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr47;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st27;
		} else if ( (*p) >= 65 )
			goto st27;
	} else
		goto st27;
	goto st0;
tr47:
#line 28 "src/vr/data/parse/attr_parser.rl"
	{ labels.emplace_back (ts, p - ts);  }
	goto st28;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
#line 806 "src/vr/data/parse/attr_parser.c"
	switch( (*p) ) {
		case 32: goto st28;
		case 44: goto st26;
		case 125: goto st39;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto st28;
	goto st0;
	}
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 30: goto tr52;
	case 31: goto tr53;
	case 32: goto tr56;
	case 33: goto tr56;
	case 6: goto tr15;
	case 7: goto tr15;
	case 8: goto tr15;
	case 9: goto tr15;
	case 10: goto tr15;
	case 34: goto tr59;
	case 11: goto tr15;
	case 35: goto tr62;
	case 36: goto tr65;
	case 12: goto tr15;
	case 13: goto tr15;
	case 14: goto tr15;
	case 37: goto tr68;
	case 15: goto tr15;
	case 16: goto tr15;
	case 17: goto tr15;
	case 18: goto tr15;
	case 38: goto tr68;
	case 19: goto tr15;
	case 20: goto tr15;
	case 21: goto tr15;
	case 39: goto tr72;
	}
	}

	_out: {}
	}

#line 79 "src/vr/data/parse/attr_parser.rl"
        
        util::destruct (at);
        
        if (VR_UNLIKELY (cs < PARSER_first_final))
        {
            if (te == nullptr)
                throw_x (parse_failure, "failed to " + ((cs == PARSER_error ? "tokenize '" : "parse '") + s) + '\'');  // TODO better capture, trim/prettify
            else
            {    
                std::string const remainder { te }; // TODO better capture, trim/prettify
                
                throw_x (parse_failure, "failed to " + ((cs == PARSER_error ? "tokenize '" : "parse '") + remainder) + '\''); 
            }
        }
    }
    
    return attrs; // counting on RVO
}

} // end of 'parse'
} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
