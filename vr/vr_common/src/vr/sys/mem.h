#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{

extern std::vector<double>
incore_histogram (addr_const_t const mem, std::size_t const extent, int32_t const bins = 10);

extern bit_set
incore_page_map (addr_const_t const mem, std::size_t const extent);

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
