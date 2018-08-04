#pragma once

#include "vr/market/books/defs.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

using book_event_schema = meta::extend_schema_t<market_event_schema,

    meta::fdef_<addr_const_t,       _book_>
>;

//............................................................................

template<typename ... FIELDs>
struct book_event_context: public meta::make_compact_struct_t<meta::select_fields_t<book_event_schema, FIELDs ...>>
{
}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
