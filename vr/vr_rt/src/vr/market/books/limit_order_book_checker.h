#pragma once

#include "vr/fields.h"
#include "vr/market/defs.h"
#include "vr/tags.h"
#include "vr/util/logging.h"
#include "vr/util/type_traits.h"

#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace md
{
//............................................................................
//............................................................................
namespace impl
{
//............................................................................
// debug assists:
//............................................................................

template<typename LIMIT_ORDER_BOOK>
struct limit_order_book_checker final
{
    using price_type            = typename LIMIT_ORDER_BOOK::price_type;
    using fast_level_ref_type   = typename LIMIT_ORDER_BOOK::fast_level_ref_type;


    template<typename BOOK_LEVEL> // returns 'lvl_qty' (from direct traversal)
    static qty_t check_book_level (int32_t const depth, BOOK_LEVEL const & book_level, bit_set const & level_alloc_map)
    {
        bool const trace = LOG_trace_enabled (3);

        book_level.m_orders.check ();
        check_nonempty (book_level.m_orders, depth); // design invariant

        std::stringstream img { };

        int32_t ord_count { };  // total orders in 'book_level'
        qty_t qty { };          // total qty in 'book_level'

        fast_level_ref_type parent { std::numeric_limits<fast_level_ref_type>::max () };

        if (trace) img << "    [" << depth << ", " << book_level.price () << "]: " << book_level.order_count () << " order(s): {";
        {
            for (auto const & ord : book_level)
            {
                // all orders in a level have the same _parent_:

                if (parent == std::numeric_limits<fast_level_ref_type>::max ())
                {
                    if (trace) img << ord.qty ();
                    parent = field<_parent_> (ord);
                }
                else
                {
                    if (trace) img << ", " << ord.qty ();
                    check_eq (field<_parent_> (ord), parent, depth);
                }

                qty += ord.qty ();
                ++ ord_count;
            }
        }
        if (trace) { img << '}'; DLOG_trace2 << img.str (); }

        // 'parent' is in 'level_alloc_map':

        check_condition (level_alloc_map.test (parent), depth, parent);

        // traversed order count is consistent with 'order_count()':

        check_eq (ord_count, book_level.order_count (), depth);

        // if enabled, book_level _qty_ is consistent with what's been accumulated during traversal:

        if (has_field<_qty_, BOOK_LEVEL> ())
        {
            check_eq (qty, book_level.qty (), depth);
        }

        return qty;
    }

    template<typename BOOK_SIDE> // returns 'lvl_count' (from direct traversal)
    static int32_t check_book_side (BOOK_SIDE const & book_side, bit_set const & level_alloc_map)
    {
        side::enum_t const s = book_side.market_side ();

        book_side.m_price_map.check ();

        int32_t lvl_count { };  // total levels on 'book_side'
        int32_t ord_count { };  // total orders on 'book_side' (currently just traced)
        qty_t qty { };      // total qty on 'book_side'

        int32_t depth { };
        price_type const * px_prev { };
        for (auto const & lvl : book_side)
        {
            price_type const & px = lvl.price ();

            // check that exact price query recovers 'lvl':

            auto const * const lvl2 = book_side.eq_price (px);
            check_eq (lvl2, & lvl, px);

            // check that price ordering is consistent with side:

            if (VR_LIKELY (px_prev))
                check_positive (sign (s) * (* px_prev - px), s, px, * px_prev);

            qty += check_book_level (depth, lvl, level_alloc_map);

            ++ lvl_count;
            ord_count += lvl.order_count (); // 'order_count()' correctness is verify by 'check_book_level()'

            px_prev = & px;
            ++ depth;
        }

        DLOG_trace2 << "  " << s << " lvl_count: " << lvl_count << ", ord_count: " << ord_count << ", qty: " << qty;

        if (LIMIT_ORDER_BOOK::const_time_depth ())
        {
            // traversed level count is consistent with 'depth ()':

            check_eq (lvl_count, book_side.depth ());
        }

        return lvl_count;
    }

    static std::tuple</* orders */int32_t, /* levels */int32_t> check_book (LIMIT_ORDER_BOOK const & book)
    {
        bit_set const level_alloc_map = book.level_pool ().allocation_bitmap ();

        // [this is now done at the view level: count of allocated orders matches oid map sizes]

        int32_t const ord_count = book.at (side::BID).m_oid_map.size () + book.at (side::ASK).m_oid_map.size (); // total orders in 'book'
        int32_t lvl_count { }; // total levels in 'book'

        // descend into and check each side:
        {
            for (side::enum_t s : side::values ())
            {
                lvl_count += check_book_side (book.at (s), level_alloc_map);
            }
        }

        // [this is now done at the view level: count of allocated levels matches what's been inferred from direct traversal]

        DLOG_trace1 << "book: total levels: " << lvl_count << ", total orders: " << ord_count;

        return std::make_tuple (ord_count, lvl_count);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'md'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
