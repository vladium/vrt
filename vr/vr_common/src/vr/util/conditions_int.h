#pragma once

#include "vr/util/type_traits.h" // [un]signed_cast

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
// single-branch checks (integer types only):

#define vr_is_within(i, limit) /* 0 <= i < limit */ \
    (::vr::unsigned_cast (i) < ::vr::unsigned_cast (limit))

#define vr_is_within_inclusive(i, limit) /* 0 <= i <= limit */ \
    (::vr::unsigned_cast (i) <= ::vr::unsigned_cast (limit))

#define vr_is_in_range(i, a, b) /* a <= i < b */ \
    (::vr::unsigned_cast ((i) - (a)) < ::vr::unsigned_cast ((b) - (a)))

#define vr_is_in_inclusive_range(i, a, b) /* a <= i <= b */ \
    (::vr::unsigned_cast ((i) - (a)) <= ::vr::unsigned_cast ((b) - (a)))

#define vr_is_in_exclusive_range(i, a, b) /* a < i < b */ \
    (::vr::unsigned_cast ((i) - (a) - 1) < ::vr::unsigned_cast ((b) - (a) - 1))


#define vr_is_power_of_2(i) \
    (((i) > 0) & ! ((i) & ((i) - 1)))

} // end of namespace
//----------------------------------------------------------------------------
