#pragma once

#include "vr/fw_string.h"
#include "vr/io/net/defs.h"     // default*link_capacity
#include "vr/market/defs.h"     // market::side, _partition_
#include "vr/market/net/defs.h" // mold_seqnum_t
#include "vr/market/prices.h"   // price_ops
#include "vr/market/ref/defs.h" // ISIN_code
#include "vr/market/sources.h"
#include "vr/meta/arrays.h"
#include "vr/meta/tags.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

template<> // specialized for ASX
struct source_traits<source::ASX>: public source_traits<>
{
    using price_type                = int32_t;  // wire
    using qty_type                  = int64_t;  // wire; note, however, that ASX/BAML will reject any values outside 'int32_t' range

    using iid_type                  = int32_t;
    using oid_type                  = int64_t;

    template<typename T_BOOK_PRICE>
    using price_traits              = price_ops<price_type, std::ratio<price_si_scale (), 10000>, T_BOOK_PRICE>;

}; // end of specialization
//............................................................................
//............................................................................

namespace ASX
{

// TODO clean this up more
// - split into itch/ouch sections or headers
// - move things into or merge with "schema.h"

constexpr source::enum_t this_source ()         { return source::ASX; }
using                    this_source_traits     = source_traits<this_source ()>;

// OUCH:

io::capacity_t constexpr ouch_link_capacity ()              { return (256 * 1024); };

// ITCH:

io::capacity_t constexpr itch_data_link_capacity ()         { return io::net::default_mcast_link_recv_capacity (); };
io::capacity_t constexpr itch_snapsnot_link_capacity ()     { return (8  * 1024 * 1024); };
io::capacity_t constexpr itch_rewind_link_capacity ()       { return (64 * 1024); };

//............................................................................

constexpr uint16_t partition_port_base (char const feed)    { return (feed == 'A' ? 21001 : 21101); }

constexpr int32_t partition_count ()                        { return 5; }

//............................................................................

timestamp_t constexpr heartbeat_timeout ()      { return (10 * _1_second ()); }

//............................................................................

using iid_t                 = this_source_traits::iid_type;
using oid_t                 = this_source_traits::oid_type;

vr_static_assert (std::is_signed<iid_t>::value); // we'd like to be able to return '-1'

//............................................................................

using order_token           = fw_string8;

//............................................................................

extern int32_t
as_price (double const x);

double
to_price (int32_t const x);

//............................................................................

struct leg_side final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('B',   BUY)
            ('C',   SELL)
        ,
        printable, parsable
    );

    static constexpr VR_PURE market::side::enum_t to_side (enum_t const e)
    {
        return (e == BUY ? market::side::BID : market::side::ASK);
    }

}; // end of enum

struct ord_side final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('B',   BUY)
            ('S',   SELL)
            ('T',   SHORT)
        ,
        printable, parsable
    );

    static constexpr VR_PURE market::side::enum_t to_side (enum_t const e)
    {
        return (e == BUY ? market::side::BID : market::side::ASK);
    }

}; // end of enum

struct trade_side final
{
    VR_NESTED_EXPLICIT_ENUM
    (
        char,
            ('B',   BUY)
            ('S',   SELL)
            (' ',   unknown) // TODO confirm with ASX
        ,
        printable, parsable
    );

}; // end of enum
//............................................................................

VR_EXPLICIT_ENUM (ITCH_product,
    uint8_t,
        (1,     option) // or warrant
        (3,     future)
        (5,     equity)
        (11,    combo)
    ,
    range, printable, parsable

); // end of enum
//............................................................................

VR_EXPLICIT_ENUM (ord_type, // OUCH
    char,
        ('Y',   LIMIT)

        ('N',   CENTER_POINT)           // price > 0: LIMIT, price == 0: MARKET
        ('D',   CENTER_POINT_DARK)
        ('B',   CENTER_POINT_BLOCK)     // price > 0: LIMIT, price == 0: MARKET
        ('F',   CENTER_POINT_DARK_MAQ)  // "dark limit center point with single fill MAQ"

        ('S',   SWEEP)              // price > 0: LIMIT, price == 0: MARKET
        ('P',   SWEEP_DUAL_POSTED)
        ('T',   SWEEP_MAQ)          // "dual-posted if price is at half-tick"

        ('C',   PRICE_BLOCK)
        ('E',   PRICE_BLOCK_MAQ)    // "with single fill MAQ"
    ,
    printable, parsable

); // end of enum
//............................................................................

VR_EXPLICIT_ENUM (ord_TIF,
    uint8_t,
        (0,  DAY)
        (3,  IOC)   // FAK
        (4,  FOK)
    ,
    printable, parsable

); // end of enum
//............................................................................

VR_EXPLICIT_ENUM (ord_state,
    uint8_t,
        (1,  LIVE)
        (2,  NOT_ON_BOOK)
    ,
    printable

); // end of enum
//............................................................................

VR_EXPLICIT_ENUM (cancel_reason,
    uint8_t,
        (0,     na)
        (1,     REQUESTED_BY_USER)
        (4,     CONNECTION_LOSS)
        (9,     FAK_ORDER_DELETED_IN_AUCTION)
        (10,    DELETED_BY_ASX)
        (20,    DELETED_BY_SYSTEM)
        (21,    INACTIVATED_BY_SYSTEM)
        (24,    INACTIVATED_BY_EOD)
    ,
    printable, parsable

); // end of enum
//............................................................................

// ITCH:

struct ord_attrs final // bitmap
{
    VR_NESTED_EXPLICIT_ENUM (
        uint16_t,
            (0x0004,    market_bid)
            (0x0008,    price_stabilization)
            (0x0032,    undisclosed)
        ,
        printable
    )

    static std::string describe (uint32_t const attrs) VR_NOEXCEPT;

}; // end of enum
//............................................................................
//............................................................................
namespace impl
{
/**
 * ASX specifics: origin timestamps and seqnums need to be tracked per partition
 *
 * @see Mold_frame_
 * @see Soup_frame_
 */
class seqnum_state
{
    protected: // ............................................................

        using seqnum_t          = mold_seqnum_t;

    public: // ...............................................................

        using seqnum_array      = std::array<seqnum_t, partition_count ()>;

        // ACCESSORs:

        seqnum_array const & seqnums () const
        {
            return m_seqnum_expected;
        }

        // MUTATORs:

        seqnum_array & seqnums ()
        {
            return m_seqnum_expected;
        }

    protected: // ............................................................

        seqnum_array m_seqnum_expected { meta::create_array_fill<seqnum_t, partition_count (), 0> () };

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
