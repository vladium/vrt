/*
 * this is based on https://github.com/vladium/static_hash
 */
#include "vr/util/crc32.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (crc32, continuity)
{
    int64_t rnd = test::env::random_seed<int64_t> ();

    int32_t const len_max           = 109;
    int32_t const len_repeats       = VR_IF_THEN_ELSE (VR_DEBUG)(200, 2000);

    std::unique_ptr<uint8_t []> const buf { new uint8_t [len_max] };

    for (uint32_t seed = 1; seed < 0xFFFFFFFF; seed = (seed << 1) | 1)
    {
        for (int32_t len = 0; len < len_max; ++ len)
        {
            for (int32_t r = 0; r < len_repeats; ++ r)
            {
                // create a "random string" of length 'len' in 'buf':

                for (int32_t i = 0; i < len; ++ i) buf [i] = test::next_random (rnd);

                // check that the reference and "fast" impls compute the same value for this string:

                auto const h_ref    = crc32_reference (buf.get (), len, seed);
                auto const h_fast   = crc32 (buf.get (), len, seed);

                ASSERT_EQ (h_fast, h_ref) << "failed for seed " << 1 << ", len " << len << ", r " << r;
            }
        }
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
