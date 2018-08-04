#pragma once

#include "vr/market/books/limit_order_book.h"
#include "vr/market/prices.h" // price_book_to_print

#include <boost/format.hpp>

#include <iterator>

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

extern std::array<boost::format, 8> const &
book_formats ();


template<typename LIMIT_ORDER_BOOK_IMPL>
VR_ASSUME_COLD std::string
__print__impl (LIMIT_ORDER_BOOK_IMPL const & book, int32_t const max_depth) VR_NOEXCEPT_IF (VR_RELEASE)
{
    assert_nonnegative (max_depth);

    using book_impl         = LIMIT_ORDER_BOOK_IMPL;

    using iterator          = typename book_impl::side_iterator;
    using level             = typename book_impl::level_type;

    using bound         = std::array<iterator, 3>; // [overlap begin, overlap end), depth end)

    std::array<bound, side::size> bounds
    { {
        { book.at (side::BID).begin (), book.at (side::BID).begin (), book.at (side::BID).end () },
        { book.at (side::ASK).begin (), book.at (side::ASK).begin (), book.at (side::ASK).end () }
    } };

    bool const both_sides = (! (book.at (side::BID).empty () | book.at (side::ASK).empty ()));
    bool const crossed = both_sides && (bounds [side::BID][0]->price () >= bounds [side::ASK][0]->price ());

    // fill the remaining 'bounds' entries:

    for (side::enum_t s : side::values ())
    {
        auto const & book_side = book.at (s);

        // determine 'bounds [s][1]':

        if (crossed)
            bounds [s][1] = book_side.find_next_worse_price (bounds [~ s][0]->price ()); // note: could still be 'end ()'
        // else leave as 'begin()'

        // determine 'bounds [s][2]' (walk at most 'max_depth' steps further away from "overlap end" = "depth begin"):

        iterator i = bounds [s][1];
        for (int32_t steps = 0; (steps < max_depth) && (i != book_side.end ());  ++ steps, ++ i)
        {
        }
        bounds [s][2] = i;
    }

    auto const & fmts = book_formats ();
    std::stringstream ss { };

    ss.put ('\n'); // helpful when a is printed by an assert/check macro

    // display non-overlap portion of ASK:
    {
        bool empty { true };
        for (iterator i = bounds [side::ASK][2]; i != bounds [side::ASK][1]; )
        {
            -- i; // reverse iteration with non-reverse iterator

            int32_t const fmt_index = 5; // TODO ((! crossed /*| ! last*/) ? side::ASK : 5);
            ss << boost::format { fmts [fmt_index] } % price_book_to_print (i->price ()) % i->evaluate_qty () % i->order_count ();

            empty = false;
        }
        if (empty)
            ss << fmts [6]; // this format instance is not mutated, not need to clone
    }

    // display either the spread divider or the crossed section:

    if (! crossed)
        ss << fmts [3]; // this format instance is not mutated, not need to clone
    {
        iterator ia = bounds [side::ASK][1];
        iterator ib = bounds [side::BID][0];

        bool a, b;

        while ((a = (ia != bounds [side::ASK][0])) /* ! */| (b = (ib != bounds [side::BID][1]))) // note: can't short-circuit, must eval both 'a' and 'b'
        {
            level const * lvla { };
            level const * lvlb { };

            if (a)
            {
                -- ia; // reverse iteration with non-reverse iterator
                lvla = & (* ia);
            }

            if (b)
            {
                lvlb = & (* ib);
                ++ ib; // forward iteration with non-reverse iterator
            }

            assert_condition ((lvla != nullptr) or (lvlb != nullptr));

            if (a && ((! b) || (lvlb->price () < lvla->price ())))
            {
                // display ASK only, un-consume BID:

                int32_t const fmt_index = side::ASK;
                ss << boost::format { fmts [fmt_index] } % price_book_to_print (lvla->price ()) % lvla->evaluate_qty () % lvla->order_count ();

                if (b) -- ib;
            }
            else if (b && ((! a) || (lvla->price () < lvlb->price ())))
            {
                // display BID only, un-consume ASK:

                int32_t const fmt_index = side::BID;
                ss << boost::format { fmts [fmt_index] } % ('(' + string_cast (lvlb->order_count ()) + ')') % lvlb->evaluate_qty () % price_book_to_print (lvlb->price ());

                if (a) ++ ia;
            }
            else // display and comsume both:
            {
                assert_nonnull (lvla);
                assert_nonnull (lvlb);
                assert_eq (lvla->price (), lvlb->price ());

                ss << boost::format { fmts [2] } % ('(' + string_cast (lvlb->order_count ()) + ')') % lvlb->evaluate_qty () % price_book_to_print (lvlb->price ()) % lvla->evaluate_qty () % lvla->order_count ();
            }
        }
    }

    // display non-overlap portion of BID:
    {
        bool empty { true };
        for (iterator i = bounds [side::BID][1]; i != bounds [side::BID][2]; ++ i)
        {
            int32_t const fmt_index = 4; // TODO ((! crossed | (steps > 0)) ? side::BID : 4);
            ss << boost::format { fmts [fmt_index] } % ('(' + string_cast (i->order_count ()) + ')') % i->evaluate_qty () % price_book_to_print (i->price ());

            empty = false;
        }
        if (empty)
            ss << fmts [7]; // this format instance is not mutated, not need to clone
    }

    return ss.str ();
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'md'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
