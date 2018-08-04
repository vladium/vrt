
#include "vr/cap_tool/ops.h"

#include "vr/cap_tool/sources/asx/ITCH_printer.h"
#include "vr/cap_tool/sources/asx/OUCH_printer.h"
#include "vr/cap_tool/util/op_common.h"

#include "vr/io/cap/cap_reader.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/sources/asx/itch/ITCH_filters.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/market/sources/asx/itch/Mold_frame_.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

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

struct run_args final
{
    cap_kind::enum_t const m_kind;
    io::mode::enum_t const m_io_mode;
    bool const m_prefix;
    util::date_t const m_date;
    std::string const m_tz;
    bitset128_t const m_mf;
    std::set<int64_t> const & m_iidf;
    bitset32_t const m_ptf;
    bitset32_t const m_pf;

}; // end of class
//............................................................................

template<cap_kind::enum_t KIND, io::mode::enum_t IO_MODE>
struct select_visit_traits;

using namespace ASX;

// ITCH:

template<>
struct select_visit_traits<cap_kind::itch, io::mode::recv>
{
    using ctx           = market_event_context<_ts_origin_, _packet_index_, _partition_, _ts_local_, _ts_local_delta_, _seqnum_, _dst_port_>;

    using pipeline      = ITCH_pipeline
                        <
                            ITCH_message_filter<ctx>,
                            ITCH_iid_filter<ctx>,
                            ITCH_product_filter<ctx>,

                            ITCH_printer<ctx> // note: 'ITCH_ts_tracker' base requires '_partition_'
                        >;
    using visitor       = pcap_<IP_<UDP_<Mold_frame_<pipeline>>>>;

}; // end of specialization

// ITCH Glimpse:

template<>
struct select_visit_traits<cap_kind::glimpse, io::mode::recv>
{
    using ctx           = market_event_context<_ts_origin_, _packet_index_, _partition_>;

    using pipeline      = ITCH_pipeline
                        <
                            ITCH_message_filter<ctx>,
                            ITCH_iid_filter<ctx>,
                            ITCH_product_filter<ctx>,

                            ITCH_printer<ctx> // note: 'ITCH_ts_tracker' base requires '_partition_'
                        >;
    using visitor       = Soup_frame_<io::mode::recv, pipeline>;

}; // end of specialization

// OUCH:

template<io::mode::enum_t IO_MODE>
struct select_visit_traits<cap_kind::ouch, IO_MODE>
{
    using ctx           = util::if_t<(IO_MODE == io::mode::recv),
                          market_event_context<_ts_origin_, _packet_index_, _partition_>,
                          market_event_context<             _packet_index_, _partition_>>;

    using pipeline      = OUCH_printer<IO_MODE, ctx>; // TODO message and iid filtering
    using visitor       = Soup_frame_<IO_MODE, pipeline>;

}; // end of specialization
//............................................................................

template<cap_kind::enum_t KIND, io::mode::enum_t IO_MODE>
void
run_impl (std::istream & in, run_args const & args, cap_format::enum_t const format)
{
    using namespace ASX;

    using reader            = cap_reader;
    using visit_traits      = select_visit_traits<KIND, IO_MODE>;

    using ctx               = typename visit_traits::ctx;
    using visitor           = typename visit_traits::visitor;

    visitor v
    {
        {
            { "date",           args.m_date },
            { "tz",             args.m_tz },
            { "out",            & std::cout },
            { "prefix",         args.m_prefix },
            { "messages",       args.m_mf },
            { "instruments",    args.m_iidf },
            { "products",       args.m_ptf },
            { "partitions",     args.m_pf }
        }
    };

    LOG_info << "reading [" << KIND << ':' << IO_MODE << ':' << format << "] ...";

    ctx c { };
    reader r { in, format };

    r.evaluate (c, v);
}
//............................................................................

template<cap_kind::enum_t KIND>
struct run_for_kind final
{
    static void evaluate (std::istream & in, run_args const & args, cap_format::enum_t const format)
    {
        switch (args.m_io_mode)
        {
            case io::mode::recv: run_impl<KIND, io::mode::recv> (in, args, format); break;
            case io::mode::send: run_impl<KIND, io::mode::send> (in, args, format); break;

            default: VR_ASSUME_UNREACHABLE (args.m_io_mode);

        } // end of switch
    }

}; // end of class
//............................................................................

void
run (std::istream & in, run_args const & args, cap_format::enum_t const format)
{
    switch (args.m_kind)
    {
        case cap_kind::itch:    run_impl<cap_kind::itch,    io::mode::recv> (in, args, format); break; // 'recv' only
        case cap_kind::glimpse: run_impl<cap_kind::glimpse, io::mode::recv> (in, args, format); break; // 'recv' only

        case cap_kind::ouch:    run_for_kind<cap_kind::ouch>::evaluate (in, args, format); break;

        default: VR_ASSUME_UNREACHABLE (args.m_kind);

    } // end of switch
}

}// end of anonymous
//----------------------------------------------------------------------------

int32_t
op_dump (string_vector const & av)
{
    fs::path in_file { };
    std::string kind_str { };
    std::string format_str { };
    std::string io_mode_str { };
    std::string date_override { };
    std::string tz { "Australia/Sydney" };
    std::string partitions { };
    bool prefix { false };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " dump [options] file" };
    opts.add_options ()
        ("input,i",         bpopt::value (& in_file)->value_name ("FILE")->required (), "input pathname")
        ("kind",            bpopt::value (& kind_str)->value_name ("itch|ouch|glimpse"), "data kind [default: infer from file name]")
        ("format",          bpopt::value (& format_str)->value_name ("pcap|wire"), "data format [default: infer from file ext]")
        ("mode",            bpopt::value (& io_mode_str)->value_name ("recv|send"), "I/O mode [default: infer from file name]")
        ("date,d",          bpopt::value (& date_override)->value_name ("DATE"), "capture date [default: infer from pathname]")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("TIMEZONE"), "tz for timestamps [default: Australia/Sydney]")
        ("message,m",       bpopt::value<std::vector<char> > (), "message type(s) to include [default: all except 'seconds']")
        ("iid,s",           bpopt::value<std::vector<int64_t> > (), "symbol iids to include [default: all]")
        ("product,p",       bpopt::value<string_vector> (), "product type(s) to include [default: all]")
        ("partitions,P",    bpopt::value (& partitions)->value_name ("<num,num,...>"), "partition(s) to include [default: all]")
        ("prefix",          bpopt::bool_switch (& prefix), "print detailed message headers [default: false]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("input", 1);     // alias 1st positional option

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

        LOG_info << "opening " << print (io::weak_canonical_path (in_file)) << " ...";
        std::unique_ptr<std::istream> const in = io::stream_factory::open_input (in_file);

        fs::path const in_filename { in_file.filename () };

        // figure out data 'kind':

        cap_kind::enum_t kind { cap_kind::na };
        {
            if (! kind_str.empty ())
                kind = to_enum<cap_kind> (kind_str);
            else if (boost::regex_match (in_filename.native (), glimpse_regex ()))
                kind = cap_kind::glimpse;
            else if (boost::regex_match (in_filename.native (), ouch_regex ()))
                kind = cap_kind::ouch;
            else if (boost::regex_match (in_filename.native (), itch_regex ()))
                kind = cap_kind::itch;

            if (VR_UNLIKELY (kind == cap_kind::na))
            {
                std::cerr << std::endl << "can't infer data kind, override with '--kind' option\n\n" << opts << std::endl;
                return 1;
            }
        }

        // figure out data 'format':

        cap_format::enum_t format  { boost::regex_match (in_filename.native (), pcap_regex ()) ? cap_format::pcap : cap_format::wire };
        {
            if (! format_str.empty ())
                format = to_enum<cap_format> (format_str);
        }

        // figure out data 'kind':

        io::mode::enum_t io_mode { io::mode::recv };
        {
            if (! io_mode_str.empty ())
                io_mode = to_enum<io::mode> (io_mode_str);
            else if (boost::regex_match (in_filename.native (), send_regex ()))
                io_mode = io::mode::send;

            // [else assume the default is ok, or run() will error out]
        }

        // [optional] date override:

        util::date_t date { };
        {
            if (! date_override.empty ())
                date = util::parse_date (date_override);
            else
                date = util::extract_date (io::weak_canonical_path (in_file).native ());
        }
        check_condition (! date.is_special ());

        // message(s):

        bitset128_t mf { ~(_one_128 () << ASX::itch::message_type::seconds) }; // default to "all except 'seconds'"
        if (args.count ("message"))
        {
            std::vector<char> const messages = args ["message"].as<std::vector<char> > ();

            mf = 0;
            for (char m : messages) mf |= (_one_128 () << static_cast<uint32_t> (m));
            LOG_info << "[filtering for message type(s): " << print (messages) << ']';
        }

        // iid(s):

        std::set<int64_t> iidf { }; // default to "no iid filtering"
        if (args.count ("iid"))
        {
            std::vector<int64_t> const instruments = args ["iid"].as<std::vector<int64_t> > ();
            for (auto const & iid : instruments) iidf.insert (iid);
            LOG_info << "[filtering for " << iidf.size () << " instrument(s): " << print (iidf) << ']';
        }

        // ITCH product type:

        bitset32_t ptf { static_cast<bitset32_t> (-1) }; // default to "all"
        if (args.count ("product"))
        {
            string_vector const products = args ["product"].as<string_vector> ();

            ptf = 0;
            for (std::string const & s : products) ptf |= (1 << ASX::ITCH_product::value (s));

            LOG_info << "[filtering for products type(s): " << print (products) << ']';
        }

        // ASX partition mask:

        bitset32_t pf { static_cast<bitset32_t> (-1) }; // default to "all"
        if (! partitions.empty ())
        {
            std::vector<int32_t> const ps { util::parse_int_list<int32_t> (partitions) };

            pf = 0;
            for (int32_t pix : ps) pf |= (1 << pix);
        }

        run_args const rargs { kind, io_mode, prefix, date, tz, mf, iidf, ptf, pf };

        run (* in, rargs, format);
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
