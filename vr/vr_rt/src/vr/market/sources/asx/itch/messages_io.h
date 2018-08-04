#pragma once

#include "vr/market/sources/asx/itch/messages.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace itch
{
//............................................................................

extern void
print_data_as_message (std::ostream & os, itch::message_type::enum_t const msg_type, addr_const_t const msg);

//............................................................................

struct print_message final
{
    print_message (itch::message_type::enum_t const msg_type, addr_const_t const msg) :
        m_msg { msg },
        m_msg_type { msg_type }
    {
    }

    friend std::ostream & operator<< (std::ostream & os, print_message const & obj) VR_NOEXCEPT
    {
        print_data_as_message (os, obj.m_msg_type, obj.m_msg);
        return os;
    }


    addr_const_t const m_msg;
    itch::message_type::enum_t const m_msg_type;

}; // end of class

} // end of 'itch'

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
