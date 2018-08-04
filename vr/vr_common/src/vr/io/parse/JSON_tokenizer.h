#pragma once

#include "vr/io/parse/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace parse
{

extern int32_t
JSON_tokenize (string_literal_t const start, std::size_t const size, std::vector<JSON_token> & dst);

inline int32_t
JSON_tokenize (std::string const & s, std::vector<JSON_token> & dst)
{
    return JSON_tokenize (s.data (), s.size (), dst);
}

} // end of 'parse'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
