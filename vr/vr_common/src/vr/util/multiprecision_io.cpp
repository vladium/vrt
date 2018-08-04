
#include "vr/util/multiprecision_io.h"

#include "vr/util/multiprecision.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace impl
{

VR_ASSUME_HOT std::string
print_native_int128 (native_int128_t const & v)
{
    std::stringstream s { };
    s << util::mpp_int128_t { v };

    return s.str ();
}

VR_ASSUME_HOT std::string
print_native_uint128 (native_uint128_t const & v)
{
    std::stringstream s { };
    s << util::mpp_uint128_t { v };

    return s.str ();
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of namespace
//----------------------------------------------------------------------------
