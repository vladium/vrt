#pragma once

#include "vr/io/net/mcast_source.h"
#include "vr/io/net/socket_handle.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{

class socket_factory final: noncopyable
{
    public: // ...............................................................

        /**
         * this can be used for joining multiple UDP multicast groups (with possibly different sources and ports)
         * but will not receive actual multicast traffic (doesn't 'bind()')
         *
         * @note group membership is kept (addref'd) as long as this socket is not closed (by destructor)
         *
         * @deprecated use
         */
        static socket_handle create_UDP_multicast_member (bool const disable_loopback, int32_t const family = AF_INET);

        /**
         * @param mss multicast source descriptors to join
         */
        static socket_handle create_and_join_UDP_multicast_member (int32_t const ifc_index, std::vector<mcast_source> const & mss, bool const disable_loopback, int32_t const family = AF_INET);

        /**
         * returns the first UDP socket to successfully open on a ::getaddrinfo('host', 'service') result
         *
         * @param set_peer if 'true' the returned UDP socket will be connect()ed
         */
        static socket_handle create_UDP_client_endpoint (std::string const & host, std::string const & service, bool const set_peer = false);

        /**
         * @param type either SOCK_RAW for raw packets or SOCK_DGRAM for cooked packets (without eth header)
         */
        static socket_handle create_link_recv_endpoint (int32_t const ifc_index, int32_t const type = SOCK_RAW);

        /**
         * @param type either SOCK_RAW for raw packets or SOCK_DGRAM for cooked packets (without eth header)
         */
        static socket_handle create_link_send_endpoint (int32_t const ifc_index, int32_t const type = SOCK_RAW);

        /**
         * @return non-blocking TCP socket handle
         */
        static socket_handle create_TCP_client_endpoint (std::string const & host, std::string const & service,
                                                         bool const connect = true, bool const disable_nagle = true);
        /**
         * @note this is currently for testing and sim/mock tools
         *
         * @return blocking TCP socket handle for accept()s
         *
         * @see socket_handle::accept()
         */
        static socket_handle create_TCP_server (std::string const & service);

    private: // ..............................................................

        friend class socket_handle;

        static void set_TCP_fd_opts (int32_t const fd, bool const disable_nagle, bool const make_non_blocking);

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
