#pragma once

#include "vr/meta/integer.h"
#include "vr/sys/defs.h"

#include <sys/syscall.h>
#include <unistd.h> // pid_t

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

class os_info final: noncopyable
{
    public: // ...............................................................

        static os_info const & instance ();

        /*
         * assumption for when this must be a compile-time constant
         */
        static constexpr int32_t static_page_size ()        { return (4 * 1024); } // TODO make a build parameter
        static constexpr int32_t static_log2_page_size ()   { return meta::static_log2_ceil<static_page_size ()>::value; }

        /*
         * as reported by 'sysconf (_SC_PAGESIZE)'
         */
        int32_t const & page_size () const
        {
            return m_page_size;
        }

        int32_t const & log2_page_size () const
        {
            return m_log2_page_size;
        }

        template<typename A> class access_by;

    private: // ..............................................................

        template<typename A> friend class access_by;

        os_info (int32_t const page_size);


        int32_t const m_page_size;
        int32_t const m_log2_page_size;

}; // end of class
//............................................................................
/**
 * returns the 'args [0]' used to create this process
 *
 * @param short_form if 'true' (default), return only the segment after the last slash
 */
extern std::string const &
proc_name (bool const short_form = true);

/**
 * @return current process' terminal width as returned by TIOCGWINSZ ioctl [or 'default_if_no_tty' on error]
 */
extern uint32_t
proc_tty_cols (uint32_t const default_if_no_tty = 120);

//............................................................................
/**
 * get [Linux-specific] current thread ID (TID)
 */
extern ::pid_t
gettid ();

/**
 * get a printable form of 'pthread_self()'
 */
extern std_uintptr_t
pthreadid ();

//............................................................................
/**
 * get [Linux-specific] CPU ID where the calling thread is currently running
 *
 * @note the return value is a PU index as used by @ref sys::affinity API
 */
VR_FORCEINLINE int32_t
cpuid ()
{
    int32_t r;
    VR_IF_DEBUG (auto const rc =) ::syscall (SYS_getcpu, & r, nullptr, nullptr);
    VR_IF_DEBUG
    (
        if (VR_UNLIKELY (rc < 0))
        {
            auto const e = errno;
            throw_x (sys_exception, "SYS_getcpu error (" + string_cast (e) + "): " + std::strerror (e));
        }
    )

    return r;
}
//............................................................................
/**
 * implemented via nanosleep(2) (and doesn't bother with possible EINTR)
 *
 * @param ns [should be <= 1 sec]
 */
extern void
short_sleep_for (timestamp_t const ns);

/**
 * implemented via clock_nanosleep(2) and CLOCK_REALTIME, TIMER_ABSTIME
 */
extern void
long_sleep_for (timestamp_t const ns);

//............................................................................
/**
 * implemented via clock_nanosleep(2) and CLOCK_REALTIME, TIMER_ABSTIME
 *
 * @note returns right away if 'ts_utc' is already in the past
 */
extern void
sleep_until (timestamp_t const ts_utc);

//............................................................................
/**
 * a shortcut to 'clock_gettime (CLOCK_REALTIME,...)', useful for testing clock
 * consistency wrt PTP, etc
 */
extern timestamp_t
realtime_utc ();

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
