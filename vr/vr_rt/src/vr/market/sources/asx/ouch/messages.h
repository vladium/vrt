#pragma once

#include "vr/market/sources/asx/common/match_ID.h"
#include "vr/market/sources/asx/schema.h"
#include "vr/meta/structs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace ouch
{
//............................................................................

template<io::mode::enum_t IO_MODE>
struct message_type; // master

//............................................................................
// nested and sub-structs:

VR_META_PACKED_STRUCT (reg_data_overlay,

    (char,              participant_capacity)
    (char,              directed_wholesale)
    (alphanum_ft<4>,    venue) // needs to be blanks for ASX OUCH
    (alphanum_ft<10>,   intermediary)
    (alphanum_ft<20>,   order_origin)
    (alphanum_ft<8>,    unused) // needs to be blanks
);
//............................................................................
// OUCH, client->server:
//............................................................................

#define VR_MARKET_OUCH_SEND_MESSAGE_SEQ \
        (submit_order)                  \
        (replace_order)                 \
        (cancel_order)                  \
    /* */

template<>
struct message_type<io::mode::send> final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('O',   submit_order)
            ('U',   replace_order)
            ('X',   cancel_order)
        ,
        range, printable, parsable
    );

};// end of enum

using send_message_type     = message_type<io::mode::send>;

//............................................................................

// 'O'
VR_META_PACKED_STRUCT (submit_order,

    (send_message_type, type)

    (otk_ft,            otk)
    (iid_ft,            iid)
    (ord_side,          side)

    (qty_ft,            qty)
    (price_ft,          price)
    (ord_TIF,           TIF)
    (int8_t,            open_close) // unused, set to zero
    (account_ft,        account)

    (alphanum_ft<15>,   customer_info)
    (alphanum_ft<32>,   exchange_info)
    (char,              participant) // clearing participant
    (be_int32_ft,       crossing_key)

    (reg_data_overlay,  reg_data)
    (ord_type,          order_type)
    (qty_ft,            short_sell_qty)
    (qty_ft,            MAQ)

); // end of class

// 'U'
VR_META_PACKED_STRUCT (replace_order,

    (send_message_type, type)

    (otk_ft,            otk)        // note: this is the last token used from any prior modification (NOT the original token)
    (otk_ft,            new_otk)

    (qty_ft,            qty)
    (price_ft,          price)
    (int8_t,            open_close) // unused, set to zero

    (account_ft,        account)
    (alphanum_ft<15>,   customer_info)
    (alphanum_ft<32>,   exchange_info)

    (reg_data_overlay,  reg_data)
    (qty_ft,            short_sell_qty)
    (qty_ft,            MAQ)

); // end of class

// 'X'
VR_META_PACKED_STRUCT (cancel_order,

    (send_message_type, type)

    (otk_ft,            otk)        // note: this is the last token used from any prior modification (NOT the original token)

); // end of class
//............................................................................
// OUCH, server->client:
//............................................................................

#define VR_MARKET_OUCH_RECV_MESSAGE_TIMESTAMPED_SEQ \
        (order_accepted)                \
        (order_rejected)                \
        (order_replaced)                \
        (order_canceled)                \
        (order_execution)               \
    /* */

#define VR_MARKET_OUCH_RECV_MESSAGE_SEQ \
        VR_MARKET_OUCH_RECV_MESSAGE_TIMESTAMPED_SEQ
    /* */

template<>
struct message_type<io::mode::recv> final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('A',   order_accepted)
            ('J',   order_rejected)
            ('U',   order_replaced)
            ('C',   order_canceled)
            ('E',   order_execution)
        ,
        range, printable, parsable
    );

};// end of enum

using recv_message_type     = message_type<io::mode::recv>;

//............................................................................
// nested and sub-structs:

VR_META_PACKED_STRUCT (OUCH_hdr,

    (recv_message_type,  type)
    (be_int64_ft,        ts)

); // end of class
//............................................................................
// messages with 'OUCH_hdr' (and thus timestamped):
//............................................................................

// 'A'
VR_META_PACKED_STRUCT (order_accepted,

    (OUCH_hdr,          hdr)

    (otk_ft,            otk)
    (iid_ft,            iid)
    (ord_side,          side)

    (oid_ft,            oid)

    (qty_ft,            qty)
    (price_ft,          price)
    (ord_TIF,           TIF)
    (int8_t,            open_close) // unused
    (account_ft,        account)

    (ord_state,         state)

    (alphanum_ft<15>,   customer_info)
    (alphanum_ft<32>,   exchange_info)
    (char,              participant) // clearing participant
    (be_int32_ft,       crossing_key)

    (reg_data_overlay,  reg_data)
    (ord_type,          order_type)
    (qty_ft,            short_sell_qty)
    (qty_ft,            MAQ)

); // end of class

// 'J'
VR_META_PACKED_STRUCT (order_rejected,

    (OUCH_hdr,          hdr)

    (otk_ft,            otk)
    (be_int32_ft,       reject_code) // 'fixnetix_reject' or exchange ITCH pass-through

); // end of class

// 'U'
VR_META_PACKED_STRUCT (order_replaced,

    (OUCH_hdr,          hdr)

    (otk_ft,            new_otk)
    (otk_ft,            otk)        // note: this is the last token used from any prior modification (NOT the original token)
    (iid_ft,            iid)
    (ord_side,          side)

    (oid_ft,            oid)

    (qty_ft,            qty)
    (price_ft,          price)
    (ord_TIF,           TIF)
    (int8_t,            open_close) // unused, set to zero
    (account_ft,        account)

    (int8_t,            state)

    (alphanum_ft<15>,   customer_info)
    (alphanum_ft<32>,   exchange_info)
    (char,              participant) // clearing participant
    (be_int32_ft,       crossing_key)

    (reg_data_overlay,  reg_data)
    (ord_type,          order_type)
    (qty_ft,            short_sell_qty)
    (qty_ft,            MAQ)

); // end of class

// 'C'
VR_META_PACKED_STRUCT (order_canceled,

    (OUCH_hdr,          hdr)

    (otk_ft,            otk)
    (iid_ft,            iid)
    (ord_side,          side)

    (oid_ft,            oid)

    (cancel_reason,     cancel_code)

); // end of class

// 'E'
VR_META_PACKED_STRUCT (order_execution,

    (OUCH_hdr,          hdr)

    (otk_ft,            otk)    // note: this is the last token used from any prior modification (NOT the original token)
    (iid_ft,            iid)

    (qty_ft,            qty)    // traded qty
    (price_ft,          price)  // traded price

    (match_ID_t,        match)

    (be_int16_ft,       deal_source)
    (int8_t,            match_attributes)

); // end of class
//............................................................................

#define VR_MARKET_OUCH_MESSAGE_SEQ  \
    VR_MARKET_OUCH_SEND_MESSAGE_SEQ \
    VR_MARKET_OUCH_RECV_MESSAGE_SEQ \
    /* */

} // end of 'ouch'
//............................................................................

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
