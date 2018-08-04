
#include "vr/io/mapped_ring_buffer.h"
#include "vr/sys/os.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (mapped_ring_buffer, construct)
{
    using size_type     = mapped_ring_buffer::size_type;

    uint32_t rnd = test::env::random_seed<uint32_t> ();
    uint32_t rnd2 = rnd;

    size_type capacity { }; // set below
    uint8_t const * r2 { }; // set below
    std::unique_ptr<mapped_ring_buffer> mrb2; // set below

    {
        mapped_ring_buffer mrb { 1 };

        capacity = mrb.capacity ();
        ASSERT_GT (capacity, 0);

        ASSERT_EQ (mrb.size (), 0); // state after construction
        ASSERT_EQ (mrb.w_window (), capacity); // state after construction

        ASSERT_EQ (mrb.r_position (), mrb.w_position ()); // state after construction

        // touch/write every byte:
        {
            uint8_t * const w = byte_ptr_cast (mrb.w_position ());

            for (size_type i = 0; i < capacity; ++ i)
            {
                w [i] = (test::next_random (rnd) & 0xFF);
            }
        }
        // check physical address wrapping (this pattern of access abuses the fact that the impl doesn't check
        // access patterns, but it's ok for testing purposes):
        {
            uint8_t const * const r_lo = byte_ptr_cast (mrb.r_position ());
            uint8_t const * const r_hi = r_lo + capacity;

            for (size_type i = 0; i < capacity; ++ i)
            {
                ASSERT_EQ (r_hi [i], r_lo [i]) << "failed at i = " << i;
            }
        }

        // movable:

        r2 = byte_ptr_cast (mrb.r_position ());

        mrb.w_advance (2);
        mrb.r_advance (1);

        mrb2.reset (new mapped_ring_buffer { std::move (mrb) });
        LOG_trace1 << "mrb moved";
    }
    // check 'mrb2':
    {
        size_type const capacity2 = mrb2->capacity ();
        ASSERT_EQ (capacity2, capacity);

        // a copy of 'mrb' state just before we moved it:

        ASSERT_EQ (mrb2->size (), 1);
        ASSERT_EQ (mrb2->w_window (), capacity - 1);

        ASSERT_EQ (mrb2->r_position (), r2 + 1);
        ASSERT_EQ (mrb2->w_position (), r2 + 2);

        // read every byte of 'mrb2', the mapping should still be valid:

        for (size_type i = 0; i < capacity2; ++ i)
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
TEST (mapped_ring_buffer, capped_capacity)
{
    using value_type            = int64_t;

    int64_t const count                 = 10000000 / sizeof (value_type); // total number of values read/written

    int32_t const count_in_one_page     = sys::os_info::instance ().page_size () / sizeof (value_type);


    for (int64_t const capacity_count : { 1, 2, count_in_one_page / 2, count_in_one_page + 2, 5 * count_in_one_page })
    {
        mapped_ring_buffer::size_type const capacity = capacity_count * sizeof (value_type); // [bytes]

        LOG_info << "capacity_count: " << capacity_count;

        uint64_t rnd = (test::env::random_seed<uint64_t> () | (count + capacity_count));

        // write data via sliding window writes, read some of it back
        {
            mapped_ring_buffer buf { capacity };

            ASSERT_GE (buf.capacity (), capacity); // 'mrb' will resize capacity to a whole number of pages

            LOG_trace1 << "buffer selected capacity " << buf.capacity ();
            check_zero (buf.capacity () % sizeof (value_type), buf.capacity ()); // able to do I/O in whole 'value_type' units

            ASSERT_EQ (buf.size (), 0); // state after construction
            ASSERT_EQ (buf.w_window (), buf.capacity ()); // state after construction

            ASSERT_EQ (buf.r_position (), buf.w_position ()); // state after construction

            LOG_trace1 << "initial position @" << buf.w_position ();

            int64_t i;
            for (i = 0; i < count; )
            {
                pos_t const w_count_limit = std::min<pos_t> (buf.w_window () / sizeof (value_type), count - i);
                pos_t const w_count = 1 + test::next_random (rnd) % w_count_limit; // can always write in [0, available)

                LOG_trace2 << "  writing " << w_count << " value(s) ...";

                value_type * const w = static_cast<value_type *> (buf.w_position ()); // 'w' has at least 'w_window_count' slots mapped
                for (int64_t k = 0; k < w_count; ++ k)
                {
                    w [k] = i ++;
                }
                addr_t const wa = buf.w_advance (w_count * sizeof (value_type));

                // skip over a random fraction of values just written, read and validate the rest:

                pos_t const r_skip = w_count - 1; // test::next_random (rnd) % w_count;

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
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
