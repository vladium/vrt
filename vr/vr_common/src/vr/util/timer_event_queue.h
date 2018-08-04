#pragma once

#include "vr/containers/util/treap_priority_queue.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{
namespace teq
{
//............................................................................

using entry_ref_type    = int32_t;
using priority_type     = int32_t;

//............................................................................

using entry_base        = intrusive::bi::bs_set_base_hook<intrusive::bi_traits::link_mode>;

template<typename V>
struct make_entry_schema
{
    using type          = meta::make_schema_t
                        <
                            meta::fdef_<timestamp_t,    _key_>,
                            meta::fdef_<V,              _value_,    meta::elide<util::is_void<V>::value>>,
                            meta::fdef_<entry_ref_type, _instance_>,
                            meta::fdef_<priority_type,  _priority_>
                        >;

}; // end of metafunction
//............................................................................

template<typename V>
struct timer_event final: public meta::make_compact_struct_t<typename make_entry_schema<V>::type, entry_base>
{
    using key_type      = timestamp_t;
    using value_type    = V; // allow 'V = void' to compile

    // boost.intrusive requirements:

    struct comparator
    {
        VR_FORCEINLINE bool operator() (timer_event const & lhs, timer_event const & rhs) const VR_NOEXCEPT
        {
            return (lhs.timestamp () < rhs.timestamp ());
        }

    }; // end of 'comparison' functor

    friend bool priority_order (timer_event const & lhs, timer_event const & rhs) VR_NOEXCEPT
    {
        return (field<_priority_> (lhs) < field<_priority_> (rhs));
    }

    // ACCESSORs:

    VR_FORCEINLINE timestamp_t const & timestamp () const VR_NOEXCEPT
    {
        return field<_key_> (* this);
    }

    VR_FORCEINLINE util::if_is_void_t<V, addr_const_t, V> const & value () const VR_NOEXCEPT
    {
        return field<_value_> (* this);
    }

    // MUTATORs:

    VR_FORCEINLINE util::if_is_void_t<V, addr_const_t, V> & value () VR_NOEXCEPT
    {
        return field<_value_> (* this);
    }

}; // end of class

} // end of 'teq'
} // end of 'impl'
//............................................................................
//............................................................................
/**
 * - no heap ops in steady state
 */
template<typename V = void>
class timer_event_queue final: public util::treap_priority_queue<impl::teq::timer_event<V>>
{
    private: // ..............................................................

        using super     = util::treap_priority_queue<impl::teq::timer_event<V>>;

    public: // ...............................................................

        using typename super::entry_type;
        using super::empty;
        using super::front;

        using super::super; // inherit constructors

        // MUTATORs:

        // optimized methods (addition to 'treap_priority_queue') that avoid
        // re-allocating an 'entry_type' slot:

        /**
         * @param e entry to be rescheduled [must be currently in the queue]
         * @param ts [must be greater than or equal to 'e.timestamp ()']
         */
        void reschedule (entry_type & e, timestamp_t const & ts)
        {
            assert_le (e.timestamp (), ts);

            super::requeue (e, ts);
        }

        /**
         * @param ts [must be greater than or equal to the timestamp of the current queue front entry]
         */
        entry_type & reschedule_front (timestamp_t const & ts)
        {
            assert_condition (! empty () && (front ()->timestamp () < ts), empty ());

            return super::requeue_front (ts);
        }

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
