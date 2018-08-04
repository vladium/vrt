#pragma once

#include "vr/io/net/defs.h"
#include "vr/util/ops_int.h"
#include "vr/util/type_traits.h"

#include <netinet/ip.h>
#include <netinet/udp.h>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename>
struct print_impl; // forward


template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<::in_addr, T>::type>
{
    static std::string evaluate (::in_addr const & a)
    {
        return io::net::print_net_addr (a);
    }

}; // end of specialization
//............................................................................

template<typename T, typename>
struct print_impl; // forward


template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<::ip, T>::type>
{
    static std::string evaluate (::ip const & ip_hdr) // TODO assumes IPv4
    {
        std::stringstream ss;

        int32_t const ip_hl     = ((ip_hdr.ip_hl) << 2);

        ss << "ip_len: " << util::net_to_host (ip_hdr.ip_len) << " (ip_hl: " << ip_hl << "), ip_p: " << static_cast<uint32_t> (ip_hdr.ip_p)
           << ", ip_src: " << io::net::print_net_addr (ip_hdr.ip_src)
           << ", ip_dst: " << io::net::print_net_addr (ip_hdr.ip_dst);

        return ss.str ();
    }

}; // end of specialization

template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<::udphdr, T>::type>
{
    static std::string evaluate (::udphdr const & udp_hdr)
    {
        std::stringstream ss;

        ss << "udp_len: " << util::net_to_host (udp_hdr.len)
           << ", src port: " << util::net_to_host (udp_hdr.source)
           << ", dst port: " << util::net_to_host (udp_hdr.dest);

        return ss.str ();
    }

}; // end of specialization


} // end of 'impl'
//............................................................................
//............................................................................

} // end of namespace
//----------------------------------------------------------------------------
