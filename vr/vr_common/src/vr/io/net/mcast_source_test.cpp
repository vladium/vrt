
#include "vr/io/net/mcast_source.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

TEST (mcast_source, sniff)
{
    {
        std::string const source { "203.0.119.214->233.71.185.16:53001, 233.71.185.17" };

        mcast_source const ms { source };

        EXPECT_EQ (ms.source (), "203.0.119.214");
        ASSERT_EQ (signed_cast (ms.groups ().size ()), 2);

        auto const & group_0 = ms.groups ()[0];
        EXPECT_EQ (std::get<0> (group_0), "233.71.185.16");
        EXPECT_EQ (std::get<1> (group_0), 53001);

        auto const & group_1 = ms.groups ()[1];
        EXPECT_EQ (std::get<0> (group_1), "233.71.185.17");
        EXPECT_LT (std::get<1> (group_1), 0); // not set

        std::string const msd_native = ms.native ();
        EXPECT_EQ (msd_native, source); // only for the above special choice of white space; in general there's no round trip
    }
}

TEST (mcast_source, backwards_compatibility)
{
    {
        std::string const source { "203.0.119.214=233.71.185.16#53001,233.71.185.17" };

        mcast_source const ms { source };

        EXPECT_EQ (ms.source (), "203.0.119.214");
        ASSERT_EQ (signed_cast (ms.groups ().size ()), 2);

        auto const & group_0 = ms.groups ()[0];
        EXPECT_EQ (std::get<0> (group_0), "233.71.185.16");
        EXPECT_EQ (std::get<1> (group_0), 53001);

        auto const & group_1 = ms.groups ()[1];
        EXPECT_EQ (std::get<0> (group_1), "233.71.185.17");
        EXPECT_LT (std::get<1> (group_1), 0); // not set
    }
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
