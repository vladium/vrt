
#include "vr/test/timing.h"

#include "vr/sys/cpu.h"
#include "vr/util/logging.h"
#include "vr/mc/mc.h"
#include "vr/util/memory.h"
#include "vr/util/singleton.h"

#include <boost/preprocessor/repetition/repeat.hpp>

#include <cmath>

#include <time.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
using namespace sys;

//............................................................................
//............................................................................
namespace
{

struct estimate_overhead final: public util::singleton_constructor<int64_t>
{
    estimate_overhead (int64_t * const obj)
    {
        cpu_info const & ci = cpu_info::instance ();

        timestamp_t const min_spin_time = 250 * _1_millisecond ();
        ::timespec ts;

#   define vr_START_STOP(z, n, data) \
            { \
                int64_t v = VR_TSC_START (); \
                VR_TSC_STOP (v); \
                tsc_min = (v < tsc_min ? v : tsc_min); \
            } \
            /* */

#   define vr_UNROLL_COUNT      199

        std::size_t const inner_repeats     = 256;

        int64_t tsc_min { std::numeric_limits<int64_t>::max () };
        int64_t repeats { };

        // temporarily pin to the current PU: (TODO use non-0 core ?)
        {
            affinity::scoped_thread_sched _ { make_bit_set (ci.PU_count ()).set (affinity::this_thread_last_PU ()) };

            VR_CHECKED_SYS_CALL (::clock_gettime (CLOCK_THREAD_CPUTIME_ID, & ts));

            timestamp_t const ts_deadline = ts.tv_sec * _1_second () + ts.tv_nsec + min_spin_time;
            timestamp_t ts_now { };

            do
            {
                for (std::size_t r = 0; r < inner_repeats; ++ r)
                {
                    BOOST_PP_REPEAT (vr_UNROLL_COUNT, vr_START_STOP, unused)
                }

                VR_CHECKED_SYS_CALL (::clock_gettime (CLOCK_THREAD_CPUTIME_ID, & ts));
                ts_now = ts.tv_sec * _1_second () + ts.tv_nsec;

                ++ repeats;
            }
            while (ts_now < ts_deadline);
        }

        LOG_trace1 << "[repeats: " << repeats << "] tsc_min = " << tsc_min;

        (* obj) = tsc_min;
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

int64_t const &
tsc_macro_overhead ()
{
    return util::singleton<int64_t, estimate_overhead>::instance ();
}
//............................................................................

void
flush_cpu_cache (int32_t const lvl)
{
#if VR_DEBUG

    if (VR_UNLIKELY (affinity::this_thread_PU_set ().count () != 1))
        throw_x (illegal_state, "flush_cpu_cache() should be invoked from a thread pinned to a single PU");

#endif // VR_DEBUG

    cpu_info const & ci = cpu_info::instance ();

    sys::cpu_info::cache::level const & l = ci.cache_info ().level_info (lvl); // data or unified cache [throws if 'lvl' is out of range]

    std::size_t const l_size = l.m_size;
    check_positive (l_size);

    int32_t const cl_step = l.m_line_size / sizeof (int64_t);
    check_positive (cl_step);

    LOG_trace1 << "L" << lvl << ": line size " << l.m_line_size << " (cl_step " << cl_step << "), size: " << l_size;

    using aligned_int64_array_ptr       = std::unique_ptr<int64_t [], void (*) (void *)>;

    // TODO it would be better if 'mem' were guaranteed not to overlap with any heap memory that might get used later
    // (either keep it around as a singleton or use BSS/data segment)

    aligned_int64_array_ptr const mem { static_cast<int64_t *> (util::aligned_alloc (l.m_line_size, l_size)), util::aligned_free };
    int64_t * const VR_RESTRICT mem_raw = mem.get ();

    std::size_t const array_size = l_size / sizeof (int64_t);
    int64_t dummy { };

    for (std::size_t i = 0; i < array_size; i += cl_step)
    {
        dummy += mc::volatile_cast (mem_raw [i]);
    }

    LOG_trace3 << "dummy: " << dummy;
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
