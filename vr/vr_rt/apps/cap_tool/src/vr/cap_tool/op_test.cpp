
#include "vr/cap_tool/ops.h"

#include "vr/cap_tool/sources/asx/ITCH_test_visitor.h"
#include "vr/cap_tool/util/op_common.h"

#include "vr/io/cap/cap_reader.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/sources/asx/itch/Mold_frame_.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/util/argparse.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace io::net;
using namespace market;

//............................................................................
//............................................................................
namespace
{

template<cap_format::enum_t FORMAT>
int32_t
run (std::istream & in, util::date_t const & date, std::string const & tz)
{
    using namespace ASX;

    using reader            = cap_reader;

    using visit_ctx         = util::if_t<(FORMAT == cap_format::pcap),
                                market_event_context<_ts_origin_, _packet_index_, _partition_, _ts_local_, _ts_local_delta_, _seqnum_,  _dst_port_>,
                                market_event_context<_ts_origin_, _packet_index_, _partition_>>;

    using pipeline          = ITCH_test_visitor<visit_ctx>; // note: 'ITCH_ts_tracker' base requires '_partition_'

    using visitor           = util::if_t<(FORMAT == cap_format::pcap),
                                pcap_<IP_<UDP_<Mold_frame_<pipeline>>>>,
                                Soup_frame_<io::mode::recv, pipeline>>;

    visitor v
    {
        {
            { "date", date },
            { "tz", tz }
        }
    };

    visit_ctx ctx { };
    reader r { in, FORMAT };

    r.evaluate (ctx, v);

    return v.error_count ();
}

}// end of anonymous
//----------------------------------------------------------------------------

int32_t
op_test (string_vector const & av)
{
    fs::path in_file { };
    std::string date_override { };
    std::string tz { "Australia/Sydney" }; // ASX

    bpopt::options_description opts { "usage: " + sys::proc_name () + " test [options] file" };
    opts.add_options ()
        ("input,i",         bpopt::value (& in_file)->value_name ("FILE")->required (), "input pathname")
        ("date,d",          bpopt::value (& date_override)->value_name ("DATE"), "capture date [default: infer from pathname]")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("TIMEZONE"), "tz for timestamps [default: Australia/Sydney]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("input", 1);     // alias 1st positional option

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

        std::unique_ptr<std::istream> const in = io::stream_factory::open_input (in_file);
        bool const input_is_pcap = boost::regex_match (in_file.filename ().string (), pcap_regex ());

        LOG_info << "reading " << (input_is_pcap ? "mold: " : "soup: ") << print (io::absolute_path (in_file));

        util::date_t date { };
        {
            if (! date_override.empty ())
                date = util::parse_date (date_override);
            else
                date = util::extract_date (io::absolute_path (in_file).native ());
        }
        check_condition (! date.is_special ());

        rc = (input_is_pcap ? run<cap_format::pcap> : run<cap_format::wire>) (* in, date, tz);
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
