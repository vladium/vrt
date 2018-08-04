
#include "vr/io/links/UDP_mcast_link.h"

#include "vr/io/net/socket_factory.h"
#include "vr/io/net/utility.h"
#include "vr/util/parse.h"

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
open_mcast_recv_link (std::string const & ifc_name, int32_t const type, net::ts_policy::enum_t const tsp)
{
    check_condition ((type == SOCK_RAW) | (type == SOCK_DGRAM), type);

    LOG_trace1 << "opening recv " << (type == SOCK_RAW ? "raw" : "cooked") << " packet socket on ifc " << print (ifc_name) << ", tsp " << print (tsp);

    int32_t const ifc_index = net::ifc_index (ifc_name);
    LOG_trace2 << "ifc " << print (ifc_name) << " index is #" << ifc_index;

    net::socket_handle sh { net::socket_factory::create_link_recv_endpoint (ifc_index, type) };
    sh.enable_rx_timestamps (ifc_index, tsp);

    return sh;
}

net::socket_handle
open_mcast_send_link (std::string const & ifc_name, int32_t const type)
{
    check_condition ((type == SOCK_RAW) | (type == SOCK_DGRAM), type);

    LOG_trace1 << "opening send " << (type == SOCK_RAW ? "raw" : "cooked") << " packet socket on ifc " << print (ifc_name);

    int32_t const ifc_index = net::ifc_index (ifc_name);
    LOG_trace2 << "ifc " << print (ifc_name) << " index is #" << ifc_index;

    return net::socket_factory::create_link_send_endpoint (ifc_index, type);
}
//............................................................................

net::socket_handle
join_mcast_sources (std::string const & ifc_name, std::vector<net::mcast_source> const & sources)
{
    int32_t const ifc_index = net::ifc_index (ifc_name);
    LOG_trace2 << "  ifc " << print (ifc_name) << " index is #" << ifc_index;

    return net::socket_factory::create_and_join_UDP_multicast_member (ifc_index, sources, /* disable_loopback */false);
}
//............................................................................

// TODO temp impl (need to deal with hw/sw timestamping)
capacity_t
mcast_send (net::socket_handle const & sh, addr_const_t const src, capacity_t const len)
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
