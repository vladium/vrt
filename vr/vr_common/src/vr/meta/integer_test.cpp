
#include "vr/meta/integer.h"

#include "vr/asserts.h"

#include <limits>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

// static_is_power_of_2:

vr_static_assert (static_is_power_of_2<1>::value);
vr_static_assert (static_is_power_of_2<8>::value);

vr_static_assert (! static_is_power_of_2<0>::value);
vr_static_assert (! static_is_power_of_2<3>::value);

//............................................................................

// static_log2_floor:

vr_static_assert (0 == static_log2_floor<1>::value);

vr_static_assert (1 == static_log2_floor<2>::value);
vr_static_assert (1 == static_log2_floor<3>::value);

vr_static_assert (2 == static_log2_floor<4>::value);
vr_static_assert (2 == static_log2_floor<5>::value);

vr_static_assert (31 == static_log2_floor<std::numeric_limits<uint32_t>::max ()>::value);
vr_static_assert (63 == static_log2_floor<std::numeric_limits<uint64_t>::max ()>::value);

// static_log2_ceil:

vr_static_assert (0 == static_log2_ceil<1>::value);

vr_static_assert (1 == static_log2_ceil<2>::value);
vr_static_assert (2 == static_log2_ceil<3>::value);

vr_static_assert (2 == static_log2_ceil<4>::value);
vr_static_assert (3 == static_log2_ceil<5>::value);

vr_static_assert (32 == static_log2_ceil<std::numeric_limits<uint32_t>::max ()>::value);
vr_static_assert (64 == static_log2_ceil<std::numeric_limits<uint64_t>::max ()>::value);

//............................................................................

// static_range:

using r_i   = static_range<int32_t, -1, 0, -2, std::numeric_limits<int32_t>::max ()>;

vr_static_assert (r_i::min () == -2);
vr_static_assert (r_i::max () == std::numeric_limits<int32_t>::max ());

using r_c   = static_range<char, 'B', 'A', 'Z'>; // like used by explicit enums

vr_static_assert (r_c::min () == 'A');
vr_static_assert (r_c::max () == 'Z');

using r_u   = static_range<std::size_t, 1, 2, 10, std::numeric_limits<std::size_t>::min ()>;

vr_static_assert (r_u::min () == 0);
vr_static_assert (r_u::max () == 10);

//............................................................................

vr_static_assert (0x80000007 == static_bitset<uint32_t, 0, 1, 2, 31>::value);
vr_static_assert (0x8000000000000007 == static_bitset<uint64_t, 0, 1, 2, 63>::value);

//............................................................................

vr_static_assert (0x0 == static_mux<>::value);
vr_static_assert (  0x0102FF == static_mux<0, 1, 2, 255>::value);
vr_static_assert (0x01020304 == static_mux<1, 2, 3, 4>::value);

//............................................................................

vr_static_assert (0x3412               == static_byteswap (static_cast<uint16_t> (0x1234)));
vr_static_assert (0x78563412           == static_byteswap (static_cast<uint32_t> (0x12345678)));
vr_static_assert (0x785634120d0c0b0a   == static_byteswap (static_cast<uint64_t> (0x0a0b0c0d12345678)));

//............................................................................

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
