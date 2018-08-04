
#include "vr/stats/stream_stats.h"

#include "vr/util/logging.h"

#include "vr/test/random.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace stats
{
//............................................................................

TEST (stream_stats, sniff) // TODO play with TSC data
{
    stream_stats<double> ss { { 0.25, 0.5, 0.75, 0.99, 0.999, 0.9999 } };

    ASSERT_EQ (ss.size (), 6);

    uint64_t rnd { test::env::random_seed<uint64_t> () };

    int64_t const sample_count  = 1000000;

    for (int64_t i = 0; i < sample_count; ++ i)
    {
        ss (test::next_random<uint64_t, double> (rnd)); // runif (0, 1)
    }

    ASSERT_EQ (signed_cast (ss.count ()), sample_count);

    for (int32_t p = 0; p < ss.size (); ++ p)
    {
        LOG_info << "quantile[" << p << "]:\t" << ss [p];

        EXPECT_GE (ss [p], 0.0);
    }
}

} // end of 'stats'
} // end of namespace
//----------------------------------------------------------------------------
