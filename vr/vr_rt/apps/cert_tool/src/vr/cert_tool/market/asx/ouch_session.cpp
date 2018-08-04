
#include "vr/cert_tool/market/asx/ouch_session.h"

#include "vr/cert_tool/market/asx/ouch_order_book.h"
#include "vr/fields.h"
#include "vr/io/files.h"
#include "vr/io/links/TCP_link.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/net/SoupTCP_.h"
#include "vr/market/sources/asx/execution.h"
#include "vr/market/sources/asx/ouch/error_codes.h"
#include "vr/meta/integer.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/util/datetime.h"
#include "vr/util/format.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"
#include "vr/mc/bound_runnable.h"
#include "vr/mc/spinlock.h"
#include "vr/util/memory.h"
#include "vr/util/parse.h"
#include "vr/sys/os.h"

#include <iostream>
#include <queue>
#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

inline void
copy_to_alphanum (std::string const & src, meta::impl::dummy_vholder::value_type & dst)
{
    // [never called]
}

namespace ASX
{
using namespace io;

//............................................................................
//............................................................................
namespace
{

template<typename I>
std::string to_otk_str (I const & x)
{
    std::stringstream s;
    s << 'T' << std::hex << std::uppercase << x;

    return s.str ();
}

int32_t
unique_counter_init ()
{
    return util::current_time_local ().time_of_day ().total_seconds ();
}
//............................................................................

struct API_request final
{
    API_request (ouch_session::enum_t const op, order_token const otk, arg_map && args) :
        m_otk { otk },
        m_args { std::move (args) },
        m_op { op }
    {
    }

    order_token const m_otk;
    arg_map const m_args;
    ouch_session::enum_t const m_op;

}; // end of class

using API_request_queue     = std::queue<std::unique_ptr<API_request> >;

using lock_type             = mc::spinlock<>;

//............................................................................

#define vr_EOL  "\r\n"

std::string const ASX_PARTICIPANT   = "6";
std::string const ASX_CROSSING_KEY  = "2";

char const ASX_PARTICIPANT_CAPACITY = 'P';
char const ASX_DIRECTED_WHOLESALE   = 'N';

using credentials           = std::vector<std::pair<std::string, std::string>>;



} // end of anonymous
//............................................................................
//............................................................................
/**
 * this PU-pinned thread handles all API requests and server responses
 */
class ouch_session::ouch_session_runnable final: public Soup_frame_<io::mode::recv,
                                                                    OUCH_visitor<mode::recv,
                                                                                 ouch_session_runnable>>, // not overriding SoupBinTCP, login handled ad hoc
                                                 public mc::bound_runnable
{
    private: // ..............................................................

        // TODO end-of-lines, split logging b/w std::cout and "console", etc

        using super         = Soup_frame_<io::mode::recv, OUCH_visitor<mode::recv, ouch_session_runnable>>;
        using bound         = mc::bound_runnable;

        using link_impl     = TCP_link<_state_, recv<_tape_, _timestamp_>, send<_tape_, _timestamp_>>; // TODO do I still use _state_?

    public: // ...............................................................

        using super::visit; // this shouldn't be necessary...


        ouch_session_runnable (ouch_session const & parent, bool const disable_nagle, std::vector<int64_t> const & sns, int32_t const PU, bool const live_run) :
            super (arg_map { /* TODO */ }),
            bound (PU),
            m_parent { parent },
            m_disable_nagle { disable_nagle },
            m_live_run { live_run }
        {
            check_ge (sns.size (), partition_count ());

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                int64_t const sn = sns [pix];

                this->seqnums ()[pix] = sn;

                m_active_partitions |= ((sn != 0) << pix); // seqnum of 0 means "don't connect"
            }

            log_state ("starting");
        }

        ~ouch_session_runnable () VR_NOEXCEPT // calls 'close ()'
        {
            close ();

            log_state ("final");
        }


        // lifecycle:

        VR_ASSUME_COLD void start (scope_path const & cfg_path)
        {
            std::cout << "using cfg " << print (cfg_path) << " ..." << vr_EOL;

            settings const & cfg = m_parent.m_config->scope (cfg_path);

            m_server = cfg ["server"]["host"];
            m_port = cfg ["server"]["port"];

            m_account = cfg ["account"];
            m_credentials = std::make_unique<credentials> (); // note: discarded after 'login()'
            {
                auto const & lp_list = cfg ["credentials"];
                for (auto const & lp : lp_list)
                {
                    check_eq (lp.size (), 2);
                    m_credentials->emplace_back (lp [0].get<std::string> (), lp [1].get<std::string> ());
                }
            }
            check_eq (m_credentials->size (), partition_count ());
        }

        VR_ASSUME_COLD void open ()
        {
            util::ptime_t const t = util::current_time_local ();
            std::string const suffix = util::format_time (t, "%Y%m%d.%H%M%S"); // TODO can take this as op option

            // date/time-specific capture dir:

            fs::path const cap_root { m_parent.m_capture_root / suffix };
            io::create_dirs (cap_root);

            fs::path const link_capture_path { cap_root / "ouch" };

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                if (! (m_active_partitions & (1 << pix))) continue; // skip inactive partitions

                std::cout << "connecting to " << " P" << pix << " (port " << (m_port + pix) <<  ", nagle disabled: " << m_disable_nagle << ") ..." << vr_EOL;

                m_link [pix] = std::make_unique<link_impl> (m_server, string_cast (m_port + pix),
                    recv_arg_map { { "capacity", ouch_link_capacity () }, { "path", link_capture_path } },
                    send_arg_map { { "capacity", ouch_link_capacity () }, { "path", link_capture_path } },
                    m_disable_nagle
                );
            }
        }

        VR_ASSUME_COLD void close () VR_NOEXCEPT // idempotent
        {
            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                m_link [pix].reset ();
            }
        }


        VR_ASSUME_COLD bool login ()
        {
            check_nonnull (m_credentials);

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                if (! (m_active_partitions & (1 << pix))) continue; // skip inactive partitions

                auto & sn_expected = this->seqnums ()[pix];

                std::string const & username = m_credentials->at (pix).first;
                std::cout << "[P" << pix << "]: logging '" << username << "' with seqnum " << sn_expected << " ..." << vr_EOL;

                link_impl & link = * m_link [pix];
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
                        copy_to_alphanum (m_credentials->at (pix).second, msg.password ());
                        util::rjust_print_decimal_nonnegative (sn_expected, msg.seqnum ().data (), msg.seqnum ().max_size ());
                    }

                    auto const rc = link.send_flush (len);

                    if (VR_UNLIKELY (! rc)) // EOF
                    {
                        std::cout << "[P" << pix << "]: EOF detected" << vr_EOL;
                        return false;
                    }
                    else
                    {
                        assert_eq (rc, len);
                        std::cout << "[P" << pix << "]: wrote " << len << " byte(s) of login request" << vr_EOL;
                    }
                }
            }

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                if (! (m_active_partitions & (1 << pix))) continue; // skip inactive partitions

                auto & sn_expected = this->seqnums ()[pix];

                // TODO convert these to Glimpse visits:

                link_impl & link = * m_link [pix];
                {
                    int32_t const hdr_len = sizeof (SoupTCP_packet_hdr);

                    std::pair<addr_const_t, capacity_t> rc { };
                    int32_t msg_len { }, msg_type { };

                    while (true) // for simplicity, this app spins here instead of continuing with other links (partitions) [code converted from a different impl of links]
                    {
                        rc = link.recv_poll ();

                        if (rc.second < hdr_len) // partial header read
                            continue;

                        SoupTCP_packet_hdr const & hdr = * static_cast<SoupTCP_packet_hdr const *> (rc.first);

                        msg_len = hdr.length () -/* ! */1;
                        assert_nonnegative (msg_len, hdr.type ());

                        if (rc.second >= hdr_len + msg_len)
                        {
                            msg_type = hdr.type ();
                            std::cout << "[P" << pix << "]: response len: " << rc.second << ", type: " << print (hdr.type ()) << ", msg len: " << msg_len << vr_EOL;

                            break; // end of spin
                        }
                        // else partial message read
                    }

                    switch (msg_type) // [SoupTCP] server login response
                    {
                        case 'A':
                        {
                            SoupTCP_login_accepted const & msg = * static_cast<SoupTCP_login_accepted const *> (addr_plus (rc.first, hdr_len));

                            std::cout << "[P" << pix << "]: login response: " << msg << vr_EOL;

                            {
                                auto const seqnum_trimmed = util::ltrim (msg.seqnum ());
                                if (VR_LIKELY (! seqnum_trimmed.empty ()))
                                {
                                    sn_expected = util::parse_decimal<int64_t> (seqnum_trimmed.m_start, seqnum_trimmed.m_size);
                                    LOG_info << "[P" << pix << "]: seqnum set to " << sn_expected;
                                }
                            }
                        }
                        break;

                        case 'J':
                        {
                            SoupTCP_login_rejected const & msg = * static_cast<SoupTCP_login_rejected const *> (addr_plus (rc.first, hdr_len));
                            // [remain in 'open' state]

                            LOG_warn << "[P" << pix << "]: login response: " << msg << " (" << (msg.reject_code ()[0] == 'A' ? "'NOT AUTHORIZED'" : "'SESSION NOT AVAILABLE'") << ')' << vr_EOL;
                        }
                        break;

                        default: throw_x (illegal_state, "unexpected login response " + print (static_cast<char> (msg_type)));

                    } // end of switch

                    link.recv_flush (hdr_len + msg_len);
                }

                send_heartbeat (link);
            }

            log_state ("post-authorize");

            return true;
        }

        // order API:

        int32_t list_book (std::ostream & out) const
        {
            std::lock_guard<lock_type> _ { m_lock };

            return m_book.list (out);
        }


        order_token submit (arg_map && args)
        {
            check_condition (args.count ("partition"));
            check_condition (args.count ("iid"));
            check_condition (args.count ("side"));
            check_condition (args.count ("qty"));
            check_condition (args.count ("price"));

            int32_t const pix = boost::lexical_cast<int32_t> (args.get<std::string> ("partition")) -/* ! */1;
            assert_within (pix, partition_count ());

            std::lock_guard<lock_type> _ { m_lock };

            int64_t const oid = m_order_counter ++; // note: side-effect [protected by the lock above]
            order_token const otk { to_otk_str (oid) };

            args.emplace ("otk", otk.to_string ()); // generated by the tool

            m_book.add (otk, pix); // throws on duplicate token
            m_API_queue.push (std::make_unique<API_request> (ouch_session::op_submit, otk, std::move (args)));

            return otk;
        }

        order_token replace (order_token const otk, arg_map && args)
        {
            std::lock_guard<lock_type> _ { m_lock };

            order_descriptor const & od = m_book.lookup (otk); // throws on lookup failure
            order_token const & otk_current = od.m_otk_current; // always use the latest known token

            int64_t const new_oid = m_order_counter ++; // note: side-effect [protected by the lock above]
            order_token const new_otk { to_otk_str (new_oid) };

            args.emplace ("otk", otk_current.to_string ());
            args.emplace ("new_otk", new_otk.to_string ()); // generated by the tool

            m_book.add_alias (otk, new_otk);
            m_API_queue.push (std::make_unique<API_request> (ouch_session::op_replace, otk_current, std::move (args)));

            return new_otk;
        }

        void cancel (order_token const otk, arg_map && args)
        {
            std::lock_guard<lock_type> _ { m_lock };

            order_descriptor const & od = m_book.lookup (otk); // throws on lookup failure
            order_token const & otk_current = od.m_otk_current; // always use the latest known token

            args.emplace ("otk", otk_current.to_string ());

            m_API_queue.push (std::make_unique<API_request> (ouch_session::op_cancel, otk_current, std::move (args)));
        }

        // runnable:

        VR_ASSUME_HOT void operator() () final override
        {
            try
            {
                bound::bind ();
                std::cout << "[worker TID " << sys::gettid () << " running on PU #" << bound::PU () << (m_live_run ? "" : " (DRY RUN)") << ']' << vr_EOL;

                if (m_live_run)
                {
                    open (); // connect

                    bool const login_ok = login (); // this blocks until all partitions log in
                    m_credentials.reset ();

                    if (! login_ok) goto disconnect;
                }

                while (! stop_flag ().is_raised ())
                {
                    // drain API queue:
                    {
                        std::lock_guard<lock_type> _ { m_lock };

                        while (! m_API_queue.empty ())
                        {
                            API_request const & r = * m_API_queue.front ();
                            DLOG_trace1 << "dequeuing API request " << print (r.m_op) << " ...";

                            if (m_live_run)
                            {
                                try
                                {
                                    switch (r.m_op)
                                    {
                                        case ouch_session::op_submit:  send_<ouch::submit_order> (r.m_args); break;
                                        case ouch_session::op_replace: send_<ouch::replace_order> (r.m_args); break;
                                        case ouch_session::op_cancel:  send_cancel_order (r.m_args); break;

                                        default: VR_ASSUME_UNREACHABLE ();

                                    } // end of switch
                                }
                                catch (std::exception const & e)
                                {
                                    LOG_error << exc_info (e);
                                }
                            }

                            m_API_queue.pop ();
                        }
                    }

                    if (m_live_run)
                    {
                        for (int32_t pix = 0; pix < partition_count (); ++ pix) // TODO bitset-based tracking of active partitions (like in 'glimpse_capture') would be more efficient
                        {
                            if (! m_link [pix]) continue; // skip inactive partitions

                            link_impl & link = * m_link [pix];
                            visit_ctx & in_ctx = m_in_ctx;

                            if (link_impl::has_state ())
                            {
                                link_state::enum_t const & s = link.state ();

                                if (s != link_state::open) // TODO distinguish connected/logged in?
                                    continue;
                            }

                            // drain server -> client bytes:'
                            {
                                static constexpr int32_t min_available  = min_size ();

                                std::pair<addr_const_t, capacity_t> const rc = link.recv_poll (); // non-blocking read

                                auto available = rc.second;
                                if (available)
                                {
                                    DLOG_trace3 << "[P" << pix << "] received " << available << " byte(s) of OUCH data" << vr_EOL;

                                    int32_t consumed { };
                                    {
                                        while (available >= min_available) // consume all complete records in the recv buffer
                                        {
                                            // TODO use hw timestamps where possible

                                            if (has_field<_partition_, visit_ctx> ())
                                            {
                                                field<_partition_> (in_ctx) = pix; // [no need to look at TCP headers to infer partition index]
                                            }
                                            if (has_field<_ts_local_, visit_ctx> ())
                                            {
                                                timestamp_t const ts = (link_impl::has_ts_last_recv () ? link.ts_last_recv () : sys::realtime_utc ());

                                                field<_ts_local_> (in_ctx) = ts;
                                            }

                                            int32_t const vrc = consume (in_ctx, addr_plus (rc.first, consumed), available);
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
                                if (link_impl::has_ts_last_send ())
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

                bound::unbind ();   // destructor always calls, but try to unbind eagerly
            }
        }

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // OUCH, client->server:
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        /*
         * submit/replace_order
         */
        template<typename REQUEST>
        void send_ (arg_map const & args)
        {
            constexpr bool is_submit = std::is_same<REQUEST, ouch::submit_order>::value; // compile-time choice

            int32_t const pix = boost::lexical_cast<int32_t> (args.get<std::string> ("partition")) -/* ! */1;
            assert_within (pix, partition_count ());

            link_impl & link = * m_link [pix];
            {
                int32_t const len = sizeof (SoupTCP_packet_hdr) + sizeof (REQUEST);
                addr_t msg_buf = link.send_allocate (len); // blank-filled by default

                SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
                {
                    hdr.length () = 1 + sizeof (REQUEST);
                    hdr.type () = 'U'; // [SoupTCP] client unsequenced data
                }

                REQUEST & msg = * static_cast<REQUEST *> (addr_plus (msg_buf, sizeof (SoupTCP_packet_hdr)));
                {
                    msg.type () = (is_submit ? ouch::message_type<mode::send>::submit_order : ouch::message_type<mode::send>::replace_order);

                    // submit_order:

                    if (has_field<_otk_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("otk");
                        copy_to_alphanum (v, field<_otk_> (msg));
                    }
                    if (has_field<_iid_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("iid");
                        field<_iid_> (msg) = boost::lexical_cast<typename iid_ft::value_type> (v);
                    }
                    if (has_field<_side_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("side");
                        field<_side_> (msg) = ord_side::value (v);
                    }

                    if (has_field<_qty_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("qty");
                        field<_qty_> (msg) = boost::lexical_cast<typename qty_ft::value_type> (v);
                    }
                    if (has_field<_price_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("price");
                        field<_price_> (msg) = as_price (boost::lexical_cast<double> (v));
                    }
                    if (has_field<_TIF_, REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("TIF", "IOC");
                        field<_TIF_> (msg) = ord_TIF::value (v); // boost::lexical_cast<int32_t> (v); // NOTE widen

                        VR_IF_DEBUG
                        (
                            int32_t const tif = field<_TIF_> (msg);
                            assert_condition (((1 << tif) & meta::static_bitset<bitset32_t, 0, 3, 4>::value), tif);
                        )
                    }

                    field<_open_close_> (msg) = 0;


                    if (has_field<_account_, REQUEST> ())
                    {
                        assert_nonempty (m_account);

                        std::string const v = args.get<std::string> ("account", m_account);
                        copy_to_alphanum (v, field<_account_> (msg));
                    }
                    // [leave 'customer_info' blank]
                    // [leave 'exchange_info' blank]
                    if (has_field<_participant_, REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("participant", ASX_PARTICIPANT);
                        field<_participant_> (msg) = v [0]; // char
                    }
                    if (has_field<_crossing_key_, REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("crossing_key", ASX_CROSSING_KEY);
                        field<_crossing_key_> (msg) = boost::lexical_cast<int32_t> (v);
                    }

                    if (has_field<_reg_data_, REQUEST> ())
                    {
                        ouch::reg_data_overlay & rd = field<_reg_data_> (msg);

                        field<_participant_capacity_> (rd) = ASX_PARTICIPANT_CAPACITY;
                        field<_directed_wholesale_> (rd) = ASX_DIRECTED_WHOLESALE;
                    }
                    if (has_field<_order_type_, REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("order_type", "LIMIT");
                        field<_order_type_> (msg) = ord_type::value (v); // char
                    }
                    if (has_field<_short_sell_qty_,REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("short_sell_qty", "0"); // default to zero
                        field<_short_sell_qty_> (msg) = boost::lexical_cast<typename qty_ft::value_type> (v);
                    }
                    if (has_field<_MAQ_, REQUEST> ())
                    {
                        std::string const v = args.get<std::string> ("MAQ", "0"); // default to no MAQ
                        field<_MAQ_> (msg) = boost::lexical_cast<typename qty_ft::value_type> (v);
                    }

                    // + replace_order:

                    if (has_field<_new_otk_, REQUEST> ())
                    {
                        std::string const & v = args.get<std::string> ("new_otk");
                        copy_to_alphanum (v, field<_new_otk_> (msg));
                    }

                    DLOG_trace2 << vr_EOL << "SENDING " << msg << vr_EOL;

                    VR_IF_DEBUG
                    (
                        if (has_field<_price_, REQUEST> ())
                        {
                            price_ft::value_type const price = field<_price_> (msg);
                            assert_positive (price); // never a MARKET order
                        }
                    )
                }

                link.send_flush (len); // note: this can return less than 'len' if stop is requested
            }

            DLOG_trace1 << vr_EOL << "[P" << pix << "]: sent " << util::class_name<REQUEST> () << vr_EOL;
        }

        void send_cancel_order (arg_map const & args)
        {
            int32_t const pix = boost::lexical_cast<int32_t> (args.get<std::string> ("partition")) -/* ! */1;
            assert_within (pix, partition_count ());

            link_impl & link = * m_link [pix];
            {
                int32_t const len = sizeof (SoupTCP_packet_hdr) + sizeof (ouch::cancel_order);
                addr_t msg_buf = link.send_allocate (len); // blank-filled by default

                SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
                {
                    hdr.length () = 1 + sizeof (ouch::cancel_order);
                    hdr.type () = 'U'; // [SoupTCP] client unsequenced data
                }

                ouch::cancel_order & msg = * static_cast<ouch::cancel_order *> (addr_plus (msg_buf, sizeof (SoupTCP_packet_hdr)));
                {
                    msg.type () = ouch::message_type<mode::send>::cancel_order;

                    {
                        std::string const & v = args.get<std::string> ("otk");
                        copy_to_alphanum (v, field<_otk_> (msg));
                    }

                    DLOG_trace2 << vr_EOL << "SENDING " << msg << vr_EOL;
                }

                link.send_flush (len); // note: this can return less than 'len' if stop is requested
            }

            DLOG_trace1 << vr_EOL << "[P" << pix << "]: sent cancel" << vr_EOL;
        }

        /*
         * special case: no actual low-level protocol payload
         */
        void send_heartbeat (link_impl & link)
        {
            int32_t const len = sizeof (SoupTCP_packet_hdr);
            addr_t msg_buf = link.send_allocate (len, false);

            SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
            {
                hdr.length () = 1;
                hdr.type () = 'R'; // [SoupTCP] client heartbeat
            }

            link.send_flush (len); // note: this can return less than 'len' if stop is requested

            DLOG_trace3 << vr_EOL << "sent heartbeat"  << vr_EOL;
        }

        // overridden visits:

        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        // OUCH, server->client:
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        using visit_ctx     = market_event_context<_ts_local_, _ts_origin_, _partition_>;


        VR_ASSUME_HOT bool visit (ouch::order_accepted const & msg, visit_ctx & ctx) // override
        {
            DLOG_trace1 << vr_EOL << "override visit of " << msg << vr_EOL;

            order_token const otk { util::rtrim (msg.otk ()) };

            std::lock_guard<lock_type> _ { m_lock };
            {
                order_descriptor & od = m_book.lookup (otk, /* add if missing */true);

                od.m_state = order_descriptor::live;
            }

            std::cout << print (otk) << " accepted" << vr_EOL;
            return super::visit (msg, ctx); // chain
        }

        VR_ASSUME_HOT bool visit (ouch::order_rejected const & msg, visit_ctx & ctx) // override
        {
            DLOG_trace1 << vr_EOL << "override " << msg << vr_EOL;

            order_token const otk { util::rtrim (msg.otk ()) };
            int32_t rc { };

            std::lock_guard<lock_type> _ { m_lock };
            {
                order_descriptor & od = m_book.lookup (otk, /* add if missing */true);

                od.m_reject_code = rc = msg.reject_code ();
                od.m_state = order_descriptor::rejected; // TODO log warning
            }

            std::cout << print (otk) << " REJECTED, code: " << print_reject_code (rc) << vr_EOL;
            return super::visit (msg, ctx); // chain
        }

        VR_ASSUME_HOT bool visit (ouch::order_replaced const & msg, visit_ctx & ctx) // override
        {
            DLOG_trace1 << vr_EOL << "override " << msg << vr_EOL;

            order_token const otk { util::rtrim (msg.otk ()) };
            order_token const new_otk { util::rtrim (msg.new_otk ()) };

            std::lock_guard<lock_type> _ { m_lock };
            {
                order_descriptor & od = m_book.lookup (otk, /* add if missing */true);

                od.m_otk_current = new_otk;
            }

            std::cout << print (otk) << " replaced: new otk " << print (new_otk) << vr_EOL;
            return super::visit (msg, ctx); // chain
        }

        VR_ASSUME_HOT bool visit (ouch::order_canceled const & msg, visit_ctx & ctx) // override
        {
            DLOG_trace1 << vr_EOL << "override " << msg << vr_EOL;

            order_token const otk { util::rtrim (msg.otk ()) };
            cancel_reason::enum_t cr { cancel_reason::na };

            std::lock_guard<lock_type> _ { m_lock };
            {
                order_descriptor & od = m_book.lookup (otk, /* add if missing */true);

                od.m_cancel_code = cr = static_cast<cancel_reason::enum_t> (msg.cancel_code ());
                od.m_state = order_descriptor::done;
            }

            std::cout << print (otk) << " canceled, reason: " << cr << vr_EOL;
            /*
             * note: if a cancel comes from a prev session (gap replay), it will have 'CONNECTION_LOSS' reason
             *       and a blank token, so it needs to be mapped via 'oid', e.g.:
             *
             * {'hdr': {'type': 'C', 'ts_utc': 1512765628876220600}, 'otk': "              ", 'instrument': 70915, 'side': 'S', 'oid': 6930268523411320078, 'reason': 4}
             */
            if (cr == cancel_reason::CONNECTION_LOSS)
            {
                std::cout << msg << vr_EOL;
            }

            return super::visit (msg, ctx); // chain
        }

        VR_ASSUME_HOT bool visit (ouch::order_execution const & msg, visit_ctx & ctx) // override
        {
            DLOG_trace1 << vr_EOL << "override " << msg << vr_EOL;

            order_token const otk { util::rtrim (msg.otk ()) };
            qty_ft::value_type qty { };
            price_ft::value_type price { };

            std::lock_guard<lock_type> _ { m_lock };
            {
                order_descriptor & od = m_book.lookup (otk, /* add if missing */true);

                od.m_qty_executed += (qty = static_cast<qty_ft::value_type> (msg.qty ()));
                price = static_cast<price_ft::value_type> (msg.price ());
            }

            std::cout << print (otk) << " executed: qty " << qty << ", price " << price << vr_EOL;
            return super::visit (msg, ctx); // chain
        }

        // misc utility:

        void log_state (std::string const & prefix)
        {
            std::cout << prefix << " seqnums: " << print (this->seqnums ()) << vr_EOL;
        }

        void request_stop ()
        {
            stop_flag ().raise ();
        }


        ouch_session const & m_parent;
        std::unique_ptr<credentials> m_credentials { }; // [set in 'start()']
        std::string m_account { }; // [set in 'start()']
        std::string m_server { }; // [set in 'start()']
        uint16_t m_port { }; // [set in 'start()'] actually, a "port base"
        bitset32_t m_active_partitions { };
        bool const m_disable_nagle;
        bool const m_live_run; // if 'false', skip operations that require actual UAT connections
        visit_ctx m_in_ctx { };
        std::unique_ptr<link_impl> m_link [partition_count ()];
        int64_t m_order_counter { unique_counter_init () }; // sets this uniquely for the day
        ouch_order_book m_book { };
        API_request_queue m_API_queue { };
        mutable lock_type m_lock { };   // cl-padded

}; // end of nested class
//............................................................................
//............................................................................

ouch_session::ouch_session (scope_path const & cfg_path, fs::path const & capture_root,
                            bool const disable_nagle, std::vector<int64_t> const & seqnums, int32_t const PU, bool const live_run) :
    m_cfg_path { cfg_path },
    m_capture_root { capture_root },
    m_runnable { std::make_unique<ouch_session_runnable> (* this, disable_nagle, seqnums, PU, live_run) }
{
    dep (m_config) = "config";

    LOG_trace1 << "configured with cfg path " << print (m_cfg_path) << ", link capture path " << print (m_capture_root);

    io::create_dirs (m_capture_root); // doing this here to error out early if don't have permissions
}
//............................................................................

void
ouch_session::start ()
{
    assert_nonnull (m_runnable);

    ouch_session_runnable & r = * m_runnable;
    r.start (m_cfg_path);

    LOG_trace1 << "starting worker thread ...";

    m_worker = boost::thread { std::ref (r) };
}

void
ouch_session::stop ()
{
    if (m_worker.joinable ())
    {
        LOG_trace1 << "joining worker thread ...";

        assert_nonnull (m_runnable);
        m_runnable->request_stop ();
        m_worker.join ();

        LOG_trace1 << "worker thread DONE";
    }
}
//............................................................................

int32_t
ouch_session::list_book (std::ostream & out) const
{
    assert_nonnull (m_runnable);
    try
    {
        return m_runnable->list_book (out);
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::list_book ABORTED:" << vr_EOL << exc_info (e);
        return { };
    }
}
//............................................................................

order_token
ouch_session::submit (arg_map && args)
{
    assert_nonnull (m_runnable);
    try
    {
        check_condition (args.count ("partition"));
        check_condition (args.count ("iid"));
        check_condition (args.count ("side"));
        check_condition (args.count ("qty"));
        check_condition (args.count ("price"));

        return m_runnable->submit (std::move (args));
    }
    catch (check_failure const & cf)
    {
        LOG_warn << "USAGE: submit (partition, iid, side, qty, price, [TIF], [order_type], [short_sell_qty], [MAQ])" << vr_EOL;
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::submit ABORTED:" << vr_EOL << exc_info (e);
    }
    return { };
}

order_token
ouch_session::replace (order_token const otk, arg_map && args)
{
    assert_nonnull (m_runnable);
    try
    {
        check_condition (args.count ("qty"));
        check_condition (args.count ("price"));

        return m_runnable->replace (otk, std::move (args));
    }
    catch (check_failure const & cf)
    {
        LOG_warn << "USAGE: replace otk (qty, price, [short_sell_qty], [MAQ])" << vr_EOL;
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::replace ABORTED:" << vr_EOL << exc_info (e);
    }
    return { };
}

void
ouch_session::cancel (order_token const otk, arg_map && args)
{
    assert_nonnull (m_runnable);
    try
    {
        return m_runnable->cancel (otk, std::move (args));
    }
    catch (check_failure const & cf)
    {
        LOG_warn << "USAGE: cancel otk" << vr_EOL;
    }
    catch (std::exception const & e)
    {
        LOG_warn << "API::cancel ABORTED:" << vr_EOL << exc_info (e);
    }
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
