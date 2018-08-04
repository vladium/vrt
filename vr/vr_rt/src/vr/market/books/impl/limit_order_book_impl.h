#pragma once

#include "vr/containers/intrusive/splay.h"
#include "vr/containers/util/chained_scatter_table.h"
#include "vr/fields.h"
#include "vr/market/books/defs.h"
#include "vr/market/prices.h"
#include "vr/market/sources.h"
#include "vr/tags.h"
#include "vr/util/logging.h"
#include "vr/util/object_pools.h"
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

using order_ref_type        = uint32_t;
using level_ref_type        = uint32_t; // ASX-55 TODO add design option to choose uint16_t for sufficiently small universes

template<typename ORDER_POOL, typename LEVEL_POOL>
struct object_pool_arena
{
    ORDER_POOL m_order_pool;
    LEVEL_POOL m_level_pool;

}; // end of class
//............................................................................
//............................................................................
namespace impl
{
using namespace impl_common; // defs from "defs.h"


template<typename T_PRICE, typename T_OID, typename USER_DATA, bitset32_t BOOK_TRAITs, bitset32_t LEVEL_TRAITs, bitset32_t ORDER_TRAITs>
class limit_order_book_impl; // forward

//............................................................................

struct ot_bit final
{
    enum enum_t
    {
        participant
    };

}; // end of enum

template<bitset32_t SELECTED>
struct book_order_schema
{
    using type          = meta::make_schema_t
                        <
                            // always present fields:

                            meta::fdef_<level_ref_type,     _parent_>,      // [immutable] references parent level
                            meta::fdef_<qty_t,              _qty_>,         // current qty of this order

                            // configurable fields:

                            meta::fdef_<participant_t,      _participant_,  meta::elide<(! (SELECTED & (1 << ot_bit::participant)))>>
                        >;

}; // end of metafunction
//............................................................................

struct lt_bit final
{
    enum enum_t
    {
        qty,            // aggregate level qty
        order_count     // aggregate level order count TODO not very clear what this means vs the list const-size option
    };

}; // end of enum

template<typename T_PRICE, bitset32_t SELECTED>
struct book_level_schema
{
    using type          = meta::make_schema_t
                        <
                            // always present fields:

                            meta::fdef_<T_PRICE,            _price_>,       // [immutable] price of this book level

                            // configurable fields:

                            meta::fdef_<qty_t,              _qty_,          meta::elide<(! (SELECTED & (1 << lt_bit::qty)))>>,
                            meta::fdef_<order_count_t,      _order_count_,  meta::elide<(! (SELECTED & (1 << lt_bit::order_count)))>>
                        >;

}; // end of metafunction
//............................................................................

struct order_mark   { }; // base for finding 'order'

template<typename ... As>
struct order: public order_mark
{
    static constexpr bitset32_t trait_set       = (util::contains<_participant_, As ...>::value     << ot_bit::participant);

}; // end of tag

using default_order         = order<_qty_>;

//............................................................................

struct level_mark   { }; // base for finding 'level'

template<typename ... As>
struct level: public level_mark
{
    using order_def         = util::find_derived_t<order_mark, default_order, As ...>;

    static constexpr bitset32_t trait_set       = (util::contains<_qty_, As ...>::value             << lt_bit::qty)
                                                | (util::contains<_order_count_, As ...>::value     << lt_bit::order_count);

}; // end of tag

using default_level         = level<default_order>; // note: default doesn't enable order queue maint

//............................................................................

struct bt_bit final
{
    enum enum_t
    {
        depth,      // if chosen, O(1) book side depth is available
        user_data
    };

}; // end of enum

template<typename ... ATTRIBUTEs>
struct book_traits
{
    using user_data_def     = util::find_derived_t<user_data_mark, default_user_data, ATTRIBUTEs ...>;
    using user_data_type    = typename user_data_def::user_type;

    static constexpr bool user_data_empty ()    { return std::is_same<user_data_type, empty_ud>::value; }

    static constexpr bitset32_t trait_set       = (util::contains<_depth_, ATTRIBUTEs ...>::value   << bt_bit::depth)
                                                | (! user_data_empty ()                             << bt_bit::user_data);

}; // end of traits

template<typename USER_DATA, bitset32_t SELECTED>
struct book_base_schema
{
    using type          = meta::make_schema_t
                        <
                            // configurable fields:

                            // ['_depth_' is materialized via a price map trait]
                            meta::fdef_<USER_DATA,          _user_data_,    meta::elide<(! (SELECTED & (1 << bt_bit::user_data)))>>
                        >;

}; // end of metafunction
//............................................................................
/*
 * choose how to pool objects
 */
template<typename T, typename T_POOL_REF>
struct book_object_pool_traits
{
    using value_type    = T;
    using pool_type     = util::object_pool<T, typename util::pool_options<T, T_POOL_REF>::type>;

    using ref_type      = typename pool_type::pointer_type;
    using fast_ref_type = typename pool_type::fast_pointer_type;

}; // end of metafunction
//............................................................................
/*
 * even if overall traits call for aggregate level values only, individual orders still
 * have to be tracked to a degree sufficient to support ITCH 'order_delete' which provides 'oid'
 * as the only identifier
 *
 * hence each 'book_order' is a node in an intrusive dl list owned by its parent 'book_level':
 *
 * TODO use a dl list only in no-_order_queue_ mode, otherwise use ? to manage queue ranks
 */
using book_order_base   = intrusive::bi::list_base_hook<intrusive::bi_traits::link_mode>;

template<bitset32_t ORDER_TRAITs>
struct book_order: public meta::make_compact_struct_t<typename book_order_schema<ORDER_TRAITs>::type, book_order_base>
{
    // ACCESSORs:

    qty_t const & qty () const VR_NOEXCEPT // always available
    {
        return field<_qty_> (* this);
    }

}; // end of class
//............................................................................

template<typename /* book_level */BOOK_LEVEL, bool USE_FIELD = false>
struct calc_sum
{
    static qty_t evaluate (BOOK_LEVEL const & lvl) VR_NOEXCEPT // O(N)
    {
        qty_t r { };

        for (auto const & o : lvl.m_orders)
        {
            r += o.qty ();
        }

        return r;
    }

}; // end of master

template<typename /* book_level */BOOK_LEVEL>
struct calc_sum<BOOK_LEVEL, /* USE_FIELD */true>
{
    static qty_t const & evaluate (BOOK_LEVEL const & lvl) VR_NOEXCEPT // O(1)
    {
        return lvl.qty ();
    }

}; // end of specialization
//............................................................................
/*
 * each 'book_level' aggregates a list of 'book_order's while itself being a node
 * of an intrusive price map tree:
 */
using book_level_base   = intrusive::bi::bs_set_base_hook<intrusive::bi_traits::link_mode>;

template<typename T_PRICE, bitset32_t LEVEL_TRAITs, bitset32_t ORDER_TRAITs>
class book_level: public meta::make_compact_struct_t<typename book_level_schema<T_PRICE, LEVEL_TRAITs>::type, book_level_base>
{
    private: // ..............................................................

        using super         = meta::make_compact_struct_t<typename book_level_schema<T_PRICE, LEVEL_TRAITs>::type, book_level_base>;
        using this_type     = book_level<T_PRICE, LEVEL_TRAITs, ORDER_TRAITs>;

    public: // ...............................................................

        using price_type    = T_PRICE;
        using order_type    = book_order<ORDER_TRAITs>;

    private: // ..............................................................

        // if 'LEVEL_TRAITs' ask for _order_count_, we don't need to ask the list impl to define
        // a field with equivalent semantics:

        static constexpr bool use_order_count_field ()      { return has_field<_order_count_, super> (); }

        using order_list    = intrusive::bi::list
                            <
                                order_type,
                                intrusive::bi::constant_time_size<(! use_order_count_field ())>, // TODO what trying to do here
                                intrusive::bi::size_type<order_count_t>
                            >;

    public: // ...............................................................

        static constexpr bool const_time_qty ()             { return has_field<_qty_, super> (); }

        /*
         * note a stateful comparator: exchange some runtime inefficiency for the convenience of having both BID/ASK sides having the same type
         */
        struct comparator
        {
            VR_FORCEINLINE comparator (side::enum_t const s) :
                m_side { s }
            {
            }

            // boost.intrusive:

            VR_FORCEINLINE bool operator() (T_PRICE const & lhs, T_PRICE const & rhs) const VR_NOEXCEPT
            {
                return (m_side == side::BID ? (rhs < lhs) : (lhs < rhs)); // TODO branchless version
            }

            side::enum_t /* const */ m_side;

        }; // end of nested class

        // ACCESSORs:

        using const_iterator    = typename order_list::const_iterator;


        price_type const & price () const VR_NOEXCEPT // always available
        {
            return field<_price_> (* this);
        }

        order_count_t order_count () const VR_NOEXCEPT // always available TODO expose mutator when _order_count_ enabled?
        {
            return (use_order_count_field () ? field<_order_count_> (* this) : m_orders.size ()); // compile-time choice
        }

        template<bool _ = const_time_qty ()>
        auto qty () const VR_NOEXCEPT -> typename std::enable_if<_, qty_t const &>::type
        {
            return field<_qty_> (* this);
        }

        template<bool _ = const_time_qty ()>
        auto qty () const VR_NOEXCEPT_IF (VR_RELEASE) -> typename std::enable_if<(! _), qty_t const &>::type
        {
            VR_ASSUME_UNREACHABLE (const_time_qty ());
        }

        // order traversal:

        const_iterator begin () const
        {
            return m_orders.cbegin ();
        }

        const_iterator end () const
        {
            return m_orders.cend ();
        }

    private: // ..............................................................

        template<typename, bool> friend class calc_sum; // grant private access
        template<typename, typename, typename, bitset32_t, bitset32_t, bitset32_t> friend class limit_order_book_impl; // grant private access
        template<source::enum_t, typename ...> friend class market_data_listener; // grant raw and mutation access
        template<typename> friend class limit_order_book_checker; // grant private access
        template<typename _LOB> friend std::string __print__impl (_LOB const & obj, int32_t const max_depth) VR_NOEXCEPT_IF (VR_RELEASE); // grant access to 'evaluate_qty ()'

        /*
         * used by __print__() impl
         */
        auto evaluate_qty () const VR_NOEXCEPT -> decltype (calc_sum<this_type, const_time_qty ()>::evaluate (* this))
        {
            return calc_sum<this_type, const_time_qty ()>::evaluate (* this);
        }


        order_list m_orders;

}; // end of class
//............................................................................

constexpr int32_t initial_oid_map_capacity ()   { return 512; }

template<typename T_OID, typename T_REF>
struct make_oid_map
{
    // TODO for oids, whether or not an identity_hash is good enough depends on
    // the data -- check low bit distribution for ASX)

    using type          = util::chained_scatter_table<T_OID, T_REF, util::identity_hash<T_OID>>; // TODO customize 'OPTIONS' if advantageous

}; // end of metafunction
//............................................................................

template<typename /* book_level */BOOK_LEVEL>
struct price_of
{
     using type     = typename BOOK_LEVEL::price_type;

     VR_FORCEINLINE type const & operator() (BOOK_LEVEL const & level) const VR_NOEXCEPT // always available
     {
         return field<_price_> (level);
     }

}; // end of 'key of value' functor


template<bool PRICE_MAP_CONST_SIZE, typename /* book_level */BOOK_LEVEL>
struct make_price_map
{
    using comparator    = typename BOOK_LEVEL::comparator;

    using type          = intrusive::bi::splay_set
                        <
                            BOOK_LEVEL,
                            intrusive::bi::key_of_value<price_of<BOOK_LEVEL>>, // enables efficient key-only queries
                            intrusive::bi::compare<comparator>,
                            intrusive::bi::constant_time_size<PRICE_MAP_CONST_SIZE>,
                            intrusive::bi::size_type<int32_t>
                        >;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'md'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
