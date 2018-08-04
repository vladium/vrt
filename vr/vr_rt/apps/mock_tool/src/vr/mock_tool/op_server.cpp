
#include "vr/mock_tool/ops.h"

#include "vr/io/files.h"
#include "vr/io/net/utility.h" // hostname()
#include "vr/io/stream_factory.h"
#include "vr/market/sources/mock/mock_mcast_server.h"
#include "vr/market/sources/mock/mock_ouch_server.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/sys/signals.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/util/env.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{
using namespace io;
using namespace market;


void
run (settings const & app_cfg, scope_path const & itch_cfg_path, scope_path const & ouch_cfg_path, util::date_t const & date, std::string const & tz)
{
    LOG_trace1 << "app cfg:\n" << print (app_cfg);

    util::di::container app { join_as_name (sys::proc_name (), "server", net::hostname ()),
        {
            { "server.itch",    1 }, // TODO set from args
            { "server.ouch",    1 }
        }
    };

    app.configure ()
        ("config",          new rt::app_cfg { app_cfg })
    ;

    if (! itch_cfg_path.empty ())
        app.add ("server.itch",     new mock_mcast_server { itch_cfg_path });
    if (! ouch_cfg_path.empty ())
        app.add ("server.ouch",     new mock_ouch_server { ouch_cfg_path });


    app.start ();
    {
        // TODO either use a shell again or pass duration/timepoint for when to stop

        app.run_for (/* TODO */100 * _1_second ());
    }
    app.stop ();
}

}// end of anonymous
//----------------------------------------------------------------------------

int32_t
op_server (string_vector const & av)
{
    std::string cfg_name { };
    scope_path itch_cfg_path { };
    scope_path ouch_cfg_path { };
    fs::path cap_root { util::getenv<fs::path> ("VR_CAP_ROOT", "") }; // TODO choose this consistently everywhere
    std::string tz { "Australia/Sydney" };
    std::string date_str { };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " server [options] file" };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg_name)->value_name ("FILE|NAME"), "configuration file/ID")
        ("itch_cfg",        bpopt::value (& itch_cfg_path)->value_name ("json_pointer"), "pointer to ITCH json cfg section")
        ("ouch_cfg",        bpopt::value (& ouch_cfg_path)->value_name ("json_pointer"), "pointer to OUCH json cfg section")
        ("date,d",          bpopt::value (& date_str)->value_name ("DATE")->required (), "session date")
        ("in,i",            bpopt::value (& cap_root)->value_name ("DIR"), "capture source dir")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("TIMEZONE"), "tz for timestamps [default: Australia/Sydney]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("in", 1);     // alias 1st positional option

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

        if (itch_cfg_path.empty () && ouch_cfg_path.empty ())
        {
            std::cerr << std::endl << "specify at least one of the cfg paths" << "\n\n" << opts << std::endl;
            return 1;
        }

        uri const app_cfg_url = rt::resolve_as_uri (cfg_name);

        util::date_t const date { util::parse_date (date_str) };
        check_condition (! date.is_special ());

        vr::json app_cfg { };
        {
            std::unique_ptr<std::istream> const in = io::stream_factory::open_input (app_cfg_url);
            (* in) >> app_cfg;
        }

        app_cfg ["app_cfg"]["date"] = gd::to_iso_string (date); // HACK

        if (! itch_cfg_path.empty ())
        {
            app_cfg ["mock_server"]["itch"]["cap_root"] = cap_root.native (); // HACK
        }

        run (app_cfg, itch_cfg_path, ouch_cfg_path, date, tz);
    }
    catch (std::exception const & e)
    {
        rc = 2;
        LOG_error << exc_info (e);
    }

    return rc;
}

} // end of namespace
//----------------------------------------------------------------------------
