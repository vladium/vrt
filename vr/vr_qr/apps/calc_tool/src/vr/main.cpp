
#include "vr/calc_tool/ops.h"

#include "vr/str_hash.h"
#include "vr/util/argparse.h"
#include "vr/util/logging.h"
#include "vr/version.h"

//----------------------------------------------------------------------------

VR_APP_MAIN ()
{
    using namespace vr;

    bpopt::options_description opts { "usage: " + sys::proc_name () + " [options] <op> [op options]" };
    opts.add_options ()
        ("op",          bpopt::value<std::string> ()->value_name ("<op>")->required (),  "operation ('sim|stats|auction')")
        ("op_args",     bpopt::value<string_vector> (),                                  "operation options")
    ;

    bpopt::positional_options_description popts { };
    {
        popts.add ("op",        1);     // alias 1st positional option as op name
        popts.add ("op_args",   -1);    // multiple positional args for ops
    }

    int32_t rc { };
    try
    {
        VR_OP_ARGPARSE (ac, av, opts, popts);

        std::string const op { args ["op"].as<std::string> () };
        switch (str_hash_32 (op))
        {
            case "sim"_hash:        rc = op_sim (op_args); break;
            case "stats"_hash:      rc = op_stats (op_args); break;
            case "auction"_hash:    rc = op_auction (op_args); break;

            default: { rc = 1; std::cerr << "unrecognized option '" << op << "'\n" << opts; }

        } // end of switch
    }
    catch (std::exception const & e)
    {
        rc = 2;
        LOG_error << exc_info (e);
    }

    return rc;
}
//----------------------------------------------------------------------------
