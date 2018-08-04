#pragma once

#include "vr/enums.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

#define VR_MARKET_SOURCE_SEQ \
        (ASX)                \
    /* */

VR_ENUM (source,
    (
        BOOST_PP_SEQ_ENUM (VR_MARKET_SOURCE_SEQ)
    ),
    iterable, printable, parsable

); // end of enum
//............................................................................
/**
 * these are common/base traits; specialized and overridden in source-specific headers
 */
template<typename source::enum_t SOURCE = source::na, typename ...>
struct source_traits
{
}; // end of default traits

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
