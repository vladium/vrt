
#include "vr/market/sources/mock/mock_ouch_server.h"

#include "vr/fields.h"
#include "vr/io/net/socket_factory.h"
#include "vr/io/pcap/pcap_reader.h"
#include "vr/io/net/utility.h" // min_size_or_zero, make_group_range_filter
#include "vr/io/stream_factory.h"
#include "vr/market/defs.h"
#include "vr/market/sources/mock/asx/mock_ouch_handlers.h"
#include "vr/market/sources/mock/mock_market_event_context.h"
#include "vr/market/sources/mock/mock_response.h"
#include "vr/mc/spinflag.h"
#include "vr/mc/spinlock.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/sys/cpu.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"

#include <boost/thread/thread.hpp>

#include <bitset>
#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
using namespace io;

//............................................................................

using spin_lock         = mc::spinlock<>;
using int_ops_checked   = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, true>;

//............................................................................

using data_link         = client_connection::data_link;

vr_static_assert (data_link::has_ts_last_recv ());
vr_static_assert (data_link::has_ts_last_send ());

constexpr int32_t data_link_capacity ()             { return (256 * 1024); }; // replicates 'market::ouch_link_capacity ()'

//............................................................................

constexpr timestamp_t client_heartbeat_timeout ()   { return (15 * _1_second ()); } // ASX-specific?
constexpr timestamp_t server_heartbeat_timeout ()   { return (1 * _1_second ()); } // ASX-specific?

//............................................................................

struct mock_ouch_server::pimpl final
{
    // select request policy impl:

    using request_handler_ctx   = mock_market_event_context<_ts_origin_, _partition_, _ts_local_, _mock_scenario_>;
    using request_handler       = ASX::scripted_handler<mock_ouch_server::pimpl, request_handler_ctx>;

    static constexpr int32_t min_available ()           { return net::min_size_or_zero<request_handler>::value (); }

    static constexpr int32_t new_cc_check_mask ()       { return 0xFFFF; }
    static constexpr int32_t new_cc_check_stagger ()    { return 100009; }

    VR_ENUM (state,
        (
            created,
            running,
            stopped
        ),
        printable

    ); // end of enum


    VR_ASSUME_COLD pimpl (mock_ouch_server & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_cfg_path { cfg_path }
    {
        LOG_trace1 << "configured with scope path " << print (cfg_path);
    }


    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        m_session_date = config.start_date ();

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_object (), cfg.type ());

        LOG_trace1 << "session date: " << m_session_date;

        m_rng_seed = config.rng_seed ();
        LOG_trace1 << "[seed = " << m_rng_seed << ']';

        check_nonzero (m_rng_seed);
        m_request_handler_args ["seed"] = m_rng_seed;

        int32_t const port_base = cfg.at ("server").at ("port");
        check_within (port_base, 64 * 1024);

        auto const & partitions = cfg.at ("partitions");

        bitset32_t pix_mask { };
        for (auto const & v : partitions)
        {
            int32_t const pix = v;
            check_within (pix, part_count ());
            pix_mask |= (1 << pix);
        }
        check_nonzero (pix_mask, partitions);

        m_active_partitions = pix_mask;
        LOG_trace1 << "active partition(s): " << std::bitset<part_count ()> (pix_mask);

        // open listening sockets for all active partitions:
        // TODO use 'thread_pool' instead of creating ad hoc threads here

        while (pix_mask > 0)
        {
            int32_t const pix_bit = (pix_mask & - pix_mask);
            int32_t const pix = int_ops_checked::log2_floor (pix_bit);
            pix_mask ^= pix_bit;

            partition & p = m_partitions [pix];

            p.m_listener.listen (port_base + pix);
            {
                // [don't make the listening thread inherit the current thread's affinity set]

                sys::affinity::scoped_thread_sched _ { make_bit_set (sys::cpu_info::instance ().PU_count ()).set () };

                p.m_listen_thread = boost::thread { std::ref (p.m_listener) };
            }
            p.m_state = state::running;
        }

        LOG_trace1 << "listening socket thread(s) started";
    }

    VR_ASSUME_COLD void stop ()
    {
        // TODO drain outgoing I/O buffers while not receiving any new requests (for more graceful client handling)

        bitset32_t pix_mask { m_active_partitions };

        while (pix_mask > 0)
        {
            int32_t const pix_bit = (pix_mask & - pix_mask);
            int32_t const pix = int_ops_checked::log2_floor (pix_bit);
            pix_mask ^= pix_bit;

            partition & p = m_partitions [pix];

            if (p.m_state == state::running)
            {
                // shut down partition listener and clean up pending accepts:

                LOG_trace1 << 'P' << pix << " requesting listener stop ...";
                p.m_listener.request_stop ();

                // now should be able to join the listener thread

                LOG_trace1 << 'P' << pix << " joining listener thread ...";
                p.m_listen_thread.join ();

                // with no new connections possible, disconnect all current client sessions:

                LOG_trace1 << 'P' << pix << " closing sessions ...";
                for (auto & kv : p.m_sessions)
                {
                    client_session & cs = kv.second;

                    cs.close ();
                }
                p.m_sessions.clear ();

                p.m_state = state::stopped;
            }
        }
    }

    // core step logic:

    VR_FORCEINLINE void step () // note: force-inlined
    {
        ++ m_step_counter;

        bitset32_t pix_mask { m_active_partitions };

        timestamp_t const now_utc = sys::realtime_utc ();

        // iterate over all active partitions:

        while (pix_mask > 0)
        {
            int32_t const pix_bit = (pix_mask & - pix_mask);
            int32_t const pix = int_ops_checked::log2_floor (pix_bit);
            pix_mask ^= pix_bit;

            step (pix, now_utc);
        }
    }

    // working a given partition:

    VR_FORCEINLINE void step (int32_t const pix, timestamp_t const now_utc)
    {
        partition & p = m_partitions [pix];

        // handle pending actions (outgoing requests):

        time_action_queue & taq = p.m_action_queue;

        while (true)
        {
            auto * const e = taq.front ();
            if (e == nullptr)
                break; // queue empty

            mock_response & r = * e->value ();

            bool const done = r.evaluate (now_utc); // support "multi-step" actions (e.g. fractional response writes)
            if (! done)
                break; // stop process events at this time without dequeueing 'e': every is correct as long as no new events get scheduled ahead of it

            taq.dequeue ();
        }

        // handle I/O in all current connections:

        // TODO modify logic to enforce 'm_action_pending_limit' per partition queue

        for (auto & kv : p.m_sessions)
        {
            client_session & cs = kv.second;

            if (VR_UNLIKELY (cs.m_state == io::client_connection::state::closed))
                continue;

            try
            {
                // incoming requests:

                assert_nonnull (cs.m_data_link);

                std::pair<addr_const_t, capacity_t> const rc = cs.m_data_link->recv_poll (); // non-blocking read

                auto available = rc.second;

                if (available >= min_available ()) // a message header is guaranteed to have been read entirely
                {
                    request_handler_ctx ctx { };

                    if (has_field<_partition_, request_handler_ctx> ())
                    {
                        field<_partition_> (ctx) = pix;
                    }
                    if (has_field<_ts_local_, request_handler_ctx> ())
                    {
                        // no matter when the request arrived to the link, 'now_utc' is the
                        // time when this action is considered for the first time:

                        field<_ts_local_> (ctx) = now_utc;
                    }
                    if (has_field<_ts_origin_, request_handler_ctx> ())
                    {
                        timestamp_t const ts_recv = cs.m_data_link->ts_last_recv ();

                        LOG_trace2 << '[' << m_step_counter << ", session " << kv.first << "]: ts_last_recv = " << print_timestamp (ts_recv);

                        // note that the recv timestamp as reported by the link is
                        // not the same as 'now_utc', hence we report the former through '_ts_origin_'
                        // (although it probably shouldn't factor into the simulated actions)

                        field<_ts_origin_> (ctx) = ts_recv;
                    }

                    LOG_trace2 << '[' << m_step_counter << ", " << print_timestamp (now_utc) << ", session " << kv.first << "] available " << available;
                    assert_condition (cs.m_request_handler); // has been set

                    int32_t consumed { };

                    do // loop over all full messages in the buffer
                    {
                        int32_t const rrc = cs.m_request_handler->consume (ctx, addr_plus (rc.first, consumed), available);
                        if (rrc < 0)
                            break; // partial message

                        // [reactions to client requests are in the event timer queue]

                        available -= rrc;
                        consumed += rrc;

                        assert_nonnegative (available);
                        assert_le (consumed, rc.second);
                    }
                    while (available >= min_available ());

                    if (consumed) cs.m_data_link->recv_flush (consumed); // no actual I/O, just buffer memory release, so never partial
                }

                // pending outgoing bytes (due to partial sends, actual or simulated):

                auto const len = (cs.m_send_committed - cs.m_send_flushed);
                if (len > 0)
                {
                    auto const rc = cs.m_data_link->send_flush (len);

                    cs.m_send_flushed += rc;
                }
                else if (VR_UNLIKELY (cs.m_state == io::client_connection::state::closing))
                {
                    assert_eq (cs.m_send_committed, cs.m_send_flushed); // don't close on a partial send

                    p.close_client_session (kv.first, cs);
                }

                // heartbeat housekeeping:

                if (VR_LIKELY (cs.m_state == io::client_connection::state::serving))
                {
                    if (VR_UNLIKELY (now_utc >= cs.m_data_link->ts_last_recv () + client_heartbeat_timeout ()))
                    {
                        LOG_warn << "timing out client session " << kv.first;

                        cs.m_state = io::client_connection::state::closing; // disable any enqueued responses
                    }
                    else if (VR_UNLIKELY (now_utc >= cs.m_data_link->ts_last_send () + server_heartbeat_timeout ()))
                    {
                        enqueue_heartbeat (pix, cs, now_utc); // note: can't send it here because a "partial" response write may be in progress
                    }
                }
            }
            catch (eof_exception const & eofe)
            {
                LOG_warn << "closing client session " << kv.first << ": " << eofe.what ();

                p.close_client_session (kv.first, cs);
            }

            // TODO GC closed sessions that have no pending requests in the queue
        }

        // periodically check for new connections:

        if (! ((m_step_counter + (new_cc_check_stagger () << pix)) & new_cc_check_mask ()))
        {
            std::lock_guard<spin_lock> _ { p.m_listener.m_lock };

            for (client_session & cs : p.m_listener.m_accepted)
            {
                int32_t const cs_ID = m_session_ID_counter ++;
                auto const i = p.m_sessions.emplace (cs_ID, std::move (cs)).first;

                // vary rng seed for different sessions:
                {
                    m_request_handler_args ["seed"] = ++ m_rng_seed;
                }

                i->second.configure_handler (* this);
            }

            p.m_listener.m_accepted.clear ();
        }
    }

    /*
     * 'client_connection' augmented with OUCH request handler
     */
    struct client_session final: public client_connection
    {
        client_session (std::unique_ptr<data_link> && link) :
            client_connection (std::move (link))
        {
        }

        void configure_handler (mock_ouch_server::pimpl & parent)
        {
            assert_condition (! m_request_handler);
            m_request_handler = std::make_unique<request_handler> (parent, * this, parent.m_request_handler_args);
        }

        VR_ASSUME_COLD void close ()
        {
            client_connection::close (); // [chain]
        }


        std::unique_ptr<request_handler> m_request_handler { };

    }; // end of class

    /*
     * (per-partition) callable for threads that listen for new client connections
     */
    struct connection_listener final
    {
        connection_listener ()  = default;

        /*
         * must be called prior to creating the executing thread
         */
        void listen (int32_t const port)
        {
            check_condition (! m_lsh); // called only once
            m_lsh.reset (new net::socket_handle { net::socket_factory::create_TCP_server (string_cast (port)) });
        }

        VR_ASSUME_COLD void request_stop ()
        {
            m_stop_requested.raise ();
        }

        // callable:

        void operator() ()
        {
            assert_nonnull (m_lsh);

            try
            {
                while (true)
                {
                    net::socket_handle csh { m_lsh->accept (m_stop_requested, 200 * _1_millisecond ()) }; // "interruptible" accept

                    timestamp_t const now_utc = sys::realtime_utc ();

                    auto const pn = csh.peer_name ();
                    LOG_info << '[' << print_timestamp (now_utc) << "] accepted new connection from " << std::get<0> (pn) << ':' << std::get<1> (pn);

                    {
                        std::lock_guard<spin_lock> _ { m_lock };

                        m_accepted.emplace_back (accept_client_connection (std::move (csh), data_link_capacity ())); // note: last use of 'csh'
                    }
                }
            }
            catch (stop_iteration const &) { }

            LOG_trace1 << '[' << print_timestamp (sys::realtime_utc ()) << "] shutting down connection listener...";
            {
                // first, make sure no new connections can be accepted:

                m_lsh.reset ();

                // next, stop all session in 'm_accepted' that haven't been picked up:

                {
                    std::lock_guard<spin_lock> _ { m_lock };

                    for (client_session & cs : m_accepted)
                    {
                        try
                        {
                            cs.close ();
                        }
                        catch (...) { }
                    }

                    m_accepted.clear ();
                }
            }

            LOG_trace1 << '[' << print_timestamp (sys::realtime_utc ()) << "] listener DONE";
        }

        std::unique_ptr<net::socket_handle> m_lsh { }; // created in 'listen()'
        std::vector<client_session> m_accepted { }; // protected by 'm_lock'
        spin_lock m_lock { };
        mc::spinflag<> m_stop_requested { };

    }; // end of nested class

    struct partition final
    {
        partition ()    = default;

        void close_client_session (int32_t const cs_ID, client_session & cs)
        {
            cs.m_data_link.reset ();
            cs.m_state = io::client_connection::state::closed;
        }

        // TODO method to GC (erase from 'm_sessions')


        connection_listener m_listener { }; // created in 'start()'
        boost::thread m_listen_thread { };  // created as not-a-thread, move-assigned in 'start()'
        boost::unordered_map<int32_t, client_session> m_sessions { };
        time_action_queue m_action_queue { };   // queue pending actions per partition, to sim partition concurrency
        state::enum_t m_state { state::created };

    }; // end of nested class


    util::date_t const & session_date () const
    {
        return m_session_date;
    }

    time_action_queue & action_queue (int32_t const pix)
    {
        assert_within (pix, part_count ());

        return m_partitions [pix].m_action_queue;
    }

    void enqueue_action (int32_t const pix, std::unique_ptr<mock_response> && action)
    {
        timestamp_t const ts_start = action->m_ts_start;

        action_queue (pix).enqueue (ts_start, std::move (action)); // note: last use of 'action'
    }

    void enqueue_heartbeat (int32_t const pix, client_connection & cc, timestamp_t const ts_start)
    {
        std::unique_ptr<mock_response> action { new ouch_heartbeat { cc, ts_start } };

        action_queue (pix).enqueue (ts_start, std::move (action)); // note: last use of 'action'
    }

    /*
     * note: returned client session does not have a handler set
     */
    static client_session accept_client_connection (net::socket_handle && accepted, int32_t const capacity)
    {
        recv_arg_map recv_args { { "capacity", capacity } };
        send_arg_map send_args { { "capacity", capacity } };

        return { std::make_unique<data_link> (std::move (accepted), recv_args, send_args) }; // TODO use 'tcp_link_factory'
    }


    mock_ouch_server & m_parent;
    scope_path const m_cfg_path;
    uint64_t m_rng_seed { };                 // set in 'start()'
    arg_map m_request_handler_args { }; // TODO fill in in start
    util::date_t m_session_date { };    // set in 'start()'
    std::array<partition, part_count ()> m_partitions { };
    int64_t m_step_counter { };
    int32_t m_action_pending_limit { }; // set in 'start()'
    bitset32_t m_active_partitions { }; // set in 'start()'
    int32_t m_session_ID_counter { };

}; // end of nested class
//............................................................................
//............................................................................

mock_ouch_server::mock_ouch_server (scope_path const & cfg_path) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    dep (m_config) = "config";
}

mock_ouch_server::~mock_ouch_server ()    = default; // pimpl
//............................................................................

void
mock_ouch_server::start ()
{
    m_impl->start ();
}

void
mock_ouch_server::stop ()
{
    m_impl->stop ();
}
//............................................................................

void
mock_ouch_server::step ()
{
    m_impl->step ();
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
