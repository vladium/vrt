#pragma once

#include "vr/asserts.h"
#include "vr/meta/integer.h"
#include "vr/util/classes.h" // destruct()
#include "vr/util/logging.h"
#include "vr/util/memory.h"
#include "vr/sys/os.h"

#include <boost/integer/static_min_max.hpp>
#include <boost/math/common_factor_ct.hpp> // note: moved to boost.integer in 1.66+

#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/*
 * this is inspired by Alexandrescu's "small object allocators", except this impl
 * is also geared towards using "short pointers" and power-of-2-sized chunks
 *
 * (as a side effect, this allows easy O(1) impl of *both* allocation and deallocation)
 */
//............................................................................
//............................................................................
namespace impl
{
/*
 * compute storage size (which may end up being > 'SIZE') carefully to satisfy
 * alignment constraints
 */
template<std::size_t SIZE, std::size_t ALIGNMENT>
struct storage_traits
{
    vr_static_assert (SIZE > 0);
    vr_static_assert (ALIGNMENT > 0);

    static constexpr std::size_t storage_size ()    { return boost::math::static_lcm<SIZE, ALIGNMENT>::value; }
    static constexpr std::size_t alignment ()       { return ALIGNMENT; }

}; // end of metafunction
//............................................................................
/*
 * by default, try to fill the assumed OS page size (without exceeding it) but keep capacity to a power-of-2 in slots
 */
template<std::size_t STORAGE_SIZE, typename = void>
struct default_chunk_capacity; // master


template<std::size_t STORAGE_SIZE>
struct default_chunk_capacity<STORAGE_SIZE,
    typename std::enable_if<(STORAGE_SIZE <= sys::os_info::static_page_size ())>::type>
{
    static constexpr std::size_t min_1  = 9; // ASX-101
    static constexpr std::size_t min_2  = meta::static_log2_floor<(sys::os_info::static_page_size () / STORAGE_SIZE)>::value;

    static constexpr std::size_t value      = (1L << boost::static_unsigned_max<min_1, min_2>::value);

}; // end of specialization

template<std::size_t STORAGE_SIZE>
struct default_chunk_capacity<STORAGE_SIZE,
    typename std::enable_if<(STORAGE_SIZE > sys::os_info::static_page_size ())>::type>
{
    static constexpr std::size_t value      = 2; // [1 chunk == 2 slots] TODO unclear what to choose here, but this is not the target use case

}; // end of specialization
//............................................................................

template<typename T_POINTER>
struct widen_for_arithmetic // widen to at least 4 bytes
{
    using type          = typename util::unsigned_type_of_size<boost::static_unsigned_max<4, sizeof (T_POINTER)>::value>::type;

}; // end of metafunction
//............................................................................

class fixed_size_allocator_base
{
    protected: // ............................................................

        VR_ASSUME_COLD int64_t create (int64_t const initial_capacity, std::size_t const storage_size, std::size_t const alignment, std::size_t const chunk_capacity);
        VR_ASSUME_COLD void destruct (int64_t const chunk_count) VR_NOEXCEPT;


        addr_t allocate_chunk (int64_t const chunk_count, std::size_t const alignment, std::size_t const chunk_size);


        std::unique_ptr</* owning */addr_t []> m_chunks; // dynamically growable array of chunks (size managed by subclass)

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<std::size_t SIZE, std::size_t ALIGNMENT>
struct min_aligned_storage_size
{
    static constexpr std::size_t value  = impl::storage_traits<SIZE, ALIGNMENT>::storage_size ();

}; // end of metafunction
//............................................................................

template
<
    std::size_t STORAGE_SIZE,       // [bytes]
    typename T_POINTER              = int32_t,
    std::size_t CHUNK_CAPACITY      = impl::default_chunk_capacity<STORAGE_SIZE>::value // [items]
>
struct allocator_options
{
    vr_static_assert (STORAGE_SIZE > 0);
    vr_static_assert (vr_is_power_of_2 (CHUNK_CAPACITY));

    static constexpr std::size_t storage_size ()    { return STORAGE_SIZE; }
    using pointer_type      = T_POINTER;
    static constexpr std::size_t chunk_capacity ()      { return CHUNK_CAPACITY; }

}; // end of traits
//............................................................................
/**
 * an allocator/pool of raw data slots with guaranteed size and alignment
 * and O(1) allocate/release implementations.
 *
 * released slots are kept in a singly-linked free list so that a {release, allocate}
 * sequence is guaranteed to recycle the same slot (and hence cache lines, etc).
 *
 * aside from minimal overhead of managing chunks of slots, there is no per-slot memory overhead.
 */
template<std::size_t SIZE, std::size_t ALIGNMENT, typename OPTIONS = allocator_options<min_aligned_storage_size<SIZE, ALIGNMENT>::value> >
class fixed_size_allocator: public impl::fixed_size_allocator_base
{
    public: // ...............................................................

        using pointer_type      = typename OPTIONS::pointer_type;
        using fast_pointer_type = typename impl::widen_for_arithmetic<pointer_type>::type;

    private: // ..............................................................

        using super         = impl::fixed_size_allocator_base;

        vr_static_assert (OPTIONS::storage_size () >= sizeof (pointer_type)); // must be able to fit a "pointer" inside a free slot

        vr_static_assert (vr_is_power_of_2 (OPTIONS::chunk_capacity ()));
        vr_static_assert (OPTIONS::chunk_capacity () > 1); // otherwise some extra edge case handling is needed in 'allocate_first_slot_of_new_chunk()' for large objects

        vr_static_assert (std::is_unsigned<fast_pointer_type>::value); // ensure bit shifting and widening are safe

        static constexpr fast_pointer_type chunk_index_shift    = meta::static_log2_floor<OPTIONS::chunk_capacity ()>::value;
        static constexpr fast_pointer_type chunk_offset_mask    = (OPTIONS::chunk_capacity () - 1);

    public: // ...............................................................

        using options       = OPTIONS;

        using size_type         = fast_pointer_type;
        using alloc_result      = std::pair<addr_t, pointer_type>;

        static constexpr std::size_t max_capacity ()            { return std::numeric_limits<pointer_type>::max (); }

        /**
         * @param initial_capacity [this is a lower bound, the impl will increase as needed to satisfy design invariants]
         */
        VR_ASSUME_COLD fixed_size_allocator (size_type const initial_capacity = options::chunk_capacity ()) :
            m_chunk_count (super::create (initial_capacity, options::storage_size (), ALIGNMENT, options::chunk_capacity ()))
        {
            check_positive (initial_capacity);
            check_nonnull (m_chunks);

            // check that initial allocation would not overflow 'pointer_type':

            check_le (static_cast<std::size_t> (m_chunk_count) * options::chunk_capacity (), max_capacity (), initial_capacity, cn_<pointer_type> ());

            // initialize the free list spanning all created chunks:

            fast_pointer_type ref = 0;
            for (fast_pointer_type chunk_index = 0; chunk_index < m_chunk_count; ++ chunk_index)
            {
                addr_t const chunk = m_chunks [chunk_index];
                assert_nonnull (chunk);

                for (fast_pointer_type i = 0; i < options::chunk_capacity (); ++ i)
                {
                    * static_cast<pointer_type *> (addr_plus (chunk, i * options::storage_size ())) = ++ ref;
                }
            }

            * static_cast<pointer_type *> (addr_plus (m_chunks [m_chunk_count - 1], (options::chunk_capacity () - 1) * options::storage_size ())) = null_ref ();
            m_free_list = 0;
        }

        VR_ASSUME_COLD ~fixed_size_allocator ()
        {
            super::destruct (m_chunk_count);
        }

        size_type capacity () const // [storage slot units]
        {
            return (m_chunk_count * options::chunk_capacity ());
        }

        // ACCESSORs:

        template<bool CHECK_BOUNDS  = VR_CHECK_INPUT>
        addr_const_t dereference (fast_pointer_type const ref) const
        {
            fast_pointer_type const chunk_index = (static_cast<fast_pointer_type> (ref) >> chunk_index_shift);
            if (CHECK_BOUNDS) check_within (chunk_index, m_chunk_count);

            assert_nonnull (m_chunks [chunk_index]);

            return addr_plus (m_chunks [chunk_index], (static_cast<fast_pointer_type> (ref) & chunk_offset_mask) * options::storage_size ());
        }

        addr_const_t operator[] (fast_pointer_type const ref) const
        {
            return dereference<false> (ref);
        }



        // MUTATORs:

        template<bool CHECK_BOUNDS  = VR_CHECK_INPUT>
        addr_t dereference (fast_pointer_type const ref)
        {
            return const_cast<addr_t> (const_cast<fixed_size_allocator const *> (this)->dereference (ref));
        }

        addr_t operator[] (fast_pointer_type const ref)
        {
            return dereference<false> (ref);
        }


        alloc_result allocate ()
        {
            fast_pointer_type const first_ref = m_free_list;

            if (VR_UNLIKELY (first_ref == null_ref ()))
            {
                return allocate_first_slot_of_new_chunk (); // extend 'm_chunks' with a new chunk and allocate it's 0th index
            }

            addr_t const first = dereference<false> (first_ref);
            m_free_list = * static_cast<pointer_type *> (first);

            DLOG_trace2 << "[chunks: " << m_chunk_count << "] allocated slot #" << first_ref;
            return { first, first_ref };
        }

        void release (fast_pointer_type const ref)
        {
            DLOG_trace2 << "[chunks: " << m_chunk_count << "] releasing slot #" << ref;

            // TODO is there a way to guard against double-releases cheaply?

            addr_t const released = dereference (ref);

            fast_pointer_type first_ref = m_free_list;
            m_free_list = ref;

            * static_cast<pointer_type *> (released) = first_ref;
        }

        /*
         * implies 'release()', of course
         */
        template<typename T>
        VR_FORCEINLINE void destruct (fast_pointer_type const ref)
        {
            DLOG_trace2 << "[chunks: " << m_chunk_count << "] destructing slot #" << ref;

            // TODO is there a way to guard against double-releases cheaply?

            addr_t const released = dereference (ref);

            fast_pointer_type first_ref = m_free_list;
            m_free_list = ref;

            util::destruct (* static_cast<T *> (released)); // before we overwrite mem with 'first_ref' next

            * static_cast<pointer_type *> (released) = first_ref;
        }

        // debug assists:

        VR_ASSUME_COLD void check () const; // note: defined in a separate file

    protected: // ............................................................

        static constexpr fast_pointer_type null_ref ()  { return static_cast<fast_pointer_type> (std::numeric_limits<pointer_type>::max ()); }


        alloc_result allocate_first_slot_of_new_chunk ()
        {
            fast_pointer_type const chunk_count = m_chunk_count;

            // before actual allocation, guard against 'pointer_type' overflow:

            check_le ((static_cast<std::size_t> (chunk_count) + 1) * options::chunk_capacity (), max_capacity (), capacity (), cn_<pointer_type> ());

            addr_t const chunk = super::allocate_chunk (chunk_count, ALIGNMENT, options::storage_size () * options::chunk_capacity ());
            m_chunk_count = chunk_count + 1;

            // add 'chunk' slots to the free list:

            fast_pointer_type const first_ref = chunk_count * options::chunk_capacity (); // correct because all the original chunks are allocated

            // point 'm_free_list' to the slot after the one that's being returned:
            // [this is correct as long as chunk capacity > 1]

            fast_pointer_type ref = m_free_list = first_ref + 1;

            for (fast_pointer_type i =/* ! */1; i < options::chunk_capacity () -/* ! */1; ++ i)
            {
                * static_cast<pointer_type *> (addr_plus (chunk, i * options::storage_size ())) = ++ ref;
            }
            * static_cast<pointer_type *> (addr_plus (chunk, (options::chunk_capacity () - 1) * options::storage_size ())) = null_ref ();

            return { chunk, first_ref };
        }

        /*
         * used for
         *  1. proper cleanup by subclass' destructor;
         *  2. debug/validation
         */
        VR_ASSUME_COLD bit_set allocation_bitmap () const
        {
            bit_set r { make_bit_set (m_chunk_count * options::chunk_capacity ()) };
            r.set (); // set to "all allocated"

            for (fast_pointer_type ref = m_free_list; ref != null_ref (); ) // TODO this is O(free list size) in time and space
            {
                r.reset (ref);

                ref = * static_cast<pointer_type const *> (dereference<false> (ref));
            }

            return r;
        }


        fast_pointer_type m_chunk_count { }; // set during construction
        fast_pointer_type m_free_list { null_ref () }; // set to the free list head during construction

}; // end of class
//............................................................................

template
<
    typename T,
    typename T_POINTER,
    std::size_t CHUNK_CAPACITY      = impl::default_chunk_capacity<min_aligned_storage_size<sizeof (T), alignof (T)>::value>::value
>
struct pool_options
{
    using type          = allocator_options<min_aligned_storage_size<sizeof (T), alignof (T)>::value, T_POINTER, CHUNK_CAPACITY>;

}; // end of metafunction

template<typename T>
using default_pool_options_t        = typename pool_options<T, int32_t>::type;

//............................................................................
/**
 * design invariants:
 *
 *  - all objects in the "allocated" set have been constructed
 *  - all objects in the "released" set have been destructed
 *  - all slots that have never been used to seat an object are POD storage and don't require
 *    any special cleanup on pool destruction
 *
 * @note this implementation can separate allocated slots from free slots and hence
 *       destruct all allocated slots on pool destruction (client code does not have
 *       to explicitly release all "leftover" objects after it's done using a pool)
 */
template<typename T, typename OPTIONS = default_pool_options_t<T> >
class object_pool: protected fixed_size_allocator<sizeof (T), alignof (T), OPTIONS>
{
    private: // ..............................................................

        using super         = fixed_size_allocator<sizeof (T), alignof (T), OPTIONS>;

    public: // ...............................................................

        using typename super::options;
        using typename super::fast_pointer_type;

        using typename super::pointer_type;
        using typename super::size_type;

        using alloc_result      = std::tuple<T &, pointer_type>;

        /**
         * @param initial_capacity [this is a lower bound, the impl will increase as needed to satisfy design invariants]
         */
        VR_ASSUME_COLD object_pool (size_type const initial_capacity = options::chunk_capacity ()) :
            super (initial_capacity)
        {
            LOG_trace1 << util::instance_name (this) << ": created with initial capacity of " << capacity () << " object(s)";
        }

        VR_ASSUME_COLD ~object_pool ()
        {
            // 'super' will de-allocate memory; destruct all allocated slots before that:

            bit_set const abm { super::allocation_bitmap () };

            LOG_trace1 << util::instance_name (this) << ": cleaning up " << abm.count () << " object(s) on destruction";

            for (auto ref = abm.find_first (); ref != bit_set::npos; ref = abm.find_next (ref))
            {
                T & obj = dereference<false> (ref);
                util::destruct (obj);
            }
        }

        // ACCESSORs:

        template<bool CHECK_BOUNDS  = VR_CHECK_INPUT>
        T const & dereference (fast_pointer_type const ref) const
        {
            return (* static_cast<T const *> (super::template dereference<CHECK_BOUNDS> (ref)));
        }

        T const & operator[] (fast_pointer_type const ref) const
        {
            return dereference<false> (ref);
        }

        using super::capacity;

        // MUTATORs:

        template<bool CHECK_BOUNDS  = VR_CHECK_INPUT>
        T & dereference (fast_pointer_type const ref)
        {
            return const_cast<T &> (const_cast<object_pool const *> (this)->operator[] (ref));
        }

        T & operator[] (fast_pointer_type const ref)
        {
            return dereference<false> (ref);
        }

        /**
         * allocate and construct an instance of 'T'
         * @param args parameters passed to the constructor
         */
        template<typename ... ARGs>
        alloc_result allocate (ARGs && ... args)
        {
            typename super::alloc_result const ar = super::allocate ();

            new (ar.first) T { std::forward<ARGs> (args) ... };

            return std::forward_as_tuple (* static_cast<T *> (ar.first), ar.second);
        }

        /**
         * destruct 'T' instance pointed to by 'ref' and release its slot back into the pool
         */
        void release (fast_pointer_type const ref)
        {
            super::template destruct<T> (ref);
        }

        // debug assists:

        using super::check;
        using super::allocation_bitmap;

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
