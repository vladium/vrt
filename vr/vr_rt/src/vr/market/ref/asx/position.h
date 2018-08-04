#pragma once

#include "vr/market/sources/asx/schema.h"
#include "vr/meta/structs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
VR_META_COMPACT_STRUCT (position,

    (std::string,                   symbol)
    (iid_t,                         iid)
    (this_source_traits::qty_type,  qty)
    (ISIN_code,                     ISIN)

); // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
