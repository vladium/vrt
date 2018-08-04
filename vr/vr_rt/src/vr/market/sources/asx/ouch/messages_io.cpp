
#include "vr/market/sources/asx/ouch/messages_io.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace ouch
{

void
print_data_as_message (std::ostream & os, message_type<io::mode::send>::enum_t const msg_type, addr_const_t const msg)
{
    // TODO use a sparse table instead of a switch

    switch (msg_type)
    {
#   define vr_CASE(r, unused, MSG) \
        case message_type<io::mode::send>:: MSG : os << (* static_cast<ouch:: MSG const *> (msg)); return; \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_MARKET_OUCH_SEND_MESSAGE_SEQ)

#   undef vr_CASE

        default:  VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

void
print_data_as_message (std::ostream & os, message_type<io::mode::recv>::enum_t const msg_type, addr_const_t const msg)
{
    // TODO use a sparse table instead of a switch

    switch (msg_type)
    {
#   define vr_CASE(r, unused, MSG) \
        case message_type<io::mode::recv>:: MSG : os << (* static_cast<ouch:: MSG const *> (msg)); return; \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_MARKET_OUCH_RECV_MESSAGE_SEQ)

#   undef vr_CASE

        default:  VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

} // end of 'ouch'

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
