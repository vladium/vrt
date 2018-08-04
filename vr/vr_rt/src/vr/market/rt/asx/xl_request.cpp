
#include "vr/market/rt/asx/xl_request.h"

#include "vr/meta/objects_io.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

std::string __print__ (xl_request const & obj) VR_NOEXCEPT
{
    std::stringstream os { };
    os << '{';
    {
        meta::print_visitor<xl_request> v { obj, os };
        meta::apply_visitor<xl_request > (v);
    }
    os << '}';

    return os.str ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
