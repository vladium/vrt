
#include "vr/calc_tool/ops.h"

#include "vr/calc_tool/market/asx/sim_calc.h"

#include "vr/io/cap/cap_reader.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/market/books/asx/market_data_listener.h"
#include "vr/market/books/asx/market_data_view.h"
#include "vr/market/books/book_event_context.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace io::net;
using namespace market;
using namespace rt;

//............................................................................
//............................................................................
namespace
{

bool
run (std::string const & cfg_name, fs::path const & in_root, util::date_t const & date, std::string const & tz, fs::path const & out_root, int32_t const sim_day)
{
    using namespace ASX;

    fs::path const check_dir = in_root / gd::to_iso_string (date) / "asx-02";
    if (! fs::exists (check_dir))
        return false;
    if (fs::exists (check_dir / "issues.txt"))
    {
        LOG_warn << "skipping " << date << ": marked with data issues";
        return false;
    }

    uri const cfg_url = rt::resolve_as_uri (cfg_name);
    fs::path const out_dir = out_root / gd::to_iso_string (date);


    util::di::container app { "sim" };

    app.configure ()
        ("config",      new app_cfg { cfg_url })
        // TODO ref_data deps
        ("ref_data",    new ref_data { io::read_json (rt::resolve_as_uri ("asx/ref.equity.db")) })
    ;

    settings const unwind { 30, 60, 120, 300, 600, 1800, 3600 };

    LOG_info << '[' << date << ']';

    app.start ();
    {
        ref_data const & rd = app ["ref_data"];
        app_cfg const & config = app ["config"];


        using book_type         = limit_order_book<price_si_t, oid_t, user_data<sim_metadata>, level<_qty_, _order_count_>>;
        using view              = market_data_view<book_type>;

        view mdv // note that the same book instances owned by 'mdv' live through both glimpse and mcast eval runs
        {
            {
                { "ref_data",       std::cref (rd) },
                { "instruments",    std::cref (config.scope ("/instruments")) } // TODO this needs to switch to "agents"
            }
        };

        using reader            = cap_reader;

        // consume all glimpse data:

        for (int32_t pix = 0; pix < partition_count (); ++ pix)
        {
            using visit_ctx         = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_>;

            using selector          = view::instrument_selector<visit_ctx>;
            using listener          = market_data_listener<this_source (), book_type, visit_ctx>;
            using stats             = stats_calc<view, visit_ctx>;

            using pipeline          = ITCH_pipeline
                                    <
                                        selector,
                                        stats, // needs to be ahead of the book listener to handle final fills
                                        listener
                                    >;

            using visitor           = Soup_frame_<io::mode::recv, pipeline>;

            visitor v
            {
                {
                    { "view",       std::cref (mdv) }, // TODO support std::ref
                    { "ref_data",   std::cref (rd) },
                }
            };

            {
                std::string const filename = "glimpse.recv.203.0.119.213_2180" + string_cast (pix + 1) + ".soup";
                std::unique_ptr<std::istream> const in = stream_factory::open_input (in_root / gd::to_iso_string (date) / "asx-02" / filename);

                reader r { * in, cap_format::wire };

                visit_ctx ctx { };
                r.evaluate (ctx, v);
            }
            LOG_info << "[glimpse partition " << pix << " DONE]";
        }
//        LOG_info << "[glimpse DONE]";

        // continue by consuming mcast data:
        {
            using visit_ctx         = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_, _ts_local_, _ts_local_delta_, _seqnum_,  _dst_port_>;

            using selector          = view::instrument_selector<visit_ctx>;
            using listener          = market_data_listener<this_source (), book_type, visit_ctx>;
            using stats             = stats_calc<view, visit_ctx>;
            using sim               = sim_calc<view, visit_ctx>;

            using pipeline          = ITCH_pipeline
                                    <
                                        selector,
                                        stats, // needs to be ahead of the book listener to handle final fills
                                        listener,
                                        sim
                                    >;

            using visitor           = pcap_<IP_<UDP_<Mold_frame_<pipeline>>>>;

            visitor v
            {
                {
                    { "date",       date },
                    { "tz",         tz },

                    { "view",       std::cref (mdv) }, // TODO support std::ref
                    { "unwind",     std::cref (unwind) },
                    { "out_dir",    out_dir },
                    { "ref_data",   std::cref (rd) },
                }
            };

            {
                std::unique_ptr<std::istream> const in = stream_factory::open_input (in_root / gd::to_iso_string (date) / "asx-02" / "mcast.recv.p1p2.pcap.zst");

                reader r { * in, cap_format::pcap };

                visit_ctx ctx { };

                try
                {
                    r.evaluate (ctx, v);
                    LOG_info << "[itch DONE]";
                }
                catch (int32_t const done)
                {
                    LOG_info << "[sim DONE]";
                }
            }
            v.get<sim> ().emit (std::cerr, sim_day);
        }
    }
    app.stop ();

    return true;
}

}// end of anonymous
//----------------------------------------------------------------------------

int32_t
op_sim (string_vector const & av)
{
    std::string cfg { };
    fs::path in_root { };
    fs::path out_root { };
    std::string tz { "Australia/Sydney" };
//    std::string date_str { };
    std::string date_range_str { };

    auto const cols = sys::proc_tty_cols ();
    bpopt::options_description opts { "usage: " + sys::proc_name () + " sim [options] file", cols, cols / 2u };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg)->value_name ("FILE|NAME"), "configuration file/ID")
        ("in,i",            bpopt::value (& in_root)->value_name ("DIR")->required (), "input dir")
        ("out,o",           bpopt::value (& out_root)->value_name ("DIR")->required (), "output dir")
//        ("date,d",          bpopt::value (& date_str)->value_name ("DATE")->required (), "date")
        ("date_range,d",    bpopt::value (& date_range_str)->value_name ("<DATE>:<DATE>")->required (), "capture date range (inclusive)")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("<TIMEZONE>"), "tz for timestamps [default: Australia/Sydney]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("in", 1);     // alias 1st positional option

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

//        util::date_t const date = util::parse_date (date_str);
        std::tuple<util::date_t, util::date_t> dates = util::parse_date_range (date_range_str);

        int32_t sim_day { };
        for (auto & d = std::get<0> (dates); d <= std::get<1> (dates); d += gd::days { 1 })
        {
            sim_day += run (cfg, in_root, d, tz, out_root, sim_day);
        }
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
