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
%%{
    machine LEXER;
    include expressions "expressions.rl";

        token           =
            name        %{ t = vr_EXPR_TOKEN_IDENTIFIER; }
          | l_paren     %{ t = vr_EXPR_TOKEN_L_PAREN; }
          | r_paren     %{ t = vr_EXPR_TOKEN_R_PAREN; }
          | lNOT        %{ t = vr_EXPR_TOKEN_NOT; }
          | lAND        %{ t = vr_EXPR_TOKEN_AND; }
          | lOR         %{ t = vr_EXPR_TOKEN_OR; }          
        ;
        
        main  :=
        |*
              token     => { expr_parser_impl (parser, t, { ts, static_cast<str_range::size_type> (te - ts) }, & tree); };
              space;
        *|;
      
    write data;

}%%
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
        
    %% write init;
    %% write exec;
    
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
