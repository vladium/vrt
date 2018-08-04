
#include "vr/stats/stream_summary.h"

#include "vr/test/random.h"
#include "vr/test/utility.h"

#include <cmath>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

using tested_value_types  = gt::Types<int32_t, int64_t>;

template<typename T> struct stream_summary_test: gt::Test {};
TYPED_TEST_CASE (stream_summary_test, tested_value_types);

} // end of anonymous
//............................................................................
//............................................................................

TEST (stream_summary_test, sniff)
{
    using summary_type          = stream_summary<int32_t>;

    summary_type ss { 128 };

    ASSERT_EQ (ss.size (), 0);
    ASSERT_EQ (ss.begin (), ss.end ());

    int32_t const values [] { 123, 456, 789, -123, -456, -789 }; // all unique

    ss.update (values, length (values));

    // validate (non-lossy) 'ss':

    for (int32_t repeat = 0; repeat < 2; ++ repeat)
    {
        ASSERT_EQ (ss.size (), signed_cast (length (values)));

        summary_type::count_type total_monitored_count { };
        for (auto const & item : ss)
        {
            ++ total_monitored_count;

            ASSERT_EQ (item.count (), 1) << "wrong count for value " << item.value ();
        }
        EXPECT_EQ (total_monitored_count, signed_cast (length (values)));
    }
}
//............................................................................

TEST (stream_summary, non_lossy)
{
    using summary_type          = stream_summary<int32_t>;

    for (int32_t repeat = 0; repeat < 2; ++ repeat)
    {
        summary_type ss { 128 };

        std::vector<int32_t> values;

        switch (repeat)
        {
            case 0: values.insert (values.end (), { 123, 456, 789, 456, 789, 789 }); break; // heavier hitters later in the stream
            case 1: values.insert (values.end (), { 789, 789, 456, 789, 456, 123 }); break; // heavier hitters earlier in the stream

        } // end of switch

        summary_type::count_type const size = values.size ();

        ss.update (& values [0], size);

        // validate (non-lossy) 'ss':

        ASSERT_EQ (ss.size (), 3); // 3 unique items in 'values'

        summary_type::count_type total_monitored_count { };
        summary_type::count_type count_prev = std::numeric_limits<summary_type::count_type>::max ();

        for (auto const & item : ss)
        {
            total_monitored_count += item.count ();

            // expected counts:

            switch (item.value ())
            {
                case 123: EXPECT_EQ (item.count (), 1); break;
                case 456: EXPECT_EQ (item.count (), 2); break;
                case 789: EXPECT_EQ (item.count (), 3); break;

                default: ADD_FAILURE () << "unexpected value " << item.value (); break;

            } // end of switch

            // non-lossy count errors should all be zero:

            ASSERT_EQ (item.count_error (), 0);

            // iteration is in the order of decreasing frequency counts:

            ASSERT_GE (count_prev, item.count ());
            count_prev = item.count ();
        }
        EXPECT_EQ (total_monitored_count, size);
    }
}
//..........................................................................

TEST (stream_summary_test, edge_cases)
{
    using value_type            = int32_t;
    using summary_type          = stream_summary<value_type>;

    // a single unique item:
    {
        for (summary_type::size_type max_size : { 1, 109, 128, 1009 })
        {
            summary_type ss { max_size };

            auto const size  = 3 * max_size;
            std::vector<value_type> values (size, 16661); // fill with one value

            ss.update (& values [0], size);

            ASSERT_EQ (ss.size (), 1);

            summary_type::count_type total_monitored_count { };
            for (auto const & item : ss)
            {
                total_monitored_count += item.count ();
                ASSERT_EQ (item.value (), 16661);
            }
            ASSERT_EQ (total_monitored_count, size);
        }
    }
    // number of unique items equal to the summary's max_size:
    {
        for (summary_type::size_type max_size : { 1, 109, 128, 1009 })
        {
            summary_type ss { max_size };

            auto const size = 3 * max_size;
            std::vector<value_type> values;

            value_type rnd { test::env::random_seed<value_type> () };

            // fill 'values' with all ints in [0, max_size) range:

            for (int32_t i = 0; i < max_size; ++ i) values.push_back (i); // this pass ensures all of them are present at least once
            for (int32_t i = max_size; i < size; ++ i) values.push_back (unsigned_cast (util::xorshift (rnd)) % max_size);

            ss.update (& values [0], size);

            ASSERT_EQ (ss.size (), max_size); // by construction of 'values'

            summary_type::count_type total_monitored_count { };
            summary_type::count_type count_prev { std::numeric_limits<summary_type::count_type>::max () };

            for (auto const & item : ss)
            {
                total_monitored_count += item.count ();

                // non-lossy count errors should all be zero:

                ASSERT_EQ (item.count_error (), 0);

                // count ordering:

                ASSERT_GE (count_prev, item.count ());
                count_prev = item.count ();
            }
            ASSERT_EQ (total_monitored_count, size);
        }
    }
}
//..........................................................................

TYPED_TEST (stream_summary_test, uniform_data_pdf)
{
    using value_type    = TypeParam; // testcase parameter

    using summary_type  = stream_summary<value_type>;
    using size_type     = typename summary_type::size_type;
    using count_type    = typename summary_type::count_type;

    using histogram_type    = boost::unordered_map<value_type, count_type>; // check structure for non-lossy counting

    // number of unique items <= summary's max_size:
    {
        const int32_t folds = 10; // meant to be ceil(N/m)

        for (size_type max_size : { 109, 128, 1009, 16661 })
        {
            summary_type ss { max_size };

            auto const size = folds * max_size;

            std::vector<value_type> values;
            histogram_type h { };

            auto const range = 2 * max_size;

            // fill 'values' with uniform ints in [0, range):

            value_type rnd { test::env::random_seed<value_type> () * 3 };

            for (int32_t i = 0; i < size; ++ i)
            {
                value_type const x = unsigned_cast (util::xorshift (rnd)) % range;

                values.push_back (x);
                ++ h.emplace (x, 0).first->second;
            }

            ss.update (& values [0], size);

            ASSERT_LE (ss.size (), max_size);

            count_type total_monitored_count {};
            count_type count_prev { std::numeric_limits<count_type>::max () };

            for (auto const & item : ss)
            {
                total_monitored_count += item.count ();

                // count errors are strictly less than counts:

                ASSERT_LT (item.count_error (), item.count ());

                // count - count_error is a guaranteed number of hits for a value:

                count_type const exact_count = h.find (item.value ())->second;

                ASSERT_LE (item.count () - item.count_error (), exact_count);

                // count ordering:

                ASSERT_GE (count_prev, item.count ());
                count_prev = item.count ();
            }
            ASSERT_EQ (total_monitored_count, size);
        }
    }
}
//..........................................................................

TYPED_TEST (stream_summary_test, skewed_data_pdf)
{
    using value_type    = TypeParam; // testcase parameter

    using summary_type  = stream_summary<value_type>;
    using size_type     = typename summary_type::size_type;
    using count_type    = typename summary_type::count_type;

    using histogram_type    = boost::unordered_map<value_type, count_type>; // check structure for non-lossy counting

    // number of unique items equal to the summary's max_size:
    {
        const int32_t folds = 10; // meants to be ceil(N/m)

        for (size_type max_size : { 109, 128, 1009, 16661 })
        {
            summary_type ss { max_size };

            auto const size = folds * max_size;

            value_type rnd { test::env::random_seed<value_type>() * 7 };

            std::vector<value_type> values;
            histogram_type h { };

            double const alpha = static_cast<double> (max_size) / 3;

            // fill 'values' with ~exponentially distributed values with scale 'alpha':

            for (int32_t i = 0; i < size; ++ i)
            {
                // TODO is there an issue here due to 'value_type' being signed?
                value_type const x = std::floor (- alpha * std::log (1.0e-8 + test::next_random<value_type, double> (rnd)));

                values.push_back (x);
                ++ h.emplace (x, 0).first->second;
            }

            ss.update (& values [0], size);

            ASSERT_LE (ss.size (), max_size);

            count_type total_monitored_count { };
            count_type count_prev { std::numeric_limits<count_type>::max () };

            for (auto const & item : ss)
            {
                total_monitored_count += item.count ();

                // count errors are strictly less than counts:

                ASSERT_LT (item.count_error (), item.count ());

                // count - count_error is a guaranteed number of hits for a value:

                count_type const exact_count = h.find (item.value ())->second;

                ASSERT_LE (item.count () - item.count_error (), exact_count);

                // count ordering:

                ASSERT_GE (count_prev, item.count ());
                count_prev = item.count ();
            }
            ASSERT_EQ (total_monitored_count, size);


            // for an exponential distribution, by the time the pdf falls off to phi * pdf(0), the probability mass
            // captured is 1 - phi;

            // verify this (approximately; how close we get to theory depends on how much of the exp
            // tail is lost with a given ratio (max_size / alpha)):

            double const phi = 0.1;
            count_type const threshold = std::floor (phi / alpha * size);

            count_type prob_mass { };
            count_type prob_mass_exact { };
            int32_t frequent_count { };

            for (auto const & item : ss)
            {
                count_type const gcount = item.count () - item.count_error ();
                count_type const count_exact = h.find (item.value ())->second;

                if (gcount >= threshold)
                {
                    prob_mass += gcount;
                    ++ frequent_count;
                }
                if (count_exact >= threshold) prob_mass_exact += count_exact;
            }

            LOG_info << '[' << frequent_count << "] approx cdf = " << static_cast<double> (prob_mass) / size << ", exact cdf = " << static_cast<double> (prob_mass_exact) / size
                     << " (asymptotic: " << (1.0 - phi) << ')';
        }
    }
}

} // end of namespace
//----------------------------------------------------------------------------
