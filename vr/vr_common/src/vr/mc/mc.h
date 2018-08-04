#pragma once

#include "vr/macros.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................
/**
 * @see ACCESS_ONCE as used by the Linux kernel code
 */
template<typename T>
VR_FORCEINLINE T const volatile &
volatile_cast (T const & v)
{
    return (* static_cast<T const volatile *> (& v));
}

template<typename T>
VR_FORCEINLINE T volatile &
volatile_cast (T & v)
{
    return (* static_cast<T volatile *> (& v));
}
//............................................................................

VR_FORCEINLINE void
compiler_fence ()
{
    asm volatile ("" : : : "memory");
}
//............................................................................

VR_FORCEINLINE void
pause ()
{
    asm volatile ("pause");
}

extern void
pause (int64_t const repeat);

//............................................................................

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
