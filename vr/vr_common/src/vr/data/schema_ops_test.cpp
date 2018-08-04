
#include "vr/data/schema_ops.h"

#include "vr/util/logging.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................
//............................................................................
namespace
{

template<typename BITSET>
std::vector<int32_t>
attr_offset_brute_force (BITSET const slot)
{
    int32_t const width = 8 * sizeof (BITSET);

    std::vector<int32_t> r (width);
    int32_t position { };


    for (int32_t index = 0; index < width; ++ index)
    {
        if (slot & (1L << index))
        {
            r [index] = position;
            position += 2;
        }
    }
    for (int32_t index = 0; index < width; ++ index)
    {
        if (! (slot & (1L << index)))
        {
            r [index] = position;
            ++ position;
        }
    }

    return r;
}
//............................................................................

using bitset_types      = gt::Types<bitset32_t, bitset64_t>;

template<typename T> struct attr_offset_test: public gt::Test { };
TYPED_TEST_CASE (attr_offset_test, bitset_types);

} // end of anonymous
//............................................................................
//............................................................................

TYPED_TEST (attr_offset_test, single_bitmap)
{
    using bitset_type   = TypeParam; // test paramete;

    int32_t const width     = 8 * sizeof (bitset_type);
    int64_t const repeats   = 100000;

    bitset_type slot = test::env::random_seed<bitset_type> ();

    for (int64_t r = 0; r < repeats; ++ r)
    {
        switch (r)
        {
            case (repeats - 4): slot =  0; break; // cover some edge cases
            case (repeats - 3): slot = -1; break; // cover some edge cases
            case (repeats - 2): slot =  1; break; // cover some edge cases
            case (repeats - 1): slot = (static_cast<bitset_type> (1) << (width - 1)); break; // cover some edge cases

            default: test::next_random (slot);

        }; //end of switch

        std::vector<int32_t> const expected = attr_offset_brute_force (slot);

        for (int32_t index = 0, index_limit = 8 * sizeof (bitset_type); index < index_limit; ++ index)
        {
            int32_t const position = impl::attr_offset<bitset_type>::evaluate (slot, index);

            ASSERT_EQ (position, expected [index]) << "failed for index " << index << " of slot " << std::hex << slot;
        }
    }
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
