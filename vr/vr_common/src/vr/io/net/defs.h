#pragma once

#include "vr/asserts.h"
#include "vr/enums.h"
#include "vr/io/defs.h" // _ts_local_, _ts_origin_
#include "vr/meta/tags.h"
#include "vr/strings.h" // print()

#include <tuple>

#include <netinet/in.h> // in_addr, INET*_ADDRSTRLEN
#include <sys/socket.h> // sockaddr, sockaddr_storage
#include <sys/types.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

VR_META_TAG (packet_index);

VR_META_TAG (src_port);
VR_META_TAG (dst_port);

VR_META_TAG (filtered);

//............................................................................

capacity_t constexpr default_mcast_link_recv_capacity ()    { return  (64 * 1024); };
capacity_t constexpr default_tcp_link_capacity ()           { return (256 * 1024); };

//............................................................................

template<int32_t PROTO>
struct proto final
{
    static constexpr int32_t value  = PROTO;

}; // end of class
//............................................................................

VR_ENUM (ts_policy, // NOTE: the order is chosen so that int casting results in 0 for 'sw' and 2 for 'hw'
    (
        sw,
        hw_fallback_to_sw,
        hw
    ),
    printable, parsable

); // end of enum
//............................................................................

using addr_and_port         = std::tuple<std::string, int32_t>;

//............................................................................

template<typename SA, typename SA_RHS>
SA const *
sockaddr_cast (SA_RHS const * const sa)
{
    return static_cast<SA const *> (static_cast<addr_const_t> (sa));
}

template<typename SA, typename SA_RHS>
SA *
sockaddr_cast (SA_RHS * const sa)
{
    return static_cast<SA *> (static_cast<addr_t> (sa));
}
//............................................................................

struct sa_descriptor final
{
    sa_descriptor () = default;

    sa_descriptor (int32_t const family, ::sockaddr * const sa, ::socklen_t const len) :
        m_sa { sa },
        m_len { len },
        m_family { family }
    {
        assert_nonnull (sa);
    }

    // ACCESSORs:

    std::string addr () const;
    uint16_t port () const;

    // FRIENDs:

    friend std::ostream & operator<< (std::ostream & os, sa_descriptor const & obj);


    ::sockaddr * const m_sa { };
    ::socklen_t const m_len { };
    int32_t const m_family { };

}; // end of class
//............................................................................

extern std::string
print_net_addr (::in_addr const & a); // note: IPv4 only for now

//............................................................................

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
