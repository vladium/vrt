#pragma once

#include "vr/containers/intrusive/treap.h"
#include "vr/fields.h"
#include "vr/tags.h"
#include "vr/util/intrinsics.h"
#include "vr/util/object_pools.h"
#include "vr/util/random.h"

#include <boost/intrusive/treap.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

template<typename T>
struct make_value_object_pool
{
    using instance_ref_type = typename meta::find_field_def_t<_instance_, T>::value_type;

    using type          = util::object_pool<T, typename util::pool_options<T, instance_ref_type>::type>; // TODO chunk capacity

}; // end of metafunction

template<typename T>
struct make_treap_queue
{
    using type          = intrusive::bi::treap_multiset
                        <
                            T,
                            intrusive::bi::compare<typename T::comparator>,
                            intrusive::bi::constant_time_size<false>,
                            intrusive::bi::size_type<int32_t>
                        >;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
// TODO design option for computing priority from stable entry contents (e.g. crc32(address))
// TODO reproducible default priority_seed
/**
 * a boost.treap used as a priority queue:
 *
 *  - boost impl is augmented with a finger to its min element, making min lookup
 *    guaranteed O(1) and min removal *expected* O(1);
 *
 *  - for arbitrary inserts, the usual expected O(log size()) treap performance applies;
 *    for inserts that are expected to be after the end of the queue (as could
 *    be the case with many sim future events) a hinted insert can be used and
 *    will have O(1) cost for a valid hint (an upper bound for the value being inserted)
 *
 *  'T' requirements:
 *
 *  1. default-constructible,
 *  2. 'T::key_type', 'T::value_type'
 *  3. 'T::comparator'
 *  4. fields tagged '_key_', '_value_', '_priority_', and '_instance_'
 */
template<typename T>
class treap_priority_queue
{
    private: // ..............................................................

        using this_type     = treap_priority_queue<T>;

        using pool_type     = typename impl::make_value_object_pool<T>::type;
        using queue_type    = typename impl::make_treap_queue<T>::type;

    public: // ...............................................................

        using entry_type    = T;
        using key_type      = typename T::key_type;
        using value_type    = typename T::value_type; // note: could be 'void'
        using size_type     = typename queue_type::size_type;
        using priority_type = typename meta::find_field_def_t<_priority_, T>::value_type;


        treap_priority_queue (size_type const initial_capacity = pool_type::options::chunk_capacity (),
                              priority_type const priority_seed = 0) :
            m_entry_pool { unsigned_cast (initial_capacity) },
            m_priority { (priority_seed ? : static_cast<priority_type> (util::i_crc32 (1, reinterpret_cast<std_uintptr_t> (this)))) }
        {
            check_nonzero (m_priority); // our "RNG" depends on this
        }

        ~treap_priority_queue ()
        {
            if (intrusive::bi_traits::safe_mode ()) // compile-time guard
            {
                // in safe linking mode, explicit intrusive list clearing is necessary, otherwise
                // bucket/item destructors will complain about being destroyed in still-linked state:

                m_queue.clear ();
            }
        }

        // ACCESSORs:

        bool empty () const
        {
            return m_queue.empty ();
        }

        /**
         * @return [nullptr if queue is empty]
         */
        VR_FORCEINLINE entry_type const * front () const
        {
            auto const bi = m_queue.begin ();
            return (bi == m_queue.end () ? nullptr : & (* bi));
        }

        // MUTATORs:

        /**
         * @return [nullptr if queue is empty]
         */
        VR_FORCEINLINE entry_type * front () // mutator is needed for 'requeue()'
        {
            return const_cast<entry_type *> (const_cast<this_type const *> (this)->front ());
        }

        // TODO in enqueue(), can I "reuse" priorities of previously released pool slots (save an RNG calls and a memstore)?

        template<bool _ = util::is_void<value_type>::value>
        auto enqueue (key_type const & key, meta::safe_referencable_t<value_type> const & value) -> util::enable_if_t<(! _), void>
        {
            typename pool_type::alloc_result ar = m_entry_pool.allocate ();

            entry_type & e = std::get<0> (ar);
            {
                field<_key_> (e) = key;
                field<_value_> (e) = value;
                field<_instance_> (e) = std::get<1> (ar);
                field<_priority_> (e) = util::xorshift (m_priority);
            }

            m_queue.insert (e);
        }

        template<bool _ = util::is_void<value_type>::value>
        auto enqueue (key_type const & key, meta::safe_referencable_t<value_type> && value) -> util::enable_if_t<(! _), void>
        {
            typename pool_type::alloc_result ar = m_entry_pool.allocate ();

            entry_type & e = std::get<0> (ar);
            {
                field<_key_> (e) = key;
                field<_value_> (e) = std::move (value);
                field<_instance_> (e) = std::get<1> (ar);
                field<_priority_> (e) = util::xorshift (m_priority);
            }

            m_queue.insert (e);
        }


        template<bool _ = util::is_void<value_type>::value>
        auto enqueue (key_type const & key) -> util::enable_if_t<_, void>
        {
            typename pool_type::alloc_result ar = m_entry_pool.allocate ();

            entry_type & e = std::get<0> (ar);
            {
                field<_key_> (e) = key;
                field<_instance_> (e) = std::get<1> (ar);
                field<_priority_> (e) = util::xorshift (m_priority);
            }

            m_queue.insert (e);
        }

        void dequeue ()
        {
            assert_nonempty (m_queue);

            auto const bi = m_queue.begin ();

            entry_type & e = * bi; // retain the pool reference
            m_queue.erase (bi);

            m_entry_pool.release (field<_instance_> (e));
        }

    protected: // ............................................................

        // an optimized interface for subclasses to use when an entry is expected
        // to be modified and re-inserted (no need to do pool release/alloc cycle):

        VR_FORCEINLINE void requeue (entry_type & e, key_type const & key)
        {
            auto const bi = queue_type::s_iterator_to (e); // note: log cost
            m_queue.erase (bi);
            {
                field<_key_> (e) = key; // note: not changing '_instance_', '_priority_' (up to the caller to modify '_value_')
            }
            m_queue.insert (e);
        }

        VR_FORCEINLINE entry_type & requeue_front (key_type const & key)
        {
            assert_nonempty (m_queue);

            auto const bi = m_queue.begin (); // note: O(1)
            m_queue.erase (bi); // there's no 'pop_front()'

            entry_type & e = (* bi);
            {
                field<_key_> (e) = key; // note: not changing '_instance_', '_priority_' (up to the caller to modify '_value_')
            }
            m_queue.insert (e);

            return e;
        }

    private: // ..............................................................

        using pool_ref_type     = typename pool_type::pointer_type;
        using instance_ref_type = typename meta::find_field_def_t<_instance_, T>::value_type;
        vr_static_assert (std::is_same<pool_ref_type, instance_ref_type>::value); // by construction


        queue_type m_queue { };
        pool_type m_entry_pool;
        priority_type m_priority;

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
