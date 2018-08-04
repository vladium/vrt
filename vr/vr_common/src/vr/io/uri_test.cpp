
#include "vr/io/uri.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (uri, construction)
{
    std::string const native { "http://abc" };

    uri const u1 { "http://abc" };
    uri const u2 { native };
    uri const u3 { std::string { "http://abc" } };

    ASSERT_EQ (u1.native (), native);
    ASSERT_EQ (u2.native (), native);
    ASSERT_EQ (u3.native (), native);

    ASSERT_EQ (u1, u2);
    ASSERT_EQ (u1, u3);

    ASSERT_EQ (hash_value (u1), hash_value (u2));
    ASSERT_EQ (hash_value (u1), hash_value (u3));

    // invalid URIs:

    EXPECT_THROW (uri { "asdfsadf" }, invalid_input);
    EXPECT_THROW (uri { "http://" }, invalid_input);

    // copy construction:
    {
        uri const u1_cc { u1 };
        ASSERT_EQ (u1_cc, u1);
    }
    // move construction:
    {
        uri u1_cc { u1 };
        uri const u1_mc { std::move (u1_cc) };
        ASSERT_EQ (u1_mc, u1);
    }
}

TEST (uri, components)
{
    {
        uri const u { "http://abc.xyz" };

        uri::components const c = u.split ();

        EXPECT_EQ (c.scheme (), "http");
        EXPECT_EQ (c.authority (), "abc.xyz");
        EXPECT_TRUE (c.path ().empty ());
        EXPECT_TRUE (c.query ().empty ());
        EXPECT_TRUE (c.fragment ().empty ());
    }
    {
        uri const u { "file:///some/file/path" };

        uri::components const c = u.split ();

        EXPECT_EQ (c.scheme (), "file");
        EXPECT_TRUE (c.authority ().empty ());
        EXPECT_EQ (c.path (), "/some/file/path");
        EXPECT_TRUE (c.query ().empty ());
        EXPECT_TRUE (c.fragment ().empty ());
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
