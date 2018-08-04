#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/mc.h"

#include <atomic>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
/**
 * TODO this impl needs mod if it's ever used in prod runtime (needs to be test-and-TAS)
 */
template<bool USE_PAUSE = true>
class spinlock final: noncopyable
{
    public: // ...............................................................

        spinlock () :
            m_lock { }
        {
        }


        VR_FORCEINLINE void lock ()
        {
            while (static_cast<std::atomic_flag &> (m_lock).test_and_set (std::memory_order_acquire))
            {
                if (USE_PAUSE) mc::pause (); // compile-time check
            }
        }

        VR_FORCEINLINE void unlock ()
        {
            static_cast<std::atomic_flag &> (m_lock).clear (std::memory_order_release);
        }

    private: // ..............................................................

        using cl_padded_atomic_flag     = cache_line_padded_field<std::atomic_flag>; // guaranteed no false sharing

        cl_padded_atomic_flag m_lock = ATOMIC_FLAG_INIT;

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
