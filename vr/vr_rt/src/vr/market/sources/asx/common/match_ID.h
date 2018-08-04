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

VR_META_PACKED_STRUCT (match_ID_t,

    (be_int64_ft,       match)
    (be_int32_ft,       combo_group)

); // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
