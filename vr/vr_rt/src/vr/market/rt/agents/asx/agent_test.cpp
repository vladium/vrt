
#include "vr/io/dao/object_DAO.h"
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/agents/asx/agent.h"
#include "vr/market/rt/asx/execution_link.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/market/rt/market_data_feed.h"
#include "vr/market/sources/mock/mock_mcast_server.h"
#include "vr/market/sources/mock/mock_ouch_server.h"
#include "vr/mc/thread_pool.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"
#include "vr/str_hash.h"
#include "vr/sys/os.h"
#include "vr/util/di/container.h"
#include "vr/util/env.h"

#include "vr/test/files.h"
#include "vr/test/utility.h"

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

class simple_agent final: public agent<simple_agent>
{
    private: // ..............................................................

        using super         = agent<simple_agent>;

    public: // ...............................................................

        simple_agent (scope_path const & cfg_path) :
            super (cfg_path)
        {
        }

        // agent:

        void evaluate ()
        {
            switch (str_hash_32 (m_scenario))
            {
                case "submit.limit/accept/cancel"_hash:
                {
                    price_si_t const test_price = 12055000; // 12.055
                    qty_t const test_qty        = 30019;

                    switch (m_step)
                    {
                        case 0:
                        {
                            m_book_size_start = ex_book ().size (); // starting size
                            VR_IF_DEBUG (m_book_otk_count_start = ex_book ().otk_count ();)

                            order_type const & o = submit_limit_order (m_liid, side::BID, test_price, test_qty);
                            LOG_info << "order " << print (o.otk ()) << " submitted";
                            LOG_trace2 << "  " << print (o);

                            m_order = & o;
                            ++ m_step;
                        }
                        break;

                        case 1:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (o.fsm ().order_state () == order_type::fsm_state::order::live)
                            {
                                LOG_info << "order " << print (o.otk ()) << " is live";
                                LOG_trace2 << "  " << print (o);

                                check_zero (o.fsm ().pending_state ());

                                check_eq (o.price (), test_price);
                                check_eq (o.qty (), test_qty);
                                check_zero (o.qty_filled ());

                                ++ m_step;
                            }
                        }
                        break;

                        case 2:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            cancel_order (o);
                            LOG_info << "order " << print (o.otk ()) << " cancelled";
                            LOG_trace2 << "  " << print (o);

                            ++ m_step;
                        }
                        break;

                        case 3:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (o.fsm ().order_state () == order_type::fsm_state::order::done)
                            {
                                LOG_info << "order " << print (o.otk ()) << " is done";
                                LOG_trace2 << "  " << print (o);

                                check_zero (o.fsm ().pending_state ());

                                check_eq (o.price (), test_price);
                                check_eq (o.qty (), test_qty);
                                check_zero (o.qty_filled ());

                                erase_order (o);
                                m_order = nullptr;

                                LOG_info << "order erased";

                                ++ m_step;
                            }
                        }
                        break;

                        case 4:
                        {
                            check_eq (ex_book ().size (), m_book_size_start); // size should revert since the order has been erased
                            VR_IF_DEBUG (check_eq (ex_book ().otk_count (), m_book_otk_count_start);)

                            m_success = true;
                            request_stop ();
                        }
                        break;

                    } // end of switch
                }
                break; // ============================================================================================


                case "submit.limit/reject"_hash:
                {
                    price_si_t const test_price = 12055000; // 12.055
                    qty_t const test_qty        = 30019;

                    switch (m_step)
                    {
                        case 0:
                        {
                            m_book_size_start = ex_book ().size (); // starting size
                            VR_IF_DEBUG (m_book_otk_count_start = ex_book ().otk_count ();)

                            order_type const & o = submit_limit_order (m_liid, side::ASK, test_price, test_qty);
                            LOG_info << "order " << print (o.otk ()) << " submitted";
                            LOG_trace2 << "  " << print (o);

                            m_order = & o;
                            ++ m_step;
                        }
                        break;

                        case 1:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (o.fsm ().order_state () == order_type::fsm_state::order::done)
                            {
                                LOG_info << "order " << print (o.otk ()) << " is done (rejected)";
                                LOG_trace2 << "  " << print (o);

                                check_zero (o.fsm ().pending_state ());

                                check_eq (o.price (), test_price);
                                check_eq (o.qty (), test_qty);
                                check_zero (o.qty_filled ());

                                check_nonzero (field<_reject_code_> (o));

                                erase_order (o);
                                m_order = nullptr;

                                LOG_info << "order erased";

                                ++ m_step;
                            }
                        }
                        break;

                        case 2:
                        {
                            check_eq (ex_book ().size (), m_book_size_start); // size should revert since the order has been erased
                            VR_IF_DEBUG (check_eq (ex_book ().otk_count (), m_book_otk_count_start);)

                            m_success = true;
                            request_stop ();
                        }
                        break;

                    } // end of switch
                }
                break; // ============================================================================================


                case "submit.limit/accept/modify/cancel"_hash:
                {
                    price_si_t const test_price [] { 12055000, 12065000 };
                    qty_t const test_qty [] = { 30019, 92 };

                    switch (m_step)
                    {
                        case 0:
                        {
                            m_book_size_start = ex_book ().size (); // starting size
                            VR_IF_DEBUG (m_book_otk_count_start = ex_book ().otk_count ();)

                            order_type const & o = submit_limit_order (m_liid, side::BID, test_price [0], test_qty [0]);
                            LOG_info << "order " << print (o.otk ()) << " submitted";
                            LOG_trace2 << "  " << print (o);

                            m_order = & o;
                            ++ m_step;
                        }
                        break;

                        case 1:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (o.fsm ().order_state () == order_type::fsm_state::order::live)
                            {
                                LOG_info << "order " << print (o.otk ()) << " is live";
                                LOG_trace2 << "  " << print (o);

                                check_zero (o.fsm ().pending_state ());

                                check_eq (o.price (), test_price [0]);
                                check_eq (o.qty (), test_qty [0]);
                                check_zero (o.qty_filled ());

                                ++ m_step;
                            }
                        }
                        break;

                        case 2:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            modify_order (o, test_price [1], test_qty [1]);
                            LOG_info << "order " << print (o.otk ()) << " modified";
                            LOG_trace2 << "  " << print (o);

                            ++ m_step;
                        }
                        break;

                        case 3:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (! o.fsm ().pending_state ())
                            {
                                LOG_info << "order " << print (o.otk ()) << " modification ack'ed";
                                LOG_trace2 << "  " << print (o);

                                check_eq (o.fsm ().order_state (), order_type::fsm_state::order::live);

                                check_eq (o.price (), test_price [1]);
                                check_eq (o.qty (), test_qty [1]);
                                check_zero (o.qty_filled ());

                                ++ m_step;
                            }
                        }
                        break;

                        case 4:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            cancel_order (o);
                            LOG_info << "order " << print (o.otk ()) << " cancelled";
                            LOG_trace2 << "  " << print (o);

                            ++ m_step;
                        }
                        break;

                        case 5:
                        {
                            assert_nonnull (m_order);
                            order_type const & o = (* m_order);

                            if (o.fsm ().order_state () == order_type::fsm_state::order::done)
                            {
                                LOG_info << "order " << print (o.otk ()) << " is done";
                                LOG_trace2 << "  " << print (o);

                                check_zero (o.fsm ().pending_state ());

                                check_eq (o.price (), test_price [1]);
                                check_eq (o.qty (), test_qty [1]);
                                check_zero (o.qty_filled ());

                                erase_order (o);
                                m_order = nullptr;

                                LOG_info << "order erased";

                                ++ m_step;
                            }
                        }
                        break;

                        case 6:
                        {
                            check_eq (ex_book ().size (), m_book_size_start); // size should revert since the order has been erased
                            VR_IF_DEBUG (check_eq (ex_book ().otk_count (), m_book_otk_count_start);)

                            m_success = true;
                            request_stop ();
                        }
                        break;

                    } // end of switch
                }
                break; // ============================================================================================

            } // end of switch
        }

        // startable:

        void start () override
        {
            super::start (); // [chain]

            m_scenario = parameters ().at ("scenario");
            LOG_info << print (ID ()) << ": test scenario " << print (m_scenario) << " ...";
            m_liid = liids ().front ();
        }

        void stop () override
        {
            LOG_info << print (ID ()) << ": test scenario " << print (m_scenario) << " done";

            super::stop (); // [chain]
        }


        bool success () const
        {
            return m_success;
        }

        int32_t const & scenario_step () const
        {
            return m_step;
        }

    private: // ..............................................................

        std::string m_scenario { };
        liid_t m_liid { -1 };
        int32_t m_book_size_start { };
        VR_IF_DEBUG (int32_t m_book_otk_count_start { };)
        order_type const * m_order { };
        int32_t m_step { };
        bool m_success { false };

}; // end of class

} // end of 'test_'
//............................................................................
//............................................................................

TEST (agent, single_instrument)
{
    using namespace test_;

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
            { "TEST", {
                "/strategies/A_STRATEGY", {
                    { "instruments", { "RIO" } },
//                    { "scenario", "submit.limit/accept/cancel" },
//                    { "scenario", "submit.limit/reject" },
                    { "scenario", "submit.limit/accept/modify/cancel" } // TODO parameterize the test
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
                { "packet_begin",   2000000 },
                { "packet_limit",   2050000 },
                { "cap_root", util::getenv<fs::path> ("VR_CAP_ROOT", "").native () } // TODO
            }}
            ,
            { "ouch", {
                { "server", { { "port", mock_ouch_port_base } } },
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
            { "server", { { "host", "localhost" }, { "port", mock_ouch_port_base } } },
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
            { "A_STRATEGY", {
                { "class", { /* ...not used yet... */ } },
            }}
        }}
    };

    int32_t const PU_default    = 0;
    int32_t const PU_mdf        = 1;
    int32_t const PU_xl         = 2;
    int32_t const PU_test       = 3;

    util::di::container app { join_as_name ("APP", test::current_test_name ()),
        {
            { "default",        PU_default },

            { "server.itch",    "default" },
            { "server.ouch",    "default" },
            { "mdf",            PU_mdf },
            { "xl",             PU_xl },
            { "test",           PU_test },
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

        ("test",       new simple_agent      { "/agents/TEST" })
    ;

    app.start ();
    {
        app.run_for (5 * _1_second ()); // will terminate sooner if a stop is requested
    }
    app.stop ();

    simple_agent const & a = app ["test"];
    EXPECT_TRUE (a.success ()) << "\tlast scenario step reached: " << a.scenario_step ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
