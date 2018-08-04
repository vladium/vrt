
#include "vr/io/archives.h"
#include "vr/io/files.h"
#include "vr/io/resizable_buffer.h"
#include "vr/io/streams.h"
#include "vr/util/env.h"

#include <boost/algorithm/string/predicate.hpp>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (tar_iterator, sniff)
{
    tar::iterator end1 { };
    tar::iterator end2 { };

    EXPECT_EQ (end1, end2);
}
//............................................................................

#define vr_TEST_TAR         "test.6.tar"
#define vr_TEST_ENTRY_COUNT (6)

//............................................................................

TEST (tar_iterator, iterate_all)
{
    fs::path const file = test::data_dir () / vr_TEST_TAR;

    int32_t count { };
    for (tar::entry const & e : tar::iterator { file })
    {
        LOG_trace1 << '"' << e.name () << "\",\tsize " << e.size ();
        ASSERT_FALSE (e.name ().empty ());

        ++ count;
    }
    LOG_info << "iterated over " << count << " entries";
    EXPECT_EQ (count, vr_TEST_ENTRY_COUNT);
}

TEST (tar_iterator, iterate_early_break)
{
    fs::path const file = test::data_dir () / vr_TEST_TAR;

    int32_t const count_break = vr_TEST_ENTRY_COUNT / 2;

    int32_t count { };
    for (auto i = tar::iterator { file }, i_limit = tar::iterator { }; count < count_break && i != i_limit; ++ i, ++ count)
    {
        tar::entry const & e = * i;

        LOG_trace1 << e.name ();
        ASSERT_FALSE (e.name ().empty ()) << "failed at count " << count;
    }
    LOG_info << "iterated over " << count << " entries";
    EXPECT_EQ (count_break, count);
}
//............................................................................
namespace
{

template<compression::enum_t COMPRESSION>
int64_t
decompress (resizable_buffer const & entry_data, int64_t const entry_size)
{
    using buffer_stream         = array_istream<>;
    using input                 = typename stream_for<COMPRESSION>::template istream<buffer_stream>;

    input in { entry_data.data (), static_cast<int32_t> (entry_size) };

    char buf [4 * 1024];

    int64_t read_total { };
    while (in) // read and discard
    {
        in.read (buf, length (buf));
        read_total += in.gcount ();
    }

    return read_total;
}

} // end of anonymous

TEST (tar_iterator, iterate_and_extract)
{
    fs::path const file = test::data_dir () / vr_TEST_TAR;

    resizable_buffer entry_data { };
    {
        int32_t count_iter { };
        int32_t count_read { };

        for (tar::entry const & e : tar::iterator { file })
        {
            LOG_trace1 << '"' << e.name () << "\",\tsize " << e.size ();
            ASSERT_FALSE (e.name ().empty ());

            bool last = false;
            if (boost::starts_with (e.name (), "AAPL_") || boost::starts_with (e.name (), "FOIL_") || (last = boost::starts_with (e.name (), "VT_")))
            {
                int8_t * const buf = entry_data.ensure_capacity (e.size ());
                int64_t const rc = e.read (buf);

                LOG_info << "\t'" << e.name () << "':\tread " << rc << " entry bytes, entry #" << count_iter;

                EXPECT_GE (entry_data.capacity (), e.size ());
                ASSERT_EQ (rc, e.size ());

                int64_t decompressed_size;

                compression::enum_t const e_compression = io::guess_compression (e.name (), false);
                switch (e_compression)
                {
                    case compression::gzip: decompressed_size = decompress<compression::gzip> (entry_data, e.size ()); break;
                    case compression::zlib: decompressed_size = decompress<compression::zlib> (entry_data, e.size ()); break;

                    default: throw_x (invalid_input, "unexpected entry compression type " + print (e_compression));

                } // end of switch

                LOG_info << "\t\t\tdecompressed " << decompressed_size << " byte(s)";
                if (e.size () > 512) { ASSERT_GT (decompressed_size, 0); }
                ASSERT_EQ (decompressed_size % 20, 0);

                ++ count_read;

                if (last) break;
            }

            ++ count_iter;
        }

        LOG_info << "iterated over " << count_iter << " entries, extracted " << count_read;
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
