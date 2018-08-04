
#include "vr/containers/util/optional_map_adaptor.h"
#include "vr/io/net/utility.h"
#include "vr/market/cap/asx/glimpse_capture.h"
#include "vr/market/cap/mcast_capture.h"
#include "vr/market/cap/utility.h"
#include "vr/rt/timer_queue/timer_queue.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"
#include "vr/version.h"

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace market;
using namespace rt;

//............................................................................

using schedule          = std::tuple<std::string, std::string, std::string>;
using affinity_map      = util::optional_map_adaptor<int32_t>;

//............................................................................

int32_t
run (util::date_t const & date, std::string const & tz, schedule const & session, fs::path const & out_dir, std::string const & capture_ID,
     std::string const & glimpse_server, uint16_t const glimpse_port_base, std::vector<int64_t> const & glimpse_seqnums,
     std::string const & mcast_ifc, std::vector<net::mcast_source> const & mcast_sources,
     net::ts_policy::enum_t const tsp, bool const disable_nagle, affinity_map const & affinities)
{
    util::di::container app { join_as_name (sys::proc_name (), io::net::hostname ()) };

    app.configure ()
        ("glimpse", new ASX::glimpse_capture { glimpse_server, glimpse_port_base, disable_nagle, glimpse_seqnums, out_dir, capture_ID, affinities.get ("PU.capture") })
        ("capture", new mcast_capture { mcast_ifc, mcast_sources, tsp, out_dir, capture_ID, affinities.get ("PU.capture") })

        ("timers",  new timer_queue {
                            {
                                { "start.glimpse", string_cast (date) + ' ' + std::get<0> (session) + ';' + tz },
                                { "start.capture", string_cast (date) + ' ' + std::get<1> (session) + ';' + tz },
                                { "stop.capture",  string_cast (date) + ' ' + std::get<2> (session) + ';' + tz }
                            },
                            affinities.get ("PU.timers") })
    ;

    mcast_capture::stats const * cs { };

    app.start ();
    {
        {
            ASX::glimpse_capture & cap = app ["glimpse"];

            cap ();
        }
        LOG_info << "--------------------------------------------------------------------";
        {
            mcast_capture & cap = app ["capture"];

            cap ();
            cs = & cap.capture_stats ();
        }
    }
    app.stop ();


    if (! cs)
        return 10;
    else
    {
        if (! cs->m_total)   // no packets captured
            return 11;
        if (cs->m_dropped)   // some packets dropped
            return 12;
    }

    return 0;
}

} // end of namespace
//----------------------------------------------------------------------------

VR_APP_MAIN ()
{
    using namespace vr;

    fs::path out_root { };
    std::string mcast_ifc { };
    std::string glimpse_server { "203.0.119.213" };
    uint16_t glimpse_port_base { 21801 };
    std::string glimpse_seqnums_str { "1,1,1,1,1" };
    std::string session_str { "06:50-06:55-19:05" };
    std::string nodename { io::net::hostname () };
    std::string tsp { string_cast (io::net::ts_policy::hw_fallback_to_sw) };
    int32_t PU_timers  { VR_IF_THEN_ELSE (VR_RELEASE)(2, 13) };
    int32_t PU_capture { VR_IF_THEN_ELSE (VR_RELEASE)(4, 15) };
    std::string tz { util::local_tz () };

    std::string const time_zone_desc { "local tz override [default: " + tz + "]" };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " [options]" };
    opts.add_options ()
        ("out,o",           bpopt::value (& out_root)->value_name ("DIR")->required (), "output dir")
        ("mcast_source,s",  bpopt::value<string_vector> ()->required (), "source spec (src_ip->(grp_ip[:grp_port])+)")
        ("mcast_ifc,i",     bpopt::value (& mcast_ifc)->value_name ("interface"), "interface on which to join [let kernel choose]")
        ("glimpse_server",  bpopt::value (& glimpse_server)->value_name ("<host>"), "glimpse hostname/addr [203.0.119.213]")
        ("glimpse_port",    bpopt::value (& glimpse_port_base)->value_name ("NUM"), "glimpse port [21801]")
        ("glimpse_seqnum",  bpopt::value (& glimpse_seqnums_str)->value_name ("<NUM,NUM,...>"), "glimpse seqnum(s) [1,1,1,1,1]")
        ("session",         bpopt::value (& session_str)->value_name ("<TIME>-<TIME>-<TIME>"), "(local) time schedule")
        ("nodename,n",      bpopt::value (& nodename)->value_name ("name"), "node name to use as file prefix [hostname]")
        ("tsp,t",           bpopt::value (& tsp), "timestamp sourcing policy [hw_fallback_to_sw]")
        ("tcpnodelay,N",    bpopt::value<bool> ()->default_value (true)->value_name ("<bool>"), "TCP_NODELAY option [true]")
        ("pu_timers",       bpopt::value (& PU_timers)->value_name ("NUM"), "PU pinning for 'timer_queue' [2]")
        ("pu_capture",      bpopt::value (& PU_capture)->value_name ("NUM"), "PU pinning for 'capture'/'glimpse' [4]")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("TIMEZONE"), time_zone_desc.c_str ())

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };

    int32_t rc { };
    try
    {
        VR_ARGPARSE (ac, av, opts, popts);

        string_vector const session = util::split (session_str, "- ");
        check_eq (session.size (), 3);

        util::date_t const today = util::current_date_in (tz); // TODO this is a workaround for ASX-100 TODO can go back to current_local_date()
        std::string const capture_ID = make_capture_ID (nodename, today);

        std::vector<net::mcast_source> mcast_sources { };
        {
            string_vector const mcast_source_strs = args ["mcast_source"].as<string_vector> ();
            for (auto const & spec : mcast_source_strs)
            {
                mcast_sources.emplace_back (spec);
            }
        }

        std::vector<int64_t> const glimpse_seqnums { util::parse_int_list<int64_t> (glimpse_seqnums_str) };
        affinity_map const affinities { { "PU.timers", PU_timers }, { "PU.capture", PU_capture } };

        run (today, tz, schedule { session [0], session [1], session [2] }, out_root, capture_ID,
             glimpse_server, glimpse_port_base, glimpse_seqnums,
             mcast_ifc, mcast_sources,
             to_enum<io::net::ts_policy> (tsp), args ["tcpnodelay"].as<bool> (), affinities);
    }
    catch (std::exception const & e)
    {
        rc = 2;
        LOG_error << exc_info (e);
    }

    return rc;
}
//----------------------------------------------------------------------------
