
#include "vr/sys/os.h"

#include "vr/asserts.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"
#include "vr/util/singleton.h"

#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if !defined (_GNU_SOURCE)
#   define _GNU_SOURCE
#endif
#include <errno.h>  // program_invocation_*name

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

template<>
struct os_info::access_by<void> final: public util::singleton_constructor<os_info>
{
    access_by (os_info * const obj)
    {
        auto const psz = ::sysconf (_SC_PAGESIZE);
        check_is_power_of_2 (psz);

        new (obj) os_info { static_cast<int32_t> (psz) };
    }

}; // end of class
//............................................................................
//............................................................................

using ops_checked       = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, true>;

os_info::os_info (int32_t const page_size) :
    m_page_size { page_size },
    m_log2_page_size { ops_checked::log2_ceil (page_size) }
{
    LOG_trace1 << "page size:\t" << m_page_size << " (log2: " << m_log2_page_size << ')';
    check_is_power_of_2 (page_size);
}

os_info const &
os_info::instance ()
{
    return util::singleton<os_info, os_info::access_by<void> >::instance ();
}
//............................................................................
//............................................................................
namespace
{

struct proc_name_impl final
{
    std::string m_name [2] { ::program_invocation_name, ::program_invocation_short_name };

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::string const &
proc_name (bool const short_form)
{
    return util::singleton<proc_name_impl>::instance ().m_name [short_form];
}

uint32_t
proc_tty_cols (uint32_t const default_if_no_tty)
{
    ::winsize w;
    if ((::ioctl (STDOUT_FILENO, TIOCGWINSZ, & w) < 0) || (w.ws_col <= 0))
        return default_if_no_tty;

    return w.ws_col;
}
//............................................................................

::pid_t
gettid ()
{
    return ::syscall (SYS_gettid);
}

std_uintptr_t
pthreadid ()
{
    return ((uintptr_t) ::pthread_self ()); // note: C-style cast by design
}
//............................................................................

void
short_sleep_for (timestamp_t const ns)
{
    if (ns <= 0) return;

    assert_le (ns, 1000000000L);

    ::timespec ts;

    ts.tv_sec = ns / 1000000000L;
    ts.tv_nsec = ns % 1000000000L;

    ::nanosleep (& ts, nullptr);
}

void
long_sleep_for (timestamp_t const ns)
{
    if (ns <= 0) return;

    ::timespec t;
    VR_CHECKED_SYS_CALL (::clock_gettime (CLOCK_REALTIME, & t));

    timestamp_t const deadline = t.tv_sec * 1000000000L + t.tv_nsec + ns;
    assert_positive (deadline);

    t.tv_sec = deadline / 1000000000L;
    t.tv_nsec = deadline % 1000000000L;

    while (true)
    {
        int32_t const rc = ::clock_nanosleep (CLOCK_REALTIME, TIMER_ABSTIME, & t, nullptr);
        if (VR_LIKELY (rc == 0))
            break;
        else // note: does not set errno
        {
            if (VR_LIKELY (rc == EINTR))
                continue;
            else
                throw_x (sys_exception, "clock_nanosleep() error (rc: " + string_cast (rc) + ')');
        }
    }
}
//............................................................................

void
sleep_until (timestamp_t const ts_utc)
{
    ::timespec t;

    t.tv_sec = ts_utc / 1000000000L;
    t.tv_nsec = ts_utc % 1000000000L;

    while (true)
    {
        int32_t const rc = ::clock_nanosleep (CLOCK_REALTIME, TIMER_ABSTIME, & t, nullptr);
        if (VR_LIKELY (rc == 0))
            break;
        else // note: does not set errno
        {
            if (VR_LIKELY (rc == EINTR))
                continue;
            else
                throw_x (sys_exception, "clock_nanosleep() error (rc: " + string_cast (rc) + ')');
        }
    }
}
//............................................................................

VR_ASSUME_HOT timestamp_t
realtime_utc ()
{
    ::timespec t;
    VR_CHECKED_SYS_CALL (::clock_gettime (CLOCK_REALTIME, & t));

    return (t.tv_sec * _1_second () + t.tv_nsec);
}

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
