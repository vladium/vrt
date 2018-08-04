#pragma once

#include "vr/fields.h"
#include "vr/market/books/limit_order_book.h"
#include "vr/market/books/market_data_listener.h"
#include "vr/market/sources/asx/itch/ITCH_ts_tracker.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
/**
 * @note this is a specialization and hence can't be moved inside the 'ASX' namespace
 */
template<typename LIMIT_ORDER_BOOK, typename CTX> // specialization for ASX
class market_data_listener<source::ASX, LIMIT_ORDER_BOOK, CTX> final: public ASX::ITCH_ts_tracker<CTX, market_data_listener<source::ASX, LIMIT_ORDER_BOOK, CTX>>
{
    private: // ..............................................................

        using super         = ASX::ITCH_ts_tracker<CTX, market_data_listener<source::ASX, LIMIT_ORDER_BOOK, CTX>>;

        vr_static_assert (has_field<_book_, CTX> ()); // how the book reference is communicated

        // TODO factor out the task of ts tracking into (can use ITCH_pipeline now instead)
        // TODO get stats on order type pdf to see which visit to hint for inlining/hotness

    public: // ...............................................................

        using book_type             = LIMIT_ORDER_BOOK;

        vr_static_assert (book_type::order::has_qty ()); // otherwise can't meaningfully track individual orders

        using book_price_type       = typename book_type::price_type;
        using price_traits          = typename source_traits<source::ASX>::price_traits<book_price_type>;

        using side_type             = typename book_type::side_type;
        using level_type            = typename book_type::level_type;
        using order_type            = typename book_type::order_type;

        using price_map_type        = typename book_type::price_map_type;
        using order_list_type       = typename book_type::order_list_type;

        using fast_order_ref_type   = typename book_type::fast_order_ref_type;
        using fast_level_ref_type   = typename book_type::fast_level_ref_type;


        market_data_listener ()     = default;

        market_data_listener (arg_map const & args) :
            market_data_listener { }
        {
        }


        // overridden ITCH visits:

        using super::visit;

        VR_ASSUME_HOT bool visit (ASX::itch::order_book_state const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            // TODO move these as return stmts
            auto const rc = super::visit (msg, ctx); // [chain]

            if (has_field<_state_, typename book_type::book_user_data> ())
            {
                DLOG_trace2 << msg;

                auto & book = current_book (ctx);

                VR_IF_DEBUG (itch::book_state::enum_t const s = field<_state_> (book.user_data ()); )
                field<_state_> (book.user_data ()) = itch::book_state::name_to_value (msg.name ());
                VR_IF_DEBUG (DLOG_trace1 << "book state transition " << print (s) << " -> " << print (field<_state_> (book.user_data ()));)
            }

            return rc;
        }


        VR_ASSUME_HOT bool visit (ASX::itch::order_add const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;

            auto & book = current_book (ctx);

            side::enum_t const s = ord_side::to_side (msg.side ());
            auto & book_side = book.at (s);

            // allocate new order from the pool:

            auto const o_ref = book.order_pool ().allocate ();
            order_type & o = std::get<0> (o_ref);

            // add [oid -> order pool ref] mapping into the oid map:

            assert_null (book_side.m_oid_map.get (msg.oid ()));
            book_side.m_oid_map.put (msg.oid (), std::get<1> (o_ref));

            book_price_type const o_price = price_traits::wire_to_book (msg.price ());
            auto const o_qty = msg.qty ();

            typename book_type::price_map_type::insert_commit_data _;
            auto pm_insert = book_side.m_price_map.insert_check (o_price, _);

            level_type * l { };

            if (VR_UNLIKELY (pm_insert.second)) // allocate new level from the pool:
            {
                auto const l_ref = book.level_pool ().allocate ();
                l = & std::get<0> (l_ref);

                // populate level fields (at least _price_ must be done before committing the insert):

                field<_price_> (* l) = o_price; // always present
                if (book_type::level::has_qty ())
                    field<_qty_> (* l) = o_qty;
                if (book_type::level::has_order_count ())
                    field<_order_count_> (* l) = 1;

                // and commit the insert:

                book_side.m_price_map.insert_commit (* l, _);

                field<_parent_> (o) = std::get<1> (l_ref); // link 'o' -> 'l'
            }
            else // this price level is already in 'book_side':
            {
                l = & (* pm_insert.first);

                // update level fields:

                assert_eq (field<_price_> (* l), o_price); // always present and set at allocation time
                if (book_type::level::has_qty ())
                    field<_qty_> (* l) += o_qty;
                if (book_type::level::has_order_count ())
                    ++ field<_order_count_> (* l);

                // since this level already existed, it can't be empty by design;
                // which means we can set _parent_ from any other order already in the list:

                assert_nonempty (l->m_orders);
                field<_parent_> (o) = field<_parent_> (l->m_orders.front ()); // link 'o' -> 'l'
            }
            assert_nonnull (l);

            // set remaining order fields:
            {
                field<_qty_> (o) = o_qty;
                // TODO ...
            }

            // finally, add the order to (the front of) the level list:

            l->m_orders.push_front (o);

            if (book_type::level::has_order_count ()) assert_eq (field<_order_count_> (* l), l->m_orders.size ()); // note: size() is not O(1), only do this check in debug builds
            VR_IF_DEBUG (if (has_field<_book_, CTX> ()) field<_book_> (ctx) = & book;)
            return rc;
        }

        VR_ASSUME_HOT bool visit (ASX::itch::order_add_with_participant const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;

            visit (static_cast<itch::order_add const &> (msg), ctx); // TODO do more if retaining participant IDs

            return rc;
        }


        VR_ASSUME_HOT bool visit (ASX::itch::order_fill const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;
            assert_positive (msg.qty ());

            auto & book = current_book (ctx);

            side::enum_t const s = ord_side::to_side (msg.side ());
            auto & book_side = book.at (s);

            // lool up [oid -> order pool ref] mapping in the oid map:
            // [allow for this to be a no-op in case we've not seen the corresponding add]

            auto const oid = msg.oid ();
            auto const qty = msg.qty ();

            md::order_ref_type * const o_ref_ptr = book_side.m_oid_map.get (oid);

            if (VR_LIKELY (o_ref_ptr != nullptr))
            {
                fast_order_ref_type const o_ref = * o_ref_ptr;

                order_type & o = book.order_pool ()[o_ref];

                auto const new_o_qty = (field<_qty_> (o) - qty);
                // TODO ASX-59 assert_nonnegative (new_o_qty, msg);

                if (new_o_qty <= 0) // note: _qty_ side-effect
                {
                    // [no need to set 'field<_qty_> (o)' since it's being discarded]

                    remove_order (oid, o, o_ref, book, book_side); // [note: will use old 'o' qty] will adjust aggregate fields for 'o's parent level
                }
                else // otherwise adjust aggregate fields for 'o's parent level:
                {
                    // [order count doesn't change]

                    if (book_type::level::has_qty ())
                    {
                        level_type & l = book.level_pool ()[field<_parent_> (o)];

                        field<_qty_> (l) -= qty;
                        assert_nonnegative (field<_qty_> (l));

                        if (book_type::level::has_order_count ()) assert_eq (field<_order_count_> (l), l.m_orders.size ()); // note: size() is not O(1), only do this check in debug builds
                    }

                    // set remaining order fields:
                    {
                        field<_qty_> (o) = new_o_qty;
                        // TODO ...
                    }
                }
            }
            else
            {
                DLOG_trace1 << "oid not in the book: " << msg;
            }

            VR_IF_DEBUG (if (has_field<_book_, CTX> ()) field<_book_> (ctx) = & book;)
            return rc;
        }

        VR_ASSUME_HOT bool visit (ASX::itch::order_fill_with_price const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;
            visit (static_cast<itch::order_fill const &> (msg), ctx); // TODO do more if retaining trade prices or other additional fields

            return rc;
        }


        VR_ASSUME_HOT bool visit (ASX::itch::order_replace const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;

            auto & book = current_book (ctx);

            side::enum_t const s = ord_side::to_side (msg.side ());
            auto & book_side = book.at (s);

            // lool up [oid -> order pool ref] mapping in the oid map:
            // [allow for this to be a no-op in case we've not seen the corresponding add]

            md::order_ref_type * const o_ref = book_side.m_oid_map.get (msg.oid ());

            if (VR_LIKELY (o_ref != nullptr))
            {
                // [oid doesn't change on 'order_replace' so no need to massage 'm_oid_map']

                order_type & o = book.order_pool ()[* o_ref];

                // find destination price level:

                book_price_type const o_price = price_traits::wire_to_book (msg.price ());
                auto const o_qty = msg.qty ();

                typename book_type::price_map_type::insert_commit_data _;
                auto pm_insert = book_side.m_price_map.insert_check (o_price, _);

                level_type * new_l { };

                if (VR_UNLIKELY (pm_insert.second)) // allocate new level from the pool:
                {
                    auto const new_l_ref = book.level_pool ().allocate ();
                    new_l = & std::get<0> (new_l_ref);

                    // initialize level fields (at least _price_ must be done before committing the insert):

                    field<_price_> (* new_l) = o_price; // always present
                    if (book_type::level::has_qty ())
                        field<_qty_> (* new_l) = 0; // is set below
                    if (book_type::level::has_order_count ())
                        field<_order_count_> (* new_l) = 0; // is set below

                    // and commit the insert:

                    book_side.m_price_map.insert_commit (* new_l, _);

                    field<_parent_> (o) = std::get<1> (new_l_ref); // link 'o' -> 'new_l'
                }
                else // this price level is already in 'book_side':
                {
                    new_l = & (* pm_insert.first);

                    assert_eq (field<_price_> (* new_l), o_price); // always present and set at allocation time

                    // since this level already existed, it can't be empty by design;
                    // which means we can set _parent_ from any other order already in the list:

                    assert_nonempty (new_l->m_orders);
                    field<_parent_> (o) = field<_parent_> (new_l->m_orders.front ()); // link 'o' -> 'new_l'
                }
                assert_nonnull (new_l);

                // splice 'o' from its current parent level to 'new_l':

                fast_level_ref_type const o_parent_ref = field<_parent_> (o);
                level_type & l = book.level_pool ()[o_parent_ref];

                if (& l != new_l)
                {
                    // set/update aggregate fields of 'new_l':

                    if (book_type::level::has_qty ())
                        field<_qty_> (* new_l) += o_qty;
                    if (book_type::level::has_order_count ())
                        ++ field<_order_count_> (* new_l);

                    // splice 'o' into 'new_l' (this is fast):

                    new_l->m_orders.splice (new_l->m_orders.begin (), l.m_orders, order_list_type::s_iterator_to (o));

                    // if this caused 'l' to become empty, remove it from the price map
                    // and release to the object pool:

                    if (VR_UNLIKELY (l.m_orders.empty ())) // note: don't use size() here (may not be O(1))
                    {
                        book_side.m_price_map.erase (price_map_type::s_iterator_to (l));
                        book.level_pool ().release (o_parent_ref);
                    }
                    else // need to adjust aggregate fields of 'l':
                    {
                        if (book_type::level::has_qty ())
                            field<_qty_> (l) -= field<_qty_> (o); // note that this is still the old value
                        if (book_type::level::has_order_count ())
                            -- field<_order_count_> (l);

                        if (book_type::level::has_order_count ()) assert_eq (field<_order_count_> (l), l.m_orders.size ()); // note: size() is not O(1), only do this check in debug builds
                    }
                }
                else // order moves within the same level TODO does this happen [often]?
                {
                    // [order count doesn't change]

                    if (book_type::level::has_qty ())
                    {
                        field<_qty_> (l) += (o_qty - field<_qty_> (o));
                        assert_nonnegative (field<_qty_> (l));
                    }
                }

                // set/update remaining order fields:
                {
                    field<_qty_> (o) = o_qty;
                    // TODO ...
                }
            }
            else
            {
                DLOG_trace1 << "oid not in the book: " << msg;
            }

            VR_IF_DEBUG (if (has_field<_book_, CTX> ()) field<_book_> (ctx) = & book;)
            return rc;
        }

        VR_ASSUME_HOT bool visit (ASX::itch::order_delete const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            auto const rc = super::visit (msg, ctx); // [chain]

            DLOG_trace2 << msg;

            auto & book = current_book (ctx);

            side::enum_t const s = ord_side::to_side (msg.side ());
            auto & book_side = book.at (s);

            // remove [oid -> order pool ref] mapping from the oid map:
            // [allow for this to be a no-op in case we've not seen the corresponding add]

            auto const oid = msg.oid ();

            auto const om_remove = book_side.m_oid_map.remove_and_get (oid);

            if (VR_LIKELY (om_remove.second))
            {
                remove_order (oid, om_remove.first, book, book_side);
            }
            else
            {
                DLOG_trace1 << "oid not in the book: " << msg;
            }

            VR_IF_DEBUG (if (has_field<_book_, CTX> ()) field<_book_> (ctx) = & book;)
            return rc;
        }

    private: // ..............................................................

        static VR_FORCEINLINE LIMIT_ORDER_BOOK & current_book (CTX & ctx)
        {
            addr_const_t const book_ref = field<_book_> (ctx);
            assert_nonnull (book_ref); // relying on a selector ahead of us in the pipeline

            return (* static_cast<LIMIT_ORDER_BOOK *> (const_cast<addr_t> (book_ref))); // this listener is special (allowed to const_cast here)
        }

        /*
         * a faster overload when 'o_ref' has already been resolved by the caller
         */
        static VR_ASSUME_HOT void remove_order (ASX::oid_t const oid, order_type const & o, fast_order_ref_type const o_ref, book_type & book, side_type & book_side)
        {
            // remove 'oid' from the oid map:

            book_side.m_oid_map.remove (oid);


            fast_level_ref_type const o_parent_ref = field<_parent_> (o);
            level_type & l = book.level_pool ()[o_parent_ref];

            // remove 'o' from its parent level:

            l.m_orders.erase (order_list_type::s_iterator_to (o));

            // if 'o' removal causes 'l' to become empty, remove it from the price map
            // and release to the object pool:

            if (VR_UNLIKELY (l.m_orders.empty ())) // note: don't use size() here (may not be O(1))
            {
                book_side.m_price_map.erase (price_map_type::s_iterator_to (l));
                book.level_pool ().release (o_parent_ref);
            }
            else // otherwise adjust 'l's aggregate fields
            {
                if (book_type::level::has_qty ())
                    field<_qty_> (l) -= field<_qty_> (o);
                if (book_type::level::has_order_count ())
                    -- field<_order_count_> (l);

                if (book_type::level::has_order_count ()) assert_eq (field<_order_count_> (l), l.m_orders.size ()); // note: size() is not O(1), only do this check in debug builds
            }

            book.order_pool ().release (o_ref);
        }

        static VR_ASSUME_HOT void remove_order (ASX::oid_t const oid, fast_order_ref_type const o_ref, book_type & book, side_type & book_side)
        {
            order_type const & o = book.order_pool ()[o_ref];

            remove_order (oid, o, o_ref, book, book_side);
        }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
