
#include "vr/io/net/socket_factory.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

TEST (socket_factory, UDP_client)
{
    for (bool set_peer : { false, true })
    {
        socket_handle const s { socket_factory::create_UDP_client_endpoint ("127.0.0.1", "echo", set_peer) };
    }
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
