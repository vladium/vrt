//----------------------------------------------------------------------------

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER vr_test

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "vr/test/tracepoints.h"

#if ! defined (_VR_TEST_TP_H) || defined (TRACEPOINT_HEADER_MULTI_READ)
#   define _VR_TEST_TP_H

#   include "vr/types.h"

#   include <lttng/tracepoint.h>

//............................................................................

TRACEPOINT_ENUM
(
    vr_test, enum_0123
    ,
    TP_ENUM_VALUES
    (
        ctf_enum_value ("ZERO",     0)
        ctf_enum_value ("ONE",      1)
        ctf_enum_value ("TWO",      2)
        ctf_enum_value ("THREE",    3)
    )
)
//............................................................................

TRACEPOINT_EVENT
(
    vr_test, event_INFO
    ,
    TP_ARGS
    (
        std::int64_t,   i8,
        double,         f8,
        std::int32_t,   i4
    )
    ,
    TP_FIELDS
    (
        ctf_integer (std::int64_t,                      ts, i8)
        ctf_float   (double,                            v1, f8)
        ctf_enum    (vr_test, enum_0123, std::int32_t,  v2, i4)
    )
)

TRACEPOINT_LOGLEVEL (vr_test, event_INFO, TRACE_INFO)
//............................................................................

TRACEPOINT_EVENT
(
    vr_test, event_NOTICE // a bit like a test clone of 'vr_rt:trade'
    ,
    TP_ARGS
    (
        std::int64_t,       ts_origin,
        std::int64_t,       ts_local,
        std::int32_t,       iid,
        std::int32_t,       side,
        std::int64_t,       qty,
        std::int64_t,       price
    )
    ,
    TP_FIELDS
    (
        ctf_integer (std::int64_t,                  ts_origin,  ts_origin)
        ctf_integer (std::int64_t,                  ts_local,   ts_local)
        ctf_integer (std::int32_t,                  iid,        iid)
        ctf_enum (vr_test, enum_0123, std::int32_t, side,       side)
        ctf_integer (std::int64_t,                  qty,        qty)
        ctf_integer (std::int64_t,                  price,      price)
    )
)

TRACEPOINT_LOGLEVEL (vr_test, event_NOTICE, TRACE_NOTICE)
//............................................................................

TRACEPOINT_EVENT
(
    vr_test, event_ALERT
    ,
    TP_ARGS
    (
        std::int64_t,   i8,
        std::int32_t,   i4
    )
    ,
    TP_FIELDS
    (
        ctf_integer (std::int64_t,  ts, i8)
        ctf_integer (std::int32_t,  v2, i4)
    )
)

TRACEPOINT_LOGLEVEL (vr_test, event_ALERT, TRACE_ALERT)
//............................................................................

#endif /* _VR_TEST_TP_H */

#include <lttng/tracepoint-event.h>

//----------------------------------------------------------------------------
