#pragma once

#include "vr/exceptions.h"
#include "vr/io/parse/defs.h" // JSON_token
#include "vr/util/classes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{

using JSON_token_vector          = std::vector<parse::JSON_token>;

template<typename T, typename = void>
struct json_io_traits
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        VR_ASSUME_UNREACHABLE (cn_<T> ()); // needs to be specialized for 'T'
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        VR_ASSUME_UNREACHABLE (cn_<T> ()); // needs to be specialized for 'T'
    }

}; // end of master

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
