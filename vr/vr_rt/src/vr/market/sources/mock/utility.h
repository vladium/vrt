#pragma once

#include "vr/util/random.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

template<typename T, typename T_RND>
T
runif_nonnegative (T const mean, T const width, T_RND & rnd)
{
    assert_nonnegative (mean);
    assert_gt (width, 2);

    T v = mean - width / 2 + (unsigned_cast (util::xorshift (rnd)) % width);
    v = std::max<T> (0, v);

    return v;
}
//............................................................................

// TODO this is basically a copy/paste from test::random
/**
 * @return a vector of size 'n' containing sizes of partitions [0, x1),[x1, x2)....[x_n-1, n)
 *         where no partition is empty and boundaries x1...x_n-1 are approximately uniformly
 *         distributed in [0, range)
 */
template<typename T, typename T_RND>
std::vector<T>
random_range_split (T const range, int32_t const n, T_RND & rnd)
{
    assert_positive (n);
    assert_ge (range, n);


    std::set<T> u { 0 }; // note: needs to be ordered
    while (signed_cast (u.size ()) < n)
        u.insert (unsigned_cast (util::xorshift (rnd)) % range);

    std::vector<T> r { };

    T x_prev { -1 };
    for (T const x : u)
    {
        if (VR_LIKELY (x_prev >= 0))
        {
            r.push_back (x - x_prev);
        }
        x_prev = x;
    }
    r.push_back (range - x_prev);

    assert_eq (static_cast<int32_t> (r.size ()), n);

    return r;
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
