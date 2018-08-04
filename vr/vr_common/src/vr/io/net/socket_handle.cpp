
#include "vr/io/net/socket_handle.h"

#include "vr/io/exceptions.h"
#include "vr/io/net/socket_factory.h"
#include "vr/io/net/utility.h"
#include "vr/sys/defs.h"        // VR_CHECKED_SYS_CALL
#include "vr/util/logging.h"
#include "vr/utility.h"

#include <cstring>

#include <fcntl.h>
#include <linux/if_packet.h>    // AF_PACKET, tpacket_stats
#include <linux/net_tstamp.h>   // SOF_TIMESTAMPING_*, hwtstamp_config
#include <linux/sockios.h>      // SIOCSHWTSTAMP
#include <net/ethernet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

int32_t
family_to_level (int32_t const family)
{
    switch (family)
    {
        case AF_INET:   return IPPROTO_IP;
        case AF_INET6:  return IPPROTO_IPV6;

        default:    return -1;

    } // end of switch
}

} // end of anonymous
//............................................................................
//............................................................................

socket_handle::socket_handle (int32_t const fd, int32_t const family, bool const peer_set) :
    m_fd { fd },
    m_family { family },
    m_peer_set { peer_set }
{
    assert_nonnegative (fd);

    // TODO bufsize
}

socket_handle::socket_handle (socket_handle && rhs) :
    m_fd { rhs.m_fd },
    m_family { rhs.m_family },
    m_opts_set { rhs.m_opts_set },
    m_ts_policy { rhs.m_ts_policy },
    m_peer_set { rhs.m_peer_set }
{
    rhs.m_fd = -1; // grab fd ownership
}


socket_handle::~socket_handle () VR_NOEXCEPT
{
    close ();
}
//............................................................................

void
socket_handle::close () VR_NOEXCEPT
{
    int32_t const fd = m_fd;
    if (fd >= 0)
    {
        m_fd = -1;
        VR_CHECKED_SYS_CALL_noexcept (::close (fd));

        LOG_trace1 << "closed socket fd " << fd;
    }
}
//............................................................................

int32_t
socket_handle::type () const
{
    int t { };
    ::socklen_t tsz { sizeof (t) };
    VR_CHECKED_SYS_CALL (::getsockopt (m_fd, SOL_SOCKET, SO_TYPE, & t, & tsz));

    return t;
}

bool
socket_handle::blocking () const
{
    int32_t const flags = VR_CHECKED_SYS_CALL (::fcntl (m_fd, F_GETFL, 0));
    return (! (flags & O_NONBLOCK));
}
//............................................................................

int32_t
socket_handle::read_buf_size () const
{
    int sz { };
    ::socklen_t rsz { sizeof (sz) };
    VR_CHECKED_SYS_CALL (::getsockopt (m_fd, SOL_SOCKET, SO_RCVBUF, & sz, & rsz));

    return sz;
}

int32_t
socket_handle::write_buf_size () const
{
    int sz { };
    ::socklen_t rsz { sizeof (sz) };
    VR_CHECKED_SYS_CALL (::getsockopt (m_fd, SOL_SOCKET, SO_SNDBUF, & sz, & rsz));

    return sz;
}
//............................................................................

addr_and_port
socket_handle::peer_name () const
{
    ::sockaddr_storage sas { };
    ::socklen_t addr_len { sizeof (sas) };

    VR_CHECKED_SYS_CALL (::getpeername (m_fd, sockaddr_cast<::sockaddr> (& sas), & addr_len));

    sa_descriptor const sa { m_family, sockaddr_cast<::sockaddr> (& sas), addr_len };

    return std::make_tuple (sa.addr (), sa.port ());
}
//............................................................................

int64_t
socket_handle::drain_rx_queue ()
{
    vr_static_assert (EAGAIN == EWOULDBLOCK);

    int64_t r { };
    int8_t buf_byte;

    for (r = 0; true; ++ r)
    {
        signed_size_t const rc = ::recv (m_fd, & buf_byte, sizeof (buf_byte), MSG_DONTWAIT);
        if (rc < 0)
        {
            auto const e = errno;
            if (VR_LIKELY (e == EWOULDBLOCK))
                break;
            else
                throw_x (sys_exception, "recv() error (" + string_cast (e) + "): " + std::strerror (e));
        }
    }

    return r;
}
//............................................................................

void
socket_handle::join_multicast (sa_descriptor const & group_sa, sa_descriptor const & source_sa, int32_t const ifc_index)
{
    if (! (m_opts_set & (1L << SO_REUSEADDR)))
    {
        LOG_trace1 << "  enabling SO_REUSEADDR for socket fd " << m_fd;

        int const on { 1 };

        VR_CHECKED_SYS_CALL (::setsockopt (m_fd, SOL_SOCKET, SO_REUSEADDR, & on, sizeof (on)));
        m_opts_set |= (1L << SO_REUSEADDR);
    }

    ::group_source_req req;
    {
        req.gsr_interface = ifc_index;

        check_ge (sizeof (req.gsr_group), group_sa.m_len);
        check_ge (sizeof (req.gsr_source), source_sa.m_len);

        // note: 'req' only sets the interface index and IP address fields (port fields are ignored)

        std::memcpy (& req.gsr_group, group_sa.m_sa, group_sa.m_len);
        std::memcpy (& req.gsr_source, source_sa.m_sa, source_sa.m_len);
    }

    VR_CHECKED_SYS_CALL (::setsockopt (m_fd, family_to_level (m_family), MCAST_JOIN_SOURCE_GROUP, & req, sizeof (req)));
    m_opts_set |= (1L << MCAST_JOIN_SOURCE_GROUP);
}

void
socket_handle::join_multicast (sa_descriptor const & group_sa, sa_descriptor const & source_sa, std::string const & ifc)
{
    join_multicast (group_sa, source_sa, (ifc.empty () ? 0 : ifc_index (ifc)));
}
//............................................................................

std::pair<uint32_t, uint32_t>
socket_handle::statistics_snapshot ()
{
    assert_eq (m_family, AF_PACKET);

    ::tpacket_stats ps;
    ::socklen_t ps_len { sizeof (ps) };

    VR_CHECKED_SYS_CALL (::getsockopt (m_fd, SOL_PACKET, PACKET_STATISTICS, & ps, & ps_len));

    return { ps.tp_packets, ps.tp_drops };
}
//............................................................................

void
socket_handle::set_multicast_ifc (int32_t const ifc_index)
{
    // TODO check m_opts_set bit

    switch (m_family)
    {
        case AF_INET:
        {
            ::in_addr a;

            if (ifc_index == 0)
                a.s_addr = ::htonl (INADDR_ANY);
            else
            {
                std::string const name = ifc_name (ifc_index);

                ::ifreq req;
                std::memset (& req, 0, sizeof (req));
                std::strncpy (req.ifr_name, name.c_str (), IFNAMSIZ);

                VR_CHECKED_SYS_CALL (::ioctl (m_fd, SIOCGIFADDR, & req));

                std::memcpy (& a, & sockaddr_cast<::sockaddr_in> (& req.ifr_addr)->sin_addr, sizeof (::in_addr));
            }

            VR_CHECKED_SYS_CALL (::setsockopt (m_fd, IPPROTO_IP,   IP_MULTICAST_IF,   & a, sizeof (::in_addr)));
            (ifc_index ? m_opts_set |= (1L << IP_MULTICAST_IF) : m_opts_set &= ~(1L << IP_MULTICAST_IF));
        }
        return;

        case AF_INET6:
        {
            ::u_int const i = ifc_index;

            VR_CHECKED_SYS_CALL (::setsockopt (m_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, & i, sizeof (i)));
            (ifc_index ? m_opts_set |= (1L << IPV6_MULTICAST_IF) : m_opts_set &= ~(1L << IPV6_MULTICAST_IF));
        }
        return;

    } // end of switch

    VR_ASSUME_UNREACHABLE ();
}

void
socket_handle::set_multicast_ifc (std::string const & ifc)
{
    set_multicast_ifc (ifc.empty () ? 0 : ifc_index (ifc));
}
//............................................................................

socket_handle
socket_handle::accept (stop_flag const & sf, timestamp_t const poll_interval)
{
    int32_t const timeout = poll_interval / _1_millisecond ();

    ::pollfd pfd { };
    {
        pfd.fd = m_fd;
        pfd.events = POLLIN | POLLRDHUP;
    }

    while (true)
    {
        int32_t const rc = VR_CHECKED_SYS_CALL (::poll (& pfd, 1, timeout));
        if ((rc == 1) & (pfd.revents & POLLIN))
            break;

        if (sf.is_raised ())
        {
            DLOG_trace1 << "accept(): stop requested";
            throw_cfx (stop_iteration);
        }
    }

    ::sockaddr_storage sas { }; // TODO not used except for logging
    ::socklen_t addr_len { sizeof (sas) };

    int32_t const fd = VR_CHECKED_SYS_CALL (::accept (m_fd, sockaddr_cast<::sockaddr> (& sas), & addr_len));

    sa_descriptor const sa { m_family, sockaddr_cast<::sockaddr> (& sas), addr_len };
    LOG_trace1 << "accepted connection from [" << sa.addr () << ':' << sa.port () << "] into fd " << fd;

    bool commit { false };
    auto _ = make_scope_exit ([fd, & commit]() { if (! commit) ::close (fd); }); // close 'fd' if the next line fails

    socket_factory::set_TCP_fd_opts (fd, /* disable_nagle */true, /* make_non_blocking */true);
    commit = true;

    return { fd, m_family, true };
}
//............................................................................

ts_policy::enum_t
socket_handle::enable_rx_timestamps (int32_t const ifc_index, ts_policy::enum_t const policy)
{
    return enable_rx_timestamps (ifc_name (ifc_index), policy);
}

ts_policy::enum_t
socket_handle::enable_rx_timestamps (std::string const & ifc, ts_policy::enum_t const policy)
{
    if (m_opts_set & (1L << SO_TIMESTAMPING)) return m_ts_policy;
    LOG_trace1 << "  enabling SO_TIMESTAMPING for socket fd " << m_fd;

    bool hw_tx_on { false };

    if (policy != ts_policy::sw)
    {
        try
        {
            ::hwtstamp_config req_cfg;
            {
                std::memset (& req_cfg, 0, sizeof (req_cfg));
                req_cfg.tx_type = HWTSTAMP_TX_ON;
                req_cfg.rx_filter = HWTSTAMP_FILTER_ALL; // TODO query for possible options here
            }
            ::ifreq req;
            {
                std::memset (& req, 0, sizeof (req));
                std::strncpy (req.ifr_name, ifc.c_str (), sizeof (req.ifr_name));
                req.ifr_data = char_ptr_cast (& req_cfg);
            }

            VR_CHECKED_SYS_CALL (::ioctl (m_fd, SIOCSHWTSTAMP, & req)); // note: update 'req' to reflect what's been granted
            LOG_trace1 << "hw timestamping obtained: tx_type " << req_cfg.tx_type << ", rx_filter " << req_cfg.rx_filter;

            hw_tx_on = (req_cfg.tx_type == HWTSTAMP_TX_ON);
        }
        catch (sys_exception const & se)
        {
            LOG_warn << "hw timestamping refused: " << se.what ();
        }
    }

    ts_policy::enum_t r;

    int flags { };
    {
        if (hw_tx_on) // implies ('hw_fallback_to_sw' or 'hw')
        {
            flags = (SOF_TIMESTAMPING_RX_HARDWARE | /* report via control msg */SOF_TIMESTAMPING_RAW_HARDWARE);
            r = ts_policy::hw;
        }
        else
        {
            if (policy == ts_policy::hw)
                throw_x (sys_exception, "hardware-only rx timestamps requested, but hardware timestamping is not available");

            flags = (SOF_TIMESTAMPING_RX_SOFTWARE | /* report via control msg */SOF_TIMESTAMPING_SOFTWARE);
            r = ts_policy::sw;
        }
    }

    VR_CHECKED_SYS_CALL (::setsockopt (m_fd, SOL_SOCKET, SO_TIMESTAMPING, & flags, sizeof (flags)));
    m_opts_set |= (1L << SO_TIMESTAMPING);

    return (m_ts_policy = r);
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
