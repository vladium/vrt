
#include "vr/containers/util/small_vector.h"
#include "vr/io/dao/object_DAO.h"
#include "vr/io/net/utility.h" // min_size_or_zero
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/asx/execution_link.h"
#include "vr/market/rt/asx/utility.h" // order_token_generator
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/market/rt/market_data_feed.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/market/sources/mock/mock_mcast_server.h"
#include "vr/market/sources/mock/mock_ouch_server.h"
#include "vr/mc/mc.h"
#include "vr/mc/thread_pool.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/util/di/container.h"
#include "vr/util/env.h"
#include "vr/util/parse.h" // ltrim
#include "vr/util/random.h" // random_shuffle

#include "vr/test/files.h"
#include "vr/test/utility.h"

#include <algorithm>
#include <bitset>
#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace test_
{
using namespace io;


using int_ops           = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, false>; // unchecked

static constexpr int32_t part_count ()      { return execution_link::poll_descriptor::width (); }

// price conversion b/w 'xl_request' and what this source uses

using price_traits      = this_source_traits::price_traits<meta::find_field_def_t<_price_, xl_request>::value_type>;

//............................................................................
/*
 * note: this is *not* how a proper strategy would be written, it's much lower level
 *       for the purposes of framework testing and logging
 */
struct execution_data_consumer final: public mc::steppable_<mc::rcu<_reader_>>, public util::di::component, public startable,
                                      public Soup_frame_<mode::recv, OUCH_visitor<mode::recv, execution_data_consumer>, execution_data_consumer>
{
    using super         = Soup_frame_<mode::recv, OUCH_visitor<mode::recv, execution_data_consumer>, execution_data_consumer>;

    using visit_ctx     = market_event_context<_ts_local_, _ts_origin_, _partition_>;

    static constexpr int32_t min_available ()   { return io::net::min_size_or_zero<super>::value (); }


    execution_data_consumer (call_traits<agent_ID>::param aid, uint64_t const seed) :
        m_aid { aid },
        m_rnd { seed }
    {
        dep (m_config) = "config";
        dep (m_agents) = "agents";
        dep (m_xl) = "xl";
        dep (m_mdf) = "mdf";
    }

    // startable:

    void start () override
    {
        rt::app_cfg const & config = (* m_config);

        m_ex.m_otk_counter = std::max<uint32_t> (1, config.start_time ().time_of_day ().total_seconds ()); // try to be "monotonic" throughout the day

        // get our traded instruments from agent cfg:

        agent_cfg const & agents = (* m_agents);

        symbol_liid_relation const & slr = agents.agents ()[m_aid].m_symbol_mapping;

        for (auto const & sl : slr.left)
        {
            liid_t const & liid = sl.second;

            LOG_info << "  symbol/liid mapping: " << print (sl.first) << " -> " << liid;

            m_ex.m_liids.push_back (liid);
        }
        assert_nonempty (m_ex.m_liids);

        // note: 'agent_cfg' randomizes liid mappings, here we also randomize our symbol iteration order:
        {
            uint64_t const rng_seed = config.rng_seed ();
            LOG_info << '[' << print (m_aid) << ": seed = " << rng_seed << ']';

            util::random_shuffle (& m_ex.m_liids [0], m_ex.m_liids.size (), rng_seed);
        }

        // config seems ok, now connect to execution:

        auto ifc = m_xl->connect (m_aid);
        {
            m_ex.m_rqueue = & ifc.queue ();
            m_ex.m_partition_mask = ifc.partition_mask ();
            m_ex.m_otk_prefix = ifc.otk_prefix ();
        }
        LOG_info << print (m_aid) << ": link connected to " << m_ex.m_rqueue << ", active partitions: " << std::bitset<part_count ()> (m_ex.m_partition_mask);

        check_nonnull (m_ex.m_rqueue);
        check_nonzero (m_ex.m_partition_mask);
        check_nonzero (m_ex.m_otk_counter);
        check_in_inclusive_range (m_ex.m_otk_prefix, 0x0, 0xF);

        LOG_info << "configured with " << m_ex.m_liids.size () << " traded instrument(s)";
    }

    void stop () override
    {
        // TODO opposite of 'connect()' (if in design)

        LOG_info << print (m_aid) << ": consumed " << m_md.m_pos << " market data byte(s) in total, checksum: " << hex_string_cast (m_md.m_checksum);

        for (int32_t pix = 0; pix < part_count (); ++ pix)
        {
            link_state const & p = m_ex.m_part [pix];

            LOG_info << print (m_aid) << ": consumed " << p.m_pos << " P" << pix << " execution data byte(s) in total, checksum: " << hex_string_cast (p.m_checksum);
        }
    }

    // steppable:

    VR_ASSUME_HOT void step () final override
    {
        // poll market data:

        rcu_read_lock (); // [no-op on x86]
        {
            vr_static_assert (market_data_feed::poll_descriptor::width () == 1);

            market_data_feed::poll_descriptor const & pd = m_mdf->poll ();

            int32_t const len = (pd [0].m_pos - m_md.m_pos);
            if (len > 0)
            {
                DLOG_trace2 << '[' << print (m_aid) << ", recv'd @ " << print_timestamp (pd [0].m_ts_local) << "]: consuming " << len << " byte(s) ...";

                // touch every byte to trigger any segfaults/sigbuses/etc:

                uint8_t const * data = static_cast<uint8_t const *> (addr_plus (pd [0].m_end, -/* ! */len));
                m_md.m_checksum = util::crc32 (data, len, m_md.m_checksum);

                check_le (m_md.m_ts_local_last, pd [0].m_ts_local, m_md.m_pos);

                m_md.m_pos = pd [0].m_pos;
                m_md.m_ts_local_last = pd [0].m_ts_local;
            }
        }
        rcu_read_unlock (); // [no-op on x86]

        // randomized delay:
        {
            mc::pause (test::next_random (m_rnd) & 0xFF);
        }

        // poll execution data (note: the RCU read "critical section" could be merged with one above if that helps):

        assert_nonnull (m_ex.m_rqueue);
        bitset32_t const partition_mask { m_ex.m_partition_mask };

        rcu_read_lock (); // [no-op on x86]
        {
            execution_link::poll_descriptor const & pd = m_xl->poll ();

            bitset32_t pix_mask { partition_mask };
            while (pix_mask > 0)
            {
                int32_t const pix_bit = (pix_mask & - pix_mask); // TODO ASX-148
                int32_t const pix = int_ops::log2_floor (pix_bit);
                pix_mask ^= pix_bit;

                recv_position const & rcvd = pd [pix];
                link_state & p = m_ex.m_part [pix];

                int32_t available = (rcvd.m_pos - p.m_pos);
                if (available > 0)
                {
                    DLOG_trace2 << '[' << print (m_aid) << ", P" << pix << ", recv'd @ " << print_timestamp (rcvd.m_ts_local) << "]: consuming " << available << " byte(s) ...";

                    // TODO rm given that there's proper Soup/OUCH parsing done instead:
                    // touch every byte to trigger any segfaults/sigbuses/etc:

                    uint8_t const * const data = static_cast<uint8_t const *> (addr_plus (rcvd.m_end, -/* ! */available));

                    p.m_checksum = util::crc32 (data, available, p.m_checksum);

                    check_le (p.m_ts_local_last, rcvd.m_ts_local, p.m_pos);

                    visit_ctx ctx { };
                    {
                        field<_ts_local_> (ctx) = rcvd.m_ts_local;
                        field<_partition_> (ctx) = pix;
                    }

                    // note: 'execution_link' takes care of Soup TCP framing

                    int32_t consumed { };

                    assert_ge (available, min_available ()); // framing guarantee
                    do
                    {
                        int32_t const rrc = consume (ctx, addr_plus (data, consumed), available);
                        if (rrc < 0)
                            break;

                        available -= rrc;
                        consumed += rrc;
                    }
                    while (VR_UNLIKELY (available >= min_available ()));

                    assert_zero (available); // RCU reader design contract

                    p.m_pos = rcvd.m_pos;
                    p.m_ts_local_last = rcvd.m_ts_local;
                }
            }
        }
        rcu_read_unlock (); // [no-op on x86]

        // randomized delay:
        {
            mc::pause (test::next_random (m_rnd) & 0xFF);
        }

        // "logic":

        switch (VR_LIKELY_VALUE (m_ex.m_state, x_state::working))
        {
            case x_state::start:
            {
                auto e = m_ex.m_rqueue->try_enqueue ();
                if (e)
                {
                    xl_request & req = xl_request_cast (e);
                    {
                        field<_type_> (req) = xl_req::start_login;
                    }
                    e.commit ();
                    LOG_info << '[' << print (m_aid) << "]: sent login request";

                    m_ex.m_part_login_pending = partition_mask;
                    m_ex.m_state = x_state::login;
                }
            }
            break;

            case x_state::login:
            {
                // [waiting for acks from all active partitions to transition us to 'working']

                if (! m_ex.m_part_login_pending)
                {
                    LOG_info << '[' << print (m_aid) << "]: login successful, working";
                    m_ex.m_state = x_state::working;
                }
            }
            break;

            case x_state::working:
            {
                // round-trip some order(s) and validate iid/side/qty/price

                if (m_ex.m_order_submit_count < 1)
                {
                    auto e = m_ex.m_rqueue->try_enqueue ();
                    if (e)
                    {
                        auto const otk_counter = m_ex.m_otk_counter;
                        liid_t const liid = m_ex.m_liids [test::next_random (m_rnd) % m_ex.m_liids.size ()]; // choose a random instrument
                        order_token const otk = order_token_generator::make_with_prefix<1> (otk_counter, m_ex.m_otk_prefix);

                        xl_request & req = xl_request_cast (e);
                        {
                            field<_type_> (req) = xl_req::submit_order;

                            field<_liid_> (req) = liid;
                            field<_otk_> (req) = otk;

                            field<_side_> (req) = ord_side::BUY;
                            field<_qty_> (req) = 700;
                            field<_price_> (req) = price_traits::print_to_book (21.15); // TODO deal with tick size

                            field<_TIF_> (req) = ord_TIF::DAY;

                            m_ex.m_order_sent = req; // save for verification
                        }
                        e.commit ();
                        LOG_info << '[' << print (m_aid) << "]: submitted an order for liid " << liid << ", otk " << print (otk);

                        m_ex.m_otk_counter = otk_counter + 1;
                        ++ m_ex.m_order_submit_count;

                        m_ex.m_state = x_state::verifying;
                    }
                }
            }
            break;

            default: // verifying
            {
                if (m_ex.m_order_ack_count == 1)
                {
                    xl_request const & req = m_ex.m_order_sent;
                    ouch::order_accepted const & ack = m_ex.m_order_ackd;

                    check_eq (ack.state (), ord_state::LIVE);

                    check_eq (util::rtrim (ack.otk ()), field<_otk_> (req));

                    check_eq (ack.side (), field<_side_> (req));
                    check_eq (ack.qty (), field<_qty_> (req));
                    check_eq (ack.price (), price_traits::book_to_wire (field<_price_> (req)));

                    m_ex.m_state = x_state::done; // all good
                }
            }
            break;
        }
    }

    // SoupTCP (overrides of base Glimpse visits versions from 'SoupTCP_'):

    void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_accepted const & msg) // override
    {
        super::visit_SoupTCP_login (ctx, soup_hdr, msg); // [chain]

        vr_static_assert (has_field<_ts_local_, visit_ctx> ());
        vr_static_assert (has_field<_partition_, visit_ctx> ());

        int32_t const pix = field<_partition_> (ctx);
        LOG_info << '[' << print (m_aid) << "]: P" << pix << " login accepted @ " << print_timestamp (field<_ts_local_> (ctx)) << ": " << msg;

        check_nonzero (m_ex.m_part_login_pending & (1 << pix));

        x_link_state & p = m_ex.m_part [pix];

        p.m_session.assign (msg.session ().data (), msg.session ().size ());
        {
            auto const msg_seqnum = util::ltrim (msg.seqnum ());
            if (VR_LIKELY (! msg_seqnum.empty ()))
            {
                p.m_snapshot_seqnum = util::parse_decimal<int64_t> (msg_seqnum.m_start, msg_seqnum.m_size);
                LOG_trace1 << "  [" << print (m_aid) << "]: P" << pix << " session " << print (p.m_session) << "] seqnum set to " << p.m_snapshot_seqnum;
            }
        }

        // check the expected mock seqnum:

        check_eq (p.m_snapshot_seqnum, 100 * (1 + pix), pix);

        m_ex.m_part_login_pending &= ~(1 << pix); // clear the pending bit
    }

    void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_rejected const & msg) // override
    {
        vr_static_assert (has_field<_partition_, visit_ctx> ());

        // not supposed to occur for this particular test:

        throw_x (illegal_state, '[' + print (m_aid) + "]: P" + string_cast (field<_partition_> (ctx)) + " login rejected: " + print (msg));
    }

    // OUCH recv overrides:

    using super::visit;

    VR_ASSUME_HOT bool visit (ouch::order_accepted const & msg, visit_ctx & ctx) // override
    {
        LOG_trace1 << "order accepted: " << msg;

        // this is a multi-agent test, only react to this agent's orders:

        if (util::rtrim (msg.otk ()) == field<_otk_> (m_ex.m_order_sent)) // in lieu of a proper order book
        {
            LOG_info << '[' << print (m_aid) << "]: received the order ack";

            m_ex.m_order_ackd = msg;
            ++ m_ex.m_order_ack_count;
        }

        return super::visit (msg, ctx); // chain
    }


    struct link_state
    {
        io::pos_t m_pos { };
        timestamp_t m_ts_local_last { -1 };
        uint32_t m_checksum { 1 };

    }; // end of nested class

    struct x_link_state: public link_state
    {
        int64_t m_snapshot_seqnum { };  // set on on successful Soup login
        std::string m_session { };      // filled in on successful Soup login

    }; // end of nested class

    VR_ENUM (x_state,
        (
            start,
            login,
            working,
            verifying,
            done
        ),
        printable

    ); // end of enum


    rt::app_cfg const * m_config { };       // [dep]
    agent_cfg const * m_agents { };         // [dep]
    execution_link * m_xl { };              // [dep]
    market_data_feed const * m_mdf { };     // [dep]

    agent_ID const m_aid;
    uint64_t m_rnd;

    link_state m_md;    // market data-specific context
    struct
    {
        execution_link::ifc::request_queue * m_rqueue { };  // set in 'start()'
        bitset32_t m_partition_mask { };                    // set in 'start()'
        x_state::enum_t m_state { x_state::start };
        util::small_vector<liid_t, 4> m_liids { };          // populated in 'start()'
        std::array<x_link_state, part_count ()> m_part { };
        bitset32_t m_part_login_pending { };
        uint32_t m_otk_counter { };     // set in 'start()'
        uint32_t m_otk_prefix { };      // set in 'start()'
        int32_t m_order_submit_count { };
        int32_t m_order_ack_count { };
        xl_request m_order_sent { };
        ouch::order_accepted m_order_ackd { };

    } m_ex;             // execution-specific context

}; // end of class

} // end of 'test_'
//............................................................................
//............................................................................

TEST (ASX_integration_execution, multiple_consumers)
{
    uint64_t const seed = test::env::random_seed<uint64_t> ();

    // HACK find a date for which capture exists and set it as session date:

    fs::path const test_input = test::find_capture (source::ASX, "<"_rop, util::current_date_in ("Australia/Sydney"));
    LOG_info << "using test data in " << print (test_input);

    util::date_t const date = util::extract_date (test_input.native ());

    uint16_t const mock_ouch_port_base = 53406;

    settings cfg
    {
        { "app_cfg", {
            { "time", util::format_time (util::ptime_t { date, pt::seconds (0) }, "%Y-%b-%d %H:%M:%S") }
        }}
        ,
        { "agents", {
            { "SA", {
                "/strategies/strategy_A", {
                    { "instruments", { "CBA", "WBC" } }
                    /* ... */
                }
            }}
            ,
            { "SB", {
                "/strategies/strategy_B", {
                    { "instruments", { "RIO", "TLS" } }
                    /* ... */
                }
            }}
        }}
        ,
        { "sql", {
            { "sql.ro", {
                { "mode",   "ro" },
                { "cache",  "shared" },
                { "pool_size",  2 },
                { "on_open", {
                    "PRAGMA read_uncommitted=true;"
                }},
                { "db", rt::resolve_as_uri ("asx/ref.equity.db").native () }
            }}
        }}
        ,
        { "mock_server", {
            { "itch", {
                { "ifc", "lo" },
                { "packet_begin",   3000000 },
                { "packet_limit",   3050000 },
                { "cap_root", util::getenv<fs::path> ("VR_CAP_ROOT", "").native () } // TODO
            }}
            ,
            { "ouch", {
                { "server", { { "port", mock_ouch_port_base } }},
                { "partitions", { 0, 1, 2, 3 }} // listen on all partitions
            }}
        }}
        ,
        { "thread_pool", {
            { "rcu", {
                { "use_RT_callback", true },
                { "callback_PU", -1 },
            }}
        }}
        ,
        { "market_data_feed", {
            { "ifc", "lo" },
            { "tsp", "hw_fallback_to_sw" }, // note: more permissive mode than prod
            { "sources", "203.0.119.212->233.71.185.8, 233.71.185.9, 233.71.185.10, 233.71.185.11, 233.71.185.12" }
        }}
        ,
        { "execution_link", {
            { "server", { { "host", "localhost" }, { "port", mock_ouch_port_base } }},
            { "account", "MOCK" },
            { "credentials", json::array ({
                { "U0", "P0" },
                { "U1", "P1" },
                { "U2", "P2" },
                { "U3", "P3" }
            })}
        }}
        ,
        { "strategies", { // it's fine to configure more agents than what's enabled for "execution_link"
            { "strategy_A", {
                { "class", { /* ... */ } },
            }}
            ,
            { "strategy_B", {
                { "class", { /* ... */ } },
            }}
            ,
            { "strategy_C", {
                { "class", { /* ... */ } },
            }}
        }}
    };

    int32_t const PU_default    = 0;
    int32_t const PU_xl         = 1;
    int32_t const PU_a0         = 2;
    int32_t const PU_a1         = 3;
    int32_t const PU_mdf        = 4;

    util::di::container app { join_as_name ("APP", test::current_test_name ()),
        {
            { "default",        PU_default },

            { "server.itch",    "default" },
            { "server.ouch",    "default" },
            { "mdf",            PU_mdf },
            { "xl",             PU_xl },
            { "a0",             PU_a0 },
            { "a1",             PU_a1 },
        }
    };

    app.configure ()
        ("config",      new rt::app_cfg { cfg })
        ("ref_data",    new ref_data { { } })
        ("DAO",         new io::object_DAO { { { "cfg.ro", "sql.ro" } } })
        ("sql",         new io::sql_connection_factory { { { "sql.ro", cfg ["sql"]["sql.ro"] } } })

        ("server.itch", new mock_mcast_server { "/mock_server/itch" })
        ("server.ouch", new mock_ouch_server  { "/mock_server/ouch" })

        ("threads",     new mc::thread_pool   { "/thread_pool" })
        ("agents",      new agent_cfg         { "/agents" })
        ("mdf",         new market_data_feed  { "/market_data_feed" })
        ("xl",          new execution_link    { "/execution_link", "server.ouch" }) // note: make "xl" wait for "server.ouch" to start

        ("a0",          new test_::execution_data_consumer { "SA", (seed * 1) })
        ("a1",          new test_::execution_data_consumer { "SB", (seed * 3) })
    ;

    try
    {
        app.start ();
        {
            app.run_for (heartbeat_timeout () + _1_second ());
        }
        app.stop ();
    }
    catch (std::exception const & e)
    {
        LOG_warn << exc_info (e);
        throw; // re-throw
    }

    // verify that agents went through the login sequence and reached 'working' state:

    test_::execution_data_consumer const & a0 = app ["a0"];
    test_::execution_data_consumer const & a1 = app ["a1"];

    EXPECT_EQ (a0.m_ex.m_state, test_::execution_data_consumer::x_state::done);
    EXPECT_EQ (a1.m_ex.m_state, test_::execution_data_consumer::x_state::done);
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
