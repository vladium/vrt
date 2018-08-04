#pragma once

#include "vr/macros.h"
#include "vr/strings_fwd.h"

#include <iostream>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace impl
{

extern VR_ASSUME_HOT std::string
print_native_int128 (native_int128_t const & v);

extern VR_ASSUME_HOT std::string
print_native_uint128 (native_uint128_t const & v);

} // end of 'impl'
//............................................................................
//............................................................................

template<>
VR_FORCEINLINE std::string
print<native_int128_t> (native_int128_t const & v)
{
    return impl::print_native_int128 (v);
}

template<>
VR_FORCEINLINE std::string
print<native_uint128_t> (native_uint128_t const & v)
{
    return impl::print_native_uint128 (v);
}

} // end of namespace
//----------------------------------------------------------------------------
namespace std
{

template<class TRAITS>
inline basic_ostream<std::string::value_type, TRAITS> &
operator<< (basic_ostream<std::string::value_type, TRAITS> & os, vr::native_int128_t const & v)
{
    return (os << vr::impl::print_native_int128 (v));
}

template<class TRAITS>
inline basic_ostream<std::string::value_type, TRAITS> &
operator<< (basic_ostream<std::string::value_type, TRAITS> & os, vr::native_uint128_t const & v)
{
    return (os << vr::impl::print_native_uint128 (v));
}

} // end of 'std'
//----------------------------------------------------------------------------
