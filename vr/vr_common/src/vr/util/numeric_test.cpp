
#include "vr/util/numeric.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
// TODO
// - signed/unsigned
// - loss of fp precision

TEST (numeric_cast, valid)
{
    // signed:

    {
        int64_t const v = numeric_cast<int64_t> (static_cast<int32_t> (123));
        EXPECT_EQ (v, 123L);
    }
    {
        int32_t const v = numeric_cast<int32_t> (static_cast<int64_t> (123));
        EXPECT_EQ (v, 123);
    }
    {
        int64_t const v = numeric_cast<int64_t> (std::numeric_limits<int32_t>::max ());
        EXPECT_EQ (v, static_cast<int64_t> (std::numeric_limits<int32_t>::max ()));
    }

    // unsigned -> signed:

    {
        int64_t const v = numeric_cast<int64_t> (static_cast<uint32_t> (123));
        EXPECT_EQ (v, 123L);
    }
    {
        int32_t const v = numeric_cast<int32_t> (static_cast<uint64_t> (123));
        EXPECT_EQ (v, 123);
    }

    // signed -> unsigned:

    {
        uint64_t const v = numeric_cast<uint64_t> (static_cast<int32_t> (123));
        EXPECT_EQ (v, 123UL);
    }
    {
        uint32_t const v = numeric_cast<uint32_t> (static_cast<int64_t> (123));
        EXPECT_EQ (v, static_cast<uint32_t> (123));
    }
}

TEST (numeric_cast, invalid)
{
    EXPECT_THROW (numeric_cast<int32_t> (std::numeric_limits<int64_t>::max ()), type_mismatch); // positive overflow
    EXPECT_THROW (numeric_cast<int32_t> (std::numeric_limits<int64_t>::min ()), type_mismatch); // negative overflow

    EXPECT_THROW (numeric_cast<uint32_t> (-1), type_mismatch); // negative overflow
    EXPECT_THROW (numeric_cast<uint64_t> (-1L), type_mismatch); // negative overflow
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
