
#include "vr/io/files.h"
#include "vr/io/mapped_files.h"
#include "vr/io/streams.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

#include <algorithm>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
// TODO
// - growing window
// - movable
// - SIGBUF case

TEST (mapped_ifile, capped_window)
{
    signed_size_t const fsz             = 100000;
    int64_t const count                 = fsz / sizeof (int64_t); // total number of values read/written
    int64_t const seek_start            = 3; // TODO vary

    vr_static_assert (fsz % sizeof (int64_t) == 0);

    int64_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (int64_t);

    for (int64_t const max_advance_count : { 1L, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
    {
        for (advice_dir::enum_t const advice : advice_dir::values ())
        {
            LOG_trace1 << "advice: " << print (advice);

            // create a file in a "known good" way, populate it with testable content:

            const fs::path file { test::unique_test_path (join_as_name (max_advance_count, advice)) }; // use different files in each pass to make sure pages don't get reused
            io::create_dirs (file.parent_path ());
            {
                fd_ostream<> out { file };

                union
                {
                    int64_t value;
                    char buf;
                };

                for (int64_t i = 0; i < count; ++ i)
                {
                    value = i;
                    out.write (& buf, sizeof (value));
                }
            }
            ASSERT_EQ (io::file_size (file), fsz);

            // read and validate written data via a sliding window mapped read pass:

            io::drop_from_cache (file); // try to have kernel re-fault-in 'file' content

            mapped_ifile in { file, advice };
            ASSERT_EQ (in.size (), fsz);

            uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + max_advance_count + advice));


            pos_t max_r_count = std::min<pos_t> (max_advance_count, count);
            pos_t r_count = 1 + test::next_random (rnd) % max_r_count;

            int64_t const * v = static_cast<int64_t const *> (in.seek (seek_start * sizeof (int64_t), r_count * sizeof (int64_t)));
            LOG_trace1 << "initial seek " << seek_start << " to @" << static_cast<addr_const_t> (v) << " (r_count: " << r_count << ')';

            int64_t i = seek_start;
            while (true)
            {
                LOG_trace2 << "  validating " << r_count << " value(s) ...";

                for (int64_t k = 0; k < r_count; ++ k, ++ i) // note: 'i' side effect
                {
                    ASSERT_EQ (v [k], i) << "failed at i = " << i << ", k = " << k << " (r_count: " << r_count << ')';
                }

                if (i == count) break; // EOF

                pos_t const read = r_count * sizeof (int64_t);

                max_r_count = std::min<pos_t> (max_advance_count, count - i);
                r_count = 1 + test::next_random (rnd) % max_r_count;

                v = static_cast<int64_t const *> (in.advance (read, r_count * sizeof (int64_t)));
                LOG_trace1 << "advanced " << read << " bytes(s) to @" << static_cast<addr_const_t> (v) << " (r_count: " << r_count << ')';
            }
        }
    }
}

TEST (mapped_ofile, capped_window)
{
    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (int64_t); // total number of values read/written

    vr_static_assert (fsz % sizeof (int64_t) == 0);

    int64_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (int64_t);

    for (int64_t const max_advance_count : { 1L, 2L, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
    {
        for (advice_dir::enum_t const advice : advice_dir::values ())
        {
            LOG_trace1 << "advice: " << print (advice);

            const fs::path file { test::unique_test_path (join_as_name (max_advance_count, advice)) };
            io::create_dirs (file.parent_path ());

            // write data via a sliding window mapped write pass and validate it via standard I/O:
            {
                mapped_ofile out { file, fsz_reserve, clobber::trunc, advice };
                ASSERT_EQ (out.size (), 0);

                uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + max_advance_count + advice));


                pos_t max_w_count = std::min<pos_t> (max_advance_count, count);
                pos_t w_count = 1 + test::next_random (rnd) % max_w_count;

                int64_t * v = static_cast<int64_t *> (out.seek (0, w_count * sizeof (int64_t)));
                LOG_trace1 << "initial seek " << 0 << " to @" << static_cast<addr_const_t> (v) << " (w_count: " << w_count << ')';

                int64_t i = 0;
                while (true)
                {
                    LOG_trace2 << "  writing " << w_count << " value(s) ...";

                    for (int64_t k = 0; k < w_count; ++ k, ++ i) // note: 'i' side effect
                    {
                        v [k] = i;
                    }

                    if (i == count) break; // EOF

                    pos_t const wrote = w_count * sizeof (int64_t);

                    max_w_count = std::min<pos_t> (max_advance_count, count - i);
                    w_count = 1 + test::next_random (rnd) % max_w_count;

                    v = static_cast<int64_t *> (out.advance (wrote, w_count * sizeof (int64_t)));
                    LOG_trace1 << "advanced " << wrote << " bytes(s) to @" << static_cast<addr_const_t> (v) << " (w_count: " << w_count << ')';
                }

                bool rc = out.truncate_and_close (count * sizeof (int64_t));
                ASSERT_TRUE (rc);

                // second truncate will be a no-op:

                rc = out.truncate_and_close (0);
                EXPECT_FALSE (rc);

            } // 'out' closes

            // re-read/validate 'file' contents via standard I/O:
            {
                io::drop_from_cache (file); // try to have kernel re-fault-in 'file' content

                ASSERT_EQ (io::file_size (file), fsz);
                {
                    fd_istream<> in { file };

                    union
                    {
                        int64_t value;
                        char buf;
                    };

                    for (int64_t i = 0; i < count; ++ i)
                    {
                        in.read (& buf, sizeof (value));
                        ASSERT_EQ (value, i);
                    }
                }
            }
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
