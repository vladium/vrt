#pragma once

#include "vr/macros.h"

#include <atomic>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{

template<typename T>
VR_FORCEINLINE T
atomic_xchg (T & v, T const v_new)
{
    return __atomic_exchange_n (& v, v_new, __ATOMIC_ACQ_REL); // TODO weaker ordering than std?
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
