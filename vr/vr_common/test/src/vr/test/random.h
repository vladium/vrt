#pragma once

#include "vr/util/random.h"

#include <limits>
#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename R, typename = void>
struct next_random_impl
{
    static R evaluate (T & rnd)
    {
        return util::xorshift (rnd); // mutates/returns 'rnd'
    }

}; // end of master

template<typename T, typename R> // specialize for floating point
struct next_random_impl<T, R,
    typename std::enable_if<std::is_floating_point<R>::value>::type>
{
    static R evaluate (T & rnd)
    {
        return (static_cast<R> (util::xorshift (rnd) / 2) / static_cast<R> (std::numeric_limits<T>::max () / 2)); // * std::numeric_limits<R>::max ());
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<typename T, typename R = T>
VR_FORCEINLINE R
next_random (T & rnd)
{
    return impl::next_random_impl<T, R>::evaluate (rnd);
}
//............................................................................
/**
 * @return a vector of size 'n' containing sizes of partitions [0, x1),[x1, x2)....[x_n-1, n)
 *         where no partition is empty and boundaries x1...x_n-1 are approximately uniformly
 *         distributed in [0, range)
 */
template<typename T, typename RND>
std::vector<T>
random_range_split (T const range, int32_t const n, RND & rnd)
{
    assert_positive (n);
    assert_ge (range, n);

    std::set<T> u { 0 }; // note: needs to be ordered
    while (signed_cast (u.size ()) < n)
        u.insert (unsigned_cast (next_random (rnd)) % range);

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
//............................................................................


} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
