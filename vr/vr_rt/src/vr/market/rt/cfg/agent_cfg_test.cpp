
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/di/container.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

TEST (agent_cfg, sniff)
{
    // create a minimal cfg to allow 'agent_cfg' to parse what it needs:

    settings cfg
    {
        { "agents", {
            { "SA", {
                "/strategies/strategy_A", {
                    { "instruments", { "BHP" } }
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

    util::di::container app { join_as_name ("APP", test::current_test_name ()) };

    app.configure ()
        ("config",      new rt::app_cfg { cfg })
        ("agents",      new agent_cfg { "/agents" })
    ;

    app.start ();
    {
        agent_cfg const & acfg = app ["agents"];

        ASSERT_EQ (acfg.liid_limit (), 3);

        boost::unordered_set<std::string> const expected_symbols { "BHP", "RIO", "TLS" };
        boost::unordered_set<std::string> unique { };

        for (liid_t liid = 0; liid < acfg.liid_limit (); ++ liid)
        {
            std::string const & liid_sym = acfg.liid_table ()[liid].m_symbol;

            ASSERT_TRUE (expected_symbols.count (liid_sym)) << "sym: " << liid_sym;
            ASSERT_TRUE (unique.insert (liid_sym).second) << "sym: " << liid_sym;
        }

        ASSERT_EQ (acfg.agents ().size (), 2);

        agent_descriptor const & SA = acfg.agents ()["SA"];
        EXPECT_EQ (signed_cast (SA.m_symbol_mapping.size ()), 1);
        EXPECT_TRUE (SA.m_symbol_mapping.left.count ("BHP"));

        agent_descriptor const & SB = acfg.agents ()["SB"];
        EXPECT_EQ (signed_cast (SB.m_symbol_mapping.size ()), 2);
        EXPECT_TRUE (SB.m_symbol_mapping.left.count ("RIO"));
        EXPECT_TRUE (SB.m_symbol_mapping.left.count ("TLS"));
    }
    app.stop ();
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
