#pragma once

#include "vr/macros.h"
#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

extern VR_NOINLINE int32_t
capture_callstack (int32_t const skip, addr_t out [], int32_t const out_limit);

//............................................................................

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
