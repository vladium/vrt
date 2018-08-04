
#include "vr/util/timer_event_queue.h"

#include "vr/test/utility.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (timer_event_queue, with_payload)
{
    char values [] { 'A', 'B', 'C' };
    using value_type    = char;

    using queue_type    = timer_event_queue<value_type>;
    using entry_type    = queue_type::entry_type;

    queue_type q { };
    ASSERT_TRUE (q.empty ());

    entry_type const * e = q.front ();
    ASSERT_TRUE (e == nullptr);

    q.enqueue (1, values [1]);
    ASSERT_FALSE (q.empty ());
    q.enqueue (0, values [0]);
    q.enqueue (2, values [2]);

    for (int32_t i = 0; i < 3; ++ i)
    {
        e = q.front ();
        ASSERT_TRUE (e);
        EXPECT_EQ (e->timestamp (), i);
        ASSERT_EQ (e->value (), values [i]);

        LOG_trace1 << '[' << i << "] priority = " << field<_priority_> (* e);

        q.dequeue ();
    }
    EXPECT_TRUE (q.empty ());
}

TEST (timer_event_queue, no_payload)
{
    using queue_type    = timer_event_queue<>;
    using entry_type    = queue_type::entry_type;

    queue_type q { };
    ASSERT_TRUE (q.empty ());

    entry_type const * e = q.front ();
    ASSERT_TRUE (e == nullptr);

    for (timestamp_t ts = 0; ts < 109; ++ ts)
    {
        q.enqueue (ts);
        ASSERT_FALSE (q.empty ());
    }

    for (int32_t i = 0; i < 109; ++ i)
    {
        e = q.front ();
        ASSERT_TRUE (e);
        EXPECT_EQ (e->timestamp (), i);

        LOG_trace2 << '[' << i << "] priority = " << field<_priority_> (* e);

        q.dequeue ();
    }
    EXPECT_TRUE (q.empty ());
}
//............................................................................

TEST (timer_event_queue, randomized_unique)
{
    using value_type    = std::string;

    using queue_type    = timer_event_queue<value_type>;
    using entry_type    = queue_type::entry_type;

    const int32_t rounds    = 1000;
    const int32_t op_count  = 100000;

    timestamp_t rnd = test::env::random_seed<timestamp_t> (); // a series of values without duplicates

    queue_type q { 1000 }; // use non-default pool capacity

    std::vector<int32_t> const rs = test::random_range_split (op_count, rounds, rnd);

    for (int32_t const r : rs)
    {
        LOG_trace1 << "round length " << r;

        ASSERT_TRUE (q.empty ());
        ASSERT_TRUE (q.front () == nullptr);

        std::vector<timestamp_t> ts_ordered { };

        // add a random number of (unique) randomized keys:

        for (int32_t i = 0; i < r; ++ i)
        {
            timestamp_t const ts = test::next_random (rnd);
            ts_ordered.push_back (ts);

            q.enqueue (ts, string_cast (ts));
        }

        std::sort (ts_ordered.begin (), ts_ordered.end ()); // expected dequeue order

        for (int32_t i = 0; i < r; ++ i)
        {
            timestamp_t const ts = ts_ordered [i];

            entry_type const * const e = q.front ();
            ASSERT_TRUE (e);

            ASSERT_EQ (string_cast (e->timestamp ()), e->value ()) << "failed for r = " << r << ", i = " << i;

            ASSERT_EQ (e->timestamp (), ts) << "failed for r = " << r << ", i = " << i;

            q.dequeue ();
        }
    }
}

TEST (timer_event_queue, randomized_duplicate)
{
    using value_type    = void;

    using queue_type    = timer_event_queue<value_type>;
    using entry_type    = queue_type::entry_type;

    const int32_t rounds    = 1000;
    const int32_t op_count  = 10000;

    timestamp_t rnd = test::env::random_seed<timestamp_t> (); // a series of values without duplicates

    queue_type q { };

    std::vector<int32_t> const rs = test::random_range_split (op_count, rounds, rnd);

    for (int32_t const r : rs)
    {
        LOG_trace1 << "round length " << r;

        ASSERT_TRUE (q.empty ());
        ASSERT_TRUE (q.front () == nullptr);

        boost::unordered_map<timestamp_t, int32_t> dup_counts { };

        // add a random number of randomized keys with duplicates:

        timestamp_t const ts_start = test::next_random (rnd);

        for (int32_t i = 0; i < r; ++ i)
        {
            timestamp_t const ts = ts_start + unsigned_cast (test::next_random (rnd) % 8);

            auto const rc = dup_counts.emplace (ts, 0);
            ++ rc.first->second;

            q.enqueue (ts);
        }

        boost::unordered_map<timestamp_t, int32_t> dup_counts_2 { };

        for (int32_t i = 0; i < r; ++ i)
        {
            entry_type const * const e = q.front ();
            ASSERT_TRUE (e);

            auto const rc = dup_counts_2.emplace (e->timestamp (), 0);
            ++ rc.first->second;

            q.dequeue ();
        }

        ASSERT_EQ (dup_counts_2, dup_counts);
    }
}
//............................................................................

TEST (timer_event_queue, reschedule)
{
    using value_type    = char;

    using queue_type    = timer_event_queue<value_type>;
    using entry_type    = queue_type::entry_type;

    const int32_t repeats   = 10000;

    queue_type q { };

    uint32_t rnd = test::env::random_seed<uint32_t> (); // note: unsigned

    timestamp_t ts_A = rnd;
    timestamp_t delta = test::next_random (rnd);
    timestamp_t ts_B = ts_A + delta;

    // start queue as 'A' <-delta-> 'B':

    q.enqueue (ts_A, 'A');
    q.enqueue (ts_B, 'B');

    // invalid input guards:

#if VR_DEBUG

    ASSERT_THROW (q.reschedule_front (-1), invalid_input);

#endif // VR_DEBUG

    // do a loop of rescheduling the front entry to jump over the next one:

    bool turn { true };

    for (int32_t i = 0; i < repeats; ++ i)
    {
        entry_type const * const first = q.front ();
        ASSERT_TRUE (first);

        if (turn)
        {
            ASSERT_EQ (first->value (), 'A');
        }
        else
        {
            ASSERT_EQ (first->value (), 'B');
        }

        timestamp_t const delta2 = test::next_random (rnd);
        entry_type const & rescheduled = q.reschedule_front (first->timestamp () + delta + delta2);
        ASSERT_EQ (& rescheduled, first); // same entry reused

        delta = delta2;
        turn = ! turn;
    }

    // similar loop but using the other reschedule method:

    for (int32_t i = 0; i < repeats; ++ i)
    {
        entry_type * const first = q.front ();
        ASSERT_TRUE (first);

        if (turn)
        {
            ASSERT_EQ (first->value (), 'A');
        }
        else
        {
            ASSERT_EQ (first->value (), 'B');
        }

        timestamp_t const delta2 = test::next_random (rnd);
        q.reschedule (* first, first->timestamp () + delta + delta2);

        delta = delta2;
        turn = ! turn;
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
