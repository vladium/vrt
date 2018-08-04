
#include "vr/cert_tool/market/asx/itch_session.h"

#include "vr/cert_tool/market/asx/glimpse_eos_printer.h"
#include "vr/cert_tool/market/asx/itch_order_book.h"
#include "vr/cert_tool/market/asx/itch_ref_data.h"
#include "vr/fields.h"
#include "vr/io/files.h"
#include "vr/io/links/TCP_link.h"
#include "vr/io/links/UDP_mcast_link.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/UDP_.h"
#include "vr/market/books/book_event_context.h"
#include "vr/market/books/limit_order_book_io.h"
#include "vr/market/books/asx/market_data_listener.h"
#include "vr/market/books/asx/market_data_view.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/market/net/SoupTCP_.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/sys/os.h"
#include "vr/util/datetime.h"
#include "vr/util/format.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"
#include "vr/mc/bound_runnable.h"
#include "vr/mc/spinlock.h"
#include "vr/util/memory.h"
#include "vr/util/parse.h"

#include <iostream>
#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
using namespace io;
using namespace io::net;

//............................................................................
//............................................................................
namespace
{

using lock_type             = mc::spinlock<>;

//............................................................................

VR_ENUM (FSA_state,
    (
        CREATED,
        SYNCING,
        READY,
        STOPPED
    ),
    printable

); // end of enum
//............................................................................

#define vr_GLIMPSE  1
#define vr_DATA     2

#define vr_EOL  "\r\n"

struct endpoint
{
    std::string m_server { };
    uint16_t m_port_base { };

}; // end of class

using credentials           = std::pair<std::string, std::string>;

} // end of anonymous
//............................................................................
//............................................................................
/**
 * this PU-pinned thread handles all API requests and server responses
 */
class itch_session::itch_session_runnable final: public mc::bound_runnable
{
    private: // ..............................................................

        using super         = mc::bound_runnable;


        using data_link_impl        = UDP_mcast_link<_state_, recv<_tape_>>; // Mold64UDP multicast, ITCH
        using snapshot_link_impl    = TCP_link<_state_, recv<_tape_>, send<_tape_, _timestamp_>>; // SoupTCP, Glimpse,


        using qob               = limit_order_book<price_si_t, oid_t, level<_qty_, _order_count_> >;
        using md_view           = market_data_view<qob>;

        // market data:

        using data_ctx          = book_event_context<_book_, _ts_local_, _ts_origin_, net::_packet_index_, net::_dst_port_, _seqnum_, _partition_>;

        using md_selector       = md_view::instrument_selector<data_ctx>;
        using md_listener       = market_data_listener<this_source (), qob, data_ctx>;

        using md_pipeline       = ITCH_pipeline
                                <
                                    md_selector,
                                    md_listener
                                >;

        // glimpse:

        using snap_ctx          = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_>;

        using sn_selector       = md_view::instrument_selector<snap_ctx>;
        using sn_listener       = market_data_listener<this_source (), qob, snap_ctx>;

        using sn_pipeline       = ITCH_pipeline
                                <
                                    glimpse_eos_printer<snap_ctx>,
                                    sn_selector,
                                    sn_listener
                                >;


        using book_map              = boost::unordered_map<iid_t, std::unique_ptr<itch_order_book>>;

    public: // ...............................................................

        vr_static_assert (snapshot_link_impl::has_ts_last_send ()); // 'recv' ts will be used but is optional

        /*
         * raw Mold64UDP multicast, ITCH
         */
        struct data_visitor final: public IP_<UDP_<Mold_frame_<md_pipeline>>>
        {
            using super     = IP_<UDP_<Mold_frame_<md_pipeline>>>;

            data_visitor (md_view & mdv) :
                super (arg_map { { "view", std::cref (mdv) }}) // 'md_selector' needs a ref to its own outer class instance
            {
            }

            data_ctx m_in_ctx { };

        }; // end of nested class

        /*
         * SoupTCP, Glimpse
         */
        struct snapshot_visitor final: public Soup_frame_<io::mode::recv, sn_pipeline>
        {
            using super     = Soup_frame_<io::mode::recv, sn_pipeline>;


            snapshot_visitor (itch_session_runnable & parent, md_view & mdv) :
                super (arg_map { { "view", std::cref (mdv) } }) // 'sn_selector' needs a ref to its own outer class instance
            {
                auto & sns = seqnums ();

                for (int32_t p = 0; p < partition_count (); ++ p)
                {
                    int64_t const sn = parent.m_sns [p];
                    sns [p] = sn;
                    LOG_trace2 << "  P" << p << ": " << sns [p] << vr_EOL; // TODO FIX without this line 'sns' ref segfaults
                }
            }

            snap_ctx m_in_ctx { };

        }; // end of nested class


        struct partition final
        {
            std::unique_ptr<snapshot_link_impl> m_snapshot_link { };

            std::string m_session_name { };
            timestamp_t m_session_login_ts { };
            int64_t m_snapshot_seqnum { -1 };
            FSA_state::enum_t m_state { FSA_state::CREATED };

        }; // end of nested class


        itch_session_runnable (itch_session const & parent, bool const use_snapshot, bool const disable_nagle, std::vector<int64_t> const & sns, int32_t const PU, bool const live_run) :
            super (PU),
            m_parent { parent },
            m_use_snapshot { use_snapshot },
            m_disable_nagle { disable_nagle },
            m_sns (sns),
            m_live_run { live_run }
        {
            check_ge (sns.size (), partition_count ());

            log_state ("starting");
        }

        ~itch_session_runnable () VR_NOEXCEPT // calls 'close ()'
        {
            close ();

            log_state ("ending");
        }

        // lifecycle:

        VR_ASSUME_COLD void start (scope_path const & cfg_path, const char feed_code)
        {
            std::cout << "using cfg " << print (cfg_path) << ", feed " << print (feed_code) << " ..." << vr_EOL;

            settings const & cfg = m_parent.m_config->scope (cfg_path);

            // feed:
            {
                settings const & feed = cfg [string_cast (feed_code)];

                m_data_ifc = feed ["ifc"];
                m_data_sources.emplace_back (feed ["sources"].get<std::string> ()); // a singleton right now, but could be an array of source specs

                if (m_use_snapshot)
                {
                    settings const & snapshot = feed ["server"]["snapshot"];
                    m_snapshot = { snapshot ["host"], snapshot ["port"] };

                    // credentials (sibling cfg; only strictly needed for Glimpse):
                    {
                        settings const & lp = cfg ["credentials"][0]; // single-entry array
                        check_eq (lp.size (), 2);

                        m_credentials = std::make_unique<credentials> (lp [0].get<std::string> (), lp [1].get<std::string> ()); // note: discarded after 'login()'
                    }
                }
            }

            // note: the same market_data_book instances owned by 'm_md_view' are mutated by both snapshot and data visitors,
            //       but happens to not be a problem here since the I/O loop is single-threaded

            // create 'm_md_view':
            {
                settings const & instruments = cfg ["instruments"]; // array

                m_md_view = std::make_unique<md_view> (arg_map { { { "ref_data", std::cref (* m_parent.m_ref_data) }, { "instruments", std::cref (instruments) } } });
            }

            // create 'm_data_visitor' (needs a ref to 'm_md_view'):
            {
                m_data_visitor = std::make_unique<data_visitor> (* m_md_view);
            }

            // create 'm_snapshot_visitor' (needs a ref to 'm_md_view'):

            if (m_use_snapshot)
            {
                m_snapshot_visitor = std::make_unique<snapshot_visitor> (* this, * m_md_view);
            }
        }

        VR_ASSUME_COLD void open ()
        {
            // TODO upgrade this to use 'create_link_capture_path()'

            util::ptime_t const t = util::current_time_local ();
            std::string const suffix = util::format_time (t, "%Y%m%d.%H%M%S");

            // date/time-specific capture dir:

            fs::path const cap_root { m_parent.m_capture_root / suffix };
            io::create_dirs (cap_root);


            fs::path const data_link_capture_path { cap_root / "mcast" };

            m_data_link = std::make_unique<data_link_impl> (m_data_ifc, m_data_sources,
                recv_arg_map { { "capacity", itch_data_link_capacity () }, { "path", data_link_capture_path } }
            );

            if (m_use_snapshot)
            {
                fs::path const snapshot_link_capture_path { cap_root / "glimpse" };

                for (int32_t pix = 0; pix < partition_count (); ++ pix)
                {
                    partition & part = m_partition [pix];

                    std::cout << "connecting to P" << pix << " ..." << vr_EOL;
                    if (m_use_snapshot)
                        std::cout << "  snapshot " << m_snapshot.m_server << ':' << (m_snapshot.m_port_base + pix) <<  " [nagle off: " << m_disable_nagle << ']' << vr_EOL;

                    if (m_use_snapshot)
                    {
                        part.m_snapshot_link = std::make_unique<snapshot_link_impl> (m_snapshot.m_server, string_cast (m_snapshot.m_port_base + pix),
                            recv_arg_map { { "capacity" , itch_snapsnot_link_capacity () }, { "path", snapshot_link_capture_path } },
                            send_arg_map { { "capacity" , itch_snapsnot_link_capacity () }, { "path", snapshot_link_capture_path } },
                            m_disable_nagle
                        );
                    }
                }
            }
            else
            {
                if (! m_use_snapshot) std::cout << "[snapshot not used]" << vr_EOL;
            }
        }

        VR_ASSUME_COLD void close () VR_NOEXCEPT // idempotent
        {
            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                partition & part = m_partition [pix];

                part.m_snapshot_link.reset ();
            }
        }

        VR_ASSUME_COLD bool login ()
        {
            check_nonnull (m_credentials);
            check_nonnull (m_snapshot_visitor);

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                auto & sn_expected = m_snapshot_visitor->seqnums ()[pix];

                std::string const & username = m_credentials->first;
                std::cout << "[P" << pix << "] logging '" << username << "' in with seqnum " << sn_expected << " ..." << vr_EOL;

                partition & part = m_partition [pix];
                snapshot_link_impl & link = * part.m_snapshot_link;

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
                        copy_to_alphanum (m_credentials->second, msg.password ());
                        util::rjust_print_decimal_nonnegative (sn_expected, msg.seqnum ().data (), msg.seqnum ().max_size ());
                    }

                    auto const rc = link.send_flush (len);

                    if (VR_UNLIKELY (! rc)) // EOF
                    {
                        std::cout << "[P" << pix << "] EOF detected" << vr_EOL;
                        return false;
                    }
                    else
                    {
                        part.m_session_login_ts = link.ts_last_send ();
                        std::cout << "  [P" << pix << "] SENT " << len << " byte(s) of login request @ " << print_timestamp (link.ts_last_send ()) << vr_EOL;
                    }
                }

                // read login response:

                int32_t const hdr_len = sizeof (SoupTCP_packet_hdr);
                int32_t msg_len { };

                while (true) // I/O loop
                {
                    if (VR_UNLIKELY (stop_flag ().is_raised ()))
                        return false;

                    std::pair<addr_const_t, capacity_t> const rc = link.recv_poll ();

                    if (rc.second < hdr_len)
                        continue;

                    int32_t msg_type { };
                    {
                        assert_ge (rc.second, hdr_len);
                        SoupTCP_packet_hdr const & hdr = * static_cast<SoupTCP_packet_hdr const *> (rc.first);

                        msg_len = hdr.length () -/* ! */1;
                        msg_type = hdr.type ();
                    }

                    std::cout << "  [P" << pix << "] response len: " << rc.second << ", type: " << print (static_cast<char> (msg_type)) << ", msg len: " << msg_len << vr_EOL;
                    assert_nonnegative (msg_len);

                    if (rc.second < hdr_len + msg_len)
                        continue;

                    switch (msg_type) // [SoupTCP] server login response
                    {
                        case 'A':
                        {
                            SoupTCP_login_accepted const & msg = * static_cast<SoupTCP_login_accepted const *> (addr_plus (rc.first, hdr_len));

                            std::cout << "  [P" << pix << "] login response: " << msg << vr_EOL;

                            {
                                auto const seqnum_trimmed = util::ltrim (msg.seqnum ());
                                if (VR_LIKELY (! seqnum_trimmed.empty ()))
                                {
                                    sn_expected = util::parse_decimal<int64_t> (seqnum_trimmed.m_start, seqnum_trimmed.m_size);
                                    std::cout << "  [P" << pix << "] seqnum set to " << sn_expected << vr_EOL;
                                }
                            }
                            {
                                part.m_session_name.assign (msg.session ().data (), msg.session ().size ());
                            }
                        }
                        break;

                        case 'J':
                        {
                            SoupTCP_login_rejected const & msg = * static_cast<SoupTCP_login_rejected const *> (addr_plus (rc.first, hdr_len));
                            // [remain in 'open' state]

                            LOG_warn << "[P" << pix << "] login response: " << msg << " (" << (msg.reject_code ()[0] == 'A' ? "'NOT AUTHORIZED'" : "'SESSION NOT AVAILABLE'") << ')' << vr_EOL;
                        }
                        break;

                        default: throw_x (illegal_state, "unexpected login response " + print (static_cast<char> (msg_type)));

                    } // end of switch

                    break; // login I/O done
                }
                link.recv_flush (hdr_len + msg_len);

                send_heartbeat (link);
            }

            log_state ("post-authorize");

            return true;
        }

        // API:

        int32_t list (arg_map && args, std::ostream & out) const
        {
            if (args.count ("symbol"))
            {
                std::string const & re = args.get<std::string> ("symbol");

                {
                    std::lock_guard<lock_type> _ { m_lock };

                    return m_rt_ref_data.list_symbols (regex_name_filter { re }, out);
                }
            }
            else if (args.count ("name"))
            {
                std::string const & re = args.get<std::string> ("name");

                {
                    std::lock_guard<lock_type> _ { m_lock };

                    return m_rt_ref_data.list_names (regex_name_filter { re }, out);
                }
            }
            else if (args.count ("iid"))
            {
                iid_t const iid = boost::lexical_cast<iid_t> (args.get<std::string> ("iid"));

                {
                    std::lock_guard<lock_type> _ { m_lock };

                    itch_ref_data::entry const & e = m_rt_ref_data [iid];
                    out << "  " << e << vr_EOL;

                    return 1;
                }
            }

            out << "  *** provide 'iid' or 'symbol'/'name' filter arg" << vr_EOL;
            return 0;
        }

        void qbook (arg_map && args, std::ostream & out) const
        {

            iid_t iid { };
            if (args.count ("iid"))
                iid = boost::lexical_cast<iid_t> (args.get<std::string> ("iid"));
            else if (args.count ("symbol"))
            {
                std::lock_guard<lock_type> _ { m_lock };

                iid = (* m_parent.m_ref_data) [args.get<std::string> ("symbol")].iid ();
            }
            else
            {
                out << "  *** provide 'iid' or 'symbol' arg" << vr_EOL;
            }

            assert_nonnull (m_md_view);

            {
                std::lock_guard<lock_type> _ { m_lock };

                qob const & book = (* m_md_view) [iid];
                out << book;
            }
        }


        int32_t obook (arg_map && args, std::ostream & out) const
        {

            iid_t iid { };
            if (args.count ("iid"))
                iid = boost::lexical_cast<iid_t> (args.get<std::string> ("iid"));
            else if (args.count ("symbol"))
            {
                std::lock_guard<lock_type> _ { m_lock };

                iid = m_rt_ref_data [args.get<std::string> ("symbol")].m_iid;
            }
            else
            {
                out << "  *** provide 'iid' or 'symbol' arg" << vr_EOL;
                return { };
            }

            {
                std::lock_guard<lock_type> _ { m_lock };

                itch_order_book const & book = lookup_or_add_book (iid);
                return book.list (out);
            }
        }


        // runnable:

        VR_ASSUME_HOT void operator() () final override
        {
            assert_nonnull (m_data_visitor);

            try
            {
                super::bind ();
                std::cout << "[worker TID " << sys::gettid () << " running on PU #" << super::PU () << (m_live_run ? "" : " (DRY RUN)") << ']' << vr_EOL;

                if (m_live_run)
                {
                    open (); // connect

                    if (m_use_snapshot)
                    {
                        bool const login_ok = login (); // this blocks until all partitions log in
                        m_credentials.reset ();

                        if (! login_ok) goto disconnect;
                    }
                }

                while (VR_LIKELY (! stop_flag ().is_raised ()))
                {
                    if (m_live_run)
                    {
                        // multicast data:

                        if (m_data_link)
                        {
                            data_link_impl & link = * m_data_link;
                            data_ctx & in_ctx = m_data_visitor->m_in_ctx;

                            // drain server -> client bytes:
                            {
                                static constexpr int32_t min_available  = data_visitor::min_size ();

                                std::pair<addr_const_t, capacity_t> const rc = link.recv_poll (); // non-blocking read

                                auto const available = rc.second;

                                if (available >= min_available) // a single datagram
                                {
                                    DLOG_trace3 << "received DATA msg of size " << available << " byte(s)" << vr_EOL;

                                    // TODO set ctx 'packet_index', 'ts_local', 'ts_origin' (hw timestamp, etc)

                                    VR_IF_DEBUG (int32_t const consumed = )m_data_visitor->consume (in_ctx, rc.first, available);
                                    assert_eq (consumed, available);

                                    link.recv_flush (available);
                                }
                            }
                        }

                        for (int32_t pix = 0; pix < partition_count (); ++ pix)
                        {
                            partition & part = m_partition [pix];

                            // Glimpse link(s):

                            if (part.m_snapshot_link)
                            {
                                using link_type     = snapshot_link_impl;

                                link_type & link = * part.m_snapshot_link;
                                snap_ctx & in_ctx = m_snapshot_visitor->m_in_ctx;

                                if (link_type::has_state ())
                                {
                                    if (link.state () != link_state::open) // TODO log the first transition to un-connected state
                                        continue;
                                }

                                // drain server -> client bytes:
                                {
                                    static constexpr int32_t min_available  = snapshot_visitor::min_size ();

                                    std::pair<addr_const_t, capacity_t> const rc = link.recv_poll (); // non-blocking read

                                    auto available = rc.second;
                                    if (available)
                                    {
                                        DLOG_trace2 << "[P" << pix << "] received " << available << " byte(s) of GLIMPSE data" << vr_EOL;

                                        int32_t consumed { };
                                        {
                                            while (available >= min_available) // consume all complete records in the recv buffer
                                            {
                                                if (has_field<_partition_, snap_ctx> ())
                                                {
                                                    field<_partition_> (in_ctx) = pix;
                                                }

                                                // TODO use hw timestamps
                                                // TODO set 'ts_origin'

                                                if (link_type::has_ts_last_recv () && has_field<_ts_local_, snap_ctx> ())
                                                {
                                                    field<_ts_local_> (in_ctx) = link.ts_last_recv ();
                                                }

                                                int32_t const vrc = m_snapshot_visitor->consume (in_ctx, addr_plus (rc.first, consumed), available);
                                                if (vrc <= 0)
                                                    break;

                                                available -= vrc;
                                                DLOG_trace2 << "[P" << pix << "] consumed " << vrc << " byte(s), available " << available << " byte(s)" vr_EOL;

                                                consumed += vrc;
                                            }
                                        }
                                        if (consumed) link.recv_flush (consumed);
                                    }
                                }

                                // client -> server heartbeats:
                                {
                                    timestamp_t const & ts_ls = link.ts_last_send ();

                                    if (sys::realtime_utc () >= ts_ls + heartbeat_timeout ())
                                        send_heartbeat (link);
                                }
                            }
                        }
                    }
                }
            }
            catch (std::exception const & e)
            {
                LOG_error << exc_info (e);
            }

disconnect: {
                close ();           // disconnect (among other things)

                super::unbind ();   // destructor always calls, but try to unbind eagerly
            }
        }


        /*
         * special case: no actual low-level protocol payload
         */
        void send_heartbeat (snapshot_link_impl & link)
        {
            int32_t const len = sizeof (SoupTCP_packet_hdr);
            addr_t msg_buf = link.send_allocate (len, false);

            SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
            {
                hdr.length () = 1;
                hdr.type () = 'R'; // [SoupTCP] client heartbeat
            }

            link.send_flush (len); // note: this can return less than 'len' if stop is requested

            DLOG_trace3 << " ------ sent heartbeat"  << vr_EOL;
        }


        // misc utility:

        void log_state (std::string const & prefix)
        {
            if (m_snapshot_visitor)
                std::cout << prefix << " glimpse seqnums: " << print (m_snapshot_visitor->seqnums ()) << vr_EOL;
            else
                std::cout << prefix << vr_EOL;
        }

        VR_FORCEINLINE itch_order_book & handle_trade (itch::trade const & msg) const
        {
            iid_t const iid = msg.iid ();
            itch_order_book & book = lookup_or_add_book (iid);

            if (msg.printable () == 'Y')
            {
                book.add_book_qty_traded (msg.qty ());
            }
            return book;
        }


        VR_FORCEINLINE itch_order_book & lookup_or_add_book (iid_t const iid) const
        {
            auto const i = m_book_map.find (iid);
            if (VR_LIKELY (i != m_book_map.end ()))
                return (* i->second);

            std::unique_ptr<itch_order_book> b { std::make_unique<itch_order_book> (m_rt_ref_data [iid]) };
            m_book_map.emplace (iid, std::move (b)); // last use of 'b'

            return (* b);
        }

        void request_stop ()
        {
            stop_flag ().raise ();
        }


        itch_session const & m_parent;
        std::unique_ptr<md_view> m_md_view { }; // [set in 'start()']
        std::unique_ptr<data_visitor> m_data_visitor { }; // [set in 'start()']
        std::unique_ptr<snapshot_visitor> m_snapshot_visitor { }; // [set in 'start()']
        std::string m_data_ifc { }; // [set in 'start()']
        std::vector<net::mcast_source> m_data_sources { }; // [set in 'start()']
        endpoint m_snapshot { }; // [set in 'start()']
        bool const m_use_snapshot;
        bool const m_disable_nagle;
        std::vector<int64_t> const m_sns;
        bool const m_live_run; // if 'false', skip operations that require actual UAT connections
        std::unique_ptr<data_link_impl> m_data_link { };
        partition m_partition [partition_count ()];
        itch_ref_data m_rt_ref_data { };
        mutable book_map m_book_map { };
        std::unique_ptr<credentials> m_credentials { }; // [set in 'start()']
        mutable lock_type m_lock { };   // cl-padded

}; // end of nested class
//............................................................................
//............................................................................

itch_session::itch_session (scope_path const & cfg_path, char const feed, bool const use_snapshot, fs::path const & capture_root,
                            bool const disable_nagle, std::vector<int64_t> const & sns, int32_t const PU, bool const live_run) :
    m_cfg_path { cfg_path },
    m_capture_root { capture_root },
    m_runnable { std::make_unique<itch_session_runnable> (* this, use_snapshot, disable_nagle, sns, PU, live_run) },
    m_feed { feed }
{
    dep (m_config) = "config";
    dep (m_ref_data) = "ref_data";

    LOG_trace1 << "configured with cfg path " << print (m_cfg_path) << ", feed " << print (m_feed) << ", link capture path " << print (m_capture_root);

    io::create_dirs (m_capture_root); // doing this here to error out early if don't have permissions
}
//............................................................................

void
itch_session::start ()
{
    assert_nonnull (m_runnable);

    itch_session_runnable & r = * m_runnable;
    r.start (m_cfg_path, m_feed);

    LOG_trace1 << "starting worker thread ...";

    m_worker = boost::thread { std::ref (r) };
}

void
itch_session::stop ()
{
    if (m_worker.joinable ())
    {
        LOG_trace1 << "joining worker thread ...";

        m_runnable->request_stop ();
        m_worker.join ();

        LOG_trace1 << "worker thread DONE";
    }
}
//............................................................................

int32_t
itch_session::list (arg_map && args, std::ostream & out) const
{
    assert_nonnull (m_runnable);
    try
    {
        return m_runnable->list (std::move (args), out);
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::list ABORTED:" << vr_EOL << exc_info (e);
        return { };
    }
}

void
itch_session::qbook (arg_map && args, std::ostream & out) const
{
    assert_nonnull (m_runnable);
    try
    {
        m_runnable->qbook (std::move (args), out);
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::qbook ABORTED:" << vr_EOL << exc_info (e);
    }
}

int32_t
itch_session::obook (arg_map && args, std::ostream & out) const
{
    assert_nonnull (m_runnable);
    try
    {
        return m_runnable->obook (std::move (args), out);
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::obook ABORTED:" << vr_EOL << exc_info (e);
        return { };
    }
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
