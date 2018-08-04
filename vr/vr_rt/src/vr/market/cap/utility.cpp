
#include "vr/market/cap/utility.h"

#include "vr/util/datetime.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

std::string
make_capture_ID (std::string const & nodename, util::date_t const & date)
{
    return join_as_name<'/'> (gd::to_iso_string (date), nodename);
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
