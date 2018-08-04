
#include "vr/util/str_range.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

std::string
__print__ (str_range const & obj) VR_NOEXCEPT
{
    return quote_string<'"'> (obj.to_string ());
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
