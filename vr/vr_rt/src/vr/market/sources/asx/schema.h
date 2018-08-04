#pragma once

#include "vr/market/net/defs.h" // alphanum_ft, be_*_t
#include "vr/market/sources/asx/defs.h"
#include "vr/meta/tags.h"
#include "vr/tags.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

// TODO maintain a schema (inheritance) hierarchy somewhere:
// TODO need a better naming scheme for net-vs-host typing

using price_ft          = util::net_int<this_source_traits::price_type, false>;
using qty_ft            = util::net_int<this_source_traits::qty_type, false>;

using iid_ft            = util::net_int<iid_t, false>; // instrument/order book ID
using oid_ft            = util::net_int<oid_t, false>; // order ID
using attrs_ft          = util::net_int<ord_attrs::enum_t, false>; // [bitmap] order attributes

//............................................................................

using participant_ft    = alphanum_ft<7>; // note: 'participant_t' is wider

vr_static_assert (util::array_size<participant_ft>::value <= participant_t::max_size ());

//............................................................................

using otk_ft            = alphanum_ft<14>; // order token
using account_ft        = alphanum_ft<10>;

//............................................................................

VR_META_TAG (hdr);
VR_META_TAG (unused);


VR_META_TAG (ts_sec);
VR_META_TAG (ts_ns);


VR_META_TAG (queue_rank);
VR_META_TAG (match);
VR_META_TAG (combo_group);


VR_META_TAG (owner);
VR_META_TAG (counterparty);

VR_META_TAG (order_attrs);
VR_META_TAG (lot_type);
VR_META_TAG (at_auction);
VR_META_TAG (printable);


VR_META_TAG (best_price);
VR_META_TAG (best_qty);


VR_META_TAG (ISIN);
VR_META_TAG (product);
VR_META_TAG (price_scale);
VR_META_TAG (nominal_scale);
VR_META_TAG (odd_lot_size);
VR_META_TAG (round_lot_size);
VR_META_TAG (block_lot_size);
VR_META_TAG (nominal_value);

VR_META_TAG (ratio);

VR_META_TAG (leg_1);
VR_META_TAG (leg_2);
VR_META_TAG (leg_3);
VR_META_TAG (leg_4);

VR_META_TAG (tick_size);
VR_META_TAG (tick_table);

//............................................................................

VR_META_TAG (ts);

VR_META_TAG (new_otk);
VR_META_TAG (TIF);
VR_META_TAG (order_type);

VR_META_TAG (open_close);

VR_META_TAG (account);
VR_META_TAG (customer_info);
VR_META_TAG (exchange_info);

VR_META_TAG (crossing_key);
VR_META_TAG (reg_data);

VR_META_TAG (short_sell_qty);
VR_META_TAG (MAQ);

VR_META_TAG (deal_source);
VR_META_TAG (match_attributes);

VR_META_TAG (participant_capacity);
VR_META_TAG (directed_wholesale);
VR_META_TAG (venue);
VR_META_TAG (intermediary);
VR_META_TAG (order_origin);

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
