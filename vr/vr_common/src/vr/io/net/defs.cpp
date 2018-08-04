
#include "vr/io/net/defs.h"

#include <arpa/inet.h>  // inet_ntop()

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

std::string
print_net_addr (::in_addr const & a)
{
    char buf [INET_ADDRSTRLEN];

    string_literal_t const s = ::inet_ntop (AF_INET, & a, buf, sizeof (buf));
    if (VR_LIKELY (s != nullptr))
        return { s };

    auto const e = errno;
    throw_x (io_exception, "inet_ntop(AF_INET,...) failed: " + std::string { std::strerror (e) });
}
//............................................................................

std::string
sa_descriptor::addr () const
{
    vr_static_assert (INET6_ADDRSTRLEN >= INET_ADDRSTRLEN);

    char buf [INET6_ADDRSTRLEN];

    std::stringstream ss { };

    switch (m_family)
    {
        case AF_INET:
        {
            string_literal_t const s = ::inet_ntop (AF_INET, & sockaddr_cast<::sockaddr_in> (m_sa)->sin_addr, buf, sizeof (buf));
            if (s)
            {
                ss << s;
                return ss.str ();
            }

            auto const e = errno;
            throw_x (io_exception, "inet_ntop(AF_INET,...) failed: " + std::string { std::strerror (e) });
        }

        case AF_INET6:
        {
            string_literal_t const s = ::inet_ntop (AF_INET6, & sockaddr_cast<::sockaddr_in6> (m_sa)->sin6_addr, buf, sizeof (buf));
            if (s)
            {
                ss << s;
                return ss.str ();
            }

            auto const e = errno;
            throw_x (io_exception, "inet_ntop(AF_INET6,...) failed: " + std::string { std::strerror (e) });
        }

    } // end of switch

    throw_x (invalid_input, "address family " + string_cast (m_family) + " not supported");
}

uint16_t
sa_descriptor::port () const
{
    switch (m_family)
    {
        case AF_INET:   return sockaddr_cast<::sockaddr_in> (m_sa)->sin_port;
        case AF_INET6:  return sockaddr_cast<::sockaddr_in6> (m_sa)->sin6_port;

    } // end of switch

    throw_x (invalid_input, "address family " + string_cast (m_family) + " not supported");
}
//............................................................................

std::ostream &
operator<< (std::ostream & os, sa_descriptor const & obj)
{
    vr_static_assert (INET6_ADDRSTRLEN >= INET_ADDRSTRLEN);

    char buf [INET6_ADDRSTRLEN];

    string_literal_t s;
    switch (obj.m_sa->sa_family)
    {
        case AF_INET:
        {
            s = ::inet_ntop (AF_INET, & sockaddr_cast<::sockaddr_in> (obj.m_sa)->sin_addr, buf, sizeof (buf));
            if (s)
                return os << s;

            auto const e = errno;
            throw_x (io_exception, "inet_ntop(AF_INET,...) failed: " + std::string { std::strerror (e) });
        }

        case AF_INET6:
        {
            s = ::inet_ntop (AF_INET6, & sockaddr_cast<::sockaddr_in6> (obj.m_sa)->sin6_addr, buf, sizeof (buf));
            if (s)
                return os << s;

            auto const e = errno;
            throw_x (io_exception, "inet_ntop(AF_INET6,...) failed: " + std::string { std::strerror (e) });
        }

    } // end of switch

    throw_x (invalid_input, "address family " + string_cast (obj.m_sa->sa_family) + " not supported");
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
