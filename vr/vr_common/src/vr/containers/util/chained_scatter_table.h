#pragma once

#include "vr/fields.h"
#include "vr/tags.h"
#include "vr/meta/integer.h"
#include "vr/util/hashing.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"
#include "vr/util/ops_int.h"

#include <boost/integer/static_min_max.hpp>
#include <boost/iterator/iterator_facade.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl_cst
{
namespace m = meta;


//............................................................................

using shortest_link_type        = int16_t;

template<typename K, typename V, typename SIZE_TYPE>
struct select_default_link_type
{
    using type_0        = m::make_compact_struct_t<m::make_schema_t
                            <
                                m::fdef_<shortest_link_type,    _next_>,
                                m::fdef_<K,                     _key_>,
                                m::fdef_<V,                     _value_>
                            > >;

    static constexpr std::size_t unused_bytes ()    { return (sizeof (type_0) - (sizeof (shortest_link_type) + sizeof (K) + sizeof (V))); }
    static constexpr std::size_t link_width ()      { return boost::static_unsigned_min<sizeof (SIZE_TYPE), (sizeof (shortest_link_type) + unused_bytes ())>::value; }

    // for now, assuming power-of-2 alignment etc:

    using type          = typename signed_type_of_size<link_width ()>::type;

}; // end of metafunction
//............................................................................

template<typename LINK_TYPE>
struct link_traits
{
    using fast_type         = typename util::signed_type_of_size<boost::static_unsigned_max<4, sizeof (LINK_TYPE)>::value>::type;

    vr_static_assert (std::is_signed<LINK_TYPE>::value);

}; // end of metafunction
//............................................................................

VR_META_TAG (PAD);

template<typename LINK_TYPE, int32_t NEXT_OFFSET, bool VARIANT = (NEXT_OFFSET >= sizeof (LINK_TYPE))>
struct make_free_entry; // master

template<typename LINK_TYPE, int32_t NEXT_OFFSET>
struct make_free_entry<LINK_TYPE, NEXT_OFFSET, false>
{
    static constexpr int32_t pad    = NEXT_OFFSET;

    using type          = m::make_packed_struct_t<m::make_schema_t // place '_prev_' after '_next_'
                        <
                            m::fdef_<m::padding<pad>,   _PAD_,   m::elide<(! pad)>>,
                            m::fdef_<LINK_TYPE,         _next_>,
                            m::fdef_<LINK_TYPE,         _prev_>
                        >>;

}; // end of specialization

template<typename LINK_TYPE, int32_t NEXT_OFFSET>
struct make_free_entry<LINK_TYPE, NEXT_OFFSET, true>
{
    static constexpr int32_t pad    = (NEXT_OFFSET - sizeof (LINK_TYPE));

    using type          = m::make_packed_struct_t<m::make_schema_t // place '_prev_' before '_next_'
                        <
                            m::fdef_<LINK_TYPE,         _prev_>,
                            m::fdef_<m::padding<pad>,   _PAD_,  m::elide<(! pad)>>,
                            m::fdef_<LINK_TYPE,         _next_>
                        >>;

}; // end of specialization
//............................................................................

template<typename K, typename V, typename SIZE_TYPE, typename /* 'void' means "auto-select" */LINK_TYPE>
struct entry_traits
{
    using size_type     = SIZE_TYPE;
    using link_type     = util::if_t
                            <
                                util::is_void<LINK_TYPE>::value,
                                typename select_default_link_type<K, V, SIZE_TYPE>::type,
                                LINK_TYPE
                            >;

    vr_static_assert (std::is_signed<link_type>::value);

    using used_entry    = m::make_compact_struct_t<m::make_schema_t
                            <
                                m::fdef_<link_type,     _next_>,
                                m::fdef_<K,             _key_>,
                                m::fdef_<V,             _value_>
                            > >;

    // [note: sign bit is not used by '_prev_']

    vr_static_assert (is_trivially_constructible<used_entry>::value && is_trivially_copy_assignable<used_entry>::value);

    using free_entry    = typename make_free_entry<link_type, m::field_offset<_next_, used_entry> ()>::type;

    vr_static_assert (m::field_offset<_next_, free_entry> () == m::field_offset<_next_, used_entry> ()); // guaranteed by 'make_free_entry'

    using entry_slot    = union
                            {
                                used_entry used;    //         _next_, _key_, _value_
                                free_entry free;    // _prev_, _next_
                            };

    vr_static_assert (sizeof (entry_slot) == sizeof (used_entry)); // TODO this will not always be true

}; // end of metafunction
//............................................................................

template<typename V, typename = void>
struct return_traits
{
    using result_const_type = V const *;
    using result_type       = V *;

    static VR_FORCEINLINE result_const_type return_const (V const & v)
    {
        return (& v);
    }

    static VR_FORCEINLINE result_type return_ (V & v)
    {
        return (& v);
    }

}; // end of master

template<typename V>
struct return_traits<V,
    util::enable_if_t<std::is_pointer<V>::value> >
{
    using result_const_type = V;
    using result_type       = V;

    static VR_FORCEINLINE result_const_type return_const (V const & v)
    {
        return v;
    }

    static VR_FORCEINLINE result_type return_ (V const & v)
    {
        return v;
    }

}; // end of master
//............................................................................

template<typename V, bool GET = false> // TODO support a return policy of "invalid value"
struct remove_result_traits
{
    using result_type   = bool;

    static constexpr result_type miss ()
    {
        return false;
    }

    static constexpr result_type hit (typename call_traits<V>::param)
    {
        return true;
    }

}; // end of master

template<typename V>
struct remove_result_traits<V, true>
{
    using result_type   = std::pair<V, bool>; // note: this must return 'V' by value, hence not using 'return_traits'

    static constexpr result_type miss ()
    {
        return { { }, false };
    }

    static VR_FORCEINLINE result_type hit (typename call_traits<V>::param v)
    {
        return { v, true };
    }

}; // end of specialization

} // end of 'impl_cst'
//............................................................................
//............................................................................

// TODO option for power-of-2 sizing vs Lemire fast mod
// TODO option for circular table vs linear + extra space at the end

struct default_chained_scatter_table_traits: virtual container::hash_is_fast<true>,
                                             virtual container::fixed_capacity<false>
{ };

struct fixed_chained_scatter_table_traits:   virtual container::hash_is_fast<true>,
                                             virtual container::fixed_capacity<true>
{ };


template // TODO add more and rework into an easier a variadic interface
<
    typename TRAITS         = default_chained_scatter_table_traits, // default is a rehashable table
    typename SIZE_TYPE      = int32_t,
    typename LINK_TYPE      = /* default means "auto-select" */void
>
struct chained_scatter_table_options
{
    using traits            = TRAITS;
    using size_type         = SIZE_TYPE;
    using link_type         = LINK_TYPE;

}; // end of class
//............................................................................
/**
 * chained scatter without coalescing
 *
 * retains good lookup/insert/delete performance at 100% load factor
 */
template<typename K, typename V, typename HASH = util::hash<K>, typename OPTIONS = chained_scatter_table_options<> >
class chained_scatter_table final: noncopyable
{
    private: // ..............................................................

        vr_static_assert (is_trivially_constructible<K>::value && is_trivially_copy_assignable<K>::value);
        vr_static_assert (is_trivially_constructible<V>::value && is_trivially_copy_assignable<V>::value);
        vr_static_assert (std::is_default_constructible<HASH>::value);

        using this_type     = chained_scatter_table<K, V, HASH, OPTIONS>;

        using entry_traits  = impl_cst::entry_traits<K, V, typename OPTIONS::size_type, typename OPTIONS::link_type>;

        using used_entry    = typename entry_traits::used_entry;    //         _next_, _key_, _value_
        using free_entry    = typename entry_traits::free_entry;    // _prev_, _next_

        using entry_slot    = typename entry_traits::entry_slot;    // allocation entry/bucket

        using return_traits             = impl_cst::return_traits<V>;

        using value_return_const_type   = typename return_traits::result_const_type;
        using value_return_type         = typename return_traits::result_type;

        template<bool CONST>
        class iterator_impl; // forward

    public: // ...............................................................

        using link_type     = typename entry_traits::link_type;
        using size_type     = typename entry_traits::size_type;

        using key_type      = K;
        using value_type    = V;
        using mapped_type   = used_entry; // note: can't make this 'std::pair'-like (yet)
        using hasher        = HASH;

        using options       = OPTIONS;

        using const_iterator    = iterator_impl<true>;
        using iterator          = iterator_impl<false>;

        static constexpr size_type max_capacity ()  { return (1 << meta::static_log2_floor<(std::numeric_limits<link_type>::max () - 1)>::value); } // note: 'link_type' asserted to be signed above
        static constexpr bool is_fixed_capacity ()  { return (! std::is_base_of<container::fixed_capacity<false>, typename options::traits>::value); }


        /**
         * @param initial_capacity [will be rounded up to a power of 2]
         */
        chained_scatter_table (size_type const initial_capacity) :
            m_capacity_mask { (1 << ops_checked::log2_ceil (initial_capacity)) - 1 },
            m_buckets { allocate_capacity (m_capacity_mask + 1) } // throws if capacity is too large for 'link_type'
        {
        }

        // ACCESSORs:

        /**
         *
         * @return [invalidated by subsequent mutating operations]
         */
        VR_ASSUME_HOT value_return_const_type get (typename call_traits<K>::param key) const;

        size_type const & size () const
        {
            return m_size;
        }

        size_type capacity () const
        {
            return (m_capacity_mask + 1);
        }

        bool empty () const
        {
            return (! size ());
        }

        // iteration:

        const_iterator begin () const;
        const_iterator end () const;

        // MUTATORs:

        /*
         * allows in-place value modification
         */
        VR_FORCEINLINE value_return_type get (typename call_traits<K>::param key)
        {
            return const_cast<value_return_type> (const_cast<this_type const *> (this)->get (key));
        }

        /*
         * will update 'key' entry in-place if present
         */
        VR_ASSUME_HOT value_return_type put (typename call_traits<K>::param key, typename call_traits<V>::param value);

        VR_ASSUME_HOT bool remove (typename call_traits<K>::param key);
        VR_ASSUME_HOT std::pair<V, bool> remove_and_get (typename call_traits<K>::param key);

        // iteration:

        iterator begin ();
        iterator end ();

        /**
         *
         * @param count new target [>= size(), will be rounded up to a power of 2]
         * @return new capacity
         */
        template<bool _ = (! is_fixed_capacity ())>
        auto rehash (size_type const count) -> util::enable_if_t<_, size_type>
        {
            size_type const target_capacity = (1 << ops_checked::log2_ceil (count));
            if (target_capacity != capacity ())
            {
                check_ge (target_capacity, size ());
                rehash_impl (target_capacity);
            }
            return capacity ();
        }

        // debug assists:

        VR_ASSUME_COLD void check () const; // note: defined in a separate file

    private: // ..............................................................

        vr_static_assert (sizeof (link_type) <= 4); // long pointers should never get chosen

        using fast_link_type    = typename impl_cst::link_traits<link_type>::fast_type;

        using usize_type        = typename std::make_unsigned<size_type>::type;

        using ops_checked       = ops_int<arg_policy<zero_arg_policy::ignore, 0>, true>;
        using ops_unchecked     = ops_int<arg_policy<zero_arg_policy::ignore, 0>, false>;

        /*
         * this is a pretty dumb implementation: scan through the bucket array while
         * counting up to 'm_size' and skipping empty slots
         *
         * TODO 'const_iterator' should const-ify '_key_'
         */
        template<bool CONST>
        struct iterator_impl final: public boost::iterator_facade<iterator_impl<CONST>, util::add_const_if_t<CONST, used_entry>, boost::forward_traversal_tag>
        {
            friend class boost::iterator_core_access;

            using value_type    = util::add_const_if_t<CONST, used_entry>;

            iterator_impl (this_type const & parent, size_type const count) VR_NOEXCEPT :
                m_parent { parent },
                m_slot { & m_parent.m_buckets [0] }, // just before possible 'begin'
                m_count { count }
            {
                increment (); // find actual 'begin'
            }

            VR_FORCEINLINE bool is_end () const
            {
                return (m_count > m_parent.m_size); // note: inequality important for correctness
            }

            // iterator_facade:

            void increment ()
            {
                ++ m_count;

                while (! is_end ())
                {
                    ++ m_slot;

                    if (field<_next_> (m_slot->free) >= 0)
                        break;
                }
            }

            value_type & dereference () const
            {
                assert_condition (! is_end ());
                assert_nonnull (m_slot);

                return m_slot->used;
            }

            bool equal (iterator_impl const & rhs) const
            {
                if (is_end ())
                    return rhs.is_end ();
                else
                    return (m_count == rhs.m_count);
            }

            this_type const & m_parent;
            entry_slot * m_slot;
            size_type m_count; // of non-empty entries iterated over (including 'm_entry')

        }; // end of nested class


        VR_NOINLINE void rehash_impl (size_type const new_capacity);

        template<bool GET>
        VR_FORCEINLINE typename impl_cst::remove_result_traits<V, GET>::result_type
        remove_impl (typename call_traits<K>::param key);


        /*
         * note: for a resizable table, the caller ensures that this method is only invoked
         *       if it's *guaranteed* to succeed
         */
        VR_ASSUME_HOT entry_slot * acquire_free_slot (entry_slot * VR_RESTRICT const buckets)
        {
            // remove the first node from the free list ('next' after the sentinel):

            entry_slot & sentinel = buckets [0];

            fast_link_type const first_free = - field<_next_> (sentinel.free) - 1; // anchored at the sentinel
            assert_nonnegative (first_free);

            if (is_fixed_capacity ()) // compile-time branch
            {
                if (VR_UNLIKELY (first_free == 0)) // table full
                    throw_x (capacity_limit, "cannot grow a fixed-capacity (" + string_cast (capacity ()) + ") table");
            }
            else
            {
                assert_nonzero (first_free); // assertion stated in the method doc
            }

            entry_slot * const ff = & buckets [first_free];

            DLOG_trace2 << "    acquired empty slot #" << (ff - buckets);

            // unlink 'e' from the free list:

            fast_link_type const ff_next = field<_next_> (ff->free);
            assert_positive (- ff_next);

            field<_prev_> (buckets [- ff_next - 1].free) = { };
            field<_next_> (sentinel.free) = ff_next;

            return ff;
        }

        VR_ASSUME_HOT void free_slot (entry_slot * const e, entry_slot * VR_RESTRICT const buckets)
        {
            fast_link_type const e_addr = (e - buckets); // TODO this is known by the caller, could be passed as parm
            assert_positive (e_addr); // points past the sentinel

            // make 'e' the first node after the sentinel:

            entry_slot & sentinel = buckets [0];

            fast_link_type const first_free = - field<_next_> (sentinel.free) - 1; // anchored at the sentinel
            assert_nonnegative (first_free);

            entry_slot * const ff = & buckets [first_free];

            field<_prev_> (ff->free) = e_addr;
            field<_next_> (e->free) = - first_free - 1; // == field<_next_> (sentinel)
            field<_prev_> (e->free) = { };
            field<_next_> (sentinel.free) = - e_addr - 1;
        }

        static std::unique_ptr<entry_slot []>  allocate_capacity (size_type const capacity)
        {
            /*
             * layout and short pointer indexing:
             *
             * free list sentinel:  buckets [0]
             * entry slots:         buckets [1, capacity]
             *
             * entry occupied by a valid key/value entry (does not have _prev_):
             *
             * _next_ > 0, index into 'buckets'
             * _next_ == 0 is "null ptr" (end of a chain)
             *
             * entry on a free list (including the sentinel):
             *
             * _next_ < 0, is -(index + 1)
             * _prev_ >= 0, index into 'buckets' (zero only if points to the sentinel)
             *
             * the range of index values used in _next_, _prev_ fields: [- capacity - 1, capacity]
             */
            DLOG_trace1 << "allocating capacity " << capacity << ", link type {" << util::class_name<link_type> () << "}"
                        << "; sizeof (entry_type) = " << sizeof (entry_slot) << ";"
                           " offsets: _next_ " << meta::field_offset<_next_, used_entry> ()
                        << ", _prev_ " << meta::field_offset<_prev_, free_entry> ()
                        << ", _key_ " << meta::field_offset<_key_, used_entry> ()
                        << ", _value_ " << meta::field_offset<_value_, used_entry> ();

            assert_is_power_of_2 (capacity);

            if (VR_UNLIKELY (static_cast<std::size_t> (capacity) > max_capacity ()))
                throw_x (invalid_input, "capacity " + string_cast (capacity) + " is larger than the max supported " + string_cast (max_capacity ()) + " for link type {" + util::class_name<link_type> () + '}');

            std::unique_ptr<entry_slot []> r { boost::make_unique_noinit<entry_slot []> (capacity + /* sentinel */1) };
            entry_slot * VR_RESTRICT const buckets = r.get ();

            // place all buckets on the circular free list:

            for (size_type index = 0; index <= capacity; ++ index)
            {
                field<_next_> (buckets [index].free) = - index - 2; // last slot's value is overwritten below
                field<_prev_> (buckets [index].free) = index - 1; // sentinel's slot value is overwritten below
            }

            // complete sentinel links:

            field<_next_> (buckets [capacity].free) = -1; // last slot points to the sentinel
            field<_prev_> (buckets [0].free) = capacity;  // sentinel points to the last slot

            return r;
        }


        size_type m_size { };
        size_type m_capacity_mask; // capacity - 1
        std::unique_ptr<entry_slot []> m_buckets;

}; // end of class
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
VR_ASSUME_HOT typename chained_scatter_table<K, V, HASH, OPTIONS>::value_return_const_type
chained_scatter_table<K, V, HASH, OPTIONS>::get (typename call_traits<K>::param key) const // TODO support 'V' = void (return 'mapped_type' ref?)
{
    DLOG_trace1 << "get(" << print (key) << "): entry [size: " << m_size << ']';

    entry_slot const * VR_RESTRICT const buckets = m_buckets.get ();

    usize_type const key_haddr { (static_cast<usize_type> (HASH { } (key)) & m_capacity_mask) +/* sentinel */1 };

    // three possible cases for the first slot ('key' hash address):
    //
    //  1. it is an empty slot (on the free list);
    //  2. [start of home collision chain] it is occupied by a key with the same hash address 'key_haddr', and
    //    (a) it contains 'key', or
    //    (b) it contains another key;
    //  3. it is occupied by a key with a different hash address.

    // in both (1) and (3) we could terminate early (if hash eval is cheap); it is not clear a priori whether
    // this would be a worthwhile optimization in case 3; note that it speeds up unsuccessful searches
    // while adding some cost to the successful ones (one extra hash eval)
    //
    // [see also comments in put() for another idea]

    entry_slot const * e = & buckets [key_haddr];
    fast_link_type     e_next = field<_next_> (e->free);

    // note that if 'e' is the starting slot, it can still be an empty one

    DLOG_trace2 << "  probing slot #" << (e - buckets) << " (empty: " << (e_next < 0) << ')';

    if (e_next < 0)
    {
        DLOG_trace1 << "get(" << print (key) << "): exit (miss)";
        return nullptr;
    }

    // [assertion: from this point on, we will only traverse non-empty slots]

    while (true)
    {
        assert_nonnegative (e_next);

        // TODO speed up case (3) in 'hash_is_fast<true>' configuration (if the first non-empty entry
        // we encounter has a different hash address, we can terminate early)

        if (field<_key_> (e->used) == key)
        {
            DLOG_trace1 << "get(" << print (key) << "): exit (hit)";
            return return_traits::return_const (field<_value_> (e->used));
        }

        if (! e_next)
        {
            DLOG_trace1 << "get(" << print (key) << "): exit (miss)";
            return nullptr;
        }

        e = & buckets [e_next];
        e_next = field<_next_> (e->used);

        DLOG_trace2 << "  probing non-empty slot #" << (e - buckets);
    }

    VR_ASSUME_UNREACHABLE ();
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
typename chained_scatter_table<K, V, HASH, OPTIONS>::const_iterator
chained_scatter_table<K, V, HASH, OPTIONS>::begin () const
{
    return { * this, 0 };
}

template<typename K, typename V, typename HASH, typename OPTIONS>
typename chained_scatter_table<K, V, HASH, OPTIONS>::const_iterator
chained_scatter_table<K, V, HASH, OPTIONS>::end () const
{
    return { * this, m_size };
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
typename chained_scatter_table<K, V, HASH, OPTIONS>::iterator
chained_scatter_table<K, V, HASH, OPTIONS>::begin ()
{
    return { * this, 0 };
}

template<typename K, typename V, typename HASH, typename OPTIONS>
typename chained_scatter_table<K, V, HASH, OPTIONS>::iterator
chained_scatter_table<K, V, HASH, OPTIONS>::end ()
{
    return { * this, m_size };
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
VR_ASSUME_HOT typename chained_scatter_table<K, V, HASH, OPTIONS>::value_return_type
chained_scatter_table<K, V, HASH, OPTIONS>::put (typename call_traits<K>::param key, typename call_traits<V>::param value)
{
    DLOG_trace1 << "put(" << print (key) << ", " << print (value) << "): entry [size: " << m_size << ']';

retry:

    entry_slot * VR_RESTRICT const buckets = m_buckets.get ();

    usize_type const key_haddr { (static_cast<usize_type> (HASH { } (key)) & m_capacity_mask) +/* sentinel */1 };
    DLOG_trace2 << "  key hash addr: " << key_haddr;

    entry_slot    * e = & buckets [key_haddr];
    fast_link_type  e_next = field<_next_> (e->free);


    if (e_next < 0) // case 1: 'e' is an empty slot
    {
        DLOG_trace2 << "  (case 1) populating empty slot #" << (e - buckets);

        // unlink 'e' from the free list:
        {
            fast_link_type const e_prev = field<_prev_> (e->free);
            assert_nonnegative (e_prev);

            field<_next_> (buckets [e_prev].free) = e_next;
            field<_prev_> (buckets [- e_next - 1].free) = e_prev;
        }

        // set 'e' fields ('next' is set to null because 'e' is the only node in its home collision chain):

fill_e: {
            field<_next_> (e->used) = { }; // null ptr

            field<_key_> (e->used) = key;
            V & v = field<_value_> (e->used);
            v = value;

            ++ m_size;

            DLOG_trace1 << "put(" << print (key) << ", " << print (value) << "): exit [size: " << m_size << ']';
            return return_traits::return_ (v);
        }
    }
    else // 'e' contains a valid key/value
    {
        // TODO if (! hash_is_fast<true>), find ways to avoid getting 'e_haddr' unconditionally in all cases here

        K const & e_key = field<_key_> (e->used);
        usize_type const e_haddr { (static_cast<usize_type> (HASH { } (e_key)) & m_capacity_mask) +/* sentinel */1 };

        if (e_haddr == key_haddr) // case 2: this is the home collision chain for 'key'
        {
            DLOG_trace2 << "  (case 2) slot #" << (e - buckets) << " contains <" << print (e_key) << ':' << print (field<_value_> (e->used)) << '>';

            // because 'key' may already be present in the chain, we have to visit all of it in any case;
            // to share more code paths with other cases, if 'key' is not present and needs to go into
            // a new slot, we will insert it as the new last chain node:

            // TODO the order of keys within a chain is still a degree of freedom in this impl; the chain could
            // be kept ordered by key, for example, to enable faster searches -- however, this would require 'K'
            // to be a totally ordered type

            entry_slot * ep = e;
            fast_link_type ep_next = e_next; // redundant var (can re-use 'e_next') for code clarity

            while (true)
            {
                DLOG_trace2 << "    checking key of slot #" << (ep - buckets) << ": <" << print (field<_key_> (ep->used)) << '>';
                assert_nonnegative (ep_next); // can't run into a free list slot within the chain

                if (field<_key_> (ep->used) == key) // 'key' found, update 'value' in place:
                {
                    DLOG_trace2 << "    updating existing entry to value " << print (value);

                    V & v = field<_value_> (ep->used);
                    v = value;

                    DLOG_trace1 << "put(" << print (key) << ", " << print (value) << "): entry [size: " << m_size << ']';
                    return return_traits::return_ (v); // no 'm_size' change
                }
                else if (! ep_next) // end of the chain, 'ep' is the last node
                {
                    // allocate a new empty slot and make it the last slot in the chain:

                    if (! is_fixed_capacity ()) // compile-time branch
                    {
                        // guarantee that a free slot will be found by rehashing and retrying if necessary:

                        if (VR_UNLIKELY (m_size == m_capacity_mask + 1))
                            goto rehash_and_retry;
                    }
                    e = acquire_free_slot (buckets);

                    field<_next_> (ep->used) = (e - buckets); // note: can also return this from 'acquire_free_slot ()'

                    goto fill_e;
                }

                ep = & buckets [ep_next];
                ep_next = field<_next_> (ep->used);
            }
        }
        else // case 3: 'e_key' is from a different home collision chain
        {
            DLOG_trace2 << "  (case 3) slot #" << (e - buckets) << " contains <" << print (e_key) << ':' << print (field<_value_> (e->used)) << "> with hash addr " << e_haddr;

            // displace the content of 'e' into a new empty slot (this choice is what makes the impl "without coalescing"):

            if (! is_fixed_capacity ()) // compile-time branch
            {
                // guarantee that a free slot will be found by rehashing and retrying if necessary:

                if (VR_UNLIKELY (m_size == m_capacity_mask + 1))
                    goto rehash_and_retry;
            }
            entry_slot * const e_displaced = acquire_free_slot (buckets);

            DLOG_trace2 << "  (case 3) displacing #" << (e - buckets) << " -> #" << (e_displaced - buckets);

            __builtin_memcpy (e_displaced, e, sizeof (entry_slot)); // no 'next' adjustment needed for 'e_displaced'

            // locate 'e's predecessor to update it's 'next' (a single chain starting at 'e_haddr' needs to be
            // traversed as a result of "no coalescing"; the only way to avoid this loop is to have 'prev'
            // links in the chained slots, i.e. use doubly linked collision chains):
            {
                entry_slot * ep = & buckets [e_haddr];
                fast_link_type ep_next = field<_next_> (ep->used);

                DLOG_trace2 << "    visiting predecessor #" << (ep - buckets);

                while (ep_next != static_cast<fast_link_type> (key_haddr))
                {
                    assert_positive (ep_next); // can't run into a free list slot or end-of-list

                    ep = & buckets [ep_next];
                    ep_next = field<_next_> (ep->used);

                    DLOG_trace2 << "    visiting predecessor #" << (ep - buckets);
                }
                assert_eq (field<_next_> (ep->used), static_cast<fast_link_type> (key_haddr)); // 'e_p' is 'e's predecessor

                field<_next_> (ep->used) = (e_displaced - buckets); // note: can also return this from 'acquire_free_slot ()'
            }

            // populate 'e' fields with incoming data ('e' becomes the only node in its home collision chain):

            goto fill_e;
        }
    }

rehash_and_retry:

    if (! is_fixed_capacity ()) // help compiler not emit any code in fixed capacity specialization
    {
        rehash_impl ((m_capacity_mask + 1) << 1);
        goto retry;
    }

    VR_ASSUME_UNREACHABLE ();
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
template<bool GET>
VR_FORCEINLINE typename impl_cst::remove_result_traits<V, GET>::result_type
chained_scatter_table<K, V, HASH, OPTIONS>::remove_impl (typename call_traits<K>::param key)
{
    DLOG_trace1 << "remove(" << print (key) << "): entry [size: " << m_size << ']';

    using result_traits     = impl_cst::remove_result_traits<V, GET>;

    // here the design choice not to coalesce pays off: removing a key is a simple node deletion
    // from a singly linked list with anchored head (a non-coalesced home collision chain)

    entry_slot * VR_RESTRICT const buckets = m_buckets.get ();

    usize_type const key_haddr { (static_cast<usize_type> (HASH { } (key)) & m_capacity_mask) +/* sentinel */1 };
    DLOG_trace2 << "  key hash addr: " << key_haddr;

    entry_slot    * e = & buckets [key_haddr];
    fast_link_type  e_next = field<_next_> (e->free);

    if (e_next < 0) // 'e' is empty
    {
        DLOG_trace1 << "remove(" << print (key) << "): exit [size: " << m_size << "] (miss)";
        return result_traits::miss ();
    }

    entry_slot    * ep = e; // predecessor of 'e'

    while (true) // occupied slots only (including the very first one)
    {
        // TODO leverage 'hash_is_fast<true>' (it is possible we're traversing a chain for a different hash address
        // so we could terminate early as soon as we detect that from the first entry)

        if (field<_key_> (e->used) == key) // 'key' found
        {
            auto const rc = result_traits::hit (field<_value_> (e->used)); // capture the value for possible return

            if (e_next) // 'e' has a successor
            {
                // displace the successor into 'e' slot:

                // TODO displacement can sometimes be avoided and perhaps the choice to leverage that
                // could be a trait option for when it might be expensive (large 'K'/'V')

                entry_slot * const e_succ = & buckets [e_next];

                DLOG_trace2 << "  displacing #" << (e_succ - buckets) << " -> #" << (e - buckets);

                __builtin_memcpy (e, e_succ, sizeof (entry_slot)); // no 'next' adjustment needed for 'e_succ'

                // free 'e_succ' (done by the common part of the 'true' return path below):

                e = e_succ;
            }
            else // 'e' has no successor
            {
                // if 'e' has a predecessor, need to null it's 'next' link:

                if (e != ep) field<_next_> (ep->used) = { };

                // free 'e':
            }

            free_slot (e, buckets);

            assert_positive (m_size);
            -- m_size;

            DLOG_trace1 << "remove(" << print (key) << "): exit [size: " << m_size << ']';
            return rc;
        }
        else if (! e_next) // end of the chain, 'key' not found
        {
            DLOG_trace1 << "remove(" << print (key) << "): exit [size: " << m_size << "] (miss)";
            return result_traits::miss ();
        }

        ep = e;
        e = & buckets [e_next];
        e_next = field<_next_> (e->used);
    }

    VR_ASSUME_UNREACHABLE ();
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
bool
chained_scatter_table<K, V, HASH, OPTIONS>::remove (typename call_traits<K>::param key)
{
    return remove_impl<false> (key);
}

template<typename K, typename V, typename HASH, typename OPTIONS>
std::pair<V, bool>
chained_scatter_table<K, V, HASH, OPTIONS>::remove_and_get (typename call_traits<K>::param key)
{
    return remove_impl<true> (key);
}
//............................................................................

template<typename K, typename V, typename HASH, typename OPTIONS>
void
chained_scatter_table<K, V, HASH, OPTIONS>::rehash_impl (size_type const new_capacity)
{
    assert_is_power_of_2 (new_capacity); // caller ensures
    assert_ge (new_capacity, m_size); // caller ensures
    assert_ne (new_capacity, m_capacity_mask + 1); // caller ensures (nothing to do otherwise)

    // trace so that rehashing events could be monitored at runtime:

    LOG_trace1 << "*** rehashing " << (m_capacity_mask + 1) << " -> " << new_capacity;

    // this is a pretty naive stop-the-world implementation, but it should be fine
    // for use cases where the table is pre-populated or well-sized at init time:

    std::unique_ptr<entry_slot []> buckets { allocate_capacity (new_capacity) }; // this inits the free list links but changes no instance state
    m_buckets.swap (buckets); // 'm_buckets' are now "new buckets"
    entry_slot * VR_RESTRICT const cur_buckets = buckets.get (); // these are "current buckets"

    size_type const i_limit = m_size;
    m_size = { };
    m_capacity_mask = new_capacity - 1;

    for (size_type e_addr =/* sentinel */1, i = 0; i < i_limit; ++ e_addr)
    {
        entry_slot const & e = cur_buckets [e_addr];
        if (field<_next_> (e.free) >= 0)
        {
            put (field<_key_> (e.used), field<_value_> (e.used));

            ++ i;
        }
    }
    assert_eq (m_size, i_limit);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
