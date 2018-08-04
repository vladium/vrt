
#include "vr/io/links/TCP_link.h"

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
open_TCP_socket (std::string const & host, std::string const & service, bool const connect, bool const disable_nagle)
{
    net::socket_handle sh { net::socket_factory::create_TCP_client_endpoint (host, service, connect, disable_nagle) };
    check_condition (! sh.blocking (), host, service);

    return sh;
}
//............................................................................

capacity_t
tcp_recv_poll (int32_t const fd, addr_t const dst, capacity_t const len)
{
    assert_positive (len); // caller ensures

    capacity_t rc = ::recv (fd, dst, len, (MSG_DONTWAIT | MSG_NOSIGNAL)); // request no SIGPIPE on errors due to peer disconnect (EPIPE will still be returned)
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socket
    {
        auto const e = errno;
        if (VR_UNLIKELY (e != EAGAIN))
            throw_x (io_exception, "recv() error (" + string_cast (e) + "): " + std::strerror (e));

        rc = 0; // EAGAIN
    }
    else if (rc == 0)
        rc = -1; // EOF

    return rc;
}


capacity_t
tcp_send (int32_t const fd, addr_const_t const src, capacity_t const len)
{
    assert_positive (len); // caller ensures

    capacity_t rc = ::send (fd, src, len, (MSG_DONTWAIT | MSG_NOSIGNAL)); // request no SIGPIPE on errors due to peer disconnect (EPIPE will still be returned)
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socketsend
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
