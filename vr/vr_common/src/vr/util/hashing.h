#pragma once

#include "vr/util/ops_int.h"

#include <boost/functional/hash.hpp>

#include <cstring>
#include <functional>
#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

template<typename T>
VR_FORCEINLINE void
hash_combine (std::size_t & seed, T const & v)
{
    boost::hash_combine (seed, v);
}
//............................................................................

struct c_str_equal: public std::binary_function<string_literal_t, string_literal_t, bool>
{
    VR_FORCEINLINE bool operator() (string_literal_t const lhs, string_literal_t const rhs) const VR_NOEXCEPT_IF (VR_RELEASE)
    {
        assert_nonnull (lhs);
        assert_nonnull (rhs);

        return (! std::strcmp (lhs, rhs));
    }

}; // end of functor

struct c_str_hasher: public std::unary_function<string_literal_t, std::size_t>
{
    VR_FORCEINLINE std::size_t operator() (string_literal_t const obj) const VR_NOEXCEPT_IF (VR_RELEASE)
    {
        assert_nonnull (obj);

        return str_hash_32 (obj);
    }

}; // end of functor
//............................................................................

template<typename T>
struct hash
{
    VR_FORCEINLINE std::size_t operator() (T const & obj) const VR_NOEXCEPT
    {
        return boost::hash<T> { }(obj);
    }

}; // end of master

template<typename ... Ts> // specialize for 'std::tuple'
struct hash<std::tuple<Ts ...> >
{
    VR_FORCEINLINE std::size_t operator() (std::tuple<Ts ...> const & obj) const VR_NOEXCEPT
    {
        return boost::hash_value (obj);
    }

}; // end of class
//............................................................................

template<typename T>
struct identity_hash
{
    VR_FORCEINLINE typename std::enable_if<is_integral_or_pointer<T>::value, std::size_t>::type
    operator() (T const & obj) const VR_NOEXCEPT
    {
        return static_cast<std::size_t> (integral_cast (obj));
    }

}; // end of class
//............................................................................

template<typename T>
struct crc32_hash
{
    VR_FORCEINLINE typename std::enable_if<is_integral_or_pointer<T>::value, std::size_t>::type
    operator() (T const & obj) const VR_NOEXCEPT
    {
        return static_cast<std::size_t> (i_crc32 (src_hash_seed (), integral_cast (obj)));
    }

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
