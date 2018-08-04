#pragma once

#include "vr/util/conditions_int.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

#define vr_is_null(p) /* p == nullptr */ \
    ((p) == nullptr)

#define vr_is_nonnull(p) /* !(p == nullptr) */ \
    (! vr_is_null (p))

//............................................................................
// single-branch range checks:

#define vr_is_addr_contained(addr, addr_a, addr_b) /* addr_a <= addr <= addr_b */ \
    vr_is_in_inclusive_range (uintptr (addr), uintptr (addr_a), uintptr (addr_b))

#define vr_is_addr_range_contained(addr, addr_length, base, base_length) /* (base <= addr) && (addr + addr_length <= base + base_length) */ \
    (vr_le (uintptr (base), uintptr (addr)) & vr_le (uintptr (addr) + addr_length, uintptr (base) + base_length))

} // end of namespace
//----------------------------------------------------------------------------
