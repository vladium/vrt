#pragma once

#include "vr/enums.h"
#include "vr/str_hash.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

#define VR_LOP_SEQ  \
        (NOT)       \
        (AND)       \
        (OR)        \
    /* */

VR_ENUM (lop,
    (
        BOOST_PP_SEQ_ENUM (VR_LOP_SEQ)
    ),
    printable, parsable

); // end of enum

constexpr lop::enum_t
operator "" _lop (char_const_ptr_t const str, std::size_t const len)
{
    switch (operator "" _hash (str, len))
    {
        case "~"_hash:  return lop::NOT;
        case "&"_hash:  return lop::AND;
        case "|"_hash:  return lop::OR;

        default: return lop::na;

    } // end of switch
}
//............................................................................

#define VR_ROP_SEQ  \
        (LT)        \
        (LE)        \
        (EQ)        \
        (NE)        \
        (GE)        \
        (GT)        \
    /* */

VR_ENUM (rop,
    (
        BOOST_PP_SEQ_ENUM (VR_ROP_SEQ)
    ),
    printable, parsable

); // end of enum

constexpr rop::enum_t
operator "" _rop (char_const_ptr_t const str, std::size_t const len)
{
    switch (operator "" _hash (str, len))
    {
        case  "<"_hash: return rop::LT;
        case "<="_hash: return rop::LE;
        case "=="_hash: return rop::EQ;
        case "!="_hash: return rop::NE;
        case ">="_hash: return rop::GE;
        case  ">"_hash: return rop::GT;

        default: return rop::na;

    } // end of switch
}

} // end of namespace
//----------------------------------------------------------------------------
