#pragma once

#include "vr/util/traits.h"

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <atomic>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{
/*
 * @throws if 'size' is not positive
 * @throws on mem allocation failure
 */
extern void *
allocate_storage (int32_t const size, std::size_t const alignment, signed_size_t const alloc_size);

//............................................................................

template<typename MT_POLICY, typename T = int32_t>
struct ref_counter_traits
{
    static_assert (lazy_false<MT_POLICY>::value, "'MT_POLICY' must be one of {'thread_safe', 'thread_unsafe'}");

}; // end of master

template<typename T>
struct ref_counter_traits<thread::safe, T>
{
    using raw_ref_count_t       = T;
    using ref_count_t           = std::atomic<raw_ref_count_t>;

}; // end of specialization

template<typename T>
struct ref_counter_traits<thread::unsafe, T>
{
    using raw_ref_count_t       = T;
    using ref_count_t           = raw_ref_count_t;

}; // end of specialization
//............................................................................

template<typename MT_POLICY>
class shared_base
{
    protected: // ............................................................

        using traits            = ref_counter_traits<MT_POLICY>;

    public: // ...............................................................

        using raw_ref_count_t   = typename traits::raw_ref_count_t;

        // ACCESSORs:

        raw_ref_count_t ref_count () const
        {
            return m_ref_count;
        }

    protected: // ............................................................

        using ref_count_t       = typename traits::ref_count_t;


        ref_count_t m_ref_count { }; // [in 'shared_array' subtypes initialized by 'create()']

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 */
template<typename DERIVED, typename MT_POLICY = thread::safe>
class shared_object: public impl::shared_base<MT_POLICY>, // TODO optimize base ordering for size
                     noncopyable
{
    private: // ..............................................................

        using super             = impl::shared_base<MT_POLICY>;
        using this_type         = DERIVED;

    public: // ...............................................................

        using ptr               = boost::intrusive_ptr<this_type>;

        // intrusive_ptr support:

        friend VR_FORCEINLINE void intrusive_ptr_add_ref (this_type * const obj)
        {
            ++ obj->m_ref_count;
        }

        friend VR_FORCEINLINE void intrusive_ptr_release (this_type * const obj)
        {
            if (! (-- obj->m_ref_count)) delete obj;
        }

        template<typename ... ARGs>
        static ptr create (ARGs && ... args)
        {
            return { new this_type { std::forward<ARGs> (args) ... } };
        }

        // ACCESSORs:

        using super::ref_count;

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
