#pragma once

#include "vr/meta/utility.h" // padding
#include "vr/sys/cpu.h"
#include "vr/util/classes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

#define VR_ALIGNAS_CL alignas (sys::cpu_info::cache::static_line_size ())

//............................................................................
//............................................................................
namespace impl
{
/**
 * @param size min number of cache lines to contain 'size'
 */
constexpr std::size_t pad_to_cache_line (std::size_t const size)
{
    return ((size + sys::cpu_info::cache::static_line_size () - 1) / sys::cpu_info::cache::static_line_size ());
}
//............................................................................

template<typename T, bool _ = util::is_fundamental_or_pointer<T>::value>
struct destruct_field; // master


template<typename T>
struct destruct_field<T, false>
{
    static VR_FORCEINLINE void evaluate (T & obj)
    {
        util::destruct (obj);
    }

}; // end of specialization

template<typename T>
struct destruct_field<T, true>
{
    static VR_FORCEINLINE void evaluate (T & obj)
    {
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

using cache_line_slot   = std::aligned_storage<sys::cpu_info::cache::static_line_size (), sys::cpu_info::cache::static_line_size ()>::type;

//............................................................................
/**
 * wraps 'T' into a class that converts to a 'T' value contained within a memory
 * slot aligned at a cache line boundary and of size that is a multiple of cache line sizes
 *
 * (the latter is useful for avoiding false sharing)
 */
template<typename T>
struct cache_line_padded_field: noncopyable
{
    template<typename ... ARGs> // allow non-default 'T' construction
    explicit cache_line_padded_field (ARGs && ... args) :
        m_field { std::forward<ARGs> (args) ... }
    {
    }

    ~cache_line_padded_field ()
    {
        impl::destruct_field<T>::evaluate (m_field); // no-op if 'T' has trivial destruction or is a fundamental or pointer type
    }

    // ACCESSORs:

    VR_FORCEINLINE T const & value () const
    {
        return m_field;
    }

    VR_FORCEINLINE operator T const & () const
    {
        return value ();
    }

    // MUTATORs:

    VR_FORCEINLINE T & value ()
    {
        return m_field;
    }

    VR_FORCEINLINE operator T & ()
    {
        return value ();
    }

    private:

        union
        {
            T m_field; // aggregate 'T'
            meta::padding<impl::pad_to_cache_line (sizeof (T)), cache_line_slot> _;
        };

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
