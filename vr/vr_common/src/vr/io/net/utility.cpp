
#include "vr/io/net/utility.h"

#include "vr/io/net/mcast_source.h"
#include "vr/sys/defs.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"
#include "vr/util/parse.h"
#include "vr/util/singleton.h"

#include <cstring>

#include <arpa/inet.h>  // inet_pton
#include <net/if.h>     // if_nametoindex
#include <sys/ioctl.h>
#include <unistd.h>     // gethostname

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................
//............................................................................
namespace
{

struct hostname_impl final
{
    hostname_impl ()
    {
       char buf [HOST_NAME_MAX + 1];
       VR_CHECKED_SYS_CALL (::gethostname (buf, sizeof (buf)));

       m_name [0] = buf;

       auto const dot = std::strchr (buf, '.');
       if (dot != nullptr) (* dot) = 0;

       m_name [1] = buf;
    }

    std::string m_name [2];

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::string const &
hostname (bool const short_form)
{
    return util::singleton<hostname_impl>::instance ().m_name [short_form];
}
//............................................................................

int32_t
ifc_index (std::string const & ifc)
{
    int32_t const ix = ::if_nametoindex (ifc.c_str ());
    if (ix) // success (non-zero return)
    {
        LOG_trace1 << "ifc " << print (ifc) << " maps to index " << ix;
        return ix;
    }
    // else error (zero return)

    auto const e = errno;
    throw_x (io_exception, "couldn't find index for interface " + print (ifc) + ": " + std::strerror (e));
}

std::string
ifc_name (int32_t const ifc_index)
{
    check_positive (ifc_index);

    char name [IF_NAMESIZE];
    string_literal_t const n = ::if_indextoname (ifc_index, name);

    if (n)
    {
        LOG_trace1 << "ifc_index " << ifc_index << " is interface " << print (name);
        return { n };
    }

    auto const e = errno;
    throw_x (io_exception, "couldn't find name of interface #" + string_cast (ifc_index) + ": " + std::strerror (e));
}
//............................................................................

void
ifc_MAC_addr (std::string const & ifc, int32_t const sock_fd, uint8_t (& addr) [8])
{
    vr_static_assert (IFHWADDRLEN <= sizeof (addr));

    ::ifreq req;
    {
        std::memset (& req, 0, sizeof (req));
        std::strncpy (req.ifr_name, ifc.c_str (), IFNAMSIZ);
    }
    VR_CHECKED_SYS_CALL (::ioctl (sock_fd, SIOCGIFHWADDR, & req));

    std::memcpy (addr, & req.ifr_hwaddr, IFHWADDRLEN);
}

void
ifc_MAC_addr (int32_t const ifc_index, int32_t const sock_fd, uint8_t (& addr) [8])
{
    ifc_MAC_addr (ifc_name (ifc_index), sock_fd, addr);
}
//............................................................................

sa_descriptor
to_net_addr (std::string const & s, ::sockaddr_storage & out)
{
    if (::inet_pton (AF_INET, s.c_str (), & sockaddr_cast<::sockaddr_in> (& out)->sin_addr) == 1)
    {
        ::sockaddr_in * const sa = sockaddr_cast<::sockaddr_in> (& out);
        sa->sin_family = AF_INET;

        return { AF_INET, sockaddr_cast<::sockaddr> (& out) , static_cast<::socklen_t> (sizeof (::sockaddr_in)) };
    }

    if (::inet_pton (AF_INET6, s.c_str (), & sockaddr_cast<::sockaddr_in6> (& out)->sin6_addr) == 1)
    {
        ::sockaddr_in6 * const sa = sockaddr_cast<::sockaddr_in6> (& out);
        sa->sin6_family = AF_INET6;

        return { AF_INET6, sockaddr_cast<::sockaddr> (& out), static_cast<::socklen_t> (sizeof (::sockaddr_in6)) };
    }

    throw_x (invalid_input, "invalid printable AF_INET[6] address " + print (s));
}

sa_descriptor
to_net_addr (std::string const & s, uint16_t const port, ::sockaddr_storage & out)
{
    sa_descriptor const r = to_net_addr (s, out);

    switch (r.m_family)
    {
        case AF_INET:  sockaddr_cast<::sockaddr_in>  (r.m_sa)->sin_port  = htons (port); break;
        case AF_INET6: sockaddr_cast<::sockaddr_in6> (r.m_sa)->sin6_port = htons (port); break;

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch

    return r;
}
//............................................................................

addr_and_port
parse_addr_and_port (std::string const & s)
{
    auto const sep_A_ix = s.find_last_of (':'); // note: be careful with IPv6
    auto const sep_B_ix = s.find_last_of ('#');

    auto const sep_ix = std::min (sep_A_ix, sep_B_ix);

    int32_t port { -1 };
    if (sep_ix != std::string::npos)
    {
        check_positive (sep_ix, s);

        port = util::parse_decimal<int32_t> (& s [sep_ix + 1], s.size () - sep_ix - 1) ;
        check_within_inclusive (port, 65535);
    }

    return std::make_tuple (s.substr (0, /* ok if 'npos' */sep_ix), port);
}
//............................................................................

IP_range_filter
make_group_range_filter (mcast_source const & ms)
{
    uint32_t lo { std::numeric_limits<uint32_t>::max () };
    uint32_t hi { };

    for (addr_and_port const & group_ap : ms.groups ())
    {
        ::sockaddr_storage group_sas { };
        sa_descriptor const group_sa { to_net_addr (std::get<0> (group_ap), group_sas) };

        check_eq (group_sa.m_family, AF_INET); // IPv4 only for now
        uint32_t const g = util::net_to_host (sockaddr_cast<::sockaddr_in>  (group_sa.m_sa)->sin_addr.s_addr);

        lo = std::min (lo, g);
        hi = std::max (hi, g);
    }

    return { lo, hi };
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
