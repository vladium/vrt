
#include "vr/io/parse/defs.h"

#include "vr/strings.h" // print()

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace parse
{
//............................................................................

std::string
__print__ (CSV_token const & obj) VR_NOEXCEPT
{
    std::stringstream s;

    s << obj.m_type << '(' << obj.str_range () << ')';

    return s.str ();
}
//............................................................................

std::string
__print__ (JSON_token const & obj) VR_NOEXCEPT
{
    std::stringstream s;

    s << obj.m_type << '(' << obj.str_range () << ')';

    return s.str ();
}

} // end of 'parse'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
