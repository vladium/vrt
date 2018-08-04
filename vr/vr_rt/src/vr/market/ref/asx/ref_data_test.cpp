
#include "vr/market/ref/asx/ref_data.h"
#include "vr/rt/cfg/resources.h"
#include "vr/util/di/container.h"

#include "vr/test/configure.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

TEST (ASX_ref_data, sniff)
{
    util::di::container app { join_as_name ("APP", test::current_test_name ()) };
    {
        test::configure_app_ref_data (app, { "RIO", "ABP", "TLS", "BHP" });
    }

    app.start ();
    {
        ref_data const & rd = app ["ref_data"];

        // basic API consistency:

        for (instrument const & i : rd)
        {
            std::string const & symbol = i.symbol ();
            iid_t const & iid = i.iid ();

            // some basic

            instrument const & i_sym = rd [symbol];
            ASSERT_EQ (& i_sym, & i) << "failed for symbol " << symbol;

            instrument const & i_iid = rd [iid];
            ASSERT_EQ (& i_iid, & i) << "failed for iid " << iid;
        }

        // check some hardcoded known-goods:
        {
            instrument const & i = rd [70915];
            EXPECT_EQ (i.symbol (), "RIO");
        }
        {
            instrument const & i = rd [85552];
            EXPECT_EQ (i.symbol (), "ABP");
        }
    }
    app.stop ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
