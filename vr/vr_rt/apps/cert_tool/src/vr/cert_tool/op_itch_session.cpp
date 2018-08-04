
#include "vr/cert_tool/ops.h"

#include "vr/cert_tool/market/asx/itch_session.h"
#include "vr/cert_tool/market/asx/itch_shell.h"
#include "vr/containers/util/optional_map_adaptor.h"
#include "vr/io/dao/object_DAO.h"
#include "vr/io/files.h"
#include "vr/io/net/utility.h"
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"
#include "vr/str_hash.h"
#include "vr/util/argparse.h"
#include "vr/util/di/container.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace market;

using affinity_map      = util::optional_map_adaptor<int32_t>;


inline void // internal linkage
run (uri const app_cfg_url, scope_path const & cfg_path, char const feed, uri const ref_data_url, bool const use_snapshot, fs::path const & out_root,
     bool const disable_nagle, std::vector<int64_t> const & seqnums, affinity_map const & affinities, bool const live_run)
{
    util::di::container app { join_as_name (sys::proc_name (), "itch", io::net::hostname ()) };

    settings sql_cfg_ro
    {
        { "mode", "ro" },
        { "cache", "shared" },
        { "db", rt::resolve_as_uri ("asx/ref.equity.db").native () },
        { "pool_size", 2 },
        { "on_open",
            {
                "PRAGMA read_uncommitted=true;"
            }
        }
    };

    app.configure ()
        ("config",      new rt::app_cfg { app_cfg_url })
        ("DAO",         new object_DAO { { { "cfg.ro", "sql.ro" } } })
        ("ref_data",    new ASX::ref_data { io::read_json (ref_data_url) })
        ("shell",       new ASX::itch_shell { })
        ("session",     new ASX::itch_session { cfg_path, feed, use_snapshot, out_root, disable_nagle, seqnums, affinities.get ("PU.itch"), live_run })
        ("sql",         new sql_connection_factory { { { "sql.ro", std::move (sql_cfg_ro) } } })
    ;

    LOG_info << "[PID " << ::getpid () << "] " << print (app.name ()) << " starting ...";
    app.start ();
    {
        ASX::itch_shell & s = app ["shell"];

        s.run ("\e[1;7m ITCH:\e[0m ");
    }
    LOG_info << "[PID " << ::getpid () << "] " << print (app.name ()) << " stopping ...";
    app.stop ();
}
//----------------------------------------------------------------------------
/*
 * ./cert_tool itch -c .../cert_tool.json -p /env/itch/prod ...
 */
int32_t
op_itch_session (string_vector const & av)
{
    std::string cfg_name { }; // required
    scope_path cfg_path { }; // required
    std::string ref_data_name { }; // required
    fs::path out_root { "/dev/shm/link.capture" }; // TODO upgrade to 'create_link_capture_path()'
    char feed { 'A' };

    std::string seqnums { "0, 0, 0, 0, 0" };
    int32_t PU_itch { 7 };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " [options] <op> [op options]" };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg_name)->value_name ("FILE|NAME")->required (), "configuration file/name")
        ("itch_cfg,p",      bpopt::value (& cfg_path)->value_name ("json_pointer")->required (), "pointer to ITCH json cfg section")
        ("refdata,r",       bpopt::value (& ref_data_name)->value_name ("FILE|NAME")->required (), "ref data file/name")
        ("out,o",           bpopt::value (& out_root)->value_name ("DIR"), "link capture output root [/dev/shm/link.capture]")
        ("feed,f",          bpopt::value (& feed)->value_name ("A|B"), "feed [A]")
        ("seqnum,n",        bpopt::value (& seqnums)->value_name ("<num,num,...>"), "seqnum(s)")
        ("use_glimpse,G",   bpopt::bool_switch ()->default_value (false), "connect to the glimpse server [false]")
        ("tcpnodelay,N",    bpopt::bool_switch ()->default_value (true), "TCP_NODELAY option [true]")
        ("pu_itch",         bpopt::value (& PU_itch)->value_name ("<num>"), "PU pinning for 'itch_session' [7]")
        ("live",            bpopt::bool_switch ()->default_value (false), "live mode [false]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

        uri const app_cfg_url = rt::resolve_as_uri (cfg_name);
        uri const ref_data_url = rt::resolve_as_uri (ref_data_name); // note: a json array of symbols

        std::vector<int64_t> const sns { util::parse_int_list<int64_t> (seqnums) };
        affinity_map const affinities { { "PU.itch", PU_itch } };

        run (app_cfg_url, cfg_path, feed, ref_data_url,
             args ["use_glimpse"].as<bool> (), out_root,
             args ["tcpnodelay"].as<bool> (), sns, affinities, args ["live"].as<bool> ());
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
