
#include "vr/market/rt/market_data_feed.h"
#include "vr/market/sources/mock/mock_mcast_server.h"
#include "vr/mc/mc.h"
#include "vr/mc/thread_pool.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/util/crc32.h"
#include "vr/util/di/container.h"
#include "vr/util/env.h"

#include "vr/test/files.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................
//............................................................................
namespace test_
{

struct market_data_consumer final: public mc::steppable_<mc::rcu<_reader_>>, public util::di::component
{
    market_data_consumer (uint64_t const seed) :
        m_rnd { seed }
    {
        dep (m_mdf) = "mdf";
    }

    ~market_data_consumer ()
    {
        LOG_info << "consumed " << m_pos << " byte(s) in total, checksum: " << hex_string_cast (m_checksum);
    }

    // steppable:

    VR_ASSUME_HOT void step () final override
    {
        // ... logic that does not need to read 'm_md' ...

        rcu_read_lock (); // [no-op on x86]
        {
            vr_static_assert (market_data_feed::poll_descriptor::width () == 1);

            market_data_feed::poll_descriptor const & pd = m_mdf->poll ();

            int32_t const len = (pd [0].m_pos - m_pos);
            if (len > 0)
            {
                DLOG_trace2 << this << " [recv'd " << m_pos << " @ " << print_timestamp (pd [0].m_ts_local) << "]: consuming " << len << " byte(s) ...";

                // touch every byte to trigger any segfaults/sigbuses/etc:

                uint8_t const * data = static_cast<uint8_t const *> (addr_plus (pd [0].m_end, -/* ! */len));
                m_checksum = util::crc32 (data, len, m_checksum);

                check_le (m_ts_local_last, pd [0].m_ts_local, m_pos);

                m_pos = pd [0].m_pos;
                m_ts_local_last = pd [0].m_ts_local;
            }
        }
        rcu_read_unlock (); // [no-op on x86]

        // ... logic that does not need to read 'm_md' ...

        // randomized delay:

        if (test::next_random<uint64_t, double> (m_rnd) < 0.05)
        {
            rcu_thread_offline ();
            {
                sys::short_sleep_for (30 * _1_microsecond ());
            }
            rcu_thread_online ();
        }
        else
        {
            mc::pause (test::next_random (m_rnd) & 0xFFFF);
        }
    }


    market_data_feed const * m_mdf { };  // [dep]

    io::pos_t m_pos { };
    timestamp_t m_ts_local_last { -1 };
    uint64_t m_rnd;
    uint32_t m_checksum { 1 };

}; // end of class

} // end of 'test_'
//............................................................................
//............................................................................
/*
 * @see 'ASX_market_data_view_test.capture_replay'
 * @see 'socket_link_test.raw_mcast'
 */
TEST (integration_market_data, multiple_consumers)
{
    using namespace test_;

    uint64_t const seed = test::env::random_seed<uint64_t> ();

    // HACK find a date for which capture exists and set it as session date:

    fs::path const test_input = test::find_capture (source::ASX, "<"_rop, util::current_date_in ("Australia/Sydney"));
    LOG_info << "using test data in " << print (test_input);

    util::date_t const date = util::extract_date (test_input.native ());

    settings cfg
    {
        { "app_cfg", {
            { "time", util::format_time (util::ptime_t { date, pt::seconds (0) }, "%Y-%b-%d %H:%M:%S") }
        }}
        ,
        { "mock_server", {
            { "itch", {
                { "ifc", "lo" },
                { "packet_begin",   3000000 },
                { "packet_limit",   3050000 },
                { "cap_root", util::getenv<fs::path> ("VR_CAP_ROOT", "").native () } // TODO
            }}
        }}
        ,
        { "thread_pool", {
            { "rcu", {
                { "use_RT_callback", true },
                { "callback_PU", -1 },
            }}
        }}
        ,
        { "market_data_feed", {
            { "ifc", "lo" },
            { "tsp", "hw_fallback_to_sw" }, // note: more permissive mode than prod
            { "sources", "203.0.119.212->233.71.185.8, 233.71.185.9, 233.71.185.10, 233.71.185.11, 233.71.185.12" }
        }}
    };

    int32_t const PU_default    = 0;
    int32_t const PU_mdf        = 1;
    int32_t const PU_c0         = 2;
    int32_t const PU_c1         = 3;

    util::di::container app { join_as_name ("APP", test::current_test_name ()),
        {
            { "default",        PU_default },

            { "server.itch",    "default" },
            { "mdf",            PU_mdf },
            { "c0",             PU_c0 },
            { "c1",             PU_c1 },
            { "c2",             "c1" },
        }
    };

    app.configure ()
        ("config",      new rt::app_cfg { cfg })

        ("server.itch", new mock_mcast_server { "/mock_server/itch" })

        ("threads",     new mc::thread_pool   { "/thread_pool" })
        ("mdf",         new market_data_feed  { "/market_data_feed" })

        ("c0",          new market_data_consumer { (seed * 1) })
        ("c1",          new market_data_consumer { (seed * 3) })
        ("c2",          new market_data_consumer { (seed * 5) })
    ;

    try
    {
        app.start ();
        {
            app.run_for (VR_IF_THEN_ELSE (VR_DEBUG)(10, 30) * _1_second ());
        }
        app.stop ();
    }
    catch (std::exception const & e)
    {
        LOG_warn << exc_info (e);
        throw; // re-throw
    }

    // TODO this test does not guarantee that all consumers will get a chance to consume
    // the exact same data because of the stopping race at the end (a consumer may get stopped
    // before poll()ing the last packet, etc)

//    market_data_consumer const & c0 = app ["c0"];
//    market_data_consumer const & c1 = app ["c1"];
//    market_data_consumer const & c2 = app ["c2"];
//
//    EXPECT_EQ (c0.m_checksum, c1.m_checksum);
//    EXPECT_EQ (c0.m_checksum, c2.m_checksum);
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
