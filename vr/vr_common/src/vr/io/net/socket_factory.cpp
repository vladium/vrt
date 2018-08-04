
#include "vr/io/net/socket_factory.h"

#include "vr/io/net/utility.h"  // note: includes <sys/socket.h>
#include "vr/sys/defs.h"        // VR_CHECKED_SYS_CALL
#include "vr/sys/os.h"
#include "vr/utility.h"
#include "vr/util/logging.h"

#include <cstring>

#include <arpa/inet.h> // hton*
#include <fcntl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <netinet/tcp.h>
//#include <netpacket/packet.h>
#include <unistd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{

socket_handle
socket_factory::create_UDP_multicast_member (bool const disable_loopback, int32_t const family)
{
    int32_t const fd = VR_CHECKED_SYS_CALL (::socket (family, SOCK_DGRAM, 0)); // TODO SOCK_CLOEXEC

    if (disable_loopback)
    {
        switch (family)
        {
            case AF_INET:
            {
                ::u_char const off { 0 } ;
                VR_CHECKED_SYS_CALL (::setsockopt (fd, IPPROTO_IP, IP_MULTICAST_LOOP, & off, sizeof (off)));
            };
            break;

            case AF_INET6:
            {
                ::u_int const off { 0 } ;
                VR_CHECKED_SYS_CALL (::setsockopt (fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, & off, sizeof (off)));
            };
            break;

            default: throw_x (invalid_input, "address family " + string_cast (family) + " not supported");

        } // end of switch
    }

    return { fd, family, false };
}
//............................................................................

socket_handle
socket_factory::create_and_join_UDP_multicast_member (int32_t const ifc_index, std::vector<mcast_source> const & mss, bool const disable_loopback, int32_t const family)
{
    // open a socket to create/maintain multicast membership(s):

    socket_handle sgrp { create_UDP_multicast_member (disable_loopback, family) };

    LOG_trace1 << "joining " << print (mss) << " ...";

    // join source groups:

    for (mcast_source const & ms : mss) // TODO make the group joins more transactional (convert to net form first, then join all, atomically)
    {
        // convert to net addresses:

        ::sockaddr_storage source_sas { };
        sa_descriptor const source_sa { net::to_net_addr (ms.source (), source_sas) };

        for (addr_and_port const group_ap : ms.groups ())
        {
            ::sockaddr_storage group_sas { };
            net::sa_descriptor const group_sa { net::to_net_addr (std::get<0> (group_ap), group_sas) }; // port is not used with the current raw socket impl

            LOG_trace1 << "  joining group on ifc #" << ifc_index << '\t' << source_sa << " -> " << group_sa;

            sgrp.join_multicast (group_sa, source_sa, ifc_index);
        }
    }

    return sgrp;
}
//............................................................................

socket_handle
socket_factory::create_UDP_client_endpoint (std::string const & host, std::string const & service, bool const set_peer)
{
    ::addrinfo hints;
    {
        std::memset (& hints, 0, sizeof (::addrinfo));

        hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
        hints.ai_socktype = SOCK_DGRAM; // UDP
    }

    ::addrinfo * gai_result { nullptr };
    int32_t rc;
    if (VR_UNLIKELY ((rc = ::getaddrinfo (host.c_str (), service.c_str (), & hints, & gai_result)) != 0))
        throw_x (io_exception, "getaddrinfo(" + print (host) + ", " + print (service) + ") failed: " + ::gai_strerror (rc));

    auto _ = make_scope_exit ([gai_result]() { if (gai_result) ::freeaddrinfo (gai_result); });

    for (::addrinfo * gai_r = gai_result; gai_r != nullptr; gai_r = gai_r->ai_next) // iterate over the 'getaddrinfo ()' result list
    {
        int32_t const fd = ::socket (gai_r->ai_family, gai_r->ai_socktype, gai_r->ai_protocol); // TODO SOCK_CLOEXEC
        if (fd < 0)
            continue;

        bool connected { false };

        if (set_peer)
        {
            if (VR_UNLIKELY (::connect (fd, gai_r->ai_addr, gai_r->ai_addrlen) < 0))
            {
                ::close (fd); // close and continue

                continue;
            }

            connected = true;
        }

        LOG_trace1 << "opened UDP socket fd " << fd << " for <" << print (host) << ':' << print (service) << "> (" << sa_descriptor { gai_r->ai_family, gai_r->ai_addr, gai_r->ai_addrlen } << "), peer set: " << connected;
        return { fd, gai_r->ai_family, connected };
    }

    auto const e = errno; // set from the last 'socket ()'
    throw_x (io_exception, "could not open UDP socket for <" + print (host) + ':' + print (service) + ">: " + std::strerror (e));
}
//............................................................................

socket_handle // TODO merge recv/send methods together and add a mode parm?
socket_factory::create_link_recv_endpoint (int32_t const ifc_index, int32_t const type)
{
    int32_t const fd = VR_CHECKED_SYS_CALL (::socket (AF_PACKET, type, htons (ETH_P_IP))); // TODO SOCK_CLOEXEC

    if (ifc_index > 0) // receive packets only from this interface
    {
        ::sockaddr_ll sa;
        {
            // set the only fields needed for bind()ing AF_PACKET sockets:

            sa.sll_family = AF_PACKET;
            sa.sll_protocol = htons (ETH_P_IP); // default to the socket's protocol
            sa.sll_ifindex = ifc_index;

            VR_CHECKED_SYS_CALL (::bind (fd, sockaddr_cast<::sockaddr> (& sa), sizeof (::sockaddr_ll)));
        }
    }

    // no auto-tuning for UDP, so do this:
    {
        int sz = 1 * 1024 * 1024; // TODO parameterize
        VR_CHECKED_SYS_CALL (::setsockopt (fd, SOL_SOCKET, SO_RCVBUF, & sz, sizeof (sz)));

#   if VR_DEBUG
        {
            sz = 0;
            ::socklen_t rsz { sizeof (sz) };
            ::getsockopt (fd, SOL_SOCKET, SO_RCVBUF, & sz, & rsz);

            LOG_trace1 << "actual rcvbuf size: " << sz;
        }
#   endif
    }

    return { fd, AF_PACKET, false };
}

socket_handle
socket_factory::create_link_send_endpoint (int32_t const ifc_index, int32_t const type)
{
    int32_t const fd = VR_CHECKED_SYS_CALL (::socket (AF_PACKET, type, htons (ETH_P_IP))); // TODO SOCK_CLOEXEC

    if (ifc_index > 0) // send packets only from this interface
    {
        ::sockaddr_ll sa;
        {
            std::memset (& sa, 0, sizeof sa);

            // set the fields sufficient for sending packets:

            sa.sll_family = AF_PACKET;
            sa.sll_protocol = htons (ETH_P_IP); // default to the socket's protocol
            sa.sll_ifindex = ifc_index;

            sa.sll_halen = ETH_ALEN;
            ifc_MAC_addr (ifc_index, fd, sa.sll_addr);

            VR_CHECKED_SYS_CALL (::bind (fd, sockaddr_cast<::sockaddr> (& sa), sizeof (::sockaddr_ll)));
        }
    }

    // no auto-tuning for UDP, so do this:
    {
        int sz = 1 * 1024 * 1024; // TODO parameterize
        VR_CHECKED_SYS_CALL (::setsockopt (fd, SOL_SOCKET, SO_SNDBUF, & sz, sizeof (sz))); // TODO ?

#   if VR_DEBUG
        {
            sz = 0;
            ::socklen_t rsz { sizeof (sz) };
            ::getsockopt (fd, SOL_SOCKET, SO_SNDBUF, & sz, & rsz);

            LOG_trace1 << "actual sndbuf size: " << sz;
        }
#   endif
    }

    return { fd, AF_PACKET, false };
}
//............................................................................

socket_handle
socket_factory::create_TCP_client_endpoint (std::string const & host, std::string const & service,
                                            bool const connect, bool const disable_nagle)
{
    ::addrinfo hints;
    {
        std::memset (& hints, 0, sizeof (::addrinfo));

        hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP
    }

    ::addrinfo * gai_result { nullptr };
    int32_t rc;
    if (VR_UNLIKELY ((rc = ::getaddrinfo (host.c_str (), service.c_str (), & hints, & gai_result)) != 0))
        throw_x (io_exception, "getaddrinfo(" + print (host) + ", " + print (service) + ") failed: " + ::gai_strerror (rc));

    auto _ = make_scope_exit ([gai_result]() { if (gai_result) ::freeaddrinfo (gai_result); });

    for (::addrinfo * gai_r = gai_result; gai_r != nullptr; gai_r = gai_r->ai_next) // iterate over the 'getaddrinfo ()' result list
    {
        int32_t const fd = ::socket (gai_r->ai_family, gai_r->ai_socktype, gai_r->ai_protocol); // TODO SOCK_CLOEXEC
        if (fd < 0)
            continue;

        bool connected { false };

        if (connect)
        {
            if (VR_UNLIKELY (::connect (fd, gai_r->ai_addr, gai_r->ai_addrlen) < 0))
            {
                ::close (fd); // close and continue

                continue;
            }

            connected = true;
        }

        // finish setting 'fd' up (note: make it non-blocking *after* connecting):

        set_TCP_fd_opts (fd, disable_nagle, /* make_non_blocking */true);

        LOG_trace1 << "opened TCP client socket fd " << fd << " for <" << print (host) << ':' << print (service) << "> (" << sa_descriptor { gai_r->ai_family, gai_r->ai_addr, gai_r->ai_addrlen } << "), peer set: " << connected;
        return { fd, gai_r->ai_family, connected };
    }

    auto const e = errno; // set from the last 'socket ()'
    throw_x (io_exception, "could not open TCP socket for <" + print (host) + ':' + print (service) + ">: " + std::strerror (e));
}

socket_handle
socket_factory::create_TCP_server (std::string const & service)
{
    ::addrinfo hints;
    {
        std::memset (& hints, 0, sizeof (::addrinfo));

        hints.ai_flags = (AI_PASSIVE | AI_NUMERICSERV);
        hints.ai_family = AF_UNSPEC;        // IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;    // TCP
    }

    ::addrinfo * gai_result { nullptr };
    int32_t rc;
    if (VR_UNLIKELY ((rc = ::getaddrinfo (nullptr, service.c_str (), & hints, & gai_result)) != 0))
        throw_x (io_exception, "getaddrinfo(<local>, " + print (service) + ") failed: " + ::gai_strerror (rc));

    auto _ = make_scope_exit ([gai_result]() { if (gai_result) ::freeaddrinfo (gai_result); });

    int32_t fd { -1 };

    for (::addrinfo * gai_r = gai_result; gai_r != nullptr; gai_r = gai_r->ai_next) // iterate over the 'getaddrinfo ()' result list
    {
        fd = ::socket (gai_r->ai_family, gai_r->ai_socktype, gai_r->ai_protocol); // TODO SOCK_CLOEXEC
        if (fd < 0)
            continue; // try next address

        {
            int const on { 1 };
            VR_CHECKED_SYS_CALL (::setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, & on, sizeof (on)));
        }

        for (int32_t retry = 0; retry < 5; ++ retry)
        {
            int32_t bind_rc { };

            if (VR_LIKELY ((bind_rc = ::bind (fd, gai_r->ai_addr, gai_r->ai_addrlen)) == 0))
            {
                VR_CHECKED_SYS_CALL (::listen (fd, 32));

                LOG_trace1 << "opened TCP server socket fd " << fd << " for <" << print (service) << "> (" << sa_descriptor { gai_r->ai_family, gai_r->ai_addr, gai_r->ai_addrlen } << ')';
                return { fd, gai_r->ai_family, false };
            }

            auto const e = errno;
            if (e != EADDRINUSE)
                break;
            else // retry
            {
                LOG_warn << "binding to " << sa_descriptor { gai_r->ai_family, gai_r->ai_addr, gai_r->ai_addrlen } << ": retry " << (retry + 1) << " [" << std::strerror (e) << ']';
                sys::long_sleep_for ((2 * _1_second ()) << retry);
            }
        }

        ::close (fd); // close 'fd' and try next address
    }

    auto const e = errno; // set from the last 'socket ()' or 'bind()'
    throw_x (io_exception, "could not bind TCP server socket for <" + print (service) + "> to any address: " + std::strerror (e));
}
//............................................................................

void
socket_factory::set_TCP_fd_opts (int32_t const fd, bool const disable_nagle, bool const make_non_blocking)
{
    // [note: Linux doesn't have SO_NOSIGPIPE]

    if (disable_nagle)
    {
        int32_t const on { 1 };
        VR_CHECKED_SYS_CALL (::setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, & on, sizeof (on)));
    }
#   if VR_DEBUG
    {
        int32_t v { };
        ::socklen_t vsz { sizeof (v) };
        ::getsockopt (fd, IPPROTO_TCP, TCP_NODELAY, & v, & vsz);

        LOG_trace1 << "actual TCP_NODELAY: " << v;
    }
#   endif
    {
        int sz = 2 * 1024 * 1024; // TODO parameterize
        VR_CHECKED_SYS_CALL (::setsockopt (fd, SOL_SOCKET, SO_RCVBUF, & sz, sizeof (sz)));

#       if VR_DEBUG
        {
            sz = 0;
            ::socklen_t rsz { sizeof (sz) };
            ::getsockopt (fd, SOL_SOCKET, SO_RCVBUF, & sz, & rsz);

            LOG_trace1 << "actual rcvbuf size: " << sz;
        }
#       endif
    }
    // TODO SO_SNDBUF as well, if requested

    if (make_non_blocking)
    {
        int32_t const flags = VR_CHECKED_SYS_CALL (::fcntl (fd, F_GETFL, 0));
        VR_CHECKED_SYS_CALL (::fcntl (fd, F_SETFL, (flags | O_NONBLOCK)));
    }
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
