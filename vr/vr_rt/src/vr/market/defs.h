#pragma once

#include "vr/enums.h"
#include "vr/fields.h"
#include "vr/fw_string.h"
#include "vr/io/defs.h" // _ts_local_, _ts_origin_, mode::{recv, send, duplex}
#include "vr/meta/tags.h"
#include "vr/util/fixed_precision.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

using agent_ID              = fw_string8;
using liid_t                = int32_t;

//............................................................................

using qty_t                 = int32_t;  // TODO may need to widen this (ITCH)
using order_count_t         = int32_t;
using participant_t         = fw_string8; // note: this is one char wider than ITCH requires; choosing this for speed/alignment

//............................................................................

template<std::size_t SIZE>
using alphanum              = std::array<char, SIZE>;

//............................................................................

// TODO terminology: "message block" ? "event" ? (esp. if derived from source ts)
VR_STRONG_ALIAS (pre_packet,        int32_t)
VR_STRONG_ALIAS (post_packet,       int32_t)

VR_STRONG_ALIAS (pre_message,       int32_t)
VR_STRONG_ALIAS (post_message,      int32_t)

//............................................................................
// generic/common field tags for market-related structures:
// TODO maintain a schema (w/ inheritance) hierarchy of field types/tags elsewhere?:

using io::_ts_origin_;
using io::_ts_local_;
using io::_ts_local_delta_;

VR_META_TAG (loid);
VR_META_TAG (liid);

VR_META_TAG (username);
VR_META_TAG (password);

VR_META_TAG (session);
VR_META_TAG (seqnum);
VR_META_TAG (partition);

VR_META_TAG (msg_count);

VR_META_TAG (type);

VR_META_TAG (oid);
VR_META_TAG (otk);

VR_META_TAG (length);
VR_META_TAG (event_code);

VR_META_TAG (iid);
VR_META_TAG (symbol);
VR_META_TAG (name);
VR_META_TAG (ccy);

VR_META_TAG (price);
VR_META_TAG (side);
VR_META_TAG (qty);
VR_META_TAG (qty_filled);
VR_META_TAG (participant);

VR_META_TAG (reject_code);
VR_META_TAG (cancel_code);

//............................................................................

template<typename CTX> // choice of timestamp to log
using select_display_ts_t   = util::if_t<(has_field<_ts_local_, CTX> ()), _ts_local_, _ts_origin_>;

//............................................................................

struct side final
{
    VR_NESTED_ENUM (
        (
            BID,
            ASK
        ),
        iterable, printable, parsable
    )

    friend constexpr enum_t operator~ (enum_t const s) VR_NOEXCEPT
    {
        return static_cast<enum_t> (ASK - s); // flips 's' to the opposite side
    }

    friend constexpr int32_t sign (enum_t const s) VR_NOEXCEPT
    {
        return (1 - (s << 1)); // BID: +1, ASK: -1
    }

}; // end of enum
//............................................................................

#define VR_MARKET_EVENT_SEQ \
        (quote)             \
        (trade)             \
    /* */

VR_ENUM (event,
    (
        BOOST_PP_SEQ_ENUM (VR_MARKET_EVENT_SEQ)
    ),
    iterable, printable, parsable

); // end of enum
//............................................................................

VR_ENUM (option,
    (
        C/*ALL */,
        P/*UT */
    ),
    iterable, printable, parsable

); // end of enum
//............................................................................

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
