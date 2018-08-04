#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

using scaled_int_t                      = int64_t;

constexpr scaled_int_t si_scale ()      { return static_cast<scaled_int_t> (10000000); }

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
