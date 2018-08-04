#pragma once

#include "vr/market/sources/asx/ouch/messages.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace ouch
{
//............................................................................

extern void
print_data_as_message (std::ostream & os, message_type<io::mode::send>::enum_t const msg_type, addr_const_t const msg);

extern void
print_data_as_message (std::ostream & os, message_type<io::mode::recv>::enum_t const msg_type, addr_const_t const msg);

//............................................................................

struct print_message final
{
    print_message (message_type<io::mode::send>::enum_t const msg_type, addr_const_t const msg) :
        m_msg { msg },
        m_io_mode { io::mode::send },
        m_send_msg_type { msg_type }
    {
    }

    friend std::ostream & operator<< (std::ostream & os, print_message const & obj) VR_NOEXCEPT
    {
        (obj.m_io_mode == io::mode::send)
                ? print_data_as_message (os, obj.m_send_msg_type, obj.m_msg)
                : print_data_as_message (os, obj.m_recv_msg_type, obj.m_msg);

        return os;
    }


    addr_const_t const m_msg;
    io::mode::enum_t const m_io_mode;
    union
    {
        message_type<io::mode::send>::enum_t const m_send_msg_type;
        message_type<io::mode::recv>::enum_t const m_recv_msg_type;
    };

}; // end of class

} // end of 'ouch'

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
