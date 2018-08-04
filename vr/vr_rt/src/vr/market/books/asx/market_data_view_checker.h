#pragma once

#include "vr/util/object_pool_checker.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
template<typename MARKET_DATA_VIEW, typename = void>
struct market_data_view_checker final
{
    using book_type         = typename MARKET_DATA_VIEW::book_type;
    using size_type         = typename MARKET_DATA_VIEW::size_type;


    static void check_view (MARKET_DATA_VIEW const & view)
    {
        auto const & arena = view.m_pool_arena;

        arena.object_pools ().m_order_pool.check ();
        arena.object_pools ().m_level_pool.check ();

        int32_t ord_count { }; // total orders in 'view'
        int32_t lvl_count { }; // total levels in 'view'

        for (size_type i = 0, i_limit = view.m_iid_map.size (); i < i_limit; ++ i)
        {
            auto const rc = view.at (i).check ();

            ord_count += std::get<0> (rc);
            lvl_count += std::get<1> (rc);
        }

        // traverse object pools and get their alloc maps:

        bit_set const order_alloc_map = arena.object_pools ().m_order_pool.allocation_bitmap ();
        bit_set const level_alloc_map = arena.object_pools ().m_level_pool.allocation_bitmap ();

        check_eq (signed_cast (order_alloc_map.count ()), ord_count);
        check_eq (signed_cast (level_alloc_map.count ()), lvl_count);
    }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
