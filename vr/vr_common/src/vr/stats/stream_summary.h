#pragma once

#include "vr/asserts.h"
#include "vr/containers/intrusive.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"

#include <boost/iterator/iterator_facade.hpp>
#include <iterator> // std::next()

//----------------------------------------------------------------------------
namespace vr
{
/**
 * @ref Metwally et al
 */
template<typename T> // split into base/T-specific
class stream_summary final: noncopyable
{
    private: // ..............................................................

        class bucket; // forward
        class iterator_impl; // forward

    public: // ...............................................................

        using size_type         = int32_t;
        using count_type        = int64_t; // TODO define via traits with a default
        using value_type        = T;

        using const_iterator    = iterator_impl;

        struct item_context final: public intrusive::bi::list_base_hook<intrusive::bi_traits::link_mode>
        {
            VR_FORCEINLINE T const & value () const;
            VR_FORCEINLINE count_type const & count () const;
            /**
             * @return upper bound on by how much 'count()' is an overestimate of true count
             */
            VR_FORCEINLINE count_type const & count_error () const;

            private:

                friend class stream_summary;

                // TODO compactify 'm_count_error' and 'm_value'

                bucket * m_parent { nullptr }; // not const: counts can migrate between buckets
                count_type m_count_error { }; // upper bound on by how much 'count()' is an overestimate of true count
                T m_value; // [not initialized by design] not const: can be replaced when summary evicts a min item

        }; // end of nested class


        stream_summary (size_type const max_size);
        ~stream_summary ();


        // ACCESSORs:

        /**
         * @return number of unique items currently being monitored [no greater that 'max_size' passed to the constructor]
         */
        size_type const & size () const
        {
            return m_items_allocated;
        }

        const_iterator begin () const
        {
            const bucket_list_const_reverse_iterator bucket_i_limit = m_buckets.rend ();
            const bucket_list_const_reverse_iterator bucket_i = m_buckets.rbegin ();

            if (bucket_i != bucket_i_limit)
                return { bucket_i_limit, bucket_i, bucket_i->m_items.rbegin () };
            else
                return { bucket_i_limit, bucket_i, item_list_const_reverse_iterator { } };
        }

        const_iterator end () const
        {
            bucket_list_const_reverse_iterator const bucket_i_limit = m_buckets.rend ();

            return { bucket_i_limit, bucket_i_limit, item_list_const_reverse_iterator { } };
        }

        // MUTATORs:

        VR_ASSUME_HOT void update (T const * const values, int32_t const size); // TODO add an overload taking an iterator range

    private: // ..............................................................

        /* overall structure is an ordered list of 'bucket''s, each of which
         * defines an item count for all items currently grouped in this bucket -- these
         * are represented by a list of 'item_context''s associated with each bucket.
         *
         * buckets are ordered by increasing count values; item_contexts are in arbitrary order
         * within their respective sub-lists; each item_context points to its parent
         * bucket via 'm_parent'.
         *
         * 'm_item_map' implements an O(1) hashmap lookup for any item currently
         * being monitored (i.e. listed under one of the buckets).
         */

        using item_list     = intrusive::bi::list<item_context, intrusive::bi::constant_time_size<false> >; // doubly linked list (constant_time_size disabled; only empty() is used)
        using item_map      = boost::unordered_map<T, item_context *>; // TODO faster map

        struct bucket final: public intrusive::bi::list_base_hook<intrusive::bi_traits::link_mode>
        {
            count_type m_count { }; // TODO compactify
            item_list m_items { };

        }; // end of nested class

        using bucket_list   = intrusive::bi::list<bucket>; // doubly linked list (defaults to constant_time_size)

        // 'begin()/end()' actually work with reverse iterators (to traverse buckets in order of decreasing counts):

        using bucket_list_const_reverse_iterator        = typename bucket_list::const_reverse_iterator;
        using item_list_const_reverse_iterator          = typename item_list::const_reverse_iterator;

        class iterator_impl final: public boost::iterator_facade<iterator_impl, item_context const, boost::forward_traversal_tag>
        {
            public:

                iterator_impl () = default;

                iterator_impl (bucket_list_const_reverse_iterator const & bucket_i_limit, bucket_list_const_reverse_iterator const & bucket_i,
                               item_list_const_reverse_iterator const & item_i) :
                    m_bucket_i_limit { bucket_i_limit },
                    m_bucket_i { bucket_i },
                    m_item_i  {item_i }
                {
                }

            private:

                friend class boost::iterator_core_access;

                // iterator_facade:

                bool equal (iterator_impl const & rhs) const
                {
                    if (m_bucket_i == m_bucket_i_limit)
                        return (rhs.m_bucket_i == rhs.m_bucket_i_limit);
                    else
                        return ((m_item_i == rhs.m_item_i) && (m_bucket_i == rhs.m_bucket_i));
                }

                item_context const & dereference () const
                {
                    return (* m_item_i);
                }

                void increment ()
                {
                    ++ m_item_i;
                    if (m_item_i == m_bucket_i->m_items.rend ())
                    {
                        ++ m_bucket_i;

                        if (m_bucket_i != m_bucket_i_limit)
                            m_item_i = m_bucket_i->m_items.rbegin ();
                    }
                }


                const bucket_list_const_reverse_iterator m_bucket_i_limit;
                bucket_list_const_reverse_iterator m_bucket_i;
                item_list_const_reverse_iterator m_item_i;

        }; // end of nested class

        void increment_item_count (item_context * const item);
        VR_FORCEINLINE void update_with_value (T const value);



        size_type const m_max_size;
        size_type m_items_allocated { };
        std::unique_ptr<bucket []> const m_bucket_holder;       // owns buckets in the intrusive list fields
        std::unique_ptr<item_context []> const m_item_holder;   // owns item contexts in the intrusive list fields
        bucket_list m_buckets; // (non-owning) intrusive list of buckets ordered by increasing 'm_count'
        item_map m_item_map; // (non-owning) map item value -> monitored
        bucket_list m_bucket_free_list; // (non-owning)

}; // end of class
//............................................................................

template<typename T>
stream_summary<T>::stream_summary (size_type const max_size) :
    m_max_size { max_size },
    m_bucket_holder { std::make_unique<bucket []> (max_size) },
    m_item_holder { std::make_unique<item_context []> (max_size) }
{
    check_positive (max_size);

    LOG_trace1 << "sizeof (item_context) = " << sizeof (item_context) << ", sizeof (bucket) = " << sizeof (bucket);

    for (size_type b = 0; b != max_size; ++ b)
    {
        m_bucket_free_list.push_back (m_bucket_holder [b]);
    }
}

template<typename T>
stream_summary<T>::~stream_summary ()
{
    if (intrusive::bi_traits::safe_mode ()) // compile-time guard
    {
        // in safe linking mode, explicit intrusive list clearing is necessary, otherwise
        // bucket/item destructors will complain about being destroyed in still-linked state:

        for (count_type b = 0; b != m_max_size; ++ b)
        {
            m_bucket_holder [b].m_items.clear ();
        }

        m_buckets.clear ();
        m_bucket_free_list.clear ();
    }
}
//............................................................................

template<typename T>
void
stream_summary<T>::increment_item_count (item_context * const item)
{
    bucket * const parent = item->m_parent;
    assert_condition (parent);

    DLOG_trace2 << "incrementing item (" << item->m_value << "), current count = " << parent->m_count;

    count_type const item_count = parent->m_count + 1;  // updated count for 'value'

    // move 'item' into the bucket for this larger count 'item_count':
    // [three cases are possible: next bucket either does not exist (EOL) or
    // has a count that's less than or equal to 'item_count']

    item_list & parent_items = parent->m_items;
    assert_nonempty (parent_items); // a bucket can only be empty in the middle of update

    // unlink 'item' from its current parent list:

    auto const ii = item_list::s_iterator_to (* item);
    parent_items.erase (ii);

    // find next bucket:

    auto const bi = bucket_list::s_iterator_to (* parent);
    auto const bi_next = std::next (bi);

    if (bi_next == m_buckets.end () || bi_next->m_count != item_count)
    {
        // a new bucket needs to be created to hold 'item_count'; however, if 'parent_items'
        // became empty due to our current item unlinking above, re-use it:

        if (VR_LIKELY (! parent_items.empty ()))
        {
            // allocate a new 'bucket':

            assert_nonempty (m_bucket_free_list);
            bucket & new_parent = m_bucket_free_list.front ();
            m_bucket_free_list.pop_front ();

            DLOG_trace2 << "allocated new bucket @" << & new_parent;

            new_parent.m_count = item_count;
            new_parent.m_items.push_back (* item);

            item->m_parent = & new_parent;

            m_buckets.insert (bi_next, new_parent);
        }
        else
        {
            DLOG_trace2 << "re-using bucket @" << parent;

            parent->m_count = item_count;
            parent_items.push_back (* item);

            // ['item''s parent link does not change]
            // ['parent' keeps its position in the bucket list]
        }
    }
    else
    {
        // move 'item' to the next bucket's item list:

        DLOG_trace2 << "moving item (" << item->m_value << ") to bucket @" << & (* bi_next) << " with count " << bi_next->m_count;

        bi_next->m_items.push_back (* item);

        item->m_parent = & (* bi_next);

        // if the parent bucket becomes empty as a result, relocate it to the free list:

        if (parent_items.empty ())
        {
            DLOG_trace2 << "freeing bucket @" << parent;

            m_buckets.erase (bi);
            m_bucket_free_list.push_front (* parent);
        }
    }
}
//............................................................................

template<typename T>
void // forced inline
stream_summary<T>::update_with_value (T const value)
{
    DLOG_trace2 << '[' << (m_max_size - m_bucket_free_list.size ()) << ", " << m_items_allocated << "] update_with_value(" << value << ')';

    auto const vi = m_item_map.find (value);

    if (VR_LIKELY (vi != m_item_map.end ()))
    {
        // update this item's count, which requires moving it to another bucket:

        item_context * const item = vi->second;
        increment_item_count (item);
    }
    else // 'value' is not in the map
    {
        item_context * item { nullptr };

        if (VR_LIKELY (m_items_allocated == m_max_size)) // monitored set size has reached max
        {
            // replace one of the items with the currently min count:

            assert_nonempty (m_buckets);
            bucket & bmin = m_buckets.front ();

            assert_nonempty (bmin.m_items);
            item = & bmin.m_items.back (); // last item in the list more likely to have been accessed recently

            // evict prev value:

            DLOG_trace2 << "evicting a min bucket @" << & bmin << ": value = " << item->m_value << ", count = " << bmin.m_count;

            m_item_map.erase (item->m_value); // update monitored set
            item->m_value = value;
            item->m_count_error = bmin.m_count;

            increment_item_count (item);
        }
        else // add a new value to the monitored set
        {
            DLOG_trace2 << "adding a new monitored item (" << value << ") ...";

            // allocate a new 'monitored':

            assert_lt (m_items_allocated, m_max_size);
            item = & m_item_holder [m_items_allocated ++];

            // allocate a new 'bucket':

            assert_nonempty (m_bucket_free_list);
            bucket & parent = m_bucket_free_list.front ();
            m_bucket_free_list.pop_front ();

            DLOG_trace2 << "allocated new bucket @" << & parent;

            parent.m_count =  { };
            parent.m_items.push_back (* item);

            item->m_parent = & parent;
            item->m_value = value;

            m_buckets.push_front (parent); // bucket with 0 count goes to the head of the list

            increment_item_count (item);
        }

        m_item_map.emplace (value, item); // update monitored set
    }
}
//............................................................................

template<typename T>
void
stream_summary<T>::update (T const * const values, int32_t const size)
{
    for (int32_t i = 0; i < size; ++ i)
    {
        assert_condition (m_items_allocated == signed_cast (m_item_map.size ()));

        update_with_value (values [i]);
    }
}
//............................................................................
//............................................................................

template<typename T> // forced inline
T const &
stream_summary<T>::item_context::value () const
{
    return m_value;
}

template<typename T> // forced inline
typename stream_summary<T>::count_type const &
stream_summary<T>::item_context::count () const
{
    return m_parent->m_count;
}

template<typename T> // forced inline
typename stream_summary<T>::count_type const &
stream_summary<T>::item_context::count_error () const
{
    return m_count_error;
}

} // end of namespace
//----------------------------------------------------------------------------
