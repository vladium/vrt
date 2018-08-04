
#include "vr/util/parse.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

string_vector
split (std::string const & s, string_literal_t const separators, bool keep_empty_tokens)
{
    string_vector r { };

    boost::split (r, s, boost::is_any_of (separators), (keep_empty_tokens ? boost::token_compress_off : boost::token_compress_on));

    return r;
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
