
#include "vr/util/cal/calendar.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

vr_static_assert (std::is_move_constructible<calendar>::value);
vr_static_assert (std::is_move_assignable<calendar>::value);

//............................................................................
// TODO
// - empty calendar, query
// - reverse iterators

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
