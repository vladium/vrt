
#include "vr/io/files.h"
#include "vr/io/mapped_tape_buffer.h"
#include "vr/io/streams.h"
#include "vr/sys/os.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (mapped_tape_buffer, construct)
{
    using size_type     = mapped_tape_buffer::size_type;

    uint32_t rnd = test::env::random_seed<uint32_t> ();
    uint32_t rnd2 = rnd;

    size_type capacity { }; // set below
    uint8_t const * r2 { }; // set below
    std::unique_ptr<mapped_tape_buffer> mtb2; // set below

    const fs::path file { test::unique_test_path () };
    io::create_dirs (file.parent_path ());

    {
        mapped_tape_buffer mtb { file, default_mmap_reserve_size () };

        capacity = mtb.capacity ();
        ASSERT_GT (capacity, 0);

        ASSERT_EQ (mtb.size (), 0); // state after construction
        ASSERT_EQ (mtb.w_window (), capacity); // state after construction

        ASSERT_EQ (mtb.r_position (), mtb.w_position ()); // state after construction

        // touch/write some bytes :
        {
            uint8_t * const w = byte_ptr_cast (mtb.w_advance (100000));

            for (size_type i = 0; i < 1000; ++ i)
            {
                w [i] = (test::next_random (rnd) & 0xFF);
            }
        }

        // movable:

        r2 = byte_ptr_cast (mtb.r_advance (100000));

        mtb.w_advance (2);
        mtb.r_advance (1);

        mtb2.reset (new mapped_tape_buffer { std::move (mtb) });
        LOG_trace1 << "mtb moved";
    }
    // check 'mtb2':
    {
        size_type const capacity2 = mtb2->capacity ();
        ASSERT_EQ (capacity2, capacity);

        // a copy of 'mtb' state just before we moved it:

        ASSERT_EQ (mtb2->size (), 1);
        ASSERT_EQ (mtb2->w_window (), capacity - 1);

        ASSERT_EQ (mtb2->r_position (), r2 + 1);
        ASSERT_EQ (mtb2->w_position (), r2 + 2);

        // read bytes that were written via 'mtb':

        for (size_type i = 0; i < 1000; ++ i)
        {
            uint8_t const v = (test::next_random (rnd2) & 0xFF);
            ASSERT_EQ (r2 [i], v) << "failed at i = " << i;
        }
    }
}
//............................................................................
/*
 * c.f. 'mapped_window_buffer.capped_capacity'
 */
TEST (mapped_tape_buffer, capped_window)
{
    using value_type            = int64_t;

    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (value_type); // total number of values read/written

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (value_type);


    for (int64_t const w_window_count : { 1, 2, count_in_one_page / 2, count_in_one_page + 2, 5 * count_in_one_page })
    {
        LOG_info << "w_window_count: " << w_window_count;

        uint64_t rnd = (test::env::random_seed<uint64_t> () | (count + w_window_count));

        const fs::path file { test::unique_test_path (join_as_name (w_window_count)) };
        io::create_dirs (file.parent_path ());

        // write data via sliding window writes, read some of it back
        {
            mapped_tape_buffer buf { file, fsz_reserve, clobber::trunc };

            ASSERT_EQ (buf.size (), 0); // state after construction
            ASSERT_EQ (buf.w_window (), buf.capacity ()); // state after construction

            ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

            LOG_trace1 << "initial position @" << buf.w_position ();

            int64_t i;
            for (i = 0; i < count; )
            {
                pos_t const w_count_limit = std::min<pos_t> (10000 / sizeof (value_type), count - i);
                pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit;

                LOG_trace2 << "  writing " << w_count << " value(s) ...";

                value_type * const w = static_cast<value_type *> (buf.w_position ());
                for (int64_t k = 0; k < w_count; ++ k)
                {
                    w [k] = i ++;
                }
                addr_t const wa = buf.w_advance (w_count * sizeof (value_type));

                // skip over a random fraction of values just written, read and validate the rest:

                pos_t const r_skip = test::next_random (rnd) % w_count;

                LOG_trace2 << "  skipping " << r_skip << " value(s) ...";

                value_type const * const r = static_cast<value_type const *> (buf.r_advance (r_skip * sizeof (value_type)));
                ASSERT_EQ (buf.w_position (), wa); // API invariant (this buffer never invalidates the other endpoint's position)

                LOG_trace2 << "  reading " << (w_count - r_skip) << " value(s) @" << static_cast<addr_const_t> (r);

                for (int64_t k = 0; k < (w_count - r_skip); ++ k)
                {
                    ASSERT_EQ (r [k], (i - w_count + r_skip + k)) << "wrong value read at k = " << k << " (i = " << i << ')';
                }

                // advance over the values just read:

                buf.r_advance ((w_count - r_skip) * sizeof (value_type));
                ASSERT_EQ (buf.w_position (), wa); // API invariant (this buffer never invalidates the other endpoint's position)

                ASSERT_EQ (buf.size (), 0); // buffer in flushed state
            }
            check_eq (i, count);

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
//............................................................................
/*
 * c.f. mapped_window_buffer.uncapped_capacity
 */
TEST (mapped_tape_buffer, uncapped_window)
{
    using value_type            = int64_t;

    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (value_type); // total number of values read/written

    vr_static_assert (fsz % sizeof (value_type) == 0);

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (value_type);

    for (int64_t const w_window_count : { 1, 2, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
    {
        LOG_info << "w_window_count: " << w_window_count;

        uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + w_window_count));

        const fs::path file { test::unique_test_path (join_as_name (w_window_count)) };
        io::create_dirs (file.parent_path ());

        // write data via sliding window writes, read some of it back
        {
            mapped_tape_buffer buf { file, fsz_reserve };

            auto const capacity = buf.capacity ();
            ASSERT_GE (capacity, fsz_reserve);

            ASSERT_EQ (buf.size (), 0); // state after construction
            ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

            LOG_trace1 << "initial position @" << buf.w_position ();

            int64_t i, j;
            for (i = 0, j = 0; i < count; )
            {
                pos_t const w_count_limit = std::min<pos_t> (w_window_count, count - i);
                pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit;

                LOG_trace2 << "  writing " << w_count << " value(s) ...";

                value_type * const w = static_cast<value_type *> (buf.w_position ());
                for (int64_t k = 0; k < w_count; ++ k)
                {
                    w [k] = i ++;
                }
                addr_t const wa = buf.w_advance (w_count * sizeof (value_type));

                ASSERT_EQ (buf.capacity (), capacity); // capacity is fixed at reserve size

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
//............................................................................

TEST (mapped_tape_buffer, growing_window)
{
    using value_type            = int64_t;

    signed_size_t const fsz             = 10000000;
    signed_size_t const fsz_reserve     = default_mmap_reserve_size ();
    int64_t const count                 = fsz / sizeof (value_type); // total number of values read/written

    vr_static_assert (fsz % sizeof (value_type) == 0);

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (int64_t);

    for (window_t const w_count_window : { 1, 2, count_in_one_page - 1, count_in_one_page, count_in_one_page + 1, 3 * count_in_one_page + 1 })
    {
        // write data via a sliding window writes but do not advance r_position:

        uint64_t rnd = (test::env::random_seed<uint64_t> () | (fsz + w_count_window));

        const fs::path file { test::unique_test_path (join_as_name (w_count_window)) };
        io::create_dirs (file.parent_path ());

        {
            mapped_tape_buffer buf { file, fsz_reserve, clobber::trunc };

            auto const capacity = buf.capacity ();
            ASSERT_GE (capacity, fsz_reserve);

            ASSERT_EQ (buf.size (), 0); // state after construction
            ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

            int64_t i;
            for (i = 0; i < count; )
            {
                pos_t const w_count_limit = std::min<pos_t> (10000, count - i);
                pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit;

                LOG_trace2 << "  writing " << w_count << " value(s) ...";

                value_type * const w = static_cast<value_type *> (buf.w_position ()); // 'w' has at least 'w_window_count' slots mapped
                for (int64_t k = 0; k < w_count; ++ k)
                {
                    w [k] = i ++;
                }
                buf.w_advance (w_count * sizeof (value_type));

                ASSERT_EQ (buf.capacity (), capacity); // capacity is fixed at reserve size
            }
            check_eq (i, count);

            // commit all written values (the loop above doesn't guarantee that):

            buf.r_advance (buf.size ());

        } // 'buf' truncates and closes


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

                for (int64_t v = 0; v < count; ++ v)
                {
                    in.read (& buf, sizeof (value));
                    ASSERT_EQ (value, v);
                }
            }
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
