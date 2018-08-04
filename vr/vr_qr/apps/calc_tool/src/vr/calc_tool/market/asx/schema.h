#pragma once

#include "vr/data/attributes_fwd.h"
#include "vr/io/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

constexpr int32_t auction_calc_depth ()     { return 5; }

extern std::vector<data::attribute> const &
auction_calc_attributes (io::format::enum_t const fmt);

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
