
#include "vr/cert_tool/ops.h"

#include "vr/cert_tool/market/asx/ouch_session.h"
#include "vr/cert_tool/market/asx/ouch_shell.h"
#include "vr/containers/util/optional_map_adaptor.h"
#include "vr/io/net/utility.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/str_hash.h"
#include "vr/util/argparse.h"
#include "vr/util/di/container.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace market;

using affinity_map      = util::optional_map_adaptor<int32_t>;


inline void // internal linkage
run (uri const app_cfg_url, scope_path const & cfg_path, fs::path const & out_root,
     bool const disable_nagle, std::vector<int64_t> const & seqnums, affinity_map const & affinities, bool const live_run)
{
    util::di::container app { join_as_name (sys::proc_name (), "ouch", io::net::hostname ()) };

    app.configure ()
        ("config",      new rt::app_cfg { app_cfg_url })
        ("shell",       new ASX::ouch_shell { })
        ("session",     new ASX::ouch_session { cfg_path, out_root, disable_nagle, seqnums, affinities.get ("PU.ouch"), live_run })
    ;

    LOG_info << "[PID " << ::getpid () << "] " << print (app.name ()) << " starting ...";
    app.start ();
    {
        ASX::ouch_shell & s = app ["shell"];

        s.run ("\e[1;7m OUCH:\e[0m ");
    }
    LOG_info << "[PID " << ::getpid () << "] " << print (app.name ()) << " stopping ...";
    app.stop ();
}
//----------------------------------------------------------------------------
/*
 * ./cert_tool ouch -c .../cert_tool.json -p /env/ouch/prod ...
 */
int32_t
op_ouch_session (string_vector const & av)
{
    std::string cfg_name { }; // required
    scope_path cfg_path { }; // required
    fs::path out_root { "/dev/shm/link.capture" }; // TODO upgrade to 'create_link_capture_path()'
    std::string seqnums { "1, 1, 1, 1, 1" };
    int32_t PU_ouch { 5 };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " [options] <op> [op options]" };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg_name)->value_name ("FILE|NAME")->required (), "configuration file/name")
        ("ouch_cfg,p",      bpopt::value (& cfg_path)->value_name ("json_pointer")->required (), "pointer to OUCH json cfg section")
        ("out,o",           bpopt::value (& out_root)->value_name ("DIR"), "link capture output root [/dev/shm/link.capture]")
        ("seqnum,n",        bpopt::value (& seqnums)->value_name ("<num,num,...>"), "seqnum(s)")
        ("tcpnodelay,N",    bpopt::bool_switch ()->default_value (true), "TCP_NODELAY option [true]")
        ("pu_ouch",         bpopt::value (& PU_ouch)->value_name ("<num>"), "PU pinning for 'ouch_session' [5]")
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
        affinity_map const affinities { { "PU.ouch", PU_ouch } };

        run (app_cfg_url, cfg_path, out_root, args ["tcpnodelay"].as<bool> (), util::parse_int_list<int64_t> (seqnums), affinities, args ["live"].as<bool> ());
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
