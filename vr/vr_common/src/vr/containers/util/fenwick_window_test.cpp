
#include "vr/containers/util/fenwick_window.h"
#include "vr/meta/integer.h"
#include "vr/sys/os.h"

#include "vr/test/utility.h"

#include <cmath>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (fenwick_window, steady_state)
{
    // for simplicity, create a window with bin resolution of 1

    constexpr int64_t window    = 32;
    constexpr int32_t bin_count = window;

    using window_counter    = fenwick_window<timestamp_t, fenwick_window_options<window, int16_t, bin_count>>;

    window_counter wc { 0 };

    LOG_info << "sizeof (window_counter) = " << sizeof (window_counter);

    timestamp_t t;

    for (t = 0; t < 3 * window + 10; ++ t)
    {
        int32_t const count = wc.add (t);
        LOG_trace1 << "[t: " << t << "]: count = " << count;

        if (t < window)
        {
            ASSERT_EQ (count, t + 1);
        }
        else
        {
            ASSERT_EQ (count, window); // sliding window with all bins holding value 1
        }
    }

    // make a time jump that's greater than 'window':
    {
        t += window + 5;
        int32_t const count = wc.add (t, 2);

        ASSERT_EQ (count, 2); // all older values have slid out of the window
    }

    for (int32_t i = 0; i < 2 * window + 10; ++ i)
    {
        ++ t;
        int32_t const count = wc.add (t, 2);
        LOG_trace1 << "[t: " << t << "]: count = " << count;

        if (i > window) // eventually reach the same steady state as in the loop above, but all bin values are 2
        {
            ASSERT_EQ (count, 2 * window); // sliding window with all bins holding value 2
        }
    }
}
//............................................................................
/*
 * sim a realistic use case: do a speculative add and if the count is greater than threshold, roll it back
 */
TEST (fenwick_window, undo)
{
    constexpr timestamp_t window    = (_1_nanosecond () << meta::static_log2_ceil<_1_second ()>::value);
    constexpr int32_t bin_count     = 64;

    using window_counter    = fenwick_window<timestamp_t, fenwick_window_options<window, int16_t, bin_count>>;

    timestamp_t const ts_start = sys::realtime_utc ();

    window_counter wc { ts_start };

    LOG_info << "window = " << window << " ns";
    LOG_info << "sizeof (window_counter) = " << sizeof (window_counter);

    uint64_t rnd = test::env::random_seed<uint64_t> (); // note: unsigned

    // ~Poisson arrivals with a rate equal to the limit rate:

    int64_t const samples = 1000000;

    int32_t const arrival_avg = ((50.0 * window) / _1_second ());
    double const alpha = (_1_second () / arrival_avg);

    int32_t limit = 2 * arrival_avg;
    LOG_info << "window limit: " << limit;


    timestamp_t ts = ts_start;
    int32_t count_prev = wc.count ();

    for (int32_t i = 0; i < samples; ++ i)
    {
        int32_t const inc = 1 + (test::next_random (rnd) & 0b10); // average 2

        int32_t count = wc.add (ts, inc);
        DLOG_trace2 << "[i: " << i << ", inc: " << inc << "]: count = " << count;

        if (count > limit) // "undo" (see class doc on the semantics)
        {
            count = wc.add (ts, - inc);
            DLOG_trace1 << "[i: " << i << ", inc: " << inc << "]: UNDONE down to " << count;

            ASSERT_LE (count, count_prev) << "[i: " << i << "] failed to undo increment of " << inc << " for time " << ts;
        }
        count_prev = count;

        timestamp_t const delta = std::floor (- alpha * std::log (1.0e-8 + test::next_random<uint64_t, double> (rnd)));
        ts += delta;
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
