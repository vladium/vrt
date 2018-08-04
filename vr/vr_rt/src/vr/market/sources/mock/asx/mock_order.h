#pragma once

#include "vr/market/sources/asx/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
class mock_order
{
    public: // ...............................................................

        oid_t m_oid { };
        iid_t m_iid { };
        ord_state::enum_t m_state { ord_state::NOT_ON_BOOK };
        ord_type::enum_t m_order_type;

        ord_side::enum_t m_side { };
        this_source_traits::price_type m_price { };
        this_source_traits::qty_type m_qty { };

        int32_t m_instance { -1 };
        int32_t m_step { };

    protected: // ............................................................

    private: // ..............................................................

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
