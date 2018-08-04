#pragma once

#include "vr/util/str_range.h"

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

template<typename T, typename = void>
struct parse_decimal_int;


template<typename T> // specialized for unsigned 'T' (runtime sign check is ineffective/redundant)
struct parse_decimal_int<T,
    typename std::enable_if<std::is_unsigned<T>::value>::type>
{
    static T evaluate (char_const_ptr_t const start, int32_t const size)
    {
        assert_positive (size);

        T r { };

        for (int32_t i = 0; i < size; ++ i)
        {
            r = (r * 10) + (start [i] - '0');
        }

        return r;
    }

}; // end of specialization

template<typename T> // specialized for signed 'T'
struct parse_decimal_int<T,
    typename std::enable_if<std::is_signed<T>::value>::type>
{
    static T evaluate (char_const_ptr_t const start, int32_t const size)
    {
        using T_unsigned        = typename std::make_unsigned<T>::type;

        bool const negative = (start [0] == '-');

        T const r_abs = parse_decimal_int<T_unsigned>::evaluate (start + negative, size - negative);

        return (negative ? - r_abs : r_abs);
    }

}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct parse_decimal_impl
{
    static int32_t evaluate (char_const_ptr_t const start, int32_t const size)
    {
        static_assert (lazy_false<T>::value, "no parse_decimal support for 'T'");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of master


template<typename T> // specialize for integral types (forward to 'print_decimal_int')
struct parse_decimal_impl<T,
    typename std::enable_if<std::is_integral<T>::value>::type> // TODO narrow down to int types
{
    static int32_t evaluate (char_const_ptr_t const start, int32_t const size)
    {
        return parse_decimal_int<T>::evaluate (start, size);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @param start [note: not required to be null-terminated]
 */
template<typename T>
T
parse_decimal (char_const_ptr_t const start, int32_t const size)
{
    assert_positive (size);
    assert_ne (start [0], '+'); // TODO support this?

    return impl::parse_decimal_int<T>::evaluate (start, size);
}

/**
 * @param start [note: not required to be null-terminated]
 */
template<typename T>
T
parse_decimal_nonnegative (char_const_ptr_t const start, int32_t const size)
{
    assert_positive (size);
    assert_ne (start [0], '-'); // verify in debug builds

    using T_unsigned        = typename std::make_unsigned<T>::type;

    return impl::parse_decimal_int<T_unsigned>::evaluate (start, size);
}
//............................................................................

extern string_vector
split (std::string const & s, string_literal_t const separators, bool keep_empty_tokens = false);

//............................................................................
/**
 * @note in-place
 */
template<std::size_t SIZE>
str_range
ltrim (std::array<char, SIZE> const & s, char const blank = ' ')
{
    int32_t i = 0;
    for ( ; i < static_cast<int32_t> (SIZE); ++ i)
    {
        if (s.data ()[i] != blank) break;
    }

    return { s.data () + i, static_cast<int32_t> (SIZE) - i };
}
/**
 * @note in-place
 */
template<std::size_t SIZE>
str_range
rtrim (std::array<char, SIZE> const & s, char const blank = ' ')
{
    int32_t i = SIZE;
    for ( ; -- i >= 0; )
    {
        if (s.data ()[i] != blank) break;
    }

    return { s.data (), i + 1 };
}
//............................................................................

template<typename I>
std::vector<I>
parse_int_list (std::string const & s)
{
    std::vector<I> r;

    string_vector const tokens = split (s, ", ");
    for (std::string const & tok : tokens)
    {
        r.push_back (parse_decimal<I> (tok.data (), tok.size ()));
    }

    return r;
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
