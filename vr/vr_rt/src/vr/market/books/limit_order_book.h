#pragma once

#include "vr/market/books/impl/limit_order_book_impl.h"
#include "vr/market/books/limit_order_book_checker.h"
#include "vr/market/books/market_data_listener.h"
#include "vr/market/defs.h"
#include "vr/util/memory.h"

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

template<typename T_OID, bool PRICE_MAP_CONST_SIZE, typename /* book_level */BOOK_LEVEL, typename /* book_object_pool_traits */ORDER_POOL_TRAITS>
class book_side
{
    private: // ..............................................................

        using order_type        = typename BOOK_LEVEL::order_type;
        using oid_map_type      = typename make_oid_map<T_OID, typename ORDER_POOL_TRAITS::ref_type>::type; // TODO could also use direct order ptrs:

        using price_map_type    = typename make_price_map<PRICE_MAP_CONST_SIZE, BOOK_LEVEL>::type;
        using price_comparator  = typename price_map_type::key_compare;


        book_side (side::enum_t const s) :
            m_price_map (price_comparator { s })
        {
        }

    public: // ...............................................................

        using level             = BOOK_LEVEL;
        using price_type        = typename level::price_type;
        using const_iterator    = typename price_map_type::const_iterator;

        // ACCESSORs:

        side::enum_t market_side () const VR_NOEXCEPT
        {
            return m_price_map.key_comp ().m_side;
        }

        // price query (return iterators; can avoid some branches for certain code patterns):

        VR_FORCEINLINE const_iterator find_eq_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            return m_price_map.find (price);
        }

        // "worse": away from the inside

        VR_FORCEINLINE const_iterator find_next_worse_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            return m_price_map.upper_bound (price);
        }

        VR_FORCEINLINE const_iterator find_eq_or_next_worse_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            return m_price_map.lower_bound (price);
        }

        // "better": towards the inside

        VR_FORCEINLINE const_iterator find_next_better_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto i = m_price_map.lower_bound (price);
            if (i != m_price_map.begin ()) // note: it's correct not to have to also check against 'm_price_map.end ()'
                -- i;

            return i;
        }

        VR_FORCEINLINE const_iterator find_eq_or_next_better_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto i = m_price_map.upper_bound (price);
            if (i != m_price_map.begin ()) // note: it's correct not to have to also check against 'm_price_map.end ()'
                -- i;

            return i;
        }


        // price query (return pointers to 'level's; note that @ref iterator_to() can always convert to an iterator):

        /**
         * @return level object at this exact price or nullptr if no match
         */
        VR_ASSUME_HOT level const * eq_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto const i = find_eq_price (price);
            if (i == m_price_map.end ())
                return nullptr;

            return (& (* i));
        }

        // "worse": away from the inside

        VR_ASSUME_HOT level const * next_worse_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto const i = find_next_worse_price (price);
            if (i == m_price_map.end ())
                return nullptr;

            return (& (* i));
        }

        VR_ASSUME_HOT level const * eq_or_next_worse_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto const i = find_eq_or_next_worse_price (price);
            if (i == m_price_map.end ())
                return nullptr;

            return (& (* i));
        }

        // "better": towards the inside

        VR_ASSUME_HOT level const * next_better_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto i = find_next_better_price (price);
            if (i == m_price_map.end ())
                return nullptr;

            return (& (* i));
        }

        VR_ASSUME_HOT level const * eq_or_next_better_price (typename call_traits<price_type>::param price) const VR_NOEXCEPT
        {
            auto i = find_eq_or_next_better_price (price);
            if (i == m_price_map.end ())
                return nullptr;

            return (& (* i));
        }

        // price level traversal (via iterators):

        /**
         * @note unlike @ref depth() without _depth_ traits, this is always O(1)
         */
        VR_ASSUME_HOT bool empty () const VR_NOEXCEPT // note: this is always O(1)
        {
            return m_price_map.empty ();
        }

        template<bool _ = PRICE_MAP_CONST_SIZE>
        auto depth () const VR_NOEXCEPT -> typename std::enable_if<(_), int32_t>::type
        {
            return m_price_map.size ();
        }

        template<bool _ = PRICE_MAP_CONST_SIZE>
        auto depth () const VR_NOEXCEPT_IF (VR_RELEASE) -> typename std::enable_if<(! _), int32_t>::type
        {
            VR_ASSUME_UNREACHABLE (PRICE_MAP_CONST_SIZE);
        }

        VR_FORCEINLINE static const_iterator iterator_to (level const & lvl) // note: static
        {
            return price_map_type::s_iterator_to (lvl);
        }

        VR_ASSUME_HOT const_iterator begin () const
        {
            return m_price_map.cbegin ();
        }

        VR_ASSUME_HOT const_iterator end () const
        {
            return m_price_map.cend ();
        }

    private: // ..............................................................

        template<typename, typename, typename, bitset32_t, bitset32_t, bitset32_t> friend class limit_order_book_impl; // grant private access
        template<source::enum_t, typename ...> friend class market_data_listener; // grant raw and mutation access
        template<typename> friend class limit_order_book_checker; // grant private access


        oid_map_type m_oid_map { initial_oid_map_capacity () };
        price_map_type m_price_map;

}; // end of class
//............................................................................
/*
 * canonicalize variadic tag sequences into bitsets to reduce the size of the
 * template universe this can get instantated into
 */
template<typename T_PRICE, typename T_OID, typename USER_DATA, bitset32_t BOOK_TRAITs, bitset32_t LEVEL_TRAITs, bitset32_t ORDER_TRAITs>
class limit_order_book_impl: public meta::make_compact_struct_t<typename book_base_schema<USER_DATA, BOOK_TRAITs>::type>
{
    private: // ..............................................................

        using super         = meta::make_compact_struct_t<typename book_base_schema<USER_DATA, BOOK_TRAITs>::type>;
        using this_type     = limit_order_book_impl<T_PRICE, T_OID, USER_DATA, BOOK_TRAITs, LEVEL_TRAITs, ORDER_TRAITs>;

    public: // ...............................................................

        using price_type            = T_PRICE;

        using oid_type              = T_OID;


        template<typename POOL_ARENA>
        limit_order_book_impl (POOL_ARENA & arena) :
            m_pool_arena { & arena.object_pools () }
        {
            for (side::enum_t s : side::values ())
            {
                new (& at (s)) side_type { s };
            }

            LOG_trace1 << "constructed book (sizeof 'USER_DATA' " << meta::storage_size_of<USER_DATA>::value << ", sizeof 'order' " << sizeof (order_type) << ", sizeof 'level' " << sizeof (level_type) << ')';
        }

        ~limit_order_book_impl ()
        {
            for (side::enum_t s : side::values ())
            {
                LOG_trace1 << s << " oid map capacity on destruction: " << at (s).m_oid_map.capacity ();

                util::destruct (at (s)); // clean up sides created with placement new
            }
        }

    protected: // ............................................................

        template<source::enum_t, typename ...> friend class market_data_listener; // grant raw and mutation access
        template<typename> friend class limit_order_book_checker; // grant private access


        using level_type            = book_level<T_PRICE, LEVEL_TRAITs, ORDER_TRAITs>;
        using order_type            = typename level_type::order_type;

        using order_pool_traits     = book_object_pool_traits<order_type, order_ref_type>;
        using order_pool_type       = typename order_pool_traits::pool_type;

        using level_pool_traits     = book_object_pool_traits<level_type, level_ref_type>;
        using level_pool_type       = typename level_pool_traits::pool_type;

        using pool_arena            = object_pool_arena<order_pool_type, level_pool_type>;

        using side_type             = book_side<T_OID, (BOOK_TRAITs & (1 << bt_bit::depth)), level_type, order_pool_traits>;
        using side_storage          = typename std::aligned_storage<sizeof (side_type), alignof (side_type)>::type; // used to side-step some array construction issues

    public: // ...............................................................

        using side_iterator         = typename side_type::const_iterator;

    protected: // ............................................................

        using price_map_type        = typename side_type::price_map_type;
        using order_list_type       = typename level_type::order_list;

        vr_static_assert (std::is_same<typename order_pool_traits::ref_type, order_ref_type>::value);

        using fast_order_ref_type   = typename order_pool_traits::fast_ref_type;
        using fast_level_ref_type   = typename level_pool_traits::fast_ref_type;


        // ACCESSORs:

        VR_FORCEINLINE order_pool_type const & order_pool () const VR_NOEXCEPT
        {
            return m_pool_arena->m_order_pool;
        }

        VR_FORCEINLINE level_pool_type const & level_pool () const VR_NOEXCEPT
        {
            return m_pool_arena->m_level_pool;
        }

        // MUTATORs:

        VR_FORCEINLINE order_pool_type & order_pool () VR_NOEXCEPT
        {
            return const_cast<order_pool_type &> (const_cast<this_type const *> (this)->order_pool ());
        }

        VR_FORCEINLINE level_pool_type & level_pool () VR_NOEXCEPT
        {
            return const_cast<level_pool_type &> (const_cast<this_type const *> (this)->level_pool ());
        }

        VR_FORCEINLINE side_type & at (side::enum_t const s) VR_NOEXCEPT
        {
            return const_cast<side_type &> (const_cast<limit_order_book_impl const *> (this)->at (s));
        }

        // __print__() support:

        template<typename _LOB> friend std::string __print__impl (_LOB const & obj, int32_t const max_depth) VR_NOEXCEPT_IF (VR_RELEASE); // impl in "limit_order_book_io.h"


        pool_arena * const m_pool_arena;
        std::array<side_storage, side::size> m_sides;

    public: // ...............................................................

        using book_user_data        = USER_DATA; // 'book_user_data' is per book instance

        /**
         * integration with @ref market_data_view
         */
        struct traits final
        {
            using pool_arena        = this_type::pool_arena;

        }; // end of public traits

        /**
         * TMP assist
         */
        struct order final
        {
            static constexpr bool has_qty ()            { return true; } // always used, for now
            static constexpr bool has_participant ()    { return (ORDER_TRAITs & (1 << ot_bit::participant)); }

        }; // end of nested scope

        /**
         * TMP assist
         */
        struct level final
        {
            static constexpr bool has_price ()          { return true; } // always used, for now
            static constexpr bool has_qty ()            { return (LEVEL_TRAITs & (1 << lt_bit::qty)); }
            static constexpr bool has_order_count ()    { return (LEVEL_TRAITs & (1 << lt_bit::order_count)); }
            static constexpr bool has_order_queue ()    { return false; } // not implemented yet

        }; // end of nested scope

        static constexpr bool const_time_depth ()       { return (BOOK_TRAITs & (1 << bt_bit::depth)); }
        static constexpr bool has_user_data ()          { return (BOOK_TRAITs & (1 << bt_bit::user_data)); }

        // ACCESSORs:

        // side indexing:

        VR_FORCEINLINE side_type const & at (side::enum_t const s) const VR_NOEXCEPT
        {
            return (* reinterpret_cast<side_type const *> (& m_sides [s]));
        }

        VR_FORCEINLINE side_type const & operator[] (side::enum_t const s) const VR_NOEXCEPT
        {
            return at (s);
        }

        // order query:

        VR_ASSUME_HOT order_type const * find_order (side::enum_t const s, typename call_traits<oid_type>::param oid) const
        {
            order_ref_type const * const o_ref_ptr = at (s).m_oid_map.get (oid);

            if (VR_LIKELY (o_ref_ptr != nullptr))
            {
                return (& order_pool ()[* o_ref_ptr]);
            }

            return nullptr;
        }

        // order to its parent price level:

        VR_ASSUME_HOT level_type const & level_of (order_type const & o) const
        {
            return level_pool () [field<_parent_> (o)];
        }

        // price level to the book's side iterator:

        VR_FORCEINLINE static side_iterator iterator_to (level_type const & lvl) // note: static
        {
            return price_map_type::s_iterator_to (lvl);
        }

        // user data access (always mutable):

        template<bool _ = has_user_data ()>
        VR_ASSUME_HOT auto user_data () const VR_NOEXCEPT -> typename std::enable_if<_, book_user_data &>::type // note: returning mutable ref
        {
            return field<_user_data_> (* const_cast<this_type *> (this));
        }

        template<bool _ = has_user_data ()>
        VR_ASSUME_HOT auto user_data () const VR_NOEXCEPT_IF (VR_RELEASE) -> typename std::enable_if<(! _), book_user_data &>::type // note: returning mutable ref
        {
            VR_ASSUME_UNREACHABLE (cn_<USER_DATA> ());
        }

        // OPERATORs:

        /*
         * TODO explain how 'max_depth' is interpreted for crossed (auction) books
         */
        friend VR_ASSUME_COLD std::string __print__ (this_type const & obj, int32_t const max_depth = 5) VR_NOEXCEPT_IF (VR_RELEASE)
        {
            return __print__impl (obj, max_depth);
        }

        friend VR_ASSUME_COLD std::ostream & operator<< (std::ostream & os, this_type const & obj) VR_NOEXCEPT_IF (VR_RELEASE)
        {
            return os << __print__ (obj);
        }

        // debug assists:

        VR_ASSUME_COLD std::tuple</* orders */int32_t, /* levels */int32_t> check () const
        {
            return limit_order_book_checker<this_type>::check_book (* this);
        }

}; // end of class
//............................................................................

template<typename T_PRICE, typename T_OID, typename ... ATTRIBUTEs>
struct make_limit_order_book
{
    using bt                = book_traits<ATTRIBUTEs ...>;
    using user_data_type    = typename bt::user_data_type; // 'USER_DATA' is per instrument (i.e. 'limit_order_book' instance)

    using lmark             = util::find_derived_t<level_mark, default_level, ATTRIBUTEs ...>;
    using omark             = typename lmark::order_def;

    using type          = limit_order_book_impl<T_PRICE, T_OID, user_data_type, bt::trait_set, lmark::trait_set, omark::trait_set>;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'md'
//............................................................................

template<typename ... /* _qty_, ... */ATTRIBUTEs>
using order                 = md::impl::order<ATTRIBUTEs ...>;

template<typename ... /* order<...>, _qty_, _order_count_, _order_queue_, ... */ATTRIBUTEs>
using level                 = md::impl::level<ATTRIBUTEs ...>;

/// also available: 'user_data<T>' defined in "defs."

//............................................................................
/**
 * TODO doc API
 */
template
<
    typename T_PRICE,       // "book" price type (likely 'price_si_t')
    typename T_OID,         // source-specific oid type
    typename ... ATTRIBUTEs // user_data<...>, level<order<_qty_, ...>, _order_queue_, ...>, ...
>
class limit_order_book final: public md::impl::make_limit_order_book<T_PRICE, T_OID, ATTRIBUTEs ...>::type
{
    private: // ..............................................................

        using super         = typename md::impl::make_limit_order_book<T_PRICE, T_OID, ATTRIBUTEs ...>::type;

    public: // ...............................................................

        template<typename POOL_ARENA>
        limit_order_book (POOL_ARENA & arena) :
            super (arena)
        {
        }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
