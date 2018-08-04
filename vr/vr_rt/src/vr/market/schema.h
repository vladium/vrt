#pragma once

#include "vr/data/attributes_fwd.h"
#include "vr/io/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace schema
{
//............................................................................

constexpr str_const TS_ORIGIN   { "ts_origin" };
constexpr str_const TS_LOCAL    { "ts_local" };

constexpr str_const PRICE       { "px" };
constexpr str_const QTY         { "qty" };
constexpr str_const SIDE        { "side" };

constexpr str_const STATE       { "state" };

constexpr str_const OPTION      { "option" };
constexpr str_const STRIKE      { "px_strike" }; // TODO concat ops for 'str_const'
constexpr str_const EXPIRATION  { "expiration" };

//............................................................................

extern std::vector<data::attribute>
market_book_attributes (io::format::enum_t const fmt, int32_t const depth);

extern std::vector<data::attribute> const &
trade_attributes (io::format::enum_t const fmt);

//............................................................................

} // end of 'schema'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
