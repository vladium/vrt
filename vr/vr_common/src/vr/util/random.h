#pragma once

#include "vr/asserts.h"
#include "vr/macros.h"
#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct xorshift_impl;


template<typename T>
struct xorshift_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int32_t)))>::type>
{
    static T const & evaluate (T & state) VR_NOEXCEPT
    {
        uint32_t y { static_cast<uint32_t> (state) }; // need unsigned shifts

        y ^= (y << 13);
        y ^= (y >> 3);
        y ^= (y << 17);

        return (state = y);
    }

}; // end of specialization

template<typename T>
struct xorshift_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int64_t)))>::type>
{
    static T const & evaluate (T & state) VR_NOEXCEPT
    {
        uint64_t y { static_cast<uint64_t> (state) }; // need unsigned shifts

        y ^= (y << 13);
        y ^= (y >> 7);
        y ^= (y << 17);

        return (state = y);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @see https://en.wikipedia.org/wiki/Xorshift
 */
template<typename T>
VR_FORCEINLINE const T &
xorshift (T & state) VR_NOEXCEPT_IF (VR_RELEASE)
{
    assert_nonzero (state);

    return impl::xorshift_impl<T>::evaluate (state);
}
//............................................................................
/**
 * a 'xorshift' adaptor for 'std::shuffle'
 */
template<typename T>
struct xorshift_urbg final
{
    vr_static_assert (std::is_unsigned<T>::value);

    using result_type   = T;

    static constexpr result_type min ()     { return std::numeric_limits<result_type>::min (); }
    static constexpr result_type max ()     { return std::numeric_limits<result_type>::max (); }

    xorshift_urbg (T const seed) :
        m_value { seed }
    {
    }

    result_type operator() ()
    {
        return xorshift (m_value);
    }

    T m_value;

}; // end of class
//............................................................................

template<typename T, typename T_SEED>
void
random_shuffle (T data [], int32_t const size, T_SEED seed)
{
    vr_static_assert (std::is_unsigned<T_SEED>::value);

    for (int32_t i = size - 1; i > 0; -- i)
    {
        int32_t const n = (xorshift (seed) % (i + 1));
        std::swap (data [i], data [n]);
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
