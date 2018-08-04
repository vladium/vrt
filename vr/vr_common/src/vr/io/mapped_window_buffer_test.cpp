
#include "vr/io/files.h"
#include "vr/io/mapped_window_buffer.h"
#include "vr/io/streams.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
// TODO
// - movable
//............................................................................
/*
 * c.f. 'mapped_window_buffer.capped_capacity'
 */
TEST (mapped_window_buffer, capped_capacity)
{
    using value_type            = int64_t;

    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (value_type); // total number of values read/written

    vr_static_assert (fsz % sizeof (value_type) == 0);

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (value_type);


    for (int64_t const capacity_count : { 1, 2, count_in_one_page / 2, count_in_one_page + 2, 5 * count_in_one_page })
    {
        mapped_window_buffer::size_type const capacity = capacity_count * sizeof (value_type); // [bytes]

        for (int64_t const w_window_count : { 1, 2, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
        {
            if (w_window_count > capacity_count) continue; // skip invalid combination

            window_t const w_window = static_cast<window_t> (w_window_count * sizeof (value_type)); // [bytes]

            LOG_info << "capacity_count: " << capacity_count << ", w_window_count: " << w_window_count;

            uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + w_window_count + capacity_count));

            const fs::path file { test::unique_test_path (join_as_name (w_window_count)) };
            io::create_dirs (file.parent_path ());

            // write data via sliding window writes, read some of it back
            {
                mapped_window_buffer buf { file, capacity, w_window, fsz_reserve, clobber::trunc };

                ASSERT_EQ (buf.capacity (), capacity);
                ASSERT_EQ (buf.w_window (), w_window);

                ASSERT_EQ (buf.size (), 0); // state after construction
                ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

                LOG_trace1 << "initial position @" << buf.w_position ();

                int64_t i;
                for (i = 0; i < count; )
                {
                    pos_t const w_count_limit = std::min<pos_t> (w_window_count, count - i);
                    pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit; // can always write in [0, w_window)

                    LOG_trace2 << "  writing " << w_count << " value(s) ...";

                    value_type * const w = static_cast<value_type *> (buf.w_position ()); // 'w' has at least 'w_window_count' slots mapped
                    for (int64_t k = 0; k < w_count; ++ k)
                    {
                        w [k] = i ++;
                    }
                    addr_t const wa = buf.w_advance (w_count * sizeof (value_type));

                    ASSERT_EQ (buf.capacity (), capacity); // due to the pattern of writes and reads in this testcase

                    // skip over a random fraction of values just written, read and validate the rest:

                    pos_t const r_skip = test::next_random (rnd) % w_count;

                    LOG_trace2 << "  skipping " << r_skip << " value(s) ...";

                    value_type const * const r = static_cast<value_type const *> (buf.r_advance (r_skip * sizeof (value_type)));
                    ASSERT_EQ (buf.w_position (), wa); // API invariant (r-advance never invalidates w-position)

                    LOG_trace2 << "  reading " << (w_count - r_skip) << " value(s) ...";

                    for (int64_t k = 0; k < (w_count - r_skip); ++ k)
                    {
                        ASSERT_EQ (r [k], (i - w_count + r_skip + k)) << "wrong value read at k = " << k << " (i = " << i << ')';
                    }

                    // advance over the values just read:

                    buf.r_advance ((w_count - r_skip) * sizeof (value_type));
                    ASSERT_EQ (buf.w_position (), wa); // API invariant (r-advance never invalidates w-position)

                    ASSERT_EQ (buf.size (), 0); // buffer in flushed state
                }
                check_eq (i, count);

                ASSERT_EQ (buf.capacity (), capacity); // design of this testcase

            } // 'buf' truncates and closes


            // re-read/validate 'file' contents via standard I/O:
            {
                io::drop_from_cache (file); // try to have kernel re-fault-in 'file' content

                ASSERT_EQ (io::file_size (file), fsz);
                {
                    fd_istream<> in { file };

                    union
                    {
                        value_type value;
                        char buf;
                    };

                    for (value_type v = 0; v < count; ++ v)
                    {
                        in.read (& buf, sizeof (value));
                        ASSERT_EQ (value, v);
                    }
                }
            }
        }
    }
}
//............................................................................

TEST (mapped_window_buffer, uncapped_capacity)
{
    using value_type            = int64_t;

    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (value_type); // total number of values read/written

    vr_static_assert (fsz % sizeof (value_type) == 0);

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (value_type);


    for (int64_t const capacity_count : { 1, 2, count_in_one_page / 2, count_in_one_page + 2, 5 * count_in_one_page })
    {
        mapped_window_buffer::size_type const capacity = capacity_count * sizeof (value_type); // [bytes]

        for (int64_t const w_window_count : { 1, 2, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
        {
            if (w_window_count > capacity_count) continue; // skip invalid combination

            window_t const w_window = static_cast<window_t> (w_window_count * sizeof (value_type)); // [bytes]

            LOG_info << "capacity_count: " << capacity_count << ", w_window_count: " << w_window_count;

            uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + w_window_count + capacity_count));

            const fs::path file { test::unique_test_path (join_as_name (w_window_count)) };
            io::create_dirs (file.parent_path ());

            // write data via sliding window writes, read some of it back
            {
                mapped_window_buffer buf { file, capacity, w_window, fsz_reserve, clobber::trunc };

                ASSERT_EQ (buf.capacity (), capacity);
                ASSERT_EQ (buf.w_window (), w_window);

                ASSERT_EQ (buf.size (), 0); // state after construction
                ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

                LOG_trace1 << "initial position @" << buf.w_position ();

                int64_t i, j;
                for (i = 0, j = 0; i < count; )
                {
                    pos_t const w_count_limit = std::min<pos_t> (w_window_count, count - i);
                    pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit; // can always write in [0, w_window)

                    LOG_trace2 << "  writing " << w_count << " value(s) ...";

                    value_type * const w = static_cast<value_type *> (buf.w_position ()); // 'w' has at least 'w_window_count' slots mapped
                    for (int64_t k = 0; k < w_count; ++ k)
                    {
                        w [k] = i ++;
                    }
                    addr_t const wa = buf.w_advance (w_count * sizeof (value_type));

                    ASSERT_GE (buf.capacity (), capacity); // capacity can only grow

                    // read and validate a random fraction of values available:

                    if (buf.size () > 0)
                    {
                        pos_t const r_count = 1 + test::next_random (rnd) % (buf.size () / sizeof (value_type));

                        LOG_trace2 << "  reading " << r_count << " value(s) ...";

                        value_type const * const r = static_cast<value_type const *> (buf.r_position ());
                        for (int64_t k = 0; k < r_count; ++ k)
                        {
                            ASSERT_EQ (r [k], j) << "wrong value read at k = " << k << " (j = " << j << ')';
                            ++ j;
                        }

                        // advance over the values just read:

                        buf.r_advance (r_count * sizeof (value_type));
                        ASSERT_EQ (buf.w_position (), wa); // API invariant (r-advance never invalidates w-position)
                    }
                }
                check_eq (i, count);
                check_le (j, count);

                // commit all written values (the loop above doesn't guarantee that):

                buf.r_advance (buf.size ());

                ASSERT_GE (buf.capacity (), capacity); // design of this testcase

            } // 'buf' truncates and closes


            // re-read/validate 'file' contents via standard I/O:
            {
                io::drop_from_cache (file); // try to have kernel re-fault-in 'file' content

                ASSERT_EQ (io::file_size (file), fsz);
                {
                    fd_istream<> in { file };

                    union
                    {
                        value_type value;
                        char buf;
                    };

                    for (value_type v = 0; v < count; ++ v)
                    {
                        in.read (& buf, sizeof (value));
                        ASSERT_EQ (value, v);
                    }
                }
            }
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
