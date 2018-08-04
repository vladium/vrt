#pragma once

#include "vr/containers/util/chained_scatter_table.h"
#include "vr/fields.h"
#include "vr/market/books/defs.h"
#include "vr/market/books/execution_listener.h"
#include "vr/market/prices.h"
#include "vr/market/rt/agents/exceptions.h"
#include "vr/meta/objects_io.h" // meta::print_visitor
#include "vr/tags.h"
#include "vr/util/logging.h"
#include "vr/util/object_pools.h"
#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ex
{
//............................................................................

struct fsm_state
{
    using bitset_type       = bitset32_t; // TODO

    VR_ENUM (order,
        (
            ERROR,

            created,
            live,   // means can potentially still be in the exchange book
            done    // means definitely no on book
        ),
        printable

    ); // end of enum

    VR_EXPLICIT_ENUM (pending,
        bitset_type,
            (0b00000000,    _)

            (0b00001000,    submit)
            (0b00010000,    replace)
            (0b00100000,    cancel)
            (0b01000000,    finalize)
        ,
        printable

    ); // end of enum

    vr_static_assert (static_cast<bitset_type> (pending::submit) > static_cast<bitset_type> (order::size));

    // ACCESSORs:

    VR_FORCEINLINE order::enum_t order_state () const
    {
        return static_cast<order::enum_t> (m_data & 0b11);
    }

    VR_FORCEINLINE bitset_type pending_state () const
    {
        return (m_data & ~0b11);
    }

    VR_FORCEINLINE explicit operator bitset_type () const
    {
        return m_data;
    }

    // MUTATORs:

    VR_FORCEINLINE void transition (order::enum_t const os, bitset_type const ps)
    {
        assert_zero (ps & 0b11);

        m_data = (os | ps);
    }

    void error_transition ()
    {
        m_data = (order::ERROR | pending_state ());
    }

    VR_FORCEINLINE void mark (pending::enum_t const p)
    {
        m_data |= p;
    }
    VR_FORCEINLINE void clear (pending::enum_t const p)
    {
        m_data &= ~ p;
    }

    // OPERATORs:

    friend VR_FORCEINLINE bool operator== (fsm_state const & lhs, fsm_state const & rhs) VR_NOEXCEPT
    {
        return (lhs.m_data == rhs.m_data);
    }
    friend VR_FORCEINLINE bool operator!= (fsm_state const & lhs, fsm_state const & rhs) VR_NOEXCEPT
    {
        return (! (lhs == rhs));
    }

    friend VR_ASSUME_COLD std::ostream & operator<< (std::ostream & os, fsm_state const & obj) VR_NOEXCEPT;


    private:

        bitset_type m_data { };

}; // end of class
//............................................................................

struct fsm_error
{
    friend VR_ASSUME_COLD std::ostream & operator<< (std::ostream & os, fsm_error const & obj) VR_NOEXCEPT;

    util::signed_type_of_size<sizeof (fsm_state::bitset_type)>::type m_lineno { };
    fsm_state m_state { };

}; // end of class
//............................................................................

using order_ref_type        = uint32_t; // TODO

template<typename ORDER_POOL>
struct object_pool_arena
{
    ORDER_POOL m_order_pool;

}; // end of class
//............................................................................
//............................................................................
namespace impl
{
using namespace impl_common; // defs from "defs.h"

//............................................................................

template<typename T_OTK>
struct make_otk_history
{
    struct type
    {
       using ptr        = std::unique_ptr<std::vector<T_OTK>>;

       friend VR_ASSUME_COLD std::string __print__ (type const & obj) VR_NOEXCEPT
       {
           std::stringstream os { };

           os << '[';
           if (obj.m_otk_trace)
           {
               os << print (* obj.m_otk_trace);
           }
           os << ']';

           return os.str ();
       }

       ptr m_otk_trace { };
    };

}; // end of metafunction
//............................................................................

template<typename T_OTK, typename = void>
struct storage_type_of
{
    using type      = T_OTK;

}; // end of master

template<typename T_OTK>
struct storage_type_of<T_OTK,
    util::void_t<typename T_OTK::storage_type> >
{
    using type      = typename T_OTK::storage_type;

}; // end of specialization
//............................................................................

struct ot_bit final
{
    enum enum_t
    {
        user_data
    };

}; // end of enum

template<typename ... As>
struct book_order_traits
{
    using user_data_def     = util::find_derived_t<user_data_mark, default_user_data, As ...>;
    using user_data_type    = typename user_data_def::user_type;

    static constexpr bool user_data_empty ()    { return std::is_same<user_data_type, empty_ud>::value; }

    static constexpr bitset32_t trait_set       = (! user_data_empty ()         << ot_bit::user_data);

}; // end of traits


template<typename T_PRICE, typename T_OTK, typename T_OID, typename T_QTY, typename T_USER_DATA, bitset32_t SELECTED>
struct book_order_schema
{
    using otk_history_impl  = typename make_otk_history<T_OTK>::type;

    using type      = meta::make_schema_t
                    <
                        // always present fields:

                        meta::fdef_<ex::fsm_state,      _state_>
                        ,
                        meta::fdef_<liid_t,             _liid_> // this could be optional (order<_liid_...>)
                        ,
                        meta::fdef_<T_OTK,              _otk_>,     // "current"
                        meta::fdef_<otk_history_impl,   _history_>, // used to keep erasure ops O(number of cxrs per oid)
                        meta::fdef_<T_OID,              _oid_>
                        ,
                        meta::fdef_<T_PRICE,            _price_>,
                        meta::fdef_<T_QTY,              _qty_>
                        ,
                        meta::fdef_<T_QTY,              _qty_filled_>
                        ,
                        meta::fdef_<order_ref_type,     _loid_>,
                        meta::fdef_<fsm_error,          _error_>,   // meaningful only if '_state_' indicates 'ERROR'
                        meta::fdef_<int32_t,            _reject_code_>,
                        meta::fdef_<uint8_t,            _cancel_code_>

                        // configurable fields:
                        ,
                        meta::fdef_<T_USER_DATA,        _user_data_,    meta::elide<(! (SELECTED & (1 << ot_bit::user_data)))>>
                    >;

}; // end of metafunction

template<typename T_PRICE, typename T_OTK, typename T_OID, typename T_QTY, typename T_USER_DATA, bitset32_t ORDER_TRAITS>
class book_order: public meta::make_compact_struct_t<typename book_order_schema<T_PRICE, T_OTK, T_OID, T_QTY, T_USER_DATA, ORDER_TRAITS>::type>
{
    private: // ..............................................................

        using this_type     = meta::make_compact_struct_t<typename book_order_schema<T_PRICE, T_OTK, T_OID, T_QTY, T_USER_DATA, ORDER_TRAITS>::type>;

    public: // ...............................................................

        using fsm_state     = ex::fsm_state;

        using price_type        = T_PRICE;
        using otk_type          = T_OTK;
        using oid_type          = T_OID;
        using qty_type          = T_QTY;
        using user_data_type    = T_USER_DATA;

        static constexpr bool has_user_data ()      { return (ORDER_TRAITS & (1 << ot_bit::user_data)); }

        // ACCESSORs:

        fsm_state const & fsm () const VR_NOEXCEPT // always available
        {
            return field<_state_> (* this);
        }

        otk_type const & otk () const VR_NOEXCEPT // always available
        {
            return field<_otk_> (* this);
        }

        oid_type const & oid () const VR_NOEXCEPT // always available
        {
            return field<_oid_> (* this);
        }

        price_type const & price () const VR_NOEXCEPT // always available
        {
            return field<_price_> (* this);
        }

        qty_type const & qty () const VR_NOEXCEPT // always available
        {
            return field<_qty_> (* this);
        }

        qty_type const & qty_filled () const VR_NOEXCEPT // always available
        {
            return field<_qty_filled_> (* this);
        }

        // user data access (always mutable):

        template<bool _ = has_user_data ()>
        VR_ASSUME_HOT auto user_data () const VR_NOEXCEPT -> typename std::enable_if<_, user_data_type &>::type // note: returning mutable ref
        {
            return field<_user_data_> (* const_cast<this_type *> (this));
        }

        template<bool _ = has_user_data ()>
        VR_ASSUME_HOT auto user_data () const VR_NOEXCEPT_IF (VR_RELEASE) -> typename std::enable_if<(! _), user_data_type &>::type // note: returning mutable ref
        {
            VR_ASSUME_UNREACHABLE (cn_<user_data_type> ());
        }

        // OPERATORs:

//        friend VR_ASSUME_COLD std::ostream & operator<< (std::ostream & os, this_type const & obj) VR_NOEXCEPT
//        {
//            // TODO enable only if 'user_data_type' is streamable
//
//            meta::print_visitor<this_type> v { obj, os };
//            meta::apply_visitor<this_type> (v);
//
//            return os;
//        }

        friend VR_ASSUME_COLD std::string __print__ (this_type const & obj) VR_NOEXCEPT
        {
            // TODO enable only if 'user_data_type' is streamable

            std::stringstream os { };

            os << '{';
            {
                meta::print_visitor<this_type> v { obj, os };
                meta::apply_visitor<this_type> (v);
            }
            os << '}';

            return os.str ();
        }

    private: // ..............................................................

        template<source::enum_t, typename ...> friend class execution_listener; // grant access to mutators

        // MUTATORs:

        VR_ASSUME_COLD void fsm_error_transition (int32_t const lineno)
        {
            field<_error_> (* this) = { lineno, field<_state_> (* this) };
            field<_state_> (* this).error_transition ();
        }

}; // end of class
//............................................................................

template<typename T_PRICE, typename T_OTK, typename T_OID, typename T_QTY, typename ... As>
struct make_book_order
{
    using ot                = book_order_traits<As ...>;
    using user_data_type    = typename ot::user_data_type;

    using type          = book_order<T_PRICE, T_OTK, T_OID, T_QTY, user_data_type, ot::trait_set>;

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

constexpr int32_t initial_book_capacity ()      { return 128; }

template<typename T_OTK, typename ORDER_TYPE>
struct make_otk_map
{
    using otk_storage_type  = typename storage_type_of<T_OTK>::type;

    using type          = util::chained_scatter_table<otk_storage_type, ORDER_TYPE *, util::crc32_hash<otk_storage_type>>;

}; // end of metafunction

template<typename T_OID, typename ORDER_TYPE>
struct make_oid_map
{
    // TODO for oids, whether or not an identity_hash is good enough depends on
    // the data -- check low bit distribution for ASX)

    using type          = util::chained_scatter_table<T_OID, ORDER_TYPE *, util::identity_hash<T_OID>>; // TODO customize 'OPTIONS' if advantageous

}; // end of metafunction
//............................................................................
/*
 * canonicalize variadic tag sequences into bitsets to reduce the size of the
 * template universe this can get instantated into
 */
template<typename/* book_order */BOOK_ORDER>
class execution_order_book_impl: noncopyable
{
    private: // ..............................................................

        using this_type     = execution_order_book_impl<BOOK_ORDER>;

    public: // ...............................................................

        using order_type        = BOOK_ORDER;

        using price_type        = typename BOOK_ORDER::price_type;

        using otk_type          = typename BOOK_ORDER::otk_type;
        using oid_type          = typename BOOK_ORDER::oid_type;
        using qty_type          = typename BOOK_ORDER::qty_type;

        static constexpr bool has_user_data ()      { return BOOK_ORDER::has_user_data (); }

        using order_user_data   = typename BOOK_ORDER::user_data_type; // 'order_user_data' is per order entry


        template<typename POOL_ARENA>
        execution_order_book_impl (POOL_ARENA & arena) :
            m_pool_arena { & arena.object_pools () }
        {
            LOG_trace1 << "constructed book (sizeof 'order_user_data' " << meta::storage_size_of<order_user_data>::value << ", sizeof 'order' " << sizeof (order_type) << ')';
        }


        // ACCESSORs:

        /**
         * @return count of unique oids in the book
         */
        auto size () const
        {
            return m_oid_map.size ();
        }

        VR_IF_DEBUG
        (
            auto otk_count () const
            {
                return m_otk_map.size ();
            }
        )

        // TODO accessors with CHECK_INPUT call-site traits

        order_type const * find_order (otk_type const & otk) const
        {
            return m_otk_map.get (static_cast<otk_storage_type> (otk));
        }

        // MUTATORs:

        order_type * find_order (otk_type const & otk)
        {
            return const_cast<order_type *> (const_cast<this_type const *> (this)->find_order (otk));
        }

        order_type & create_order (liid_t const liid, otk_type const & otk)
        {
            assert_null (m_otk_map.get (static_cast<otk_storage_type> (otk)), otk); // not an existing entry

            // allocate new order from the pool:

            auto const o_ref = order_pool ().allocate ();
            order_type & o = std::get<0> (o_ref);
            {
                vr_static_assert (has_field<_loid_, order_type> ());

                field<_loid_> (o) = std::get<1> (o_ref);

                field<_liid_> (o) = liid;
                field<_otk_> (o) = otk;
                field<_oid_> (o) = 0; // don't leave uninitialized in case we end up erasing after a reject

                field<_qty_filled_> (o) = 0;

                field<_reject_code_> (o) = 0;
                field<_cancel_code_> (o) = 0; // 'ASX::cancel_reason::na'
            }
            m_otk_map.put (static_cast<otk_storage_type> (otk), & o);

            return o;
        }

        void alias_order (order_type & o, otk_type const & new_otk)
        {
            assert_null (m_otk_map.get (static_cast<otk_storage_type> (new_otk)), new_otk); // not an existing entry

            // TODO do fsm state compatibility check here or in ex mgr?

            m_otk_map.put (static_cast<otk_storage_type> (new_otk), & o);

            // push 'new_otk' onto the history:

            std::unique_ptr<std::vector<otk_type>> & ptr = field<_history_> (o).m_otk_trace;
            if (! ptr) ptr = std::make_unique<std::vector<otk_type>> (); // TODO init capacity?

            std::vector<otk_type> & otk_trace = (* ptr);
            otk_trace.push_back (new_otk);
        }

        void erase_order (order_type & o)
        {
            // check that 'o' is in a state compatible with getting erased:
            {
                // nothing is known to be pending and it's not 'live':

                fsm_state const & state = o.fsm ();

                if (VR_UNLIKELY ((!! state.pending_state ()) | (state.order_state () == fsm_state::order::live)))
                    throw_cfx (agent_runtime_exception);
            }

            // oid map (note that 'o' may never got mapped, in which case
            // the following would be a no-op assuming zero is never a valid oid):

            m_oid_map.remove (field<_oid_> (o));

            // otk map:

#       if VR_DEBUG
            {
                // we should recover the same order slot from the "current" otk field:

                std::pair<order_type *, bool> const rc = m_otk_map.remove_and_get (static_cast<otk_storage_type> (field<_otk_> (o)));
                assert_condition (rc.second, field<_otk_> (o), field<_oid_> (o));
                assert_eq (rc.first, & o, field<_otk_> (o), field<_oid_> (o));
            }
#       else
            {
                m_otk_map.remove (static_cast<otk_storage_type> (field<_otk_> (o)));
            }
#       endif

            // process '_history_' if not null:
            {
                std::unique_ptr<std::vector<otk_type>> & ptr = field<_history_> (o).m_otk_trace;
                if (ptr != nullptr)
                {
                    std::vector<otk_type> const & otk_trace = (* ptr);
                    for (otk_type const & old_otk : otk_trace)
                    {
                        m_otk_map.remove (static_cast<otk_storage_type> (old_otk));
                    }
                    ptr->resize (0); // note: not resetting 'ptr', will re-use vector capacity
                }
            }

            // finally, release the pool slot:

            order_pool ().release (field<_loid_> (o));
        }

    private: // ..............................................................

        template<source::enum_t, typename ...> friend class execution_listener; // grant raw and mutation access

        using otk_storage_type      = typename storage_type_of<otk_type>::type;

        using otk_map_type          = typename make_otk_map<otk_type, order_type>::type; // otk -> ptr to order
        using oid_map_type          = typename make_oid_map<oid_type, order_type>::type; // oid -> ptr to order

        using order_pool_traits     = book_object_pool_traits<order_type, order_ref_type>;
        using order_pool_type       = typename order_pool_traits::pool_type;

        using fast_order_ref_type   = typename order_pool_traits::fast_ref_type;

        using pool_arena            = object_pool_arena<order_pool_type>;

        // ACCESSORs:

        VR_FORCEINLINE order_pool_type const & order_pool () const VR_NOEXCEPT
        {
            return m_pool_arena->m_order_pool;
        }

        // MUTATORs:

        VR_FORCEINLINE order_pool_type & order_pool () VR_NOEXCEPT
        {
            return const_cast<order_pool_type &> (const_cast<this_type const *> (this)->order_pool ());
        }

        pool_arena * const m_pool_arena;
        otk_map_type m_otk_map { initial_book_capacity () };
        oid_map_type m_oid_map { initial_book_capacity () };

    public: // ...............................................................

        /**
         * integration with @ref execution_view
         */
        struct traits final
        {
            using pool_arena        = this_type::pool_arena;

        }; // end of public traits

}; // end of class
//............................................................................
/*
 * for the same reason as above, extract only the types we have to have from 'SOURCE_TRAITS'
 */
template<typename T_PRICE, typename T_OTK, typename SOURCE_TRAITS, typename ... ATTRIBUTEs>
struct make_execution_order_book
{
    using oid_type          = typename SOURCE_TRAITS::oid_type;
    using qty_type          = typename SOURCE_TRAITS::qty_type;

    using order_type        = typename make_book_order<T_PRICE, T_OTK, oid_type, qty_type, ATTRIBUTEs ...>::type;

    using type          = execution_order_book_impl<order_type>;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'ex'
//............................................................................
/**
 */
template
<
    typename T_PRICE,       // "book" price type (likely 'price_si_t')
    typename T_OTK,         // order token type
    typename SOURCE_TRAITS, // defines source-specific 'oid_type', 'qty_type', TODO the book can use int32_t for qty, narrower than the wire field
    typename ... ATTRIBUTEs // user_data<...>, ... TODO
>
class execution_order_book final: public ex::impl::make_execution_order_book<T_PRICE, T_OTK, SOURCE_TRAITS, ATTRIBUTEs ...>::type
{
    private: // ..............................................................

        using super         = typename ex::impl::make_execution_order_book<T_PRICE, T_OTK, SOURCE_TRAITS, ATTRIBUTEs ...>::type;

    public: // ...............................................................

        template<typename POOL_ARENA>
        execution_order_book (POOL_ARENA & arena) :
            super (arena)
        {
        }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
