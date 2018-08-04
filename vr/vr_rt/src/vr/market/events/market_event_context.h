#pragma once

#include "vr/io/events/event_context.h"
#include "vr/market/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

using market_event_schema  = meta::extend_schema_t<io::event_schema,

    meta::fdef_<int64_t,            _seqnum_>,
    meta::fdef_<int32_t,            _partition_>
>;

//............................................................................

template<typename ... FIELDs>
struct market_event_context: public meta::make_compact_struct_t<meta::select_fields_t<market_event_schema, FIELDs ...>>
{
}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
