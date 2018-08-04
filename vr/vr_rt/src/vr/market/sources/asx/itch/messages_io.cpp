
#include "vr/market/sources/asx/itch/messages_io.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace itch
{

void
print_data_as_message (std::ostream & os, message_type::enum_t const msg_type, addr_const_t const msg)
{
    // TODO use a sparse table instead of a switch

    switch (msg_type)
    {
#   define vr_CASE(r, unused, MSG) \
        case message_type:: MSG :  os << (* static_cast<itch:: MSG const *> (msg)); return; \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_MARKET_ITCH_MESSAGE_SEQ)

#   undef vr_CASE

        default:  VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

} // end of 'itch'

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
