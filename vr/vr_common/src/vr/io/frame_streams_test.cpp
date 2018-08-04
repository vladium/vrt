
#include "vr/data/dataframe.h"
#include "vr/io/csv/CSV_frame_streams.h"
#include "vr/io/defs.h"
#include "vr/io/files.h"
#include "vr/io/hdf/HDF_frame_streams.h"
#include "vr/util/crc32.h"

#include <fstream>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
using namespace data;

//............................................................................
//............................................................................
namespace
{
std::vector<attribute> const &
test_attrs (const bool text_format)
{
    static std::vector<attribute> const g_all_attrs
    {
        attribute::parse ("A_cat1: {A, b, cd};"
                          "A_cat2: {A, AA, A.B2C, ABC_DFGH};"
                          "A_ts: time;"
                          "A_i4: i4;"
                          "A_i8: i8;"
                          "A_f4: f4;"
                          "A_f8: f8;")
    };

    // NOTE: only attribute types that we can CSV-read/write without loss

    static std::vector<attribute> const g_text_attrs
    {
        attribute::parse ("A_cat1: {A, b, cd};"
                          "A_cat2: {A, AA, A.B2C, ABC_DFGH};"
                          "A_ts: time;"
                          "A_i4: i4;"
                          "A_i8: i8;")
    };

    return (text_format ? g_text_attrs : g_all_attrs);
}

struct checksum_visitor final // stateless
{
    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void operator() (atype_<ATYPE>,
                                    addr_const_t const col_raw, dataframe::size_type const row_count, int32_t & chksum) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        chksum = util::crc32 (byte_ptr_cast (col_raw), row_count * sizeof (data_type), chksum);
    }

}; // end of class
//............................................................................

struct CSV_factory
{
    using ostream_type              = CSV_frame_ostream<std::ofstream>;
    using istream_type              = CSV_frame_istream<std::ifstream>;

    static ostream_type create (fs::path const & file)
    {
        return { { }, file.c_str (), default_ostream_mode () };
    }

    static istream_type open (fs::path const & file)
    {
        return { { }, file.c_str (), (std::ios_base::in | std::ios_base::binary ) };
    }

}; // end of class

struct HDF_factory
{
    using ostream_type              = HDF_frame_ostream<>;
    using istream_type              = HDF_frame_istream<>;

    static ostream_type create (fs::path const & file)
    {
        return { { }, file };
    }

    static istream_type open (fs::path const & file)
    {
        return { { }, file };
    }

}; // end of class

struct HDF_zlib_factory: public HDF_factory
{
    static ostream_type create (fs::path const & file)
    {
        return { { { "filter", filter::zlib } }, file };
    }

}; // end of class

struct HDF_blosc_factory: public HDF_factory
{
    static ostream_type create (fs::path const & file)
    {
#   if VR_USE_BLOSC // ASX-60
        return { { { "filter", filter::blosc } }, file };
#   else
        return { { }, file };
#   endif // VR_USE_BLOSC
    }

}; // end of class
//............................................................................

template<typename FACTORY>
struct scenario
{
    using factory                   = FACTORY;

}; // end of scenario

using scenarios         = gt::Types<CSV_factory, HDF_factory, HDF_zlib_factory, HDF_blosc_factory>;

template<typename T> struct frame_stream_test: public gt::Test { };
TYPED_TEST_CASE (frame_stream_test, scenarios);

} // end of anonymous
//............................................................................
//............................................................................

TYPED_TEST (frame_stream_test, round_trip)
{
    using scenario          = TypeParam; // test parameter

    using factory                   = scenario;

    using ostream_type              = typename factory::ostream_type;
    using istream_type              = typename factory::istream_type;

    bool const text_output          = std::is_same<factory, CSV_factory>::value;

    int64_t rnd = test::env::random_seed<int64_t> ();

    int64_t row_target = VR_IF_THEN_ELSE (VR_DEBUG)(1, 10);
    for (int32_t r = 0; r < 3; ++ r, row_target *= 100)
    {
        LOG_info << "using target row count " << row_target;

        const fs::path root { test::unique_test_path () };
        io::create_dirs (root);

        for (attr_schema::size_type as_size = 1; as_size < 70; as_size += (1 + unsigned_cast (test::next_random (rnd)) % 10))
        {
            LOG_trace1 << "using schema size " << as_size;

            attr_schema::ptr as;
            {
                std::vector<attribute> attrs;
                {
                    std::vector<attribute> const & choices = test_attrs (text_output);

                    for (int32_t i = 0; i < as_size; ++ i)
                    {
                        attribute const & exemplar = choices [test::next_random (rnd) % choices.size ()];

                        attrs.emplace_back ("A." + string_cast (i), exemplar.type ());
                    }
                }

                as.reset (new attr_schema { std::move (attrs) }); // last use of 'attrs'
            }

            const fs::path file { root / string_cast (as_size) };


            int64_t total_rows_written { };
            int64_t total_rows_read { };

            std::unique_ptr<int32_t []> const checksums_w { boost::make_unique_noinit<int32_t []> (as_size) };
            std::unique_ptr<int32_t []> const checksums_r { boost::make_unique_noinit<int32_t []> (as_size) };
            for (int32_t i = 0; i < as_size; ++ i) checksums_w [i] = checksums_r [i] = i + 1;

            {
                ostream_type os { factory::create (file) };

                int64_t const row_capacity = 1 << (5 + unsigned_cast (test::next_random (rnd)) % 10);

                dataframe src { row_capacity, as };

                while (total_rows_written < row_target)
                {
                    int64_t const row_batch = 1 + unsigned_cast (test::next_random (rnd)) % row_capacity;
                    src.resize_row_count (row_batch);

                    test::random_fill (src, rnd); // note: no NAs
                    os.write (src);

                    ASSERT_TRUE (os.schema ());
                    EXPECT_EQ (* os.schema (), * as);

                    // update 'chechsums_w':

                    for (width_type c = 0; c < as_size; ++ c)
                    {
                        attribute const & a = as->at<false> (c);

                        dispatch_on_atype (a.atype (), checksum_visitor { }, src.at_raw<false> (c), row_batch, checksums_w [c]);
                    }

                    total_rows_written += row_batch;
                }
            }
            LOG_trace2 << "wrote " << total_rows_written << " row(s)";
            {
                istream_type is { factory::open (file) };

                int64_t const row_capacity = 1 << (0 + unsigned_cast (test::next_random (rnd)) % 16);

                dataframe dst { row_capacity, as };

                for (int64_t rc; (rc = is.read (dst)) > 0; total_rows_read += rc)
                {
                    LOG_trace3 << "  read " << rc << " row(s)";

                    ASSERT_TRUE (is.schema ());
                    EXPECT_EQ (* is.schema (), * as);

                    // update 'chechsums_r':

                    for (width_type c = 0; c < as_size; ++ c)
                    {
                        attribute const & a = as->at<false> (c);

                        dispatch_on_atype (a.atype (), checksum_visitor { }, dst.at_raw<false> (c), rc, checksums_r [c]);
                    }
                }
            }
            LOG_trace2 << "read " << total_rows_read << " row(s)";

            ASSERT_EQ (total_rows_read, total_rows_written) << "failed for schema size " << as_size << '\n' << (* as);

            // confirm that column-wise checksums match:

            for (width_type i = 0; i < as_size; ++ i)
            {
                LOG_trace2 << "checksum w " << std::hex << checksums_w [i] << ", checksum r " << std::hex << checksums_r [i];
                ASSERT_EQ (checksums_r [i], checksums_w [i]) << "failed for column " << i;
            }
        }
    }
}

TYPED_TEST (frame_stream_test, round_trip_NA)
{
    using scenario          = TypeParam; // test parameter

    using factory                   = scenario;

    using ostream_type              = typename factory::ostream_type;
    using istream_type              = typename factory::istream_type;
    
    int64_t rnd = test::env::random_seed<int64_t> ();
    
    const fs::path file { test::unique_test_path () };
    io::create_dirs (file.parent_path ());

    attr_schema::ptr const as (new attr_schema { test_attrs (false) });

    int64_t const row_capacity = 1024;
    
    {
        ostream_type os { factory::create (file) };

        dataframe src { row_capacity, as };

        src.resize_row_count (row_capacity);
        test::random_fill (src, rnd, 0.3); // note: with NAs

        os.write (src);
    }
    // TODO this currently just checks that NAs will be parsed in columns of all types, but
    // should also check that data round-trips in non-text formats:
    {
        istream_type is { factory::open (file) };

        dataframe dst { row_capacity, as };

        is.read (dst);
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
