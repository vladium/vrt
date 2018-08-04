#pragma once

#include "vr/macros.h"
#include "vr/types.h"

#if !defined (__SSE4_2__)
#   error SSE 4.2+ needs to be enabled at the compiler level (use -msse4.2/-mavx/etc)
#endif

#include <x86intrin.h> // actually only need <nmmintrin.h> for SSE4.2

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 * Intel intrinsics are ok for C code, but awkward for C++ whenever it makes sense
 * to templatize and/or overload a name: wrap them into force-inlined alternatives.
 */
VR_FORCEINLINE uint32_t
i_crc32 (uint32_t const crc, uint8_t  const x)    { return _mm_crc32_u8  (crc, x); }

VR_FORCEINLINE uint32_t
i_crc32 (uint32_t const crc, uint16_t const x)    { return _mm_crc32_u16 (crc, x); }

VR_FORCEINLINE uint32_t
i_crc32 (uint32_t const crc, uint32_t const x)    { return _mm_crc32_u32 (crc, x); }
/*
 * for consuming an 8-byte value, x86 crc32c instruction requires a 64-bit register
 * even though the upper 32 bits are not used; we force a more consistent API than
 * the raw intrinsics:
 */
VR_FORCEINLINE uint32_t
i_crc32 (uint32_t const crc, uint64_t const x)    { return _mm_crc32_u64 (crc, x); }

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
