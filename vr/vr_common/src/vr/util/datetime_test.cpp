
#include "vr/util/datetime.h"
#include "vr/sys/os.h"

#include "vr/util/logging.h"

#if VR_USE_BOOST_LOCALE
#   include <boost/locale.hpp>
#endif // VR_USE_BOOST_LOCALE

#include "vr/test/utility.h"

#include <sys/time.h>
#include <time.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (to_ptime, round_trip)
{
    // test for no overflow as long as we stay below 'timestamp_max ()':
    {
        ptime_t const now = to_ptime (sys::realtime_utc ());
        LOG_info << "UTC time: " << now;
        ptime_t const fut { { 2035, now.date ().month (), now.date ().day () }, now.time_of_day () };

        timestamp_t const fut_ts = to_timestamp (fut);
        ptime_t const fut_pt = to_ptime (fut_ts);

        LOG_trace1 << "fut: " << fut << ", fut_ts: " << fut_ts << ", fut_pt: " << fut_pt;

        EXPECT_EQ (fut_pt, fut);
    }
    {
        timestamp_t const fut_ts = 2063210368447950654L;

        ptime_t const fut_pt = to_ptime (fut_ts);
        timestamp_t const ts = to_timestamp (fut_pt);

        LOG_trace1 << "fut_ts: " << fut_ts << ", fut_pt: " << fut_pt << ", ts: " << ts;

        EXPECT_EQ (fut_pt.time_of_day ().total_microseconds (), 64768447950/*000*/); // nanoseconds get truncated, not rounded to nearest

        EXPECT_EQ (fut_ts - ts, VR_IF_THEN_ELSE (VR_BOOST_DATE_TIME_NANOSECONDS)(0, 654)) << "fut_ts: " << fut_ts << ", ts: " << ts;
    }
}
//............................................................................
/*
 * vary 'digits_secs' arg to 'print_timestamp_as_ptime()', verify correct parsing
 */
TEST (print_timestamp_as_ptime, round_trip)
{
    timestamp_t const ts_master = 2063210368447950654L;

    int64_t factor { 1000000000 };

    for (int32_t digits_secs = 0; digits_secs <= 9; ++ digits_secs, factor /= 10)
    {
        timestamp_t const ts = (ts_master / factor) * factor;

        std::stringstream ss;
        print_timestamp_as_ptime (ss, ts, digits_secs);

        std::string const ts_str = ss.str ();
        LOG_info << ts_str;

        timestamp_t const ts2 = parse_ptime_as_timestamp (ts_str.data (), ts_str.size ());
        LOG_trace1 << ts2;

        EXPECT_EQ (ts2, ts) << "failed for " << digits_secs << " sec digits";
    }
}
//............................................................................

TEST (format_date, sniff) // TODO test output string
{
    date_t const d = current_date_local ();
    LOG_info << "formatted date string: " << print (format_date (d, ">%Y%m%d<"));
}

TEST (format_time, sniff) // TODO test output string
{
    ptime_t const t = current_time_local ();
    LOG_info << "formatted time string: " << print (format_time (t, ">>%Y%m%d.%H%M%S<<"));
}
//............................................................................
/*
 * vary 'digits_secs' arg to 'print_timestamp_as_ptime()', verify correct parsing
 */
TEST (parse_HHMMSS, sniff)
{
    string_literal_t const s { "235959123456789" };

    {
        int32_t const digits_secs = 0;
        timestamp_t const ts_expected = 1000L * (pt::hours { 23 } + pt::minutes { 59 } + pt::seconds { 59 }).total_microseconds ();

        EXPECT_EQ (parse_HHMMSS (s, digits_secs), ts_expected);
    }
    {
        int32_t const digits_secs = 6;
        timestamp_t const ts_expected = 1000L * (pt::hours { 23 } + pt::minutes { 59 } + pt::seconds { 59 } + pt::microseconds { 123456 }).total_microseconds ();

        EXPECT_EQ (parse_HHMMSS (s, digits_secs), ts_expected);
    }
    {
        int32_t const digits_secs = 9;
        timestamp_t const ts_expected = 789 + 1000L * (pt::hours { 23 } + pt::minutes { 59 } + pt::seconds { 59 } + pt::microseconds { 123456 }).total_microseconds ();

        EXPECT_EQ (parse_HHMMSS (s, digits_secs), ts_expected);
    }
}
//............................................................................

TEST (parse_duration_as_timestamp, sniff)
{
    {
        std::string const s { "10:11:12.123456789" };

        timestamp_t const ts_expected = (10 * 3600 + 11 * 60 + 12) * _1_second () + 123456789;
        timestamp_t const ts = parse_duration_as_timestamp (s.data (), s.size ());

        EXPECT_EQ (ts, ts_expected);

        {
            timestamp_t const ts_expected2 = (10 * 3600 + 11 * 60 + 12) * _1_second () + 123000000;
            timestamp_t const ts2 = parse_duration_as_timestamp (s.data (), s.size () - 6); // only 3 fraction digits

            ASSERT_NE (ts2, ts);
            EXPECT_EQ (ts2, ts_expected2);
        }
    }
}
//............................................................................

TEST (extract_date, sniff)
{
    {
        std::string const s { "19901001adfasdfa" };

        date_t const d = extract_date (s);
        date_t const d_expected { 1990, 10, 1 };

        EXPECT_EQ (d, d_expected);
    }
    {
        std::string const s { "adfasdfa19901001" };

        date_t const d = extract_date (s);
        date_t const d_expected { 1990, 10, 1 };

        EXPECT_EQ (d, d_expected);
    }
    {
        std::string const s { "19901001" };

        date_t const d = extract_date (s);
        date_t const d_expected { 1990, 10, 1 };

        EXPECT_EQ (d, d_expected);
    }

    {
        std::string const s { "919901001" }; // '9' can't start an acceptable date

        date_t const d = extract_date (s);
        date_t const d_expected { 1990, 10, 1 };

        EXPECT_EQ (d, d_expected);
    }

    // invalid input:
    {
        std::string const s { "9901001" }; // '9' can't start an acceptable date

        date_t const d = extract_date (s);
        date_t const d_expected { gd::not_a_date_time };

        EXPECT_EQ (d, d_expected);
    }
}
//............................................................................
#if VR_USE_BOOST_LOCALE

TEST (locale, icu_backend)
{
    namespace bl    = boost::locale;

    bool have_icu { false };

    auto const backends = bl::localization_backend_manager::global ().get_all_backends ();
    for (std::string const & name : backends)
    {
        if (name == "icu")
        {
            have_icu = true;
            break;
        }
    }

    EXPECT_TRUE (have_icu);
}

#endif // VR_USE_BOOST_LOCALE
//............................................................................

TEST (tzs_equivalent, sniff)
{
    EXPECT_TRUE (tzs_equivalent ("Asia/Tokyo", "Asia/Tokyo"));
    EXPECT_FALSE (tzs_equivalent ("Asia/Tokyo", "Australia/Sydney"));
}
//............................................................................

TEST (tz_offset, NY_2012)
{
    date_t const dA { 2012, 3, 10 }; // before DST start
    EXPECT_EQ (NY_tz_offset (dA), -5 * 3600 * _1_second ());

    date_t const dB { 2012, 3, 12 }; // after DST start
    EXPECT_EQ (NY_tz_offset (dB), -4 * 3600 * _1_second ());

    date_t const dC { 2012, 11, 3 }; // before DST end
    EXPECT_EQ (NY_tz_offset (dC), -4 * 3600 * _1_second ());

    date_t const dD { 2012, 11, 5 }; // after DST end
    EXPECT_EQ (NY_tz_offset (dD), -5 * 3600 * _1_second ());
}

TEST (tz_offset, NY_2017)
{
    date_t const dA { 2017, 3, 11 }; // before DST start
    EXPECT_EQ (NY_tz_offset (dA), -5 * 3600 * _1_second ());

    date_t const dB { 2017, 3, 13 }; // after DST start
    EXPECT_EQ (NY_tz_offset (dB), -4 * 3600 * _1_second ());

    date_t const dC { 2017, 11, 4 }; // before DST end
    EXPECT_EQ (NY_tz_offset (dC), -4 * 3600 * _1_second ());

    date_t const dD { 2017, 11, 6 }; // after DST end
    EXPECT_EQ (NY_tz_offset (dD), -5 * 3600 * _1_second ());
}
//............................................................................

TEST (tz_offset, misc)
{
    date_t const d { 2017, 11, 14 }; // manually fixed date

    EXPECT_EQ (tz_offset (d, "UTC"), 0);

    EXPECT_EQ (tz_offset (d, "Asia/Tokyo"), 9 * 3600 * _1_second ());
    EXPECT_EQ (tz_offset (d, "Australia/Sydney"), 11 * 3600 * _1_second ());

    std::string const & tz = local_tz ();
    LOG_info << "local tz: " << print (tz) << ", UTC offset on " << d << ": " << std::showpos << tz_offset (d, tz) / (3600 * _1_second ());

    // invalid input:

    EXPECT_THROW (tz_offset (d, ""), invalid_input);
    EXPECT_THROW (tz_offset (d, "Australia/SydneyBAD"), invalid_input);
}
//............................................................................
/*
 * TODO test more than just lack of exceptions
 */
TEST (current_date_in, sniff)
{
    date_t const d_utc { current_date_in ("UTC") };

    LOG_info << "current UTC date: " << d_utc << " UTC";

    {
        date_t const d_jap { current_date_in ("Asia/Tokyo") };
        date_t const d_aus { current_date_in ("Australia/Sydney") };

        LOG_info << "current dates: " << d_jap << " Asia/Tokyo, " << d_aus << " Australia/Sydney";
    }
}
/*
 * TODO test more than just lack of exceptions
 */
TEST (current_time_in, sniff)
{
    ptime_t const t_utc { current_time_in ("UTC") };

    LOG_info << "current UTC time: " << t_utc << " UTC";

    {
        ptime_t const t_jap { current_time_in ("Asia/Tokyo") };
        ptime_t const t_aus { current_time_in ("Australia/Sydney") };

        LOG_info << "current times: " << t_jap << " Asia/Tokyo, " << t_aus << " Australia/Sydney";
    }
}
//............................................................................

// TODO finish
// - print_* fns relying on stream facets -- check tz correctness

TEST (clocks, DISABLED_sanity)
{
    timestamp_t ts_gtod { };
    timestamp_t ts_cgt_rt { };
    timestamp_t ts_lt { };

    char lt_buf [128];

    {
        ::timeval tv;
        ::gettimeofday (& tv, nullptr); // also used by 'pt::microsec_clock::universal_time ()'

        ::timespec ts;
        ::clock_gettime (CLOCK_REALTIME, & ts);

        ::time_t t = ::time (nullptr);
        ::tm * const lt = ::localtime (& t);
        ts_lt = ::mktime (lt) * _1_second ();

        ::strftime (lt_buf, sizeof (lt_buf), "%Y-%m-%d %H:%M:%S", lt);

        ts_gtod = tv.tv_sec * _1_second () + tv.tv_usec * _1_microsecond ();
        ts_cgt_rt = ts.tv_sec * _1_second () + ts.tv_nsec;
    }

    LOG_info << "localtime(): " << lt_buf;

    LOG_info << "\n  ts_gtod:\t"  << ts_gtod << " (" << print_timestamp (ts_gtod) << " UTC)"
             << "\nts_cgt_rt:\t"  << ts_cgt_rt << " (" << print_timestamp (ts_cgt_rt) << " UTC)"
             << "\n    ts_lt:\t"  << ts_lt << " (" << print_timestamp (ts_lt) << ")";
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
