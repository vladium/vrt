#pragma once

#include "vr/market/currencies.h"
#include "vr/market/sources/asx/schema.h"
#include "vr/meta/structs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

VR_META_COMPACT_STRUCT (tick_size_entry,

    (price_si_t,            tick_size)
    (price_si_t,            begin)

); // end of class

inline bool
operator== (tick_size_entry const & lhs, tick_size_entry const & rhs)
{
    return ((lhs.tick_size () == rhs.tick_size ()) && (lhs.begin () == rhs.begin ()));
}

inline bool
operator< (tick_size_entry const & lhs, tick_size_entry const & rhs)
{
    return (lhs.begin () < rhs.begin ());
}
//............................................................................
/*
 * TODO eventually need to split into 'security'/'instrument' concepts
 * (to separate "security" from "instrument tradable at a particular venue")
 */
VR_META_COMPACT_STRUCT (instrument,

    (std::string,           symbol)
    (iid_t,                 iid)
    (int32_t,               partition)
    (ITCH_product,          product)
    (currency,              ccy)
    (ISIN_code,             ISIN)
    (std::string,           name)

    (std::vector<tick_size_entry>, tick_table) // note: ordered by 'begin'

); // end of class
//............................................................................

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
