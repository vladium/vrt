
#include "vr/market/sources/mock/mock_mcast_server.h"

#include "vr/fields.h"
#include "vr/io/events/event_context.h"
#include "vr/io/links/UDP_mcast_link.h"
#include "vr/io/mapped_ring_buffer.h"
#include "vr/io/net/io.h" // print() override for ::ip
#include "vr/io/net/socket_factory.h"
#include "vr/io/pcap/pcap_reader.h"
#include "vr/io/net/utility.h" // min_size_or_zero, make_group_range_filter
#include "vr/io/stream_factory.h"
#include "vr/market/defs.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"
#include "vr/util/timer_event_queue.h"

#include <net/ethernet.h>
#include <netinet/in.h> // IPPROTO_*
#include <netinet/ip.h>

#include <deque>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
using namespace io;

//............................................................................
//............................................................................

struct IP_mcast_io_base
{
    static constexpr int32_t eth_hdr_len ()     { return sizeof (::ether_header); }
    static constexpr int32_t min_size ()        { return (eth_hdr_len () + sizeof (::ip)); } // raw

}; // end of class
//............................................................................

struct send_event final
{
    timestamp_t const m_ts_start; // "soft" scheduled time
    int32_t const m_packet_size;

}; // end of class

using send_event_queue  = std::deque<send_event>;
using mcast_link        = UDP_mcast_link<send<_timestamp_, _ring_>>;

//............................................................................
/**
 * c.f 'IP_mcast_sender' in "socket_link_test.cpp"
 */
template<typename SEND_LINK> // TODO 'SEND_LINK' is always 'mcast_link' for now
class ITCH_mcast_transform final: public IP_mcast_io_base
{
    public: // ...............................................................

        ITCH_mcast_transform (send_event_queue & seq, SEND_LINK & link) :
            m_send_event_queue { seq },
            m_send_link { link }
        {
        }

        int64_t const & enqueued_count () const
        {
            return m_enqueued_count;
        }

        /*
         * consume the captured packet at 'data', place its output version it into 'm_send_link',
         * and schedule a send timer event
         */
        template<typename CTX>
        int32_t
        consume (CTX & ctx, addr_const_t/* ::ether_header */const data, int32_t const packet_size)
        {
            vr_static_assert (has_field<_ts_local_, CTX> ());

            ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (data, eth_hdr_len ()));

            if (ip_hdr->ip_p == IPPROTO_UDP) // use only UDP [mcast] packets
            {
                timestamp_t const ts_local = field<_ts_local_> (ctx);
                timestamp_t const ts_local_prev = m_ts_local_prev;

                // TODO add an opt for setting the emit rate, faking Mold timestamps, etc

                int64_t interval { };
                if (VR_LIKELY (ts_local_prev))
                {
                    // note: across *different* partition ports, there is a small probability of mcast
                    // packets read out of order from the capture raw socket;
                    // rather than restore the true physical '_ts_local_' ordering it is much easier for our
                    // mock purposes here to clip any negative deltas to zero:

                    // (similary, a true playback sim would have to edit the Mold timestamps which
                    // requires an app-level visitor)

                    interval = std::max<timestamp_t> (ts_local - ts_local_prev, 0);

                    DLOG_trace3 << "replay interval " << interval << " ns (actual ts_local delta: " << (ts_local - ts_local_prev) << " ns)";
                }
                else
                {
                    m_ts_mock = sys::realtime_utc ();
                }
                m_ts_local_prev = ts_local;

                DLOG_trace2 << '[' << m_enqueued_count << "] enqueuing raw packet of size " << packet_size << " byte(s): " << print (* ip_hdr);

                addr_t const packet = m_send_link.send_allocate (packet_size, /* blank init */false);
                std::memcpy (packet, data, packet_size);

                m_ts_mock += interval;
                m_send_event_queue.push_back (send_event { m_ts_mock, packet_size });

                ++ m_enqueued_count;
            }

            return packet_size;
        }

    private: // ..............................................................

        send_event_queue & m_send_event_queue;
        SEND_LINK & m_send_link;
        timestamp_t m_ts_local_prev { };
        timestamp_t m_ts_mock { };
        int64_t m_enqueued_count { };

}; // end of class
//............................................................................
//............................................................................

struct mock_mcast_server::pimpl final
{
    using pcap_defs         = pcap_reader<>;

    using processor         = ITCH_mcast_transform<mcast_link>; // pcap encapsulation added to the visitor
    using size_type         = typename mapped_ring_buffer::size_type;

    static constexpr int32_t min_available ()       { return pcap_defs::read_offset (); }

    VR_ENUM (state,
        (
            created,
            running,
            add_src,
            draining,
            done
        ),
        printable

    ); // end of enum

    static constexpr size_type initial_buf_capacity ()  { return (256 * 1024); }


    VR_ASSUME_COLD pimpl (mock_mcast_server & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_cfg_path { cfg_path }
    {
        LOG_trace1 << "configured with scope path " << print (cfg_path);
    }


    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_object (), cfg.type ());

        util::date_t const effective_date = config.start_date ();

        m_tz = cfg.value ("tz", m_tz);
        m_tz_offset = util::tz_offset (effective_date, m_tz);

        fs::path const data_root = cfg.at ("cap_root").get<std::string> (); // required
        fs::path const in_file = data_root / "ASX" / gd::to_iso_string (effective_date) / "asx-02" / "mcast.recv.p1p2.pcap.zst";

        LOG_info << "using source data in " << print (in_file);

        m_packet_begin = cfg.value ("packet_begin", 0L);
        m_packet_limit = cfg.value ("packet_limit", std::numeric_limits<int64_t>::max ());

        LOG_info << "packet range [" << m_packet_begin << ", " << m_packet_limit << ')';
        check_lt (m_packet_begin, m_packet_limit); // limit is inclusive of skip

        m_in = stream_factory::open_input (in_file);
        pcap_defs::read_header (* m_in);

        std::string const ifc = cfg.value ("ifc", "lo");
        int32_t const capacity = cfg.value ("capacity", 64 * 1024);

        open_mcast (ifc, capacity);
        check_nonnull (m_send_link);

        m_processor = std::make_unique<processor> (m_send_event_queue, * m_send_link);

        if (m_packet_begin > 0) skip_start_packets ();

        m_state = state::add_src;
    }

    VR_ASSUME_COLD void stop ()
    {
        m_processor.reset ();
        m_send_link.reset ();
        m_in.reset ();

        LOG_info << "read " << m_packet_index << " source packet(s), skipped " << m_packet_begin << ", sent " << m_send_count;
    }

    // core step logic:

    VR_FORCEINLINE void step () // note: force-inlined
    {
        DLOG_trace2 << '[' << m_packet_index << '/' << m_packet_limit << ", state: " << m_state << "]: available " << m_available << ", queue size " << (m_processor->enqueued_count () - m_send_count);

        switch (VR_LIKELY_VALUE (m_state, state::running))
        {
            case state::add_src:
            {
                addr_t const w = m_buf.w_position (); // invariant: points at the next byte available for writing

                size_type const r_count = read_fully (* m_in, m_buf.w_window (), w);
                DLOG_trace1 << m_state << ": r_count = " << r_count;

                if (VR_LIKELY (r_count > 0))
                {
                    m_buf.w_advance (r_count);
                    m_available += r_count;
                }
                else // EOF
                {
                    m_src_eof = true;
                    LOG_trace1 << "reached end of source data,";
                }

                m_state = state::running;
            }
            // !!! FALL THROUGH !!!
            /* no break */

            case state::running:
            {
                if (VR_UNLIKELY (m_src_eof | (m_packet_index >= m_packet_limit)))
                {
                    LOG_trace1 << "reached src EOF or packet limit, stopping ...";
                    m_state = state::draining;
                    break;
                }

                if (m_processor->enqueued_count () > m_send_count)
                {
                    m_state = state::draining;
                    break;
                }

                if (VR_UNLIKELY (m_available < min_available ())) // ensure we can read up to the included length field
                {
                    m_state = state::add_src; // need to read more bytes into 'm_buf'
                    break;
                }

                addr_const_t const r = m_buf.r_position (); // invariant: points at the start of a capture header (which may or may not have been read fully)

                pcap_defs::pcap_hdr_type const & pcap_hdr = * reinterpret_cast<pcap_defs::pcap_hdr_type const *> (r);

                size_type const pcap_incl_len = pcap_hdr.incl_len ();
                size_type const pcap_size = min_available () + pcap_incl_len;

                if (VR_UNLIKELY (m_available < pcap_size)) // ensure we can read the entire packet
                {
                    m_state = state::add_src; // need to read more bytes into 'm_buf'
                    break;
                }

                DLOG_trace3 << "    [" << m_packet_index << ", " << print_timestamp (pcap_hdr.get_timestamp (), m_tz_offset) << "]: pcap incl/orig len: " << pcap_incl_len << '/' << pcap_hdr.orig_len ();

                assert_le (pcap_incl_len, static_cast<size_type> (pcap_hdr.orig_len ())); // data invariant
                assert_within (pcap_hdr.ts_fraction (), pcap_defs::pcap_hdr_type::ts_fraction_limit ()); // data invariant

                {
                    auto & ctx = const_cast<pcap_defs::pcap_hdr_type &> (pcap_hdr); // this is ugly, but is needed to support templated CRTP-style derivations without too much pain

                    if (has_field<net::_packet_index_, pcap_defs::pcap_hdr_type> ())
                    {
                        field<net::_packet_index_> (ctx) = m_packet_index;
                    }

                    VR_IF_DEBUG (int32_t const pvrc = )m_processor->consume (ctx, addr_plus (r, min_available ()), pcap_incl_len);
                    assert_eq (pvrc, pcap_incl_len);
                }

                m_available -= pcap_size;
                m_buf.r_advance (pcap_size); // consumed

                ++ m_packet_index;
            }
            break;

            case state::draining:
            {
                timestamp_t const now_utc = sys::realtime_utc ();

                for (int32_t k = 0; true; ++ k) // send all queued up packets that are due to be emitted
                {
                    if (VR_UNLIKELY (m_send_event_queue.empty ()))
                    {
                        m_state = (m_src_eof ? state::done : state::running);
                        break;
                    }

                    send_event const & e = m_send_event_queue.front ();
                    if (e.m_ts_start > now_utc) // have more to emit but not yet (stay in this state)
                    {
                        break;
                    }
                    else
                    {
                        DLOG_trace3 << '[' << k << "] delta = " << (now_utc -e.m_ts_start);

                        int32_t const packet_size = e.m_packet_size; // copy before dequeuing
                        m_send_event_queue.pop_front ();

                        DLOG_trace2 << '[' << m_send_count << "] sending raw packet of size " << packet_size << " byte(s)";

                        auto const rc = m_send_link->send_flush (packet_size);
                        check_eq (rc, packet_size);

                        ++ m_send_count;
                    }
                }
            }
            break;

            default: /* state::done */
            {
                m_parent.request_stop ();
            }
            break;

        } // end of switch
    }


    void open_mcast (std::string const & ifc, int32_t const capacity)
    {
        m_send_link = std::make_unique<mcast_link> (ifc, send_arg_map { { "capacity", capacity } });
    }

    void skip_start_packets () // TODO may be more useful to skip until a particular time into the capture session
    {
        LOG_info << "skipping " << m_packet_begin << " initial packet(s) ...";

        addr_const_t r = m_buf.r_position ();
        addr_t w = m_buf.w_position ();

        int64_t skipped_count { };
        timestamp_t ts_capture { };

        for (size_type r_count; (skipped_count < m_packet_begin) && ((r_count = read_fully (* m_in, m_buf.w_window (), w)) > 0); )
        {
            DLOG_trace2 << "  r_count = " << r_count;

            w = m_buf.w_advance (r_count);
            m_available += r_count;

            while (VR_LIKELY ((skipped_count < m_packet_begin) & (m_available >= min_available ())))
            {
                pcap_defs::pcap_hdr_type const & pcap_hdr = * reinterpret_cast<pcap_defs::pcap_hdr_type const *> (r);

                size_type const pcap_incl_len = pcap_hdr.incl_len ();
                size_type const pcap_size = min_available () + pcap_incl_len;

                if (VR_UNLIKELY (m_available < pcap_size))
                    break; // need to read more bytes into 'm_buf'


                ts_capture = pcap_hdr.get_timestamp ();
                DLOG_trace1 << "    [" << skipped_count << ", " << print_timestamp (ts_capture, m_tz_offset) << "]: pcap incl/orig len: " << pcap_incl_len << '/' << pcap_hdr.orig_len ();

                assert_le (pcap_incl_len, static_cast<size_type> (pcap_hdr.orig_len ())); // data invariant
                assert_within (pcap_hdr.ts_fraction (), pcap_defs::pcap_hdr_type::ts_fraction_limit ()); // data invariant

                m_available -= pcap_size;
                r = m_buf.r_advance (pcap_size); // consumed

                ++ skipped_count;
            }
        }

        m_packet_index = skipped_count;
        LOG_info << "skipped " << skipped_count << " initial packet(s) (last skipped ts " << print_timestamp (ts_capture, m_tz_offset) << ' ' << m_tz << ')';
    }


    mock_mcast_server & m_parent;
    scope_path const m_cfg_path;
    std::string m_tz { "Australia/Sydney" };    // possibly reset in 'start()'
    timestamp_t m_tz_offset { };                // set in 'start()'
    std::unique_ptr<std::istream> m_in { }; // opened in 'start()'
    std::unique_ptr<mcast_link> m_send_link { }; // created in 'start()'
    send_event_queue m_send_event_queue { };
    std::unique_ptr<processor> m_processor { }; // created in 'start()'
    mapped_ring_buffer m_buf { initial_buf_capacity () };
    int64_t m_packet_begin { }; // set in 'start()'
    int64_t m_packet_limit { }; // set in 'start()'
    int64_t m_packet_index { }; // [inclusive of 'm_packet_skip']
    size_type m_available { };
    int64_t m_send_count { };
    state::enum_t m_state { state::created };
    bool m_src_eof { false };

}; // end of nested class
//............................................................................
//............................................................................

mock_mcast_server::mock_mcast_server (scope_path const & cfg_path) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    dep (m_config) = "config";
}

mock_mcast_server::~mock_mcast_server ()    = default; // pimpl
//............................................................................

void
mock_mcast_server::start ()
{
    m_impl->start ();
}

void
mock_mcast_server::stop ()
{
    m_impl->stop ();
}
//............................................................................

void
mock_mcast_server::step ()
{
    m_impl->step ();
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
