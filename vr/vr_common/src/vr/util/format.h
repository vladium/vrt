#pragma once

#include "vr/util/ops_int.h"

#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{
//............................................................................

extern const char g_digit_pairs [201];

//............................................................................

template<typename T, typename = void>
struct print_decimal_int;


template<typename T> // specialized for unsigned 'T' (runtime sign check is ineffective/redundant)
struct print_decimal_int<T,
    typename std::enable_if<std::is_unsigned<T>::value>::type>
{
    static int32_t evaluate (T x, char * const dst)
    {
        using ops           = ops_int<arg_policy<zero_arg_policy::return_special, 0>, false>; // note: this handles 'x' being zero correctly

        auto const digits10 = 1 + ops::ilog10_floor (x);

        char * c = & dst [digits10 - 1];

        while (x >= 100)
        {
            auto const r = x % 100;
            x /= 100;

            __builtin_memcpy (c - 1, g_digit_pairs + 2 * r, 2);
            c -= 2;
        }

        if (x < 10)
        {
            * c = '0' + x;
        }
        else
        {
            __builtin_memcpy (c - 1, g_digit_pairs + 2 * x, 2);
        }

        return digits10;
    }

}; // end of specialization

template<typename T> // specialized for signed 'T'
struct print_decimal_int<T,
    typename std::enable_if<std::is_signed<T>::value>::type>
{
    static int32_t evaluate (T x, char * dst)
    {
        using T_unsigned        = typename std::make_unsigned<T>::type;

        bool const sign_digit = (x < 0);

        if (sign_digit)
        {
            x = - x;

            * dst ++ = '-';
        }

        return (sign_digit + print_decimal_int<T_unsigned>::evaluate (static_cast<T_unsigned> (x), dst));
    }

}; // end of specialization
//............................................................................

template<typename T>
struct rjust_print_decimal_int
{
    vr_static_assert (std::is_unsigned<T>::value);

    static int32_t evaluate (T x, char * const dst, int32_t const width)
    {
        int32_t digits10 { };

        char * c = & dst [width - 1];

        while (x >= 100)
        {
            auto const r = x % 100;
            x /= 100;

            __builtin_memcpy (c - 1, g_digit_pairs + 2 * r, 2);
            c -= 2;

            digits10 += 2;
        }

        if (x < 10)
        {
            * c = '0' + x;

            ++ digits10;
        }
        else
        {
            __builtin_memcpy (c - 1, g_digit_pairs + 2 * x, 2);

            digits10 += 2;
        }

        return digits10;
    }

}; // end of class
//............................................................................

template<typename T, typename = void>
struct print_decimal_impl
{
    static int32_t evaluate (T x, char * const dst)
    {
        static_assert (lazy_false<T>::value, "no print_decimal support for 'T'");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of master


template<typename T> // specialize for integral types (forward to 'print_decimal_int')
struct print_decimal_impl<T,
    typename std::enable_if<std::is_integral<T>::value>::type> // TODO narrow down to int types
{
    static int32_t evaluate (T x, char * const dst)
    {
        return print_decimal_int<T>::evaluate (x, dst);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note NOT inferring signed/unsigned from 'T' by design (the important aspect
 *       is the runtime value domain)
 */
template<typename T>
int32_t
print_decimal (T const & x, char * const dst)
{
    return impl::print_decimal_impl<T>::evaluate (x, dst);
}
//............................................................................
/**
 * for integral types, allow the caller to override runtime sign checking
 *
 * @note NOT inferring signed/unsigned from 'T' by design (the important aspect
 *       is the runtime value domain)
 */
template<typename T>
typename std::enable_if<std::is_integral<T>::value, int32_t>::type // TODO narrow down to int types
print_decimal_nonnegative (T const & x, char * const dst)
{
    assert_nonnegative (x); // verify in debug builds

    using T_unsigned        = typename std::make_unsigned<T>::type;

    return impl::print_decimal_int<T_unsigned>::evaluate (static_cast<T_unsigned> (x), dst);
}

template<typename T>
typename std::enable_if<std::is_integral<T>::value, int32_t>::type // TODO narrow down to int types
rjust_print_decimal_nonnegative (T const & x, char * const dst, int32_t const width)
{
    assert_nonnegative (x); // verify in debug builds

    using T_unsigned        = typename std::make_unsigned<T>::type;

    return impl::rjust_print_decimal_int<T_unsigned>::evaluate (static_cast<T_unsigned> (x), dst, width);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
