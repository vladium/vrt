
#include "vr/rt/timer_queue/timer_queue.h"

#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/sys/os.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................

TEST (timer_queue, clock_correctness)
{
    util::di::container app { join_as_name ("APP", test::current_test_name ()) };

    int32_t const PU_timers { 0 }; // TODO set dynamically based on cpu_info

    std::string const & tz = util::local_tz ();
    auto const t0_utc = util::to_ptime (sys::realtime_utc ());

    timestamp_t const local_tz_offset = util::tz_offset (t0_utc.date (), tz);


    timestamp_t const ts_utc = util::to_timestamp (t0_utc + pt::seconds { 2 }); // 2 sec from now, in UTC
    timestamp_t const ts_loc = ts_utc + (2 * _1_second ()) + local_tz_offset;   // 4 sec from now, in local

    // TODO canonic timestamp-with-tz format

    std::string const t_utc_spec = util::print_timestamp_as_ptime (ts_utc) + ";UTC";
    LOG_info << "UTC timer spec: " << print (t_utc_spec);

    std::string const t_loc_spec = util::print_timestamp_as_ptime (ts_loc) + ';' + tz;
    LOG_info << "local timer spec: " << print (t_loc_spec);


    app.configure ()
        ("timers",  new timer_queue {
            {
                { "t.utc", t_utc_spec },
                { "t.loc", t_loc_spec }
            },
            PU_timers })
    ;

    constexpr timestamp_t spin_pause = 10 * _1_millisecond ();
    constexpr int32_t i_limit = (4 * _1_second ()) / spin_pause; // 2x * 2 sec

    app.start ();
    {
        timer_queue const & timers = app ["timers"];

        timer const & t_utc = timers ["t.utc"];
        timer const & t_loc = timers ["t.loc"];

        int32_t i;

        for (i = 0; (i < i_limit) && ! t_utc.expired (); ++ i)
        {
            sys::short_sleep_for (spin_pause);
        }
        LOG_trace1 << "i = " << i;
        EXPECT_TRUE ((1 < i) && (i < i_limit)) << "timer 't.utc' didn't expire as expected, i = " << i;

        for (i = 0; (i < i_limit) && ! t_loc.expired (); ++ i)
        {
            sys::short_sleep_for (spin_pause);
        }
        LOG_trace1 << "i = " << i;
        EXPECT_TRUE ((1 < i) && (i < i_limit)) << "timer 't.loc' didn't expire as expected, i = " << i;
    }
    app.stop ();
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
