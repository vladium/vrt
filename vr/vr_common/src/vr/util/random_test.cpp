
#include "vr/util/random.h"

#include "vr/test/utility.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

using int_types   = gt::Types<int32_t, int64_t>;

template<typename T> struct xorshift_test: public gt::Test { };
TYPED_TEST_CASE (xorshift_test, int_types);

TYPED_TEST (xorshift_test, cycle_length) // theoretical cycles are 2^32-1 and 2^64-1
{
    using T         = TypeParam; // test parameter

    int64_t repeats = 10000000;

    T rand = test::env::random_seed<T> ();

    for (int64_t r = 0; r < repeats; ++ r)
    {
        ASSERT_NE (0, rand) << "failed at repeat " << r;

        xorshift (rand);
    }
}
//............................................................................

TEST (shuffle, std_xorshift_urbg)
{
    xorshift_urbg<uint64_t> g { test::env::random_seed<uint64_t> () };

    std::vector<char> v { 'A', 'B', 'C' };
    std::shuffle (v.begin (), v.end (), g);

    LOG_trace1 << "after xorshift_urbg shuffle: " << print (v);
}

TEST (shuffle, random_shuffle)
{
    std::vector<char> v { 'A', 'B', 'C' };
    random_shuffle (& v [0], v.size (), test::env::random_seed<uint64_t> ());

    LOG_trace1 << "after random_shuffle shuffle: " << print (v);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
