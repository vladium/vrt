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

namespace itch
{
//............................................................................
// ITCH, server->client:
//............................................................................

#define VR_MARKET_ITCH_RECV_MESSAGE_TIMESTAMPED_SEQ \
        (order_book_dir)                \
        (combo_order_book_dir)          \
        (tick_size_table_entry)         \
        (system_event)                  \
        (order_book_state)              \
        (auction_update)                \
        (order_add)                     \
        (order_add_with_participant)    \
        (order_fill)                    \
        (order_fill_with_price)         \
        (order_replace)                 \
        (order_delete)                  \
        (trade)                         \
    /* */

#define VR_MARKET_ITCH_RECV_MESSAGE_SEQ \
        (seconds)                       \
        /* convention: 'seconds' is the head of the sequence */ \
        (end_of_snapshot)               \
        /* convention: 'end_of_snapshot' is the next element */ \
        VR_MARKET_ITCH_RECV_MESSAGE_TIMESTAMPED_SEQ \
    /* */

struct message_type final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('T',   seconds)
            ('G',   end_of_snapshot)
            ('R',   order_book_dir)
            ('M',   combo_order_book_dir)
            ('L',   tick_size_table_entry)
            ('S',   system_event)
            ('O',   order_book_state)
            ('Z',   auction_update)
            ('A',   order_add)
            ('F',   order_add_with_participant)
            ('E',   order_fill)
            ('C',   order_fill_with_price)
            ('U',   order_replace)
            ('D',   order_delete)
            ('P',   trade)
        ,
        range, printable, parsable
    );

    static VR_PURE int32_t offsetof_iid (enum_t const e); // defined after the msg defs below TODO make this constexpr (c++14 or otherwise)
    static VR_PURE int32_t size (enum_t const e); // defined after the msg defs below TODO make this constexpr (c++14 or otherwise)

};// end of enum
//............................................................................

VR_EXPLICIT_ENUM (boolean,
    char,
        ('N',   no)
        ('Y',   yes)
    ,
    printable, parsable

); // end of enum
//............................................................................

VR_EXPLICIT_ENUM (sys_event_type,
    char,
        ('O',   START_OF_MESSAGES)
        ('C',   END_OF_MESSAGES)
    ,
    printable, parsable

); // end of enum

VR_EXPLICIT_ENUM (ord_lot_type,
    uint8_t,
        (0,     undefined)
        (1,     odd)
        (2,     round)
        (3,     block)
        (4,     AON)
    ,
    printable, parsable

); // end of enum
//............................................................................
// nested and sub-structs:

VR_META_PACKED_STRUCT (ITCH_hdr,

    (message_type,      type)
    (be_int32_ft,       ts_ns)

); // end of class

VR_META_PACKED_STRUCT (combo_leg,

    (alphanum_ft<32>,   symbol)
    (leg_side,          side)
    (be_int32_ft,       ratio)

); // end of class
//............................................................................

// 'T'
VR_META_PACKED_STRUCT (seconds,

    (message_type,      type)       // NOTE: not a full 'ITCH_hdr'
    (be_int32_ft,       ts_sec)

); // end of class
//............................................................................

// 'G'
VR_META_PACKED_STRUCT (end_of_snapshot,

    (message_type,      type)       // NOTE: not a full 'ITCH_hdr'
    (alphanum_ft<20>,   seqnum)     // ASCII seqnum of the next 'SoupTCP_sequenced_data', right-padded with spaces

); // end of class
//............................................................................
// messages with 'ITCH_hdr' (and thus timestamped):
//............................................................................

// 'R'
VR_META_PACKED_STRUCT (order_book_dir,

    (ITCH_hdr,          hdr)
    (iid_ft,            iid)

    (alphanum_ft<32>,       symbol)
    (alphanum_ft<32>,       name)
    (ISIN_code,             ISIN)
    (ITCH_product,          product)
    (alphanum_ft<3>,        ccy)
    (be_int16_ft,           price_scale)
    (be_int16_ft,           nominal_scale)
    (be_int32_ft,           odd_lot_size)
    (be_int32_ft,           round_lot_size)
    (be_int32_ft,           block_lot_size)
    (be_int64_ft,           nominal_value)

); // end of class

// 'M'
VR_META_PACKED_STRUCT (combo_order_book_dir, order_book_dir,

    (combo_leg,         leg_1)
    (combo_leg,         leg_2)
    (combo_leg,         leg_3)
    (combo_leg,         leg_4)

); // end of class

// 'L'
VR_META_PACKED_STRUCT (tick_size_table_entry,

    (ITCH_hdr,          hdr)
    (iid_ft,            iid)

    (be_int64_ft,       tick_size)
    (be_int32_ft,       begin)
    (be_int32_ft,       end)

); // end of class
//............................................................................

// 'S'
VR_META_PACKED_STRUCT (system_event,

    (ITCH_hdr,          hdr)

    (sys_event_type,    event_code)

); // end of class

// 'O'
VR_META_PACKED_STRUCT (order_book_state,

    (ITCH_hdr,          hdr)
    (iid_ft,            iid)

    (alphanum_ft<20>,   name)

); // end of class
//............................................................................

// 'Z'
VR_META_PACKED_STRUCT (auction_update,

    (ITCH_hdr,          hdr)
    (iid_ft,            iid)

    (sides<qty_ft>,     qty)
    (price_ft,          price)

    (sides<price_ft>,   best_price)
    (sides<qty_ft>,     best_qty)

); // end of class
//............................................................................

// 'A'
VR_META_PACKED_STRUCT (order_add,

    (ITCH_hdr,          hdr)

    (oid_ft,            oid)
    (iid_ft,            iid)
    (ord_side,          side)

    (be_int32_ft,       queue_rank)
    (qty_ft,            qty)
    (price_ft,          price)

    (be_int16_ft,       order_attrs) // bitmap
    (ord_lot_type,      lot_type)

); // end of class

// 'F'
VR_META_PACKED_STRUCT (order_add_with_participant, order_add,

    (participant_ft,    participant)

); // end of class
//............................................................................

// 'E'
VR_META_PACKED_STRUCT (order_fill,

    (ITCH_hdr,          hdr)

    (oid_ft,            oid)
    (iid_ft,            iid)
    (ord_side,          side)

    (qty_ft,            qty)
    (match_ID_t,        match)

    (participant_ft,    owner)
    (participant_ft,    counterparty)

); // end of class

// 'C'
VR_META_PACKED_STRUCT (order_fill_with_price, order_fill,

    (price_ft,          price)
    (boolean,           at_auction)
    (boolean,           printable)

); // end of class

// 'U'
VR_META_PACKED_STRUCT (order_replace,

    (ITCH_hdr,          hdr)

    (oid_ft,            oid)
    (iid_ft,            iid)
    (ord_side,          side)

    (be_int32_ft,       queue_rank)
    (qty_ft,            qty)
    (price_ft,          price)

    (be_int16_ft,       order_attrs) // bitmap

); // end of class

// 'D'
VR_META_PACKED_STRUCT (order_delete,

    (ITCH_hdr,          hdr)

    (oid_ft,            oid)
    (iid_ft,            iid)
    (ord_side,          side)

); // end of class
//............................................................................

// 'P'
VR_META_PACKED_STRUCT (trade,

    (ITCH_hdr,          hdr)

    (match_ID_t,        match)
    (trade_side,        side)
    (qty_ft,            qty)
    (iid_ft,            iid)
    (price_ft,          price)

    (participant_ft,    owner)
    (participant_ft,    counterparty)

    (boolean,           printable)
    (boolean,           at_auction)

); // end of class
//............................................................................
//............................................................................
namespace impl
{

template<typename MSG, typename = void>
struct offsetof_iid_impl
{
    static constexpr int32_t value      = -1;

}; // end of master

template<typename MSG> // specialize for messages that have '_instrument_'
struct offsetof_iid_impl<MSG, util::void_t<decltype (std::declval<MSG> ().iid ())> >
{
    static constexpr int32_t value      = meta::field_offset<_iid_, MSG> ();

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

inline VR_PURE int32_t
message_type::offsetof_iid (enum_t const e) // TODO make this constexpr (c++14 or otherwise)
{
    // TODO use a sparse table instead of a switch

    switch (e)
    {
#   define vr_CASE(r, unused, MSG)  case MSG : return impl::offsetof_iid_impl<itch:: MSG >::value; \
            /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_MARKET_ITCH_RECV_MESSAGE_SEQ)

#   undef vr_CASE

        default:  VR_ASSUME_UNREACHABLE ();

    } // end of switch
}
//............................................................................

inline VR_PURE int32_t
message_type::size (enum_t const e) // TODO make this constexpr (c++14 or otherwise)
{
    // TODO use a sparse table instead of a switch

    switch (e)
    {
#   define vr_CASE(r, unused, MSG)  case MSG : return (sizeof (itch:: MSG )); \
            /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_MARKET_ITCH_RECV_MESSAGE_SEQ)

#   undef vr_CASE

        default:  VR_ASSUME_UNREACHABLE ();

    } // end of switch
}
//............................................................................

struct book_state final
{
    VR_NESTED_ENUM (
        (
            PRE_OPEN,
            PRE_NR,
            OPEN,
            PRE_CSPA,
            CSPA,
            ADJUST,
            ADJUST_ON,
            PURGE_ORDERS,
            SYSTEM_MAINTENANCE,
            CLOSE,
            TRADING_HALT,
            SUSPEND,
            OPEN_QUOTE_DISPLAY
        ),
        iterable, printable // iterable for frame persistence
    );

    using name_type     = std::decay<decltype (std::declval<order_book_state const> ().name ())>::type;

    static enum_t name_to_value (name_type const & name)
    {
        switch (str_hash_32 (name.data (), util::array_size<name_type>::value))
        {
            case "PRE_OPEN            "_hash: return PRE_OPEN;
            case "PRE_NR              "_hash: return PRE_NR;
            case "OPEN                "_hash: return OPEN;
            case "PRE_CSPA            "_hash: return PRE_CSPA;
            case "CSPA                "_hash: return CSPA;
            case "ADJUST              "_hash: return ADJUST;
            case "ADJUST_ON           "_hash: return ADJUST_ON;
            case "PURGE_ORDERS        "_hash: return PURGE_ORDERS;
            case "SYSTEM_MAINTENANCE  "_hash: return SYSTEM_MAINTENANCE;
            case "CLOSE               "_hash: return CLOSE;
            case "TRADING_HALT        "_hash: return TRADING_HALT;
            case "SUSPEND             "_hash: return SUSPEND;
            case "OPEN_QUOTE-DISPLAY  "_hash: return OPEN_QUOTE_DISPLAY;

            default: throw_x (invalid_input, "unexpected book state " + print (name));

        } // end of switch
    }

}; // end of enum
//............................................................................

#define VR_MARKET_ITCH_MESSAGE_SEQ  \
    VR_MARKET_ITCH_RECV_MESSAGE_SEQ \
    /* */

} // end of 'itch'

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
