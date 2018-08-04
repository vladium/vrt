
#include "vr/market/cap/mcast_capture.h"

#include "vr/fields.h"
#include "vr/io/events/event_context.h"
#include "vr/io/files.h"
#include "vr/io/mapped_window_buffer.h"
#include "vr/io/net/io.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/socket_factory.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/net/utility.h"
#include "vr/io/pcap/pcaprec_hdr.h"
#include "vr/rt/timer_queue/timer_queue.h"
#include "vr/sys/os.h"
#include "vr/util/parse.h"
#include "vr/util/logging.h"

#include <pcap/pcap.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
using namespace io;
using namespace io::net;
using namespace rt;

//............................................................................
//............................................................................
namespace
{
//............................................................................

using IP_filter_context     = event_context<_filtered_, _packet_index_, _ts_local_>; // no benefit in _ts_local_delta_ unless used for quality control
using IP_proto_filter       = IP_<UDP_<>>; // only interested in UDP (the fact that it will be ITCH multicast is ensured with host setup)

//............................................................................

constexpr int32_t min_out_capacity  = 1024 * 1024;  // TODO c.f. 'itch_data_link_capacity ()'
constexpr int32_t min_out_window    =   64 * 1024;  // the link will increase this if the socket has a larger recv buffer

using packet_hr_type                = io::pcaprec_hdr<util::subsecond_duration_ns, /* little endian */true>;

constexpr int32_t packet_hdr_size   = sizeof (packet_hr_type);

//............................................................................

int32_t
capture_linktype_for (net::socket_handle const & s)
{
    int32_t const s_type = s.type ();
    switch (s_type)
    {
        case SOCK_RAW:      return DLT_EN10MB;      // LINKTYPE_ETHERNET
        case SOCK_DGRAM:    return DLT_LINUX_SLL;   // LINKTYPE_LINUX_SLL, Linux cooked capture encapsulation

        default: throw_x (invalid_input, "unexpected socket type " + string_cast (s_type));

    } // end of switch
}

} // end of anonymous
//............................................................................
//............................................................................

mcast_capture::mcast_capture (std::string const & ifc, std::vector<net::mcast_source> const & sources, net::ts_policy::enum_t const tsp,
                              fs::path const & out_dir, std::string const & capture_ID,
                              int32_t const PU) :
    mc::bound_runnable (PU),
    m_ifc { ifc },
    m_sources (sources), // note: vector init
    m_out_file { out_dir / capture_ID / join_as_name ("mcast", "recv", ifc, "pcap") },
    m_tsp { tsp }
{
    dep (m_timer_queue) = "timers";

    // create output dir (trigger any fs errors early, if any):

    io::create_dirs (m_out_file.parent_path ());
}
//............................................................................

mcast_capture::stats const &
mcast_capture::capture_stats () const
{
    // TODO validate that run() has been invoked

    return m_stats;
}
//............................................................................

void
mcast_capture::operator() ()
{
    using super         = mc::bound_runnable;

    assert_nonnull (m_timer_queue);
    timer_queue const & timers = (* m_timer_queue);

    timer const & start_timer = timers ["start.capture"];
    timer const & stop_timer  = timers ["stop.capture"];

    int64_t packet_index { };
    std::pair<uint32_t, uint32_t> ss { };
    uint32_t ip_filtered_count { };

    int32_t const ifc_index = net::ifc_index (m_ifc);
    LOG_info << "ifc " << print (m_ifc) << " index is #" << ifc_index;

    super::bind ();
    {
        // create IP filter:

        IP_filter_context ip_filter_ctx { };
        IP_proto_filter ip_proto_filter { { } };

        // open a socket to create/maintain multicast membership(s):

        net::socket_handle const sgrp { net::socket_factory::create_and_join_UDP_multicast_member (ifc_index, m_sources, /* disable_loopback */false) };

        // open a socket for packet capture:

        net::socket_handle scap { net::socket_factory::create_link_recv_endpoint (ifc_index) };

        net::ts_policy::enum_t const tsp = scap.enable_rx_timestamps (ifc_index, m_tsp);
        LOG_info << "timestamp source is " << print (tsp);

        // create output file (trigger any fs errors early, if any):
        // [note: create/allocate *after* PU binding]

        // ['mapped_ofile' could be used now as well, but this preps design for concurrent producer/consumer impl]

        io::mapped_window_buffer out { m_out_file, min_out_capacity, min_out_window, default_mmap_reserve_size () };
        LOG_info << "created output buffer " << print (m_out_file) << ", min capacity: " << out.capacity () << ", write window: " << out.w_window ();

        int64_t recv_total { };
        addr_t w = out.w_position ();

        // emit pcap file header:
        {
            ::pcap_file_header * const h = static_cast<::pcap_file_header *> (w);

            h->magic = VR_PCAP_MAGIC_LE_NS;
            h->version_major = PCAP_VERSION_MAJOR;
            h->version_minor = PCAP_VERSION_MINOR;
            h->thiszone = 0; // GMT
            h->sigfigs = packet_hr_type::ts_fraction_digits ();
            h->snaplen = out.w_window ();
            h->linktype = capture_linktype_for (scap); // coordinate with 'scap' socket type

            constexpr int32_t step  = sizeof (::pcap_file_header);

            w = out.w_advance (step);
            out.r_advance (step); // "commit" 'h'

            recv_total += step;
        }
        check_zero (out.size ()); // 'out' is flushed


        int32_t const fd = scap.fd ();
        int32_t const ts_ix = (tsp == net::ts_policy::sw ? 0 : 2);

        ::iovec iov;
        {
            iov.iov_len = out.w_window ();
        }
        union
        {
            ::cmsghdr cm;
            char data [64];
        }
        msg_control;
        ::msghdr msg;
        {
            std::memset (& msg, 0, sizeof (msg));

            msg.msg_control = & msg_control;

            msg.msg_iov = & iov;
            msg.msg_iovlen = 1;
        }

        // to avoid benign packet drops registering in netstat, etc during this startup wait, a better strategy
        // would be to use a "draining loop" that would actually consume packets, but not write them to the capture:
        //
        // [on the other hand, filling up the RX queue will "warm up" the queue memory in this core's cache]

        sys::sleep_until (start_timer.ts_utc ());
        auto const drain_rc = scap.drain_rx_queue (); // otherwise early packets in the queue create a benign gap at the start of the sequence

        LOG_info << "=====[ capture started (discarded " << drain_rc << " stale packet(s)) ]=====";

        scap.statistics_snapshot (); // reset stats counters

        try
        {
            while (! stop_timer.expired ())
            {
                // invariant: when waiting for 'recvmsg()' the iov destination ('dst') points to just
                // after a blank/pre-allocated packet header:

                addr_t const dst = addr_plus (w, packet_hdr_size);

                iov.iov_base = dst;
                msg.msg_controllen = sizeof (msg_control);

                // TODO consider using 'recvmmsg()' for draining packets if type of 'out' permits

                signed_size_t const rc = ::recvmsg (fd, & msg, 0); // TODO should make this non-blocking
                VR_IF_DEBUG (timestamp_t const ts_sys_utc = sys::realtime_utc ();)

                if (VR_UNLIKELY (rc < 0))
                {
                    auto const e = errno;
                    throw_x (io_exception, "recvmsg() error: " + std::string { std::strerror (e) });
                }

                // IP-level filtering:
                {
                    if (has_field<_packet_index_, IP_filter_context> ())
                    {
                        field<_packet_index_> (ip_filter_ctx) = packet_index;
                    }

                    vr_static_assert (has_field<_filtered_, IP_filter_context> ());

                    field<_filtered_> (ip_filter_ctx) = false;
                    {
                        VR_IF_DEBUG (int32_t const ipf_rc = )ip_proto_filter.consume (ip_filter_ctx, dst, rc);
                        assert_eq (ipf_rc, rc); // 'rc' always represents a complete IP datagram
                    }
                    if (VR_UNLIKELY (field<_filtered_> (ip_filter_ctx)))
                    {
                        ++ ip_filtered_count;

                        continue;
                    }
                }

                int64_t ts_utc_sec { }, ts_utc_nsec { };

                for (::cmsghdr * cmsg = CMSG_FIRSTHDR (& msg); cmsg != nullptr; cmsg = CMSG_NXTHDR (& msg, cmsg))
                {
                    if (cmsg->cmsg_type == SCM_TIMESTAMPING)
                    {
                        ::timespec * const ts = reinterpret_cast<::timespec * > (CMSG_DATA (cmsg));
                        DLOG_trace3 << "sw ts: " << ts [0].tv_sec << ':' << ts [0].tv_nsec
                                  << ", hw ts: " << ts [2].tv_sec << ':' << ts [2].tv_nsec;

                        ts_utc_sec = ts [ts_ix].tv_sec;
                        ts_utc_nsec = ts [ts_ix].tv_nsec;

                        break; // exit loop as soon as we get the RX ts
                    }
                    else
                    {
                        LOG_warn << "  unexpected cmsg_type " << cmsg->cmsg_type;
                    }
                }

#           if VR_DEBUG
                {
                    timestamp_t const ts_utc = ts_utc_sec * _1_second () + ts_utc_nsec;

                    auto const ts_diff = (ts_sys_utc - ts_utc);

                    DLOG_trace3 << "ts diff (sys - msg) = " << ts_diff;

                    timestamp_t constexpr max_ts_delta_error    = 10 * _1_millisecond ();

                    if (VR_UNLIKELY (std::abs (ts_diff) > max_ts_delta_error))
                        LOG_warn << "large time difference (" << (ts_sys_utc - ts_utc) <<  " ns), capture: " << print_timestamp (ts_utc) << ", system: " << print_timestamp (ts_sys_utc);
                }
#           endif // VR_DEBUG

                // populate packet header and commit both header and payload:
                {
                    packet_hr_type * const ph = static_cast<packet_hr_type *> (w);

                    ph->ts_sec () = ts_utc_sec;
                    ph->ts_fraction () = ts_utc_nsec;
                    ph->incl_len () = rc;
                    ph->orig_len () = rc;

                    DLOG_trace2 << "msg size " << rc << "\t(" << util::print_timestamp_as_ptime (ph->get_timestamp (), packet_hr_type::ts_fraction_digits ()) << " UTC, " << ts_utc_sec << ':' << ts_utc_nsec << ')';

                    const int32_t step  = (packet_hdr_size + rc);

                    w = out.w_advance (step); // w-advance over the bytes just written
                    out.r_advance (step); // "commit" the bytes just written

                    recv_total += step;
                }

                ++ packet_index;
            }

            ss = scap.statistics_snapshot ();

            LOG_info << "=====[ capture DONE (total: " << ss.first << ", filtered: " << ip_filtered_count << ", dropped: " << ss.second << ") ]=====";
        }
        catch (std::exception const & e)
        {
            LOG_error << "capture thread ABORTED: " << exc_info (e);
        }

    } // 'out' truncates and closes
    super::unbind (); // destructor always calls, but try to unbind eagerly


    m_stats.m_total = ss.first;
    m_stats.m_dropped = ss.second;
    m_stats.m_filtered = ip_filtered_count;
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
