//----------------------------------------------------------------------------

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER vr_rt

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "vr/rt/tracepoints.h"

#if ! defined (_VR_RT_TP_H) || defined (TRACEPOINT_HEADER_MULTI_READ)
#   define _VR_RT_TP_H

#   include "vr/market/sources/asx/defs.h"

#   include <lttng/tracepoint.h>

//............................................................................
/*
TRACEPOINT_EVENT_CLASS
(
    vr_rt, trade_event
    ,
    TP_ARGS
    (
        vr::timestamp_t,            ts_origin,
        vr::timestamp_t,            ts_local,
        vr::market::ASX::iid_t,     iid,
        vr::market::qty_t,          qty,
        vr::market::price_si_t,     price
    )
    ,
    TP_FIELDS
    (
        ctf_integer (vr::timestamp_t,           ts_origin,  ts_origin)
        ctf_integer (vr::timestamp_t,           ts_local,   ts_local)
        ctf_integer (vr::market::ASX::iid_t,    iid,        iid)
        ctf_integer (vr::market::qty_t,         qty,        qty)
        ctf_integer (vr::market::price_si_t,    price,      price)
    )
)

TRACEPOINT_EVENT_INSTANCE
(
    vr_rt, trade_event, trade
    ,
    TP_ARGS
    (
        vr::timestamp_t,        ts_origin,
        vr::timestamp_t,        ts_local,
        vr::market::ASX::iid_t, iid,
        vr::market::qty_t,      qty,
        vr::market::price_si_t, price
    )
)

TRACEPOINT_LOGLEVEL (vr_rt, trade, TRACE_NOTICE)
*/
//............................................................................

#endif /* _VR_RT_TP_H */

#include <lttng/tracepoint-event.h>

//----------------------------------------------------------------------------
