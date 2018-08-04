
#include "vr/rt/tracing/tracing_ctl.h"

#include "vr/io/files.h"
#include "vr/settings.h"
#include "vr/sys/cpu.h"
#include "vr/sys/os.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"

#include "vr/test/tracepoints.h"
#include "vr/test/utility.h"

#include <boost/thread/thread.hpp>

#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf/iterator.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................
//............................................................................
namespace
{

struct trace_event_emitter
{
    trace_event_emitter (int64_t const begin, int64_t const end) :
        m_begin { begin },
        m_end { end }
    {
    }

    void operator() ()
    {
        for (int64_t i = m_begin; i < m_end; ++ i)
        {
            tracepoint (vr_test, event_NOTICE, i, i, i, (i & 0x3), i, i);
        }
    }

    int64_t const m_begin;
    int64_t const m_end;

}; // end of class
//............................................................................

using bt_context_ptr        = std::unique_ptr<::bt_context, void (*) (::bt_context *)>;
using bt_iter_ptr           = std::unique_ptr<::bt_ctf_iter, void (*) (::bt_ctf_iter *)>;

struct trace_stats
{
    int64_t m_events { }; // this counts all events without distinction
    int64_t m_discarded_events { };
};

trace_stats
count_trace_events (fs::path const & path)
{
    bt_context_ptr const ctx_ptr { ::bt_context_create (), ::bt_context_put };
    ::bt_context * const ctx = ctx_ptr.get ();

    // find and all all traces below 'path':

    int32_t trace_count { };

    for (fs::directory_entry const & e : fs::recursive_directory_iterator { path })
    {
        if (fs::is_directory (e.status ()) && ("index" == e.path ().filename ()))
        {
            int32_t const thid = ::bt_context_add_trace (ctx, e.path ().parent_path ().c_str (), "ctf", nullptr, nullptr, nullptr);
            check_nonnegative (thid);

            timestamp_t const ts_utc_begin = ::bt_trace_handle_get_timestamp_begin (ctx, thid, BT_CLOCK_REAL);
            timestamp_t const ts_utc_end = ::bt_trace_handle_get_timestamp_end (ctx, thid, BT_CLOCK_REAL);

            LOG_trace1 << "added a trace handle ID " << thid << " [" << ::bt_trace_handle_get_path (ctx, thid) << ']';
            LOG_trace2 << "  contains events in UTC time range [" << util::print_timestamp_as_ptime (ts_utc_begin) << ", " << util::print_timestamp_as_ptime (ts_utc_end) << ']';

            ++ trace_count;
        }
    }
    check_positive (trace_count);

    ::bt_iter_pos begin { };
    {
        begin.type = BT_SEEK_BEGIN;
    }

    bt_iter_ptr const iter_ptr { ::bt_ctf_iter_create (ctx, & begin, nullptr), ::bt_ctf_iter_destroy }; // note: at most one of these can exist per context
    ::bt_ctf_iter * const iter { iter_ptr.get () };
    check_nonnull (iter);

    // iterate over matching (TODO) events and count them:

    trace_stats r { };

    for (::bt_ctf_event const * e; (e = ::bt_ctf_iter_read_event (iter)) != nullptr; ++ r.m_events)
    {
        auto const discarded = ::bt_ctf_get_lost_events_count (iter);
        if (VR_UNLIKELY (discarded > 0))
        {
            r.m_discarded_events += discarded;
            LOG_trace2 << "  discarded = " << discarded;
        }

//        string_literal_t e_name = ::bt_ctf_event_name (e);
//        LOG_trace2 << static_cast<addr_const_t> (e_name);

        ::bt_iter_next (::bt_ctf_get_iter (iter));
    }

    return r;
}

} // end of anonymous
//............................................................................
//............................................................................
// TODO
// - selectively enabling events by name

TEST (tracing_ctl, main_thread)
{
    std::string const out_dir_pattern { (test::temp_dir () / test::current_test_name () / "trace.%Y%m%d").string () };
    fs::path out_dir { };

    for (int64_t subbuf_size : { 16 * 1024L, 512 * 1024L })
    {
        {
            util::di::container app { join_as_name ("APP", test::current_test_name ()) };

            app.configure ()
                ("tracing",     new tracing_ctl { {
                                                    { "session", test::current_test_name () },
                                                    { "out_dir", out_dir_pattern },
                                                    { "subbuf.size", subbuf_size },
                                                    { "events",
                                                        {
                                                            { { "name", "vr_test:*" } }
                                                        }
                                                    }
                                                } }
                )
            ;

            constexpr int64_t emit_count    = 100009;

            app.start ();
            {
                for (int64_t i = 0; i < emit_count; ++ i)
                {
                    tracepoint (vr_test, event_NOTICE, i, i, i, (i & 0x3), i, i);
                }
            }
            app.stop ();

            tracing_ctl const & tracing = app ["tracing"];

            auto const & loss_stats = tracing.loss_stats ();

            EXPECT_EQ (loss_stats.m_lost_packets, 0); // expected in 'discard' subbuf mode

            out_dir = std::get<1> (tracing.session ());
            trace_stats const tst = count_trace_events (out_dir);

            // check that event counts (emitted and discarded) match between what the tracer reported
            // and what was read independently from the trace log files:

            EXPECT_EQ (loss_stats.m_discarded_events, tst.m_discarded_events) << "failed with subbuf size " << subbuf_size;
            LOG_info << "all event (present + discarded) count from the trace: " << (loss_stats.m_discarded_events + tst.m_events);
            ASSERT_EQ (loss_stats.m_discarded_events + tst.m_events, emit_count) << "failed with subbuf size " << subbuf_size;

        } // 'app' destructs

        io::clean_dir (out_dir, true);
    }
}
//............................................................................

TEST (tracing_ctl, multicore)
{
    std::string const out_dir_pattern { (test::temp_dir () / test::current_test_name () / "trace.%Y%m%d").string () };
    fs::path out_dir { };

    for (int64_t subbuf_size : { 16 * 1024L, 512 * 1024L })
    {
        {
            util::di::container app { join_as_name ("APP", test::current_test_name ()) };

            app.configure ()
                ("tracing",     new tracing_ctl { {
                                                    { "session", test::current_test_name () },
                                                    { "out_dir", out_dir_pattern },
                                                    { "subbuf.size", subbuf_size },
                                                    { "events",
                                                        {
                                                            { { "name", "vr_test:*" } }
                                                        }
                                                    }
                                                } }
                )
            ;

            constexpr int64_t emit_count    = 1000009;
            const int32_t concurrency       = sys::cpu_info::instance ().count (sys::hw_obj::core) - 2;

            LOG_info << "[concurrency = " << concurrency << ']';

            app.start ();
            {
                std::vector<boost::thread> threads { };

                int64_t step = (emit_count + concurrency - 1) / concurrency; // TODO randomize this more, for less load symmetry

                for (int32_t t = 0; t < concurrency; ++ t)
                {
                    threads.emplace_back (trace_event_emitter { t * step, std::min ((t + 1) * step, emit_count) });
                }

                for (int32_t t = 0; t < concurrency; ++ t)
                {
                    threads [t].join ();
                }
            }
            app.stop ();

            tracing_ctl const & tracing = app ["tracing"];

            auto const & loss_stats = tracing.loss_stats ();

            EXPECT_EQ (loss_stats.m_lost_packets, 0); // expected in 'discard' subbuf mode

            out_dir = std::get<1> (tracing.session ());
            trace_stats const tst = count_trace_events (out_dir);

            // check that event counts (emitted and discarded) match between what the tracer reported
            // and what was read independently from the trace log files:

            EXPECT_EQ (loss_stats.m_discarded_events, tst.m_discarded_events) << "failed with subbuf size " << subbuf_size;
            LOG_info << "all event (present + discarded) count from the trace: " << (loss_stats.m_discarded_events + tst.m_events);
            ASSERT_EQ (loss_stats.m_discarded_events + tst.m_events, emit_count) << "failed with subbuf size " << subbuf_size;

        } // 'app' destructs

        io::clean_dir (out_dir, true);
    }
}
//............................................................................

TEST (tracing_ctl, loglevels)
{
    std::string const out_dir_pattern { (test::temp_dir () / test::current_test_name () / "trace.%Y%m%d").string () };
    fs::path out_dir { };

    // TODO expose "trace_event_loglevel" without lttng #includes

    for (std::string loglevel : { "emerg", "alert", "notice", "info", "all" })
    {
        {
            util::di::container app { join_as_name ("APP", test::current_test_name ()) };

            app.configure ()
                ("tracing",     new tracing_ctl { {
                                                    { "session", test::current_test_name () },
                                                    { "out_dir", out_dir_pattern },
                                                    { "events",
                                                        {
                                                            { { "name", "vr_test:*" }, { "loglevel", loglevel } }
                                                        }
                                                    }
                                                } }
                )
            ;

            constexpr int64_t loop_length   = 1009;

            app.start ();
            {
                for (int64_t i = 0; i < loop_length; ++ i)
                {
                    tracepoint (vr_test, event_INFO, i, i, (i & 0x3));

                    for (int32_t r = 0; r < 2; ++ r)
                    {
                        tracepoint (vr_test, event_NOTICE, i, i, i, (i & 0x3), i, i);
                    }
                    for (int32_t r = 0; r < 4; ++ r)
                    {
                        tracepoint (vr_test, event_ALERT, i, i);
                    }
                }
            }
            app.stop ();

            tracing_ctl const & tracing = app ["tracing"];

            auto const & loss_stats = tracing.loss_stats ();

            EXPECT_EQ (loss_stats.m_lost_packets, 0); // expected in 'discard' subbuf mode

            out_dir = std::get<1> (tracing.session ());
            trace_stats const tst = count_trace_events (out_dir);

            // check that event counts (emitted and discarded) match between what the tracer reported
            // and what was read independently from the trace log files:

            EXPECT_EQ (loss_stats.m_discarded_events, tst.m_discarded_events) << "failed with loglevel " << print (loglevel);

            int64_t emit_count { };
            switch (str_hash_32 (loglevel))
            {
                case "emerg"_hash:      emit_count = 0; break; // none enabled
                case "alert"_hash:      emit_count = (4) * loop_length; break; // only 'event_ALERT'
                case "notice"_hash:     emit_count = (2 + 4) * loop_length; break; // 'event_NOTICE', 'event_ALERT'

                case "info"_hash:
                case "all"_hash:        emit_count = (1 + 2 + 4) * loop_length; break; // all three

            } // end of switch

            LOG_info << '[' << loglevel << "] all event (present + discarded) count from the trace: " << (loss_stats.m_discarded_events + tst.m_events);
            ASSERT_EQ (loss_stats.m_discarded_events + tst.m_events, emit_count) << "failed with loglevel " << print (loglevel);

        } // 'app' destructs

        io::clean_dir (out_dir, true);
    }
}

TEST (tracing_ctl, clock_plugin)
{
    std::string const out_dir_pattern { (test::temp_dir () / test::current_test_name () / "trace.%Y%m%d").string () };
    fs::path out_dir { };

    {
        {
            util::di::container app { join_as_name ("APP", test::current_test_name ()) };

            app.configure ()
                ("tracing",     new tracing_ctl { {
                                                    { "session", test::current_test_name () },
                                                    { "out_dir", out_dir_pattern },
                                                    { "events",
                                                        {
                                                            { { "name", "vr_test:*" } }
                                                        }
                                                    }
                                                } }
                )
            ;

            constexpr int64_t emit_count    = 10009;

            app.start ();
            {
                for (int64_t i = 0; i < emit_count; ++ i)
                {
                    tracepoint (vr_test, event_NOTICE, i, /* ts_local */sys::realtime_utc (), i, (i & 0x3), i, i);
                }
            }
            app.stop ();

            tracing_ctl const & tracing = app ["tracing"];

            auto const & loss_stats = tracing.loss_stats ();

            EXPECT_EQ (loss_stats.m_lost_packets, 0); // expected in 'discard' subbuf mode

            out_dir = std::get<1> (tracing.session ());
            trace_stats const tst = count_trace_events (out_dir);
            ASSERT_EQ (loss_stats.m_discarded_events + tst.m_events, emit_count);

            // TODO read CTF metadata and verify
            // TODO calc stats on (CTC timestamp - ts_local)

        } // 'app' destructs

        io::clean_dir (out_dir, true);
    }
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
