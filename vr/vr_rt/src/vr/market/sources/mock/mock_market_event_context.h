#pragma once

#include "vr/fw_string.h"
#include "vr/market/events/market_event_context.h"
#include "vr/meta/tags.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

VR_META_TAG (mock_scenario);

//............................................................................

using mock_market_event_schema  = meta::extend_schema_t<market_event_schema,

    meta::fdef_<std::string,    _mock_scenario_>
>;

//............................................................................

template<typename ... FIELDs>
struct mock_market_event_context: public meta::make_compact_struct_t<meta::select_fields_t<mock_market_event_schema, FIELDs ...>>
{
}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
