#pragma once

#include "vr/preprocessor.h"
#include "vr/types.h"

#include <boost/multiprecision/cpp_int.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

using mpp_int128_t          = boost::multiprecision:: VR_IF_THEN_ELSE (VR_RELEASE)(int128_t,  checked_int128_t);
using mpp_uint128_t         = boost::multiprecision:: VR_IF_THEN_ELSE (VR_RELEASE)(uint128_t, checked_uint128_t);

constexpr mpp_uint128_t _mpp_one_128 ()     { return mpp_uint128_t { 1 }; }

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
