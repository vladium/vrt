#pragma once

#include "vr/util/di/dep_injection_fwd.h"
#include "vr/startable.h"
#include "vr/types.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/latch.hpp>
#include <boost/thread/mutex.hpp>

#include <atomic>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
class container; // forward
/**
 * a utility for sequencing 'startable's run by a container through their
 * start/stop phases
 *
 * @note lifecycle methods are run by container-created (and possibly PU-bound)
 *       threads to allow PU-/thread-localization of data structures created by
 *       these methods
 */
class container_barrier final: noncopyable
{
    public: // ...............................................................

        using turn_map      = boost::unordered_map<startable const *, int32_t>;

        container_barrier (container & parent, std::size_t const latch_count, turn_map && turns);

        // these are invoked from different threads
        // (bound/unbound runnables as well as the container's main thread)

        void start_begin (); // container

        bool start (startable & cs) VR_NOEXCEPT_IF (VR_RELEASE);
        void start_end_wait ();

        void run_end_wait ();
        void stop_begin (); // container

        void stop (startable & cs) VR_NOEXCEPT_IF (VR_RELEASE);
        void stop_end ();

    private: // ..............................................................

        // note: not bothering with alignment/padding here
        // since this is not a performance-critical path

        container & m_parent;
        turn_map const m_turns;
        boost::mutex m_cv_mutex { };
        boost::condition_variable m_cv { };
        boost::latch m_start_latch;
        boost::latch m_stop_latch; // TODO this needs to count successful starts?
        int32_t m_turn { std::numeric_limits<int32_t>::min () };
        std::atomic<int32_t> m_start_count { 0 };

}; // end of class

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
