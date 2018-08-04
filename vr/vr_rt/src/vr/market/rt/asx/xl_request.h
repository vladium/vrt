#pragma once

#include "vr/market/sources/asx/schema.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

// TODO namespacing

VR_ENUM (xl_req,
    (
        start_login,
        submit_order,
        replace_order,
        cancel_order
    ),
    printable

); // end of enum

using xl_request_schema  = meta::make_schema_t
<
    meta::fdef_<xl_req,         _type_>
    ,
    meta::fdef_<liid_t,         _liid_>,
    meta::fdef_<order_token,    _otk_>,
    meta::fdef_<order_token,    _new_otk_>
    ,
    meta::fdef_<ord_side,       _side_>,        // TODO OUCH/ASX-specific type for now
    meta::fdef_<price_si_t,     _price_>,
    meta::fdef_<qty_t,          _qty_>
    ,
    meta::fdef_<ord_TIF,        _TIF_>          // TODO ASX-specific type for now
>;
//............................................................................

struct xl_request final: public meta::make_compact_struct_t<xl_request_schema>
{
    friend std::string __print__ (xl_request const & obj) VR_NOEXCEPT;

}; // end of class

using xl_request_storage    = std::aligned_storage<sizeof (xl_request), alignof (xl_request)>;

//............................................................................

template<typename DEQ_RESULT>
VR_FORCEINLINE xl_request const &
xl_request_cast (DEQ_RESULT const & entry)
{
    return reinterpret_cast<xl_request const &> (static_cast<xl_request_storage const &> (entry));
}

template<typename ENQ_RESULT>
VR_FORCEINLINE xl_request &
xl_request_cast (ENQ_RESULT & entry)
{
    return reinterpret_cast<xl_request &> (static_cast<xl_request_storage &> (entry));
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
