
#include "vr/io/files.h"
#include "vr/io/streams.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/di/container.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................
/*
 * create a temp json file, load it as 'app_cfg'
 */
TEST (app_cfg, sniff)
{
    fs::path const file { test::unique_test_path ("json") };
    io::create_dirs (file.parent_path ());

    io::fd_ostream<> os { file };
    {
        os << R"(
{
    "val_0"   : 12345,
    "scope_0" : 
        {
            "scope_1" : ["V.0", "V.1", "V.2"]
        }
})"        << std::endl;
    }

    util::di::container app { join_as_name ("APP", test::current_test_name ()) };

    app.configure ()
        ("config", new app_cfg { io::uri { file } } )
    ;

    app.start ();
    {
        app_cfg const & cfg = app ["config"];

        int32_t const val_0 = cfg.root ().at ("val_0");
        EXPECT_EQ (val_0, 12345);

        ASSERT_TRUE (cfg.scope_exists ("/scope_0/scope_1"));

        settings const & abc = cfg.scope ("/scope_0/scope_1");
        ASSERT_EQ (3, signed_cast (abc.size ()));

        ASSERT_FALSE (cfg.scope_exists ("/scope_0/nonEXISTENT"));
        ASSERT_THROW (cfg.scope ("/scope_0/nonEXISTENT"), invalid_input);

        int32_t i = 0;
        for (auto const & v : abc)
        {
            std::string const expected = join_as_name ("V", i);
            EXPECT_EQ (v.get<std::string> (), expected);

            ++ i;
        }

        settings const & abc2 = cfg.root ().at ("scope_0").at ("scope_1");
        EXPECT_EQ (& abc2, & abc);
    }
    app.stop ();
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
