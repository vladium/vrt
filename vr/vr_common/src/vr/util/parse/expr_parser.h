#pragma once

#include "vr/util/parse/expressions.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace parse
{

extern std::unique_ptr<expr_node>
parse_expr (char_const_ptr_t const start, std::size_t const size);

inline std::unique_ptr<expr_node>
parse_expr (std::string const & s)
{
    return parse_expr (s.data (), s.size ());
}

} // end of 'parse'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
