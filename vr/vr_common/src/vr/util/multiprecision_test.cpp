
#include "vr/util/multiprecision.h"
#include "vr/util/multiprecision_io.h" // this makes 'is_streamable<>'6 tests pass
#include "vr/util/type_traits.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

vr_static_assert (util::is_streamable<native_int128_t, std::ostream>::value);
vr_static_assert (util::is_streamable<native_uint128_t, std::ostream>::value);

//............................................................................
// TODO
// - signed type versions

TEST (multiprecision, convert_to_native)
{
    for (int32_t shift = 0; shift < 128; ++ shift)
    {
        mpp_uint128_t const v { _mpp_one_128 () << shift }; // single bit

        // mpp -> native:

        native_uint128_t const nv = v.convert_to<native_uint128_t> ();

        uint64_t const nv_hi = static_cast<uint64_t> (nv >> 64);
        uint64_t const nv_lo = static_cast<uint64_t> (nv);

        if (shift < 64)
        {
            ASSERT_EQ (nv_hi, 0UL);
            ASSERT_EQ (nv_lo, (1UL << shift));
        }
        else
        {
            ASSERT_EQ (nv_hi, (1UL << (shift - 64)));
            ASSERT_EQ (nv_lo, 0UL);
        }

        // native -> mpp:

        mpp_uint128_t const v2 { nv };
        ASSERT_EQ (v2, v);
    }
}

TEST (multiprecision, convert_to_native_randomized)
{
    uint64_t rnd = test::env::random_seed<uint64_t> ();

    constexpr int32_t repeats   = 1000000;

    for (int32_t r = 0; r < repeats; ++ r)
    {
        native_uint128_t nv { rnd };
        nv = (nv << (test::next_random (rnd) % 128)) + nv;

        mpp_uint128_t const v { nv };

        // mpp -> native:

        native_uint128_t const nv2 = v.convert_to<native_uint128_t> ();
        ASSERT_EQ (nv2, nv);

        // native -> mpp:

        mpp_uint128_t const v2 { nv };
        ASSERT_EQ (v2, v);
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
