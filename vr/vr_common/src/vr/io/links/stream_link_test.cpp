
#include "vr/io/files.h"
#include "vr/io/links/stream_link.h"
#include "vr/io/streams.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

template<typename LINK_TYPE, typename BUFFER_TAG>
struct link_factory;

template<typename LINK_TYPE>
struct link_factory<LINK_TYPE, _ring_>
{
    static std::unique_ptr<LINK_TYPE> create (std::istream & is, capacity_t const capacity)
    {
        return std::make_unique<LINK_TYPE> (is, recv_arg_map { { "capacity", capacity } });
    }

    static std::unique_ptr<LINK_TYPE> create (std::ostream & os, capacity_t const capacity)
    {
        return std::make_unique<LINK_TYPE> (os, send_arg_map { { "capacity", capacity } });
    }

}; // end of specialization

template<typename LINK_TYPE>
struct link_factory<LINK_TYPE, _tape_>
{
    template<typename S>
    static std::unique_ptr<LINK_TYPE> create (S & s, capacity_t const capacity)
    {
        using arg_type      = typename util::if_t<std::is_base_of<std::istream, S>::value, recv_arg_map, send_arg_map>;

        fs::path const file { test::unique_test_path () };
        io::create_dirs (file.parent_path ());

        return std::make_unique<LINK_TYPE> (s, arg_type { { "capacity", capacity },
            { "file", file },
            { "w_window", static_cast<window_t> (std::min<window_t> (2 * 1024, capacity / 2)) } });
    }

}; // end of specialization
//............................................................................

using buffer_tags        = gt::Types
<
    _ring_,
    _tape_
>;

template<typename T> struct stream_link_test: public gt::Test { };
TYPED_TEST_CASE (stream_link_test, buffer_tags);

} // end of anonymous
//............................................................................
//............................................................................

TYPED_TEST (stream_link_test, recv)
{
    using buffer_tag        = TypeParam; // test parameter

    using recv_link         = stream_link<recv<buffer_tag> >; // TODO vary other aspects as well (move recv<...> into the type param)

    vr_static_assert (recv_link::link_mode () == mode::recv);

    vr_static_assert (! recv_link::has_state ());
    vr_static_assert (! recv_link::has_ts_last_recv ());
    vr_static_assert (! recv_link::has_ts_last_send ());


    constexpr int32_t size  = 10000009;
    std::unique_ptr<int8_t []> const array { boost::make_unique_noinit<int8_t []> (size) };

    int64_t const seed = test::env::random_seed<int64_t> (); // memorize the seed for replaying the data stream
    int64_t rnd = seed;

    // populate 'array':

    for (int64_t i = 0; i < size; ++ i)
    {
        array [i] = static_cast<int8_t> (test::next_random (rnd));
    }

    // read 'array' data via a recv link (with varied capacities) and validate:

    for (capacity_t const capacity : { 109, 4096, 10009, size - 1, size, size + 1 })
    {
        rnd = seed; // replay

        {
            array_istream</* TODO vary */> is { array.get (), size };
            std::unique_ptr<recv_link> const rl_ptr = link_factory<recv_link, buffer_tag>::create (is, capacity);

            recv_link & rl = * rl_ptr;

            // state after construction:

            if (recv_link::has_ts_last_recv ()) { ASSERT_EQ (rl.ts_last_recv (), 0); }

            int32_t spin = unsigned_cast (rnd) % 4;

            for (int64_t rc_total = 0; rc_total < size; )
            {
                std::pair<addr_const_t, capacity_t> const rc = rl.recv_poll ();
                LOG_trace1 << "  recv_poll rc.second: " << rc.second;

                if (rc.second > 0)
                {
                    if (recv_link::has_ts_last_recv ()) { ASSERT_GT (rl.ts_last_recv (), 0); }

                    if (-- spin < 0) // simulate not flushing after every poll
                    {
                        int8_t const * const r = static_cast<int8_t const *> (rc.first);

                        for (int32_t i = 0; i < rc.second; ++ i)
                        {
                            test::next_random (rnd);

                            ASSERT_EQ (r [i], static_cast<int8_t> (rnd)) << "failed at " << (rc_total + i);
                        }

                        rc_total += rc.second;
                        rl.recv_flush (rc.second);

                        spin = unsigned_cast (rnd) % 4;
                    }
                }
            }
        }
    }
}

TYPED_TEST (stream_link_test, send)
{
    using buffer_tag        = TypeParam; // test parameter

    using send_link         = stream_link<send<buffer_tag> >; // TODO vary other aspects as well (move send<...> into the type param)

    vr_static_assert (send_link::link_mode () == mode::send);

    vr_static_assert (! send_link::has_state ());
    vr_static_assert (! send_link::has_ts_last_recv ());
    vr_static_assert (! send_link::has_ts_last_send ());


    constexpr int32_t size  = 10000009;
    std::unique_ptr<int8_t []> const array { boost::make_unique_noinit<int8_t []> (size) };

    int64_t const seed = test::env::random_seed<int64_t> (); // memorize the seed for replaying the data stream
    int64_t rnd = seed;

    // fill 'array' with known data via a recv link (with varied capacities) and validate:

    for (capacity_t const capacity : { 109, 4096, 10009, size - 1, size, size + 1 })
    {
        rnd = seed; // replay

        {
            std::memset (array.get (), 0, size); // reset

            array_ostream</* TODO vary */> os { array.get (), size };
            std::unique_ptr<send_link> const sl_ptr = link_factory<send_link, buffer_tag>::create (os, capacity);

            send_link & sl = * sl_ptr;

            // state after construction:

            if (send_link::has_ts_last_send ()) { ASSERT_EQ (sl.ts_last_send (), 0); }

            int32_t spin = unsigned_cast (rnd) % 4;
            capacity_t wc_pending { };

            for (int64_t wc_total = 0; wc_total + wc_pending < size; ) // this testcase doesn't need to output precisely 'size' bytes, but just to stay consistent with the 'recv' version
            {
                if (! std::is_same<_ring_, buffer_tag>::value || (sl.send_w_window () > 0)) // [only relevant for '_ring_'] if no write space left, flush first
                {
                    int32_t const len = 1 + unsigned_cast (rnd) % std::min<int64_t> (size - wc_total - wc_pending, sl.send_w_window ());

                    int8_t * const w = static_cast<int8_t *> (sl.send_allocate (len));
                    wc_pending += len;

                    {
                        ASSERT_TRUE (w != nullptr);

                        for (int64_t i = 0; i < len; ++ i)
                        {
                            w [i] = static_cast<int8_t> (test::next_random (rnd));
                        }
                    }
                }

                if (-- spin < 0) // simulate not flushing after every allocate
                {
                    auto const rc = sl.send_flush (wc_pending);
                    LOG_trace1 << "  send_flush rc: " << rc;

                    ASSERT_EQ (rc, wc_pending) << "failed to send " << wc_pending << " byte(s)";

                    if (send_link::has_ts_last_send ()) { ASSERT_GT (sl.ts_last_send (), 0); }

                    wc_total += rc;
                    wc_pending = 0;

                    spin = unsigned_cast (rnd) % 4;
                }
            }
            if (wc_pending > 0) sl.send_flush (wc_pending);
        }


        // validate 'array' contents:

        rnd = seed; // replay

        for (int32_t i = 0; i < size; ++ i)
        {
            int64_t const v = test::next_random (rnd);

            ASSERT_EQ (array [i], static_cast<int8_t> (v)) << "failed at " << i;
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
