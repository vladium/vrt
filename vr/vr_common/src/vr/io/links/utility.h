#pragma once

#include "vr/types.h"

#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{

// TODO move elsewhere

inline void
blank_fill (addr_t const addr, int32_t const len)
{
    std::memset (addr, ' ', len);
}

} // end of namespace
//----------------------------------------------------------------------------
