#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

// TODO concat, int_to_string

constexpr string_literal_t
str_end (string_literal_t const s)
{
    return (* s ? str_end (s + 1) : s);
}

constexpr string_literal_t
path_stem_impl (string_literal_t const begin, string_literal_t const s)
{
    return (* s == '/' ? (s + 1) : (s == begin ? begin : path_stem_impl (begin, s - 1)));
}

constexpr string_literal_t
path_stem (string_literal_t const s)
{
    return path_stem_impl (s, str_end (s));
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
