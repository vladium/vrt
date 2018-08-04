
#include "vr/io/files.h"
#include "vr/io/streams.h"
#include "vr/io/stream_factory.h"
#include "vr/io/uri.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"
#include "vr/utility.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
/*
 * return next random variate (used as [un]compressible data source)
 */
template<typename I>
VR_FORCEINLINE I
next_variate (bool const compressible, I & rnd)
{
    test::next_random (rnd);

    return (compressible ? (rnd % 1000) : rnd);
}

} // end of anonymous
//............................................................................
//............................................................................

using exception_scenarios       = gt::Types
<
    io::exceptions,
    io::no_exceptions
>;
//............................................................................

template<typename T> struct stream_utility: public gt::Test { };
TYPED_TEST_CASE (stream_utility, exception_scenarios);

TYPED_TEST (stream_utility, read_until_eof)
{
    using exc_policy            = TypeParam; // test parameter

    fs::path const file { test::unique_test_path () };
    io::create_dirs (file.parent_path ());

    int32_t const io_size   = 16661;

    {
        std::ofstream out { file.c_str (), default_ostream_mode () };

        std::unique_ptr<char []> const data { boost::make_unique_noinit<char []> (io_size) };
        for (int32_t i = 0; i < io_size; ++ i) data [i] = i & 0xFF;

        out.write (data.get (), io_size);
    }

    for (int32_t buf_size : { 1, 109, io_size, 3 * io_size })
    {
        std::ifstream in { file.c_str (), default_istream_mode () };
        setup_istream (in, exc_policy { });

        std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) };

        std::stringstream out;
        int64_t const rc = read_until_eof (in, buf.get (), buf_size, out); // must not throw regardless of 'exc_policy'

        ASSERT_EQ (rc, io_size) << "failed for buf_size = " << buf_size;
        std::string const out_str = out.str ();
        ASSERT_EQ (signed_cast (out_str.size ()), io_size);
    }
}

TYPED_TEST (stream_utility, read_fully)
{
    using exc_policy            = TypeParam; // test parameter

    fs::path const file { test::unique_test_path () };
    io::create_dirs (file.parent_path ());

    int32_t const io_size   = 16661;

    {
        std::ofstream out { file.c_str (), default_ostream_mode () };

        std::unique_ptr<char []> const data { boost::make_unique_noinit<char []> (io_size) };
        for (int32_t i = 0; i < io_size; ++ i) data [i] = i & 0xFF;

        out.write (data.get (), io_size);
    }

    {
        std::ifstream in { file.c_str (), default_istream_mode () };
        setup_istream (in, exc_policy { });

        std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (io_size) };

        int64_t const rc = read_fully (in, io_size, buf.get ()); // must not throw regardless of 'exc_policy'

        ASSERT_EQ (rc, io_size);

        for (int32_t i = 0; i < io_size; ++ i)
        {
            char const c_expected = i & 0xFF;
            ASSERT_EQ (buf [i], c_expected) << "failed at offset i = " << i;
        }
    }
}
//............................................................................

template<typename T> struct istream_length_test: public gt::Test { };
TYPED_TEST_CASE (istream_length_test, exception_scenarios);

TYPED_TEST (istream_length_test, sniff)
{
    using exc_policy            = TypeParam; // test parameter

    // an iostream determines its length from how much has been written so far:
    {
        std::stringstream is { };
        setup_istream (is, exc_policy { });

        EXPECT_EQ (istream_length (is), 0);

        is.write ("abcd", 4);
        EXPECT_EQ (istream_length (is), 4);

        char drain [4];
        is.read (drain, 4);

        EXPECT_EQ (istream_length (is), 4);

        is.read (drain, 1); // attempt to read past EOF
        EXPECT_EQ (istream_length (is), -1);
    }
    // 'array_istream' is constructed with the knowledge of its source buffer size:
    {
        char buf [3] { 0 };
        array_istream<exc_policy> is { buf };

        EXPECT_EQ (istream_length (is), 3);

        char drain [3];
        is.read (drain, 3);

        EXPECT_EQ (istream_length (is), 3);

        is.read (drain, 1); // attempt to read past EOF
        EXPECT_EQ (istream_length (is), -1);
    }
    {
        fs::path const file { test::unique_test_path () };
        io::create_dirs (file.parent_path ());

        {
            fd_ostream<no_exceptions> os { file };
            os.write ("abcd", 4);
        }

        fd_istream<exc_policy> is { file };

        EXPECT_EQ (istream_length (is), 4);

        char drain [4];
        is.read (drain, 4);

        EXPECT_EQ (istream_length (is), 4);

        is.read (drain, 1); // attempt to read past EOF
        EXPECT_EQ (istream_length (is), -1);
    }
}
//............................................................................
//............................................................................

template
<
    compression::enum_t CF,
    bool COMPRESSIBLE,
    typename EXC_POLICY
>
struct compression_scenario
{
    static constexpr compression::enum_t cf     = CF;
    static constexpr bool compressible          = COMPRESSIBLE;
    using exc_policy                            = EXC_POLICY;

}; // end of scenario

#define vr_SCENARIO(r, params) ,compression_scenario<compression:: BOOST_PP_SEQ_ELEM (0, params), BOOST_PP_SEQ_ELEM (1, params), BOOST_PP_SEQ_ELEM (2, params) >

using compression_scenarios     = gt::Types
<
    compression_scenario<compression::gzip, true, exceptions>
    BOOST_PP_SEQ_FOR_EACH_PRODUCT (vr_SCENARIO, (BOOST_PP_SEQ_TAIL (VR_IO_COMPRESSION_SEQ)) ((false)(true)) ((exceptions)(no_exceptions)))
>;

#undef vr_SCENARIO

//............................................................................

template<typename T> struct compression_streams: public gt::Test { };
TYPED_TEST_CASE (compression_streams, compression_scenarios);


TYPED_TEST (compression_streams, round_trip_file)
{
    using scenario              = TypeParam; // test parameters

    constexpr compression::enum_t cf    = scenario::cf;
    constexpr bool compressible         = scenario::compressible;
    using exc_policy                    = typename scenario::exc_policy;

    using out_stream                    = typename stream_for<cf>::template ostream<std::ofstream, exc_policy>;
    using in_stream                     = typename stream_for<cf>::template istream<std::ifstream, exc_policy>;


    int64_t val;

    fs::path const file { test::unique_test_path (compression::file_extension [cf]) };
    io::create_dirs (file.parent_path ());

    for (int32_t size = 1; size < 1050000; size = (size < 1024 ? size + 1 : size * 2 + 1))
    {
        LOG_trace1 << "writing " << size << (compressible ? " compressible" : " random") << " values ...";

        int64_t const seed = (test::env::random_seed<int64_t> () | size); // memorize the seed for replaying the data stream
        int64_t rnd = seed;

        {
            out_stream out { file.c_str () };

            for (int32_t r = 0; r < size; ++ r)
            {
                val = next_variate (compressible, rnd);

                out.write (char_ptr_cast (& val), sizeof (val));
            }
        }

        // 'guess_compression ()':
        {
            compression::enum_t const guessed_cf = guess_compression (file, /* check content */true);
            EXPECT_EQ (guessed_cf, cf);
        }
        // support by 'stream_factory':
        {
            std::unique_ptr<std::istream> const in = stream_factory::open_input (file);
            ASSERT_TRUE (in);
        }

        rnd = seed; // replay data

        {
            in_stream in { file.c_str () };

            int32_t count;
            for (count = 0; in.read (char_ptr_cast (& val), sizeof (val)); ++ count)
            {
                int64_t const val2 = next_variate (compressible, rnd);

                ASSERT_EQ (val, val2) << "[size: " << size << ", compressible: " << compressible << "] failed for value #" << count;
            }
            ASSERT_EQ (size, count);
        }
    }
}
//............................................................................

// TODO edge cases of 'N' or 'size' being zero

template<typename T> struct array_streams: public gt::Test { };
TYPED_TEST_CASE (array_streams, exception_scenarios);

TYPED_TEST (array_streams, round_trip)
{
    using exc_policy            = TypeParam; // test parameter

    union
    {
        int32_t rnd;
        char buf;
    };

    for (int32_t size = 1; size < 10000; size = (size < 1024 ? size + 1 : size * 2 + 1))
    {
        LOG_trace1 << "writing " << size << " bytes ...";

        std::unique_ptr<int8_t []> const array { boost::make_unique_noinit<int8_t []> (size) };

        int32_t const seed = (test::env::random_seed<int32_t> () | size);
        rnd = seed;

        {
            array_ostream<exc_policy> out { array.get (), size };

            for (int32_t r = 0; r < size; ++ r)
            {
                out.write (& buf, 1); // note: a single byte write

                test::next_random (rnd);
            }
        }
        {
            array_istream<exc_policy> in { array.get (), size };

            union
            {
                int32_t rnd2;
                char buf2;
            };

            rnd2 = seed;

            int32_t count;
            for (count = 0; in.read (& buf, 1); ++ count) // note: single byte reads
            {
                ASSERT_EQ (buf, buf2) << "failed for byte #" << count;

                test::next_random (rnd2);
            }
            EXPECT_EQ (size, count);
        }
    }
}


using size_scenarios        = gt::Types
<
    util::int_<int32_t, 1>,
    util::int_<int32_t, 10>,
    util::int_<int32_t, 109>
>;

template<typename T> struct static_sized_array_streams: public gt::Test { };
TYPED_TEST_CASE (static_sized_array_streams, size_scenarios);

TYPED_TEST (static_sized_array_streams, round_trip)
{
    const int32_t size          = TypeParam::value; // test parameter

    union
    {
        int32_t rnd;
        char buf;
    };

    {
        LOG_trace1 << "writing " << size << " bytes ...";

        int8_t array [size];

        int32_t const seed = (test::env::random_seed<int32_t> () | size);
        rnd = seed;

        {
            array_ostream<> out { array }; // constructor that takes a compile-time sized array

            for (int32_t r = 0; r < size; ++ r)
            {
                out.write (& buf, 1); // note: a single byte write

                test::next_random (rnd);
            }
        }
        {
            array_istream<> in { array }; // constructor that takes a compile-time sized array

            union
            {
                int32_t rnd2;
                char buf2;
            };

            rnd2 = seed;

            int32_t count;
            for (count = 0; in.read (& buf, 1); ++ count) // note: single byte reads
            {
                ASSERT_EQ (buf, buf2) << "failed for byte #" << count;

                test::next_random (rnd2);
            }
            EXPECT_EQ (size, count);
        }
    }
}
//............................................................................

TEST (eol_streams, output)
{
    std::stringstream ss { };
    eol_ostream eol_ss { ss };

    ASSERT_EQ (& ss, & eol_ss.os ()); // retains a reference

#define vr_STR "\nline 1\r\nline 2\rline 3\n\n"

    eol_ss << vr_STR << std::flush;

    std::unique_ptr<char []> buf { std::make_unique<char []> (256) };

    int32_t const len = read_fully (ss, 256, buf.get ());
    ASSERT_EQ (len, 18 + 2 + 2 + 2 + 4);

    // validate that all EOLs have been "canonicalized":

    int32_t eol_count { } ;
    for (int32_t i = 0; i < len; ++ i)
    {
        if (buf [i] == '\n')
        {
            ASSERT_TRUE ((i > 0) && (buf [i - 1] == '\r')) << "i: " << i << ", prev char: " << buf [i - 1];
            ++ eol_count;
        }
    }

    EXPECT_EQ (eol_count, 5);

#undef vr_STR
}
//............................................................................

template<typename T> struct fd_streams: public gt::Test { };
TYPED_TEST_CASE (fd_streams, exception_scenarios);

TYPED_TEST (fd_streams, fd_scope)
{
    using exc_policy            = TypeParam; // test parameter

    fs::path const file { test::unique_test_path () };
    io::create_dirs (file.parent_path ());


    // constructors taking an 'fs::path' always own the descriptor:
    {
        int32_t fd { -1 };
        {
            fd_ostream<exc_policy> out { file };
            fd = out.fd ();

            EXPECT_TRUE (io::fd_valid (fd));
        }
        EXPECT_FALSE (io::fd_valid (fd));

        compression::enum_t const sf = guess_compression (file, /* check content */true);
        EXPECT_EQ (sf, compression::none);


        fd = -1;
        {
            fd_istream<exc_policy> in { file };
            fd = in.fd ();

            EXPECT_TRUE (io::fd_valid (fd));
        }
        EXPECT_FALSE (io::fd_valid (fd));
    }

    // constructors taking an fd also require explicit ownership arg:
    {
        int32_t const fd = ::open (file.c_str (), O_RDWR);
        ASSERT_GE (fd, 0);
        VR_SCOPE_EXIT ([fd]() { ::close (fd); });

        {
            fd_ostream<exc_policy> out { fd, /* close_fd_on_destruct */false };
            int32_t const fd2 = out.fd ();

            ASSERT_EQ (fd2, fd);
            EXPECT_TRUE (io::fd_valid (fd));

            out << "some content";
        }
        EXPECT_TRUE (io::fd_valid (fd)); // 'out' was instructed not to close it
        {
            fd_ostream<exc_policy> out { fd, /* close_fd_on_destruct */true };
            int32_t const fd3 = out.fd ();

            ASSERT_EQ (fd3, fd);
            EXPECT_TRUE (io::fd_valid (fd));

            out << "more content";
        }
        EXPECT_FALSE (io::fd_valid (fd)); // 'out' was instructed to close it
    }
    // same as above, but with input streams:
    {
        int32_t const fd = ::open (file.c_str (), O_RDONLY);
        ASSERT_GE (fd, 0);
        VR_SCOPE_EXIT ([fd]() { ::close (fd); });

        {
            fd_istream<exc_policy> in { fd, /* close_fd_on_destruct */false };
            int32_t const fd2 = in.fd ();

            ASSERT_EQ (fd2, fd);
            EXPECT_TRUE (io::fd_valid (fd));
        }
        EXPECT_TRUE (io::fd_valid (fd)); // 'in' was instructed not to close it
        {
            fd_istream<exc_policy> in { fd, /* close_fd_on_destruct */true };
            int32_t const fd3 = in.fd ();

            ASSERT_EQ (fd3, fd);
            EXPECT_TRUE (io::fd_valid (fd));
        }
        EXPECT_FALSE (io::fd_valid (fd)); // 'in' was instructed to close it
    }
}
//............................................................................
//............................................................................
namespace
{

string_vector const g_test_urls
{
    "https://linux.die.net/man/2/nanosleep",
    "https://arxiv.org/pdf/adap-org/9911006.pdf"
};

string_vector const g_test_retry_urls
{
    "http://localhost:8080/test.ints.100000.bin"        // (10^5 / 4) int32_t's with a certain bit pattern
};

string_vector const g_test_error_urls
{
    "https://www.invalidAERWEhost.com/asdfa",           // bad hostname
    "https://www.google.com/invalid2342ASDF234ref",     // bad reference
    "invalidasdfa://www/google.com"                     // bad schema
};

} // end of anonymous
//............................................................................
//............................................................................

// TODO
// - test empty payload

TEST (http_streams, input)
{
    for (std::string const & url : g_test_urls)
    {
        LOG_info << "reading [" << url << "] ...";
        int64_t payload_size { -1 };

        for (int32_t buf_size : { /* edge case */1, 109, 4 * 1024, 8 * 1024 + 1, 1000000 })
        {
            std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) };
            LOG_trace2 << "buf is @" << static_cast<addr_t> (buf.get ());

            // for testing, set retry parameters for fast failure and/or to avoid needlessly lengthy tests:

            http_istream<> in { url, arg_map { { "retry_max", 1 }, { "retry_delay", 10000000L } } };

            util::ptime_t start, stop;
            std::stringstream dst;
            int64_t read_total { };
            {
                start = util::to_ptime (sys::realtime_utc ());

                read_total = read_until_eof (in, buf.get (), buf_size, dst);

                stop = util::to_ptime (sys::realtime_utc ());
            }
            ASSERT_GT (read_total, 0);

            auto const duration = (stop - start).total_microseconds ();
            LOG_info << "  read " << read_total << " byte(s) in " << duration << " usec, rate " << std::setprecision (3) << static_cast<double> (read_total) / duration << " MiB/s";

            if (payload_size >= 0)
                ASSERT_EQ (read_total, payload_size) << "failed for buf of size " << buf_size << " for [" << url << ']';
            else
                payload_size = read_total;

            LOG_trace2 << "CONTENT\n" << dst.str ();
        }

        int32_t const buf_size  = payload_size;
        std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) };

        // test an edge case: user buffer size exactly equal to the payload size:
        {
            http_istream<> in { url, arg_map { { "retry_max", 1 }, { "retry_delay", 10000000L } } };

            int64_t const read_total = read_fully (in, buf_size, buf.get ());
            ASSERT_EQ (read_total, payload_size) << "failed for buf of size " << buf_size << " for [" << url << ']';
        }

        // test with randomized read count requests:
        {
            uint64_t rnd = test::env::random_seed<uint64_t> ();

            // for testing, set retry parameters for fast failure and/or to avoid needlessly lengthy tests:

            http_istream<> in { url, arg_map { { "retry_max", 1 }, { "retry_delay", 10000000L } } };

            int64_t read_total { };
            {
                for (int64_t rc = 0; (in.read (buf.get (), 1 + test::next_random (rnd) % buf_size), rc = in.gcount ()) > 0; read_total += rc)
                {
                    LOG_trace1 << "rc: " << rc;
                }
            }
            ASSERT_EQ (read_total, payload_size) << "failed for randomized read counts for [" << url << ']';
        }
    }
}

TEST (http_streams, aborted_read)
{
    for (std::string const & url : g_test_urls)
    {
        LOG_info << "reading [" << url << "] ...";

        for (int32_t buf_size : { 109, 1000000 })
        {
            std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) };
            LOG_trace2 << "buf is @" << static_cast<addr_t> (buf.get ());

            http_istream<> in { url };

            {
                int64_t rc;
                for (int32_t i = 0; (in.read (buf.get (), buf_size), rc = in.gcount ()) > 0; ++ i)
                {
                    LOG_trace1 << '[' << i << "] rc: " << rc;
                    if (i > 1)
                    {
                        in.close (); // should be ok, no exceptions here or at 'in' destruction
                        break;
                    }
                }
            }
        }
    }
}
//............................................................................

//TEST (vlad, create_testfile)
//{
//    int32_t const count = 100000 / 4;
//    {
//        std::ofstream out { "testfile.100000.bin", default_ostream_mode () };
//
//        std::unique_ptr<int32_t []> const data { boost::make_unique_noinit<int32_t []> (count) };
//        for (int32_t i = 0; i < count; ++ i) data [i] = 0x70000000 + i;
//
//        out.write (char_ptr_cast (data.get ()), count * 4);
//    }
//}
//............................................................................

template<typename T> struct http_streams: public gt::Test { };
TYPED_TEST_CASE (http_streams, exception_scenarios);

TYPED_TEST (http_streams, DISABLED_retries) // TODO re-enable when have a testcase listerner to auto-start darkhttpd
{
    using exc_policy            = TypeParam; // test parameter

    int32_t const value_count   { 100000 / sizeof (int32_t) };
    std::unique_ptr<int32_t []> const values { boost::make_unique_noinit<int32_t []> (value_count) };

    for (std::string const & url : g_test_retry_urls)
    {
        LOG_info << "reading [" << url << "] ...";

        for (int32_t buf_size : { 2, 1661, 1000000 })
        {
            std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) }; // I/O buffer

            // read (with simulated retries) a binary file with known content and check the received value sequence:

            std::fill (values.get (), values.get () + value_count, -1); // reset content

            // for testing, set retry parameters for fast failure and/or to avoid needlessly lengthy tests:

            http_istream<exc_policy> in { url, arg_map { { "retry_max", 2 }, { "retry_delay", 10000000L } } };

            // [no I/O, no exceptions so far]

            {
                // wrap 'data_buf in a receiving stream:
                array_ostream<exceptions> content { byte_ptr_cast (values.get ()), value_count * sizeof (int32_t) };

                read_until_eof (in, buf.get (), buf_size, content);
            }

            for (int32_t i = 0; i < value_count; ++ i)
            {
                ASSERT_EQ (values [i], 0x70000000 + i) << "[buf size: " << buf_size << "] corrupt value at index " << i;
            }
        }
    }
}

TYPED_TEST (http_streams, failures)
{
    using exc_policy            = TypeParam; // test parameter

    for (std::string const & url : g_test_error_urls)
    {
        LOG_info << "reading [" << url << "] ...";

        for (int32_t buf_size : { 2, 109, 1000000 })
        {
            std::unique_ptr<char []> const buf { boost::make_unique_noinit<char []> (buf_size) };

            // for testing, set retry parameters for fast failure and/or to avoid needlessly lengthy tests:

            http_istream<exc_policy> in { url, arg_map { { "retry_max", 3 }, { "retry_delay", 1000000L } } };

            // [no I/O, no exceptions so far]

            null_ostream discard { };

            if (std::is_same<exc_policy, io::exceptions>::value) // compile-time branch
            {
                EXPECT_THROW (read_until_eof (in, buf.get (), buf_size, discard), io_exception);
            }
            else
            {
                EXPECT_NO_THROW (read_until_eof (in, buf.get (), buf_size, discard));
            }

            EXPECT_FALSE (in.good ());
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
