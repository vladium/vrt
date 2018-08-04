
#include "vr/mc/lf_spsc_buffer.h"
#include "vr/mc/spinflag.h"
#include "vr/util/intrinsics.h"

#include "vr/test/mc.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................
//............................................................................
namespace
{

using stop_flag             = mc::spinflag<true>;

//............................................................................

struct record
{
    int64_t m_i64;
    int32_t m_i32;

}; // end of class

struct record2
{
    int64_t m_seqnum;
    uint64_t m_data [6];
    uint32_t m_chksum;

}; // end of class
//............................................................................

template<typename BUFFER, int64_t PAUSE>
struct buffer_consumer
{
    buffer_consumer (BUFFER & buf, int64_t const repeats, stop_flag & sf, int64_t const seed) :
        m_buf { buf },
        m_repeats { repeats },
        m_seed { seed },
        m_stop_flag { sf }
    {
    }

    void operator() ()
    {
        uint64_t rnd = m_seed;

        int64_t r { };
        try
        {
            while (! m_stop_flag.is_raised () && (r < m_repeats))
            {
                if (PAUSE > 0) mc::pause (test::next_random (rnd) % PAUSE); // a random pause, if enabled

                auto rc = m_buf.try_dequeue ();
                if (rc)
                {
                    record2 const & v = rc;
                    {
                        check_eq (v.m_seqnum, r);

                        uint32_t chksum { 1 };
                        for (uint64_t d : v.m_data)
                        {
                            chksum = util::i_crc32 (chksum, d);
                        }
                        check_eq (v.m_chksum, chksum);
                    }

                    if (PAUSE > 0) mc::pause (test::next_random (rnd) % PAUSE); // a random pause, if enabled

                    ++ r;
                }
            }
        }
        catch (...)
        {
            m_stop_flag.raise ();
            m_failure = std::current_exception ();
        }
        m_r_completed = r;
        LOG_info << "consumer DONE [completed: " << r << ']';
    }

    BUFFER & m_buf;
    int64_t const m_repeats;
    int64_t const m_seed;
    int64_t m_r_completed { };
    std::exception_ptr m_failure { };
    stop_flag & m_stop_flag;

}; // end of class

template<typename BUFFER, int64_t PAUSE>
struct buffer_producer
{
    buffer_producer (BUFFER & buf, int64_t const repeats, stop_flag & sf, int64_t const seed) :
        m_buf { buf },
        m_repeats { repeats },
        m_seed { seed },
        m_stop_flag { sf }
    {
    }

    void operator() ()
    {
        uint64_t rnd = m_seed;

        int64_t r { };
        try
        {
            while (! m_stop_flag.is_raised () && (r < m_repeats))
            {
                if (PAUSE > 0) mc::pause (test::next_random (rnd) % PAUSE); // a random pause, if enabled

                auto rc = m_buf.try_enqueue ();
                if (rc)
                {
                    record2 & v = rc;
                    {
                        v.m_seqnum = r;

                        uint32_t chksum { 1 };
                        for (uint64_t & d : v.m_data)
                        {
                            d = test::next_random (rnd);
                            chksum = util::i_crc32 (chksum, d);
                        }
                        v.m_chksum = chksum;
                    }

                    if (PAUSE > 0) mc::pause (test::next_random (rnd) % PAUSE); // a random pause, if enabled

                    if (! PAUSE || (test::next_random<uint64_t, double> (rnd) > 0.001)) // small random chance of aborting the op
                    {
                        rc.commit ();
                        ++ r;
                    }
                }
            }
        }
        catch (...)
        {
            m_stop_flag.raise ();
            m_failure = std::current_exception ();
        }
        m_r_completed = r;
        LOG_info << "producer DONE [completed: " << r << ']';
    }

    BUFFER & m_buf;
    int64_t const m_repeats;
    int64_t const m_seed;
    int64_t m_r_completed { };
    std::exception_ptr m_failure { };
    stop_flag & m_stop_flag;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

TEST (lf_spsc_buffer, sniff)
{
    constexpr int32_t capacity  = 32;
    using buffer_type           = lf_spsc_buffer<record, capacity>;

    vr_static_assert (sizeof (buffer_type) == (2 + capacity) * sys::cpu_info::cache::static_line_size ()); // impl detail, but kind of important to check

    buffer_type buf { };

    ASSERT_EQ (buf.size (), 0);

    // empty queue dequeue should fail:

    for (int32_t i = 0; i < 3; ++ i)
    {
        auto dr = buf.try_dequeue ();
        ASSERT_FALSE (dr);
    }

    // enqueue a value:
    {
        auto er = buf.try_enqueue ();
        ASSERT_TRUE (er);
        {
            record & v = er;
            {
                v.m_i64 = -1L;
                v.m_i32 = 12345;
            }
        }
        er.commit ();
    }
    ASSERT_EQ (buf.size (), 1);

    // aborted enqueue changes nothing:
    {
        auto er = buf.try_enqueue ();
        ASSERT_TRUE (er);
        {
            record & v = er;
            {
                v.m_i64 = -2L;
                v.m_i32 = 333;
            }
        }
    }
    ASSERT_EQ (buf.size (), 1);

    // dequeue what should be the first value:
    {
        auto dr = buf.try_dequeue ();
        ASSERT_TRUE (dr);
        {
            record const & v = dr;
            {
                EXPECT_EQ (v.m_i64, -1L);
                EXPECT_EQ (v.m_i32, 12345);
            }
        }
    }
    ASSERT_EQ (buf.size (), 0); // empty again

    // 'capacity' enqueues should succeed:

    for (int32_t i = 0; i < capacity; ++ i)
    {
        auto er = buf.try_enqueue ();
        ASSERT_TRUE (er);
        {
            record & v = er;
            {
                v.m_i64 = - i;
                v.m_i32 = i;
            }
        }
        er.commit ();
    }
    ASSERT_EQ (buf.size (), capacity); // full

    // full queue enqueue should fail and have no state change:

    for (int32_t i = 0; i < 3; ++ i)
    {
        auto dr = buf.try_enqueue ();
        ASSERT_FALSE (dr);
    }
    ASSERT_EQ (buf.size (), capacity); // still full

    // dequeue all values:

    for (int32_t i = 0; i < capacity; ++ i)
    {
        auto dr = buf.try_dequeue ();
        ASSERT_TRUE (dr);
        {
            record const & v = dr;
            {
                EXPECT_EQ (v.m_i64, - i);
                EXPECT_EQ (v.m_i32, i);
            }
        }
    }
    ASSERT_EQ (buf.size (), 0); // empty again

    // empty queue dequeue should fail:

    for (int32_t i = 0; i < 3; ++ i)
    {
        auto dr = buf.try_dequeue ();
        ASSERT_FALSE (dr);
    }
}
//............................................................................
//............................................................................

template
<
    int32_t CAPACITY,
    int64_t C_PAUSE,
    int64_t P_PAUSE
>
struct scenario
{
    static constexpr int32_t capacity ()    { return CAPACITY; }
    static constexpr int64_t c_pause ()     { return C_PAUSE; }
    static constexpr int64_t p_pause ()     { return P_PAUSE; }

}; // end of scenario

#define vr_CAPACITY_SEQ     (1)(8)(128)
#define vr_PAUSE_SEQ        (0)(0x010)(0x0100)

#define vr_SCENARIO(r, product) ,scenario< BOOST_PP_SEQ_ENUM (product) >

using scenarios         = gt::Types
<
    scenario<32, 0, 0>
    BOOST_PP_SEQ_FOR_EACH_PRODUCT (vr_SCENARIO, (vr_CAPACITY_SEQ)(vr_PAUSE_SEQ)(vr_PAUSE_SEQ))
>;

#undef vr_SCENARIO
#undef vr_PAUSE_SEQ
#undef vr_CAPACITY_SEQ

//............................................................................
//............................................................................

template<typename T> struct lf_spsc_buffer_test: public gt::Test { };
TYPED_TEST_CASE (lf_spsc_buffer_test, scenarios);

/*
 * vary scenarios:
 *  - different capacities (including non-prod case of high-contention)
 *  - slow/fast or fast/slow consumer/producer, respectively
 */
TYPED_TEST (lf_spsc_buffer_test, multicore)
{
    using scenario              = TypeParam; // test parameters

    constexpr int32_t capacity  = scenario::capacity ();

    using buffer_type           = lf_spsc_buffer<record2, capacity>;

    buffer_type buf { };

    using consumer_task         = buffer_consumer<buffer_type, scenario::c_pause ()>;
    using producer_task         = buffer_producer<buffer_type, scenario::p_pause ()>;

    int32_t const PU_C          = 2;
    int32_t const PU_P          = 3;

    int64_t const repeats       = (capacity == 1 ? 10000 : 500000); // don't make high-contention scenarios too lengthy
    int64_t const seed          = test::env::random_seed<int64_t> ();

    stop_flag sf { }; // cl-padded

    test::task_container tasks { };

    tasks.add ({ consumer_task { buf, repeats, sf, seed }, PU_C }, "consumer");
    tasks.add ({ producer_task { buf, repeats, sf, seed }, PU_P }, "producer");

    tasks.start ();
    tasks.stop ();

    consumer_task const & c = tasks ["consumer"];
    producer_task const & p = tasks ["producer"];

    bool ok { true };

    if (c.m_failure)
    {
        try
        {
            std::rethrow_exception (c.m_failure);
        }
        catch (std::exception const & e)
        {
            LOG_warn << "consumer failure: " << exc_info (e);
        }
    }
    else
    {
        EXPECT_EQ (c.m_r_completed, repeats);
    }

    if (p.m_failure)
    {
        try
        {
            std::rethrow_exception (p.m_failure);
        }
        catch (std::exception const & e)
        {
            LOG_warn << "producer failure: " << exc_info (e);
        }
    }
    else
    {
        EXPECT_EQ (p.m_r_completed, repeats);
    }

    if (! ok) ADD_FAILURE () << "scenario<" << capacity << ", " << scenario::c_pause () << ", " << scenario::p_pause () << "> failed";
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
