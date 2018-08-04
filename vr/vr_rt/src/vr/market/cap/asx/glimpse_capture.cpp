
#include "vr/market/cap/asx/glimpse_capture.h"

#include "vr/io/files.h"
#include "vr/io/links/TCP_link.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/io/net/defs.h"
#include "vr/rt/timer_queue/timer_queue.h"
#include "vr/sys/os.h"
#include "vr/mc/bound_runnable.h"
#include "vr/util/format.h"
#include "vr/util/parse.h"

#include <bitset>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
using namespace io;
using namespace io::net;
using namespace rt;

//............................................................................
//............................................................................
namespace
{
// TODO move elsewhere, do not hardcode:

std::pair<std::string, std::string> const g_credentials { "SUMG01", "K76BDA44" };

} // end of anonymous
//............................................................................
//............................................................................

struct glimpse_capture::pimpl final: public Soup_frame_<io::mode::recv, ITCH_visitor<pimpl>, /* SoupBinTCP overrides */pimpl>,
                                     public mc::bound_runnable
{
    using super         = Soup_frame_<io::mode::recv, ITCH_visitor<pimpl>, pimpl>;
    using bound         = mc::bound_runnable;

    using link_impl     = TCP_link<recv<_timestamp_, _tape_>, send<_timestamp_> >; // SoupTCP, Glimpse,

    using visit_ctx     = market_event_context<_ts_origin_, _partition_, _ts_local_>; // no benefit in _ts_local_delta_

    struct partition_context final
    {
        VR_NESTED_ENUM (
            (
                created,
                open,
                in_login,
                in_session,
                closed
            ),
            printable, parsable

        ); // end of nested enum

        void close () VR_NOEXCEPT // idempotent
        {
            if (m_state != closed)
            {
                m_state = closed;
                try
                {
                    m_link.reset ();
                }
                catch (std::exception const & e)
                {
                    LOG_warn << "error while closing: " << exc_info (e);
                }
            }
        }

        std::unique_ptr<link_impl> m_link { };
        std::string m_session_name { };
        int64_t m_snapshot_seqnum { -1 }; // note: this is Mold seqnum
        enum_t m_state { created };

    }; // end of nested class


    pimpl (glimpse_capture const & parent,
           std::string const & server, uint16_t const port_base, bool const disable_nagle, std::vector<int64_t> const & login_seqnums,
           fs::path const & out_dir, std::string const & capture_ID,
           int32_t const PU) :
       super (arg_map { /* TODO */ }),
       bound (PU),
       m_parent { parent },
       m_endpoint { server, port_base },
       m_out_path { out_dir / capture_ID / "glimpse" },
       m_disable_nagle { disable_nagle }
    {
        check_ge (login_seqnums.size (), partition_count ());

        for (int32_t pix = 0; pix < partition_count (); ++ pix)
        {
            int64_t const sn = login_seqnums [pix];

            seqnums ()[pix] = sn; // note: this is a Glimpse seqnum

            m_active_partitions |= ((sn != 0) << pix); // seqnum of 0 means the server will not send a snapshot
        }

        LOG_info << "active partition set: " << std::bitset<partition_count ()> { m_active_partitions };

        // create output dir (trigger any fs errors early, if any):

        io::create_dirs (m_out_path.parent_path ()); // note: 'm_out_path' is just a path prefix (the links will add more)
        LOG_info << "created output dir " << print (m_out_path.parent_path ());
    }

    // lifecycle:

    void open ()
    {
        for (int32_t pix = 0; pix < partition_count (); ++ pix)
        {
            if (! (m_active_partitions & (1 << pix))) continue; // skip inactive partitions

            partition_context & part = m_partition [pix];

            LOG_info << "connecting to partition " << pix << " (" << std::get<0> (m_endpoint) << ':' << (std::get<1> (m_endpoint) + pix)
                     << ", nagle off: " << m_disable_nagle << ") ...";

            part.m_link = std::make_unique<link_impl> (std::get<0> (m_endpoint), string_cast (std::get<1> (m_endpoint) + pix),
                recv_arg_map { { "capacity", itch_snapsnot_link_capacity () }, { "path", m_out_path } },
                send_arg_map { { "capacity", itch_snapsnot_link_capacity () }, { "path", m_out_path } },
                m_disable_nagle);

            part.m_state = partition_context::open;
        }
    }

    void close () VR_NOEXCEPT // idempotent
    {
        for (int32_t pix = 0; pix < partition_count (); ++ pix)
        {
            partition_context & part = m_partition [pix];

            if (part.m_state != partition_context::created)
            {
                LOG_info << "disconnecting from partition " << pix <<  " (" << print (part.m_session_name) << ", snapshot seqnum " << part.m_snapshot_seqnum << ") ...";

                part.close (); // idempotent
            }
        }
    }

    bool start_login ()
    {
        for (int32_t pix = 0; pix < partition_count (); ++ pix)
        {
            if (! (m_active_partitions & (1 << pix))) continue; // skip inactive partitions

            auto & sn_expected = seqnums ()[pix];

            std::string const & username = g_credentials.first;
            LOG_info << "[partition " << pix << "] logging into '" << username << "' with seqnum " << sn_expected << " ...";

            partition_context & part = m_partition [pix];
            link_impl & link = * part.m_link;

            // send login request:
            {
                int32_t const len = sizeof (SoupTCP_packet_hdr) + sizeof (SoupTCP_login_request);
                addr_t msg_buf = link.send_allocate (len); // blank-filled by default

                SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
                {
                    hdr.length () = 1 + sizeof (SoupTCP_login_request);
                    hdr.type () = 'L'; // [SoupTCP] client login request
                }

                SoupTCP_login_request & msg = * static_cast<SoupTCP_login_request *> (addr_plus (msg_buf, sizeof (SoupTCP_packet_hdr)));
                {
                    copy_to_alphanum (username, msg.username ());
                    copy_to_alphanum (g_credentials.second, msg.password ());
                    util::rjust_print_decimal_nonnegative (sn_expected, msg.seqnum ().data (), msg.seqnum ().max_size ());
                }

                auto const rc = link.send_flush (len);

                if (VR_UNLIKELY (! rc)) // EOF
                {
                    LOG_error << "[partition " << pix << "] EOF detected";
                    return false;
                }
                else
                {
                    part.m_state = partition_context::in_login;
                    LOG_trace1 << "  [partition " << pix << "] sent " << len << " byte(s) of login request @ " << print_timestamp (link.ts_last_send ());
                }
            }
        }

        return true;
    }

    VR_FORCEINLINE void mark_inactive (int32_t const pix) VR_NOEXCEPT
    {
        m_active_partitions &= ~(1 << pix);
    }

    // glimpse_capture:

    VR_ASSUME_HOT void operator() () override
    {
        assert_nonnull (m_parent.m_timer_queue);
        timer_queue const & timers = (* m_parent.m_timer_queue);

        timer const & start_timer = timers ["start.glimpse"];

        try
        {
            bound::bind ();
            std::cout << "[worker TID " << sys::gettid () << " running on PU #" << bound::PU () << std::endl;

            sys::sleep_until (start_timer.ts_utc ());
            LOG_info << "=====[ glimpse capture started ]=====";
            {
                open ();                // connect

                if (! start_login ())   // this returns when all partitions have sent login requests
                    goto disconnect;
            }

            timestamp_t ts_prev { };

            while (m_active_partitions)
            {
                bitset32_t const active_partitions = m_active_partitions;

                for (int32_t pix = 0; pix < partition_count (); ++ pix) // TODO ise a bit iterator? (even though this form is more unrollable) iterate over active partitions only (with a suitable definition of "active")
                {
                    if (! (active_partitions & (1 << pix)))
                        continue;

                    partition_context & part = m_partition [pix];
                    assert_nonnull (part.m_link); // TODO this should be redundant give the active mask

                    link_impl & link = * part.m_link;


                    // drain and process server -> client bytes:
                    {
                        static constexpr int32_t min_available  = min_size ();

                        std::pair<addr_const_t, capacity_t> const rc = link.recv_poll (); // non-blocking read

                        auto available = rc.second;
                        DLOG_trace3 << "[p #" << pix << "] received " << available << " byte(s) of GLIMPSE data";

                        int32_t consumed { };
                        {
                            visit_ctx in_ctx { };

                            while (available >= min_available) // consume all complete records in the recv buffer
                            {
                                // TODO use hw timestamps if possible?

                                if (has_field<_partition_, visit_ctx> ())
                                {
                                    field<_partition_> (in_ctx) = pix;
                                }
                                if (has_field<_ts_local_, visit_ctx> () || has_field<_ts_local_delta_, visit_ctx> ())
                                {
                                    timestamp_t const ts = (link_impl::has_ts_last_recv () ? link.ts_last_recv () : sys::realtime_utc ());

                                    if (has_field<_ts_local_, visit_ctx> ())
                                    {
                                        field<_ts_local_> (in_ctx) = ts;
                                    }
                                    if (has_field<_ts_local_delta_, visit_ctx> ())
                                    {
                                        field<_ts_local_delta_> (in_ctx) = ts - ts_prev;
                                        ts_prev = ts;
                                    }
                                }

                                int32_t const vrc = consume (in_ctx, addr_plus (rc.first, consumed), available);
                                if (vrc <= 0)
                                    break;

                                available -= vrc;
                                consumed += vrc;
                            }
                        }
                        if (consumed) link.recv_flush (consumed);
                    }

                    // client -> server heartbeats:
                    {
                        if (link_impl::has_ts_last_send ())
                        {
                            timestamp_t const & ts_ls = link.ts_last_send ();

                            if (sys::realtime_utc () >= ts_ls + heartbeat_timeout ())
                                send_heartbeat (link);
                        }
                    }
                }
            }

            LOG_info << "=====[ glimpse capture DONE ]=====";
        }
        catch (std::exception const & e)
        {
            LOG_error << "glimpse capture ABORTED: " << exc_info (e);
        }

disconnect:
        {
            close ();           // disconnect (among other things)

            bound::unbind ();   // destructor always calls, but try to unbind eagerly
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Glimpse, server->client:
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // overridden Glimpse visits [base versions from 'SoupTCP_<>']:

    void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_accepted const & msg) // override
    {
        super::visit_SoupTCP_login (ctx, soup_hdr, msg); // [chain]

        vr_static_assert (has_field<_partition_, visit_ctx> ());

        int32_t const pix = field<_partition_> (ctx);
        LOG_trace1 << "  [partition " << pix << "] login accepted: " << msg;

        partition_context & part = m_partition [pix];

        part.m_session_name.assign (msg.session ().data (), msg.session ().size ());
        {
            auto const msg_seqnum = util::ltrim (msg.seqnum ());
            if (VR_LIKELY (! msg_seqnum.empty ()))
            {
                part.m_snapshot_seqnum = util::parse_decimal<int64_t> (msg_seqnum.m_start, msg_seqnum.m_size);
                LOG_trace1 << "  [partition " << pix << ", session " << print (part.m_session_name) << "] seqnum set to: " << part.m_snapshot_seqnum;
            }
        }
        part.m_state = partition_context::in_session;
    }

    void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_rejected const & msg) // override
    {
        vr_static_assert (has_field<_partition_, visit_ctx> ());

        int32_t const pix = field<_partition_> (ctx);
        LOG_warn << "  [partition " << pix << "] login rejected: " << msg << " (" << (msg.reject_code ()[0] == 'A' ? "'NOT AUTHORIZED'" : "'SESSION NOT AVAILABLE'") << ')';

        mark_inactive (pix);

        super::visit_SoupTCP_login (ctx, soup_hdr, msg); // [chain]
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Glimpse ITCH, server->client:
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // overridden ITCH visits:

    using super::visit;

    VR_ASSUME_HOT int32_t visit (itch::end_of_snapshot const & msg, visit_ctx & ctx) // override
    {
        vr_static_assert (has_field<_partition_, visit_ctx> ());

        int32_t const pix = field<_partition_> (ctx);

        try
        {
            m_partition [pix].m_snapshot_seqnum = boost::lexical_cast<int64_t> (util::ltrim (msg.seqnum (), '0').to_string ()); // NOTE: left-padded with zero chars, not blanks
        }
        catch (std::exception const & e)
        {
            LOG_error << "error parsing seqnum field: " << exc_info (e);
        }
        LOG_trace1 << "  [partition " << pix << "] end of snapshot message: " << msg;
        LOG_trace1 << "[partition " << pix << "] snapshot seqnum " << m_partition [pix].m_snapshot_seqnum;

        mark_inactive (pix);

        return super::visit (msg, ctx);
    }

    // misc:

    void send_heartbeat (link_impl & link)
    {
        int32_t const len = sizeof (SoupTCP_packet_hdr);
        addr_t msg_buf = link.send_allocate (len, /* special case: no actual low-level protocol payload */false);

        SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
        {
            hdr.length () = 1;
            hdr.type () = 'R'; // [SoupTCP] client heartbeat
        }

        link.send_flush (len); // note: this can return less than 'len' if stop is requested

        DLOG_trace3 << " ------ sent heartbeat";
    }


    glimpse_capture const & m_parent;
    io::net::addr_and_port m_endpoint;
    fs::path const m_out_path;
    bitset32_t m_active_partitions { };
    bool const m_disable_nagle;
    partition_context m_partition [partition_count ()];

}; // end of class
//............................................................................
//............................................................................

glimpse_capture::glimpse_capture (std::string const & server, uint16_t const port_base, bool const disable_nagle, std::vector<int64_t> const & login_seqnums,
                                  fs::path const & out_dir, std::string const & capture_ID,
                                  int32_t const PU) :
    m_impl { std::make_unique<pimpl> (* this, server, port_base, disable_nagle, login_seqnums, out_dir, capture_ID, PU) }
{
    dep (m_timer_queue) = "timers";
}

glimpse_capture::~glimpse_capture ()    = default;

//............................................................................

void
glimpse_capture::operator() ()
{
    m_impl->operator() ();
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
