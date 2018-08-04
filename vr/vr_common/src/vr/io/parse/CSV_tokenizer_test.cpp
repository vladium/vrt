
#include "vr/io/parse/CSV_tokenizer.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace parse
{

TEST (CSV_tokenizer, sniff)
{
    std::string const line { "NA,110782,2033-Dec-01 23:59:59,+147933,-15768,\"X\",'ABC',+0.22451,\"A\",2030-Oct-17 08:07:06.118048237,-0.22451E-10" };

    std::vector<CSV_token> tokens;
    int32_t const token_count = CSV_tokenize (line, tokens);

    LOG_trace1 << print (tokens);

    ASSERT_EQ (token_count, 11);

    int32_t t { };
    for (CSV_token::enum_t e : { CSV_token::NA_token, CSV_token::num_int, CSV_token::datetime, CSV_token::num_int, CSV_token::num_int, CSV_token::quoted_name, CSV_token::quoted_name, CSV_token::num_fp, CSV_token::quoted_name, CSV_token::datetime, CSV_token::num_fp })
    {
        ASSERT_EQ (tokens [t].m_type, e) << "wrong token " << t << ": " << print (tokens [t]);
        ++ t;
    }
}

} // end of 'parse'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
