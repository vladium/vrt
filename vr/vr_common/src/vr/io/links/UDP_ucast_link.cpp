
#include "vr/io/links/UDP_ucast_link.h"

#include "vr/io/net/socket_factory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

net::socket_handle
open_ucast_socket (std::string const & host, std::string const & service, bool const set_peer)
{
    return net::socket_factory::create_UDP_client_endpoint (host, service, set_peer);
}
//............................................................................

capacity_t
ucast_recv_poll (net::socket_handle const & sh, addr_t const dst, capacity_t const len)
{
    assert_positive (len); // caller ensures

    capacity_t rc = ::recv (sh.fd (), dst, len, MSG_DONTWAIT);
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socket
    {
        auto const e = errno;
        if (VR_UNLIKELY (e != EAGAIN))
            throw_x (io_exception, "recv() error (" + string_cast (e) + "): " + std::strerror (e));

        rc = 0; // EAGAIN
    }

    return rc;
}

capacity_t
ucast_send (net::socket_handle const & sh, addr_const_t const src, capacity_t const len)
{
    assert_positive (len); // caller ensures

    capacity_t rc = ::send (sh.fd (), src, len, MSG_DONTWAIT/* ? | MSG_CONFIRM */);
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socket
    {
        auto const e = errno;
        if (VR_UNLIKELY (e != EAGAIN))
            throw_x (io_exception, "send() error (" + string_cast (e) + "): " + std::strerror (e));

        rc = 0; // EAGAIN
    }

    return rc;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
