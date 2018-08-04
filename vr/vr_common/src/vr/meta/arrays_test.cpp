
#include "vr/meta/arrays.h"

#include "vr/asserts.h"

#include "vr/test/utility.h"

#include <type_traits>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

auto constexpr a0   = create_array_fill<int32_t, 5, 123> ();

vr_static_assert (std::is_same<decltype (a0), std::array<int32_t, 5> const>::value);
vr_static_assert (a0.size () == 5);
vr_static_assert (a0 [0] == 123);
vr_static_assert (a0 [4] == 123);

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
