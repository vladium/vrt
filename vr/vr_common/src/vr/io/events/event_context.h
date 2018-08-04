#pragma once

#include "vr/io/net/defs.h"
#include "vr/meta/objects.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{

using event_schema  = meta::make_schema_t
<
    meta::fdef_<timestamp_t,    _ts_local_>,
    meta::fdef_<timestamp_t,    _ts_local_delta_>,
    meta::fdef_<timestamp_t,    _ts_origin_>,

    meta::fdef_<int64_t,        net::_packet_index_>,
    meta::fdef_<uint16_t,       net::_src_port_>,
    meta::fdef_<uint16_t,       net::_dst_port_>,
    meta::fdef_<bool,           net::_filtered_>
>;
//............................................................................

template<typename ... FIELDs>
struct event_context: public meta::make_compact_struct_t<meta::select_fields_t<event_schema, FIELDs ...>>
{
}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
