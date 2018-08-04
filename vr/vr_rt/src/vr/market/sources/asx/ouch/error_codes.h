#pragma once

#include "vr/enums.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

#define VR_ASX_ERROR_FIXNETIX_SEQ                       \
    (31,    start)                                      \
    /* */                                               \
    (32,    REJ_RESTRICED_SYMBOL_BUY)                   \
    (33,    REJ_RESTRICED_SYMBOL_SELL)                  \
    (34,    REJ_RESTRICED_SYMBOL_SHO)                   \
    (35,    REJ_SELL_SHORT_EXEMPT_NOT_ALLOWED)          \
    (36,    REJ_ISO_ORDERS_NOT_ALLOWED)                 \
    (37,    REJ_MARKET_ORDERS_NOT_ALLOWED)              \
    (38,    REJ_PRICE_OUT_OF_RANGE)                     \
    (39,    REJ_ADV_OUT_OF_RANGE)                       \
    (40,    REJ_SUBPENNY_RULE)                          \
    (41,    REJ_ORDER_VALUE_TOO_SMALL)                  \
    (42,    REJ_ORDER_VALUE_TOO_LARGE)                  \
    (43,    REJ_ORDER_QUANTITY_TOO_SMALL)               \
    (44,    REJ_ORDER_QUANTITY_TOO_LARGE)               \
    (45,    REJ_DUP_CLIENT_ORDER_ID)                    \
    (46,    REJ_UNKNOWN_SYMBOL)                         \
    (47,    REJ_ILLEGAL_TIF)                            \
    (48,    REJ_IXECUTE_STOPPED_TRADING)                \
    (49,    REJ_DAYCAP_EXCEEDED)                        \
    (50,    REJ_LOCATE_BROKER_ID_NOT_PRESENT)           \
    (51,    REJ_ORDER_TABLE_HASH_COLLISION)             \
    (52,    REJ_ORDER_RATE_TOO_FAST)                    \
    (53,    REJ_ILLEGAL_SIDE_TYPE)                      \
    (54,    REJ_CAPACITY_INCORRECT)                     \
    (55,    REJ_UNSUPPORTED_MSG)                        \
    (56,    REJ_DEFAULT_CODE)                           \
    (57,    REJ_EXCEEDS_OUTSTANDING_SHARES)             \
    (58,    REJ_RESTRICED_SYMBOL_IPO)                   \
    (59,    REJ_ILLEGAL_ORDER_TYPE)                     \
    (60,    REJ_IXECUTE_RESTRICT_TRADING)               \
    (61,    REJ_COVERED_SHORT_SELLING)                  \
    (62,    REJ_INVALID_TICK_SIZE)                      \
    (63,    REJ_INVALID_LOT_SIZE)                       \
    (101,   REJ_NO_MATCH_OR_FULL)                       \
    (102,   REJ_MISSING_FIELD)                          \
    (103,   REJ_TIMEOUT_ERROR)                          \
    (104,   REJ_SOUTH_REASON)                           \
    (105,   REJ_AMEND_UP_NOT_ALLOWED)                   \
    (106,   REJ_BUY_AGENCY_DISALLOWED)                  \
    (107,   REJ_SELL_AGENCY_DISALLOWED)                 \
    (108,   REJ_SS_SSE_AGENCY_DISALLOWED)               \
    (109,   REJ_BUY_PRINCIPAL_DISALLOWED)               \
    (110,   REJ_SELL_PRINCIPAL_DISALLOWED)              \
    (111,   REJ_SS_SSE_PRINCIPAL_DISALLOWED)            \
    (112,   REJ_BUY_ALL_DISALLOWED)                     \
    (113,   REJ_SELL_ALL_DISALLOWED)                    \
    (114,   REJ_SS_ALL_DISALLOWED)                      \
    (115,   REJ_DAYCAP_BUYSIDE_EXCEEDED)                \
    (116,   REJ_DAYCAP_SELLSIDE_EXCEEDED)               \
    (117,   REJ_SELL_EXCEEDS_POSITION)                  \
    (118,   REJ_SS_EXCEEDS_BORROW)                      \
    (119,   REJ_REFERENCED_ORDER_UNACKNOWLEDGED)        \
    (120,   REJ_VENUE_BUSY_REJECT)                      \
    (121,   REJ_BUY_OPEN_ORDERS_ON_SYMBOL_TOO_LARGE)    \
    (122,   REJ_SELL_OPEN_ORDERS_ON_SYMBOL_TOO_LARGE)   \
    (123,   REJ_AMEND_SYMBOL_MISMATCH)                  \
    (124,   REJ_ORDER_ENTRY_NOT_FOUND)                  \
    (125,   REJ_OT_SEARCH_TIMEOUT)                      \
    (126,   REJ_DAYCAP_NET_EXCEEDED)                    \
    /* */                                               \
    (127,   limit)                                      \
    /* */

VR_EXPLICIT_ENUM (fixnetix_reject,
    int32_t,
        VR_ASX_ERROR_FIXNETIX_SEQ
    ,
    printable

); // end of enum
//............................................................................

inline std::string
print_reject_code (int32_t const rc)
{
    std::stringstream ss;

    ss << '(' << rc << ')';

    if (vr_is_in_exclusive_range (rc, fixnetix_reject::start, fixnetix_reject::limit))
        ss << ' ' << static_cast<fixnetix_reject::enum_t> (rc);

    return ss.str ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
