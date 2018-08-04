
#include "vr/rt/timer_queue/timer_queue.h"

#include "vr/util/datetime.h"
#include "vr/util/logging.h"
#include "vr/mc/mc.h"
#include "vr/mc/bound_runnable.h"
#include "vr/util/parse.h"
#include "vr/sys/os.h"

#include <sys/timerfd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................
/*
 * this is a quick HACK, need smth like a timer wheel, support recurring events, etc
 */
class timer_queue::timer_expiration_runnable final: public mc::bound_runnable,
                                                    noncopyable
{
    private: // ..............................................................

        using super         = mc::bound_runnable;

    public: // ...............................................................

        timer_expiration_runnable (int32_t const PU, schedule_map const & schedule) :
            super (PU),
            m_schedule (schedule) // note: container init
        {
        }

        ~timer_expiration_runnable () VR_NOEXCEPT // calls 'close ()'
        {
            close ();
        }


        void open ()
        {
            m_timer_fd = VR_CHECKED_SYS_CALL (::timerfd_create (CLOCK_REALTIME, TFD_CLOEXEC));
        }

        void close () VR_NOEXCEPT // idempotent
        {
            int32_t fd = mc::volatile_cast (m_timer_fd);
            if (fd >= 0)
            {
                mc::volatile_cast (m_timer_fd) = -1;
                VR_CHECKED_SYS_CALL_noexcept (::close (fd));
            }
        }

        // runnable:

        void operator() () final override
        {
            try
            {
                super::bind ();
                open ();

                for (auto const & kv : m_schedule)
                {
                    int32_t const fd = mc::volatile_cast (m_timer_fd);
                    if (fd < 0) break; // closed

                    timestamp_t const ts_utc = kv.first;
                    LOG_trace1 << "  scheduled time point " << ts_utc << " (" << print_timestamp (ts_utc) << " UTC)";

                    ::itimerspec its;
                    {
                        // not periodic:

                        its.it_interval.tv_sec = 0;
                        its.it_interval.tv_nsec = 0;

                        // absolute timeout:

                        its.it_value.tv_sec = ts_utc / _1_second ();
                        its.it_value.tv_nsec = ts_utc % _1_second ();

                        assert_nonnegative (its.it_value.tv_sec);
                        assert_nonnegative (its.it_value.tv_nsec);
                    }

                    VR_CHECKED_SYS_CALL (::timerfd_settime (fd, TFD_TIMER_ABSTIME, & its, nullptr));

                    uint64_t tio;
                    if (VR_UNLIKELY (::read (fd, & tio, sizeof (tio)) != sizeof (tio))) // block waiting for the timer expiry
                    {
                        LOG_trace1 << "timerfd read() aborted";
                        break;
                    }

                    LOG_trace1 << "  woke up @ " << " (" << print_timestamp (sys::realtime_utc ()) << " UTC)";

                    for (timer * const t : kv.second)
                    {
                        assert_nonnull (t);
                        t->expire ();
                    }
                }
            }
            catch (std::exception const & e)
            {
                LOG_error << exc_info (e);
            }

            close ();
            super::unbind (); // destructor always calls, but try to unbind eagerly
        }


        schedule_map const & m_schedule;
        int32_t m_timer_fd { -1 };

}; // end of nested class
//............................................................................
//............................................................................

timer_queue::timer_queue (schedule_spec const & schedule, int32_t const PU)
{
    m_timers.reserve (schedule.size ());

    for (auto const & kv : schedule)
    {
        string_vector const spec = util::split (kv.second, ";");
        check_eq (spec.size (), 2);
        std::string const & tz = spec [1];

        timestamp_t const ts_local = util::parse_ptime_as_timestamp (spec [0].data (), spec [0].size ());
        timestamp_t const ts_utc = ts_local - util::tz_offset (util::to_ptime (ts_local).date (), tz);

        LOG_trace2 << "  adding timer " << print (kv.first) << " for " << print (spec [0]) << ": " << ts_utc << " (" << print_timestamp (ts_utc) << " UTC)";

        m_timers.emplace_back (ts_utc);
        timer * const t = & m_timers.back ();

        auto i = m_schedule.find (ts_utc);
        if (i == m_schedule.end ())
            i = m_schedule.emplace (ts_utc, timer_ref_set { }).first;

        timer_ref_set & ts_set = i->second;

        ts_set.push_back (t);
        m_name_map.emplace (kv.first, t);
    }

    m_runnable = std::make_unique<timer_expiration_runnable> (PU, m_schedule);

    LOG_trace1 << "configured " << m_name_map.size () << " named timer(s), " << m_schedule.size () << " distinct time point(s)";
}
//............................................................................

void
timer_queue::start ()
{
    assert_nonnull (m_runnable);

    LOG_trace1 << "starting worker thread ...";

    m_worker = boost::thread { std::ref (* m_runnable) };
}

void
timer_queue::stop ()
{
    if (m_worker.joinable ())
    {
        LOG_trace1 << "joining worker thread ...";

        m_runnable->close ();
        m_worker.join ();

        LOG_trace1 << "worker thread DONE";
    }
}
//............................................................................

timer const &
timer_queue::operator[] (std::string const & name) const
{
    auto const i = m_name_map.find (name);
    if (VR_UNLIKELY (i == m_name_map.end ()))
        throw_x (invalid_input, "no timer with name " + print (name));

    return (* i->second);
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
