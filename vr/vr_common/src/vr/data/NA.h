#pragma once

#include "vr/fw_string.h"
#include "vr/util/ops_int.h"

#include <cmath>
#include <limits>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

#define VR_R_NA_MARKER  0x7A2

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct NA_ops; // master


template<typename T>
struct NA_ops<T,
    typename std::enable_if<std::is_signed<T>::value>::type>
{
    static constexpr T value ()     { return std::numeric_limits<T>::min (); } // constexpr in c++11

    static VR_FORCEINLINE bool is_value (T const & x)
    {
        return (x == value ());
    }

}; // end of specialization

template<typename T>
struct NA_ops<T,
    typename std::enable_if<std::is_unsigned<T>::value>::type>
{
    static constexpr T value ()     { return static_cast<T> (-1); } // unfortunate that pandas didn't follow R's simpler approach here

    static VR_FORCEINLINE bool is_value (T const & x)
    {
        return (x == value ());
    }

}; // end of specialization


template<>
struct NA_ops<float>
{
    static constexpr float value ()     { return __builtin_nanf (VR_TO_STRING (VR_R_NA_MARKER)); } // NOTE: nanf

    static VR_FORCEINLINE bool is_value (float const & x)
    {
        return (static_cast<uint32_t> (vr_rorx_int32 (util::bit_cast (x), 16)) == (0x7FC0 | (VR_R_NA_MARKER << 16)));
    }

}; // end of specialization

template<>
struct NA_ops<double>
{
    static constexpr double value ()    { return __builtin_nan (VR_TO_STRING (VR_R_NA_MARKER)); }

    // likely the fastest R-compatible is_NA() impl I've ever written:

    static VR_FORCEINLINE bool is_value (double const & x)
    {
        return (static_cast<uint32_t> (vr_rorx_int64 (util::bit_cast (x), 48)) == (0x7FF8 | (VR_R_NA_MARKER << 16)));
    }

}; // end of specialization


template<typename E> // specialize for typesafe enums (NOTE: this must be consistent with (unsigned) 'enum_int_t')
struct NA_ops<E,
    typename std::enable_if<is_typesafe_enum<E>::value>::type>
{
    using E_lowered     = typename E::enum_t;

    static constexpr E_lowered value () { return E::na; };

}; // end of specialization

template<typename T> // specialize for plain enums
struct NA_ops<T,
    typename std::enable_if<std::is_enum<T>::value>::type>
{
    static constexpr T value ()         { return static_cast<T> (enum_NA ()); };

    static VR_FORCEINLINE bool is_value (T const & x)
    {
        return (x == value ());
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

constexpr str_const         NA_NAME     { VR_ENUM_NA_NAME };
extern std::string const    NA_NAME_str;

/*
 * c.f. 'enum_NA()'
 */
inline fw_string4
NA_name ()
{
    return fw_string4 { VR_ENUM_NA_NAME, sizeof (VR_ENUM_NA_NAME) - 1 };
}
//............................................................................
/**
 * return an R/pandas-compatible NA value for 'T'
 *
 * @note for 'double' the value is one of the NaNs (but the converse is not true)
 *
 * @see is_NA()
 * @see mark_NA()
 */
template<typename T>
constexpr lower_if_typesafe_enum_t<T>
NA ()
{
    return impl::NA_ops<T>::value ();
};

template<typename T> // TODO a version that takes an addr_const_t and thus saves the cost of loading into xmm reg for fp Ts:
VR_FORCEINLINE bool
is_NA (T const & x)
{
    return impl::NA_ops<T>::is_value (x);
}
//............................................................................
/**
 * convenience assist with type inference
 */
template<typename T>
VR_FORCEINLINE void
mark_NA (T & x)
{
    x = NA<T> ();
}

} // end of 'data'
//............................................................................
//............................................................................
namespace impl
{

template<typename T, uint64_t NA_REPR>
struct maybe_NA_impl final
{
    VR_FORCEINLINE maybe_NA_impl (T const & x) :
        m_x { & x }
    {
    }

    friend std::ostream & operator<< (std::ostream & os, maybe_NA_impl const & obj) VR_NOEXCEPT
    {
        if (VR_UNLIKELY (data::is_NA (* obj.m_x)))
            return os << fw_string8 { NA_REPR };
        else
            return os << (* obj.m_x);
    }

    friend std::string __print__ (maybe_NA_impl const & obj) VR_NOEXCEPT
    {
        if (VR_UNLIKELY (data::is_NA (* obj.m_x)))
            return print (fw_string8 { NA_REPR });
        else
            return print (* obj.m_x);
    }

    T const * const m_x;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<uint64_t NA_REPR = fw_cstr (VR_ENUM_NA_NAME), typename T>
VR_FORCEINLINE impl::maybe_NA_impl<T, NA_REPR>
maybe_NA (T const & x)
{
    return { x };
}

} // end of namespace
//----------------------------------------------------------------------------
