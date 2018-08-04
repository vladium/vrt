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
%%{
    machine PARSER;
    include CSV "grammars.rl";

        token           =
            datetime    %{ t = CSV_token::datetime; }
          | num_fp      %{ t = CSV_token::num_fp; }
          | num_int     %{ t = CSV_token::num_int; } #// if both 'fp' and 'int' match, choose the weaker 'int' by letting it overwrite 't'
          | qname       %{ t = CSV_token::quoted_name; }
          | NA_token    %{ t = CSV_token::NA_token; }
        ;
        
        main  :=
        |*
              token     => { tokens.emplace_back (ts, te - ts, t); };
              ',';
        *|;
      
    write data;

}%%
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
            
        %% write init;
        %% write exec;
                
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
