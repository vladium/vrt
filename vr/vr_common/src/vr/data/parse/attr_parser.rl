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
%%{
    machine PARSER;
    include attributes "grammars.rl";

        attr_name       = name >{ ts = p; } %{ attr_names.emplace_back (ts, p - ts);  }; 
        attr_name_list  = attr_name ( WS ',' WS attr_name )* ;
        
        label           = name >{ ts = p; } %{ labels.emplace_back (ts, p - ts);  }; 
        label_list      = '{' WS label ( WS ',' WS label )* WS '}' ;
        
        attr_type       =
            ts %{ new (& at) attr_type { atype::timestamp }; }
          | i4 %{ new (& at) attr_type { atype::i4 }; }
          | i8 %{ new (& at) attr_type { atype::i8 }; }
          | f4 %{ new (& at) attr_type { atype::f4 }; }
          | f8 %{ new (& at) attr_type { atype::f8 }; }
          | label_list %{ new (& at) attr_type { label_array::create_and_intern (labels) }; labels.clear (); }
        ;
        
        attr_group      = attr_name_list WS ':' WS attr_type %{ for (auto const & n : attr_names) attrs.emplace_back (n, at); attr_names.clear (); };  
        attr_group_list = attr_group ( WS ';' WS attr_group )* WS ';'? ;
      
        
        main  :=
        |*
            attr_group_list;
            WS;
        *|;

      
    write data;

}%%
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
    
        %% write init;
        %% write exec;
        
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
