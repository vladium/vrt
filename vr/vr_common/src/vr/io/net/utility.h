#pragma once

#include "vr/io/net/defs.h"
#include "vr/io/net/filters.h"
#include "vr/util/type_traits.h"

#include <boost/integer/static_min_max.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
class mcast_source; // forward

//............................................................................

template<typename V, typename = void>
struct min_size_or_zero
{
    static constexpr int32_t value ()       { return 0; }

}; // end of master

template<typename V>
struct min_size_or_zero<V,
    util::void_t<decltype (V::min_size ())>>
{
    static constexpr int32_t value ()       { return V::min_size (); }

}; // end of specialization
//............................................................................

template<typename ... Es>
struct min_min_size;

template<typename E>
struct min_min_size<E>
{
    static constexpr int32_t value ()   { return min_size_or_zero<E>::value (); }

}; // end of metafunction

template<typename E, typename ... Es>
struct min_min_size<E, Es ...>
{
    static constexpr int32_t value ()   { return boost::static_signed_min<min_size_or_zero<E>::value (), min_min_size<Es ...>::value ()>::value; }

}; // end of metafunction
//............................................................................

extern std::string const &
hostname (bool const short_form = true);

//............................................................................

extern int32_t
ifc_index (std::string const & ifc);

extern std::string
ifc_name (int32_t const ifc_index);

//............................................................................

extern void
ifc_MAC_addr (std::string const & ifc, int32_t const sock_fd, uint8_t (& addr) [8]);

extern void
ifc_MAC_addr (int32_t const ifc_index, int32_t const sock_fd, uint8_t (& addr) [8]);

//............................................................................
/**
 * @return AF_INET or AF_INET6 result within 'out'
 */
extern sa_descriptor
to_net_addr (std::string const & s, ::sockaddr_storage & out);

extern sa_descriptor
to_net_addr (std::string const & s, uint16_t const port, ::sockaddr_storage & out);

//............................................................................

/**
 * @param s "ipv4/6", "ipv4/6:port"
 * @return [if no port token specified, 2nd slot will be -1]
 */
extern addr_and_port
parse_addr_and_port (std::string const & s);

//............................................................................

extern IP_range_filter
make_group_range_filter (mcast_source const & ms);

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
