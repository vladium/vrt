#pragma once

#include "vr/util/conditions_addr.h"
#include "vr/util/conditions_int.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
// unary:
//............................................................................

#define vr_is_positive(v) /* 0 < v */ \
    (0 < (v))

#define vr_is_nonnegative(v) /* 0 <= v */ \
    (0 <= (v))

#define vr_is_zero(v) /* 0 == v */ \
    (0 == (v))

#define vr_is_nonzero(v) /* !(0 == v) */ \
    (! vr_is_zero (v))

//............................................................................

#define vr_is_empty(c) /* c.empty () */ \
    ((c).empty ())

#define vr_is_nonempty(c) /* !(c.empty ()) */ \
    (! vr_is_empty (c))

//............................................................................
// binary:
//............................................................................

// for less operator overloading requirements, use only operator< and operator==:

#define vr_eq(lhs, rhs) /* lhs == rhs */ \
    ((lhs) == (rhs))

#define vr_ne(lhs, rhs) /* !(lhs == rhs) */ \
    (! vr_eq (lhs, rhs))

#define vr_lt(lhs, rhs) /* lhs < rhs */ \
    ((lhs) < (rhs))

#define vr_le(lhs, rhs) /* !(rhs < lhs) */ \
    (! vr_lt (rhs, lhs))

#define vr_gt(lhs, rhs) /* rhs < lhs */ \
    vr_lt (rhs, lhs)

#define vr_ge(lhs, rhs) /* !(lhs < rhs) */ \
    (! vr_lt (lhs, rhs))


} // end of namespace
//----------------------------------------------------------------------------
