
#include "vr/ref_tool/ops.h"

#include "vr/ref_tool/sources/asx/ITCH_refdata_parser.h"

#include "vr/io/cap/cap_reader.h"
#include "vr/io/dao/object_DAO.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/net/utility.h" // hostname()
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/io/stream_factory.h"
#include "vr/market/events/market_event_context.h"
#include "vr/market/sources/asx/itch/ITCH_filters.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/rt/cfg/resources.h"
#include "vr/sys/os.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

#include <iterator>

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

using instrument_map        = boost::unordered_map<ASX::iid_t, ASX::instrument>;


void
process_data (std::istream & in, util::date_t const & date, std::string const & tz, int32_t const pix, bitset32_t const ptf, instrument_map & out)
{
    using namespace ASX;

    using reader            = cap_reader;

    using visit_ctx         = market_event_context<_ts_origin_, _packet_index_, _partition_>;

    using pipeline          = ITCH_pipeline
                            <
                                ITCH_message_filter<visit_ctx>,
                                ITCH_product_filter<visit_ctx>,
                                ITCH_refdata_parser<visit_ctx>
                            >;

    using visitor           = Soup_frame_<io::mode::recv, pipeline>;

    bitset128_t mf
    {
        (_one_128 () << itch::message_type::order_book_dir) |
        (_one_128 () << itch::message_type::combo_order_book_dir) |
        (_one_128 () << itch::message_type::tick_size_table_entry)
    };

    visitor v
    {
        {
            { "date", date },
            { "tz", tz },
            { "messages", mf },
            { "products", ptf }
        }
    };

    visit_ctx ctx { };
    {
        field<_partition_> (ctx) = pix;
    }

    reader r { in, cap_format::wire };

    r.evaluate (ctx, v);

    ITCH_refdata_parser<visit_ctx> const & parser = v.get<ITCH_refdata_parser<visit_ctx>> ();

    int32_t const out_i_count = out.size ();
    instrument_map const & im = parser.instruments ();

    out.insert (im.begin (), im.end ());

    if (out.size () != out_i_count + im.size ())
        throw_x (invalid_input, "iid overlap b/w partitions");
}

bool
process_date (fs::path const & root_dir, util::date_t const & date, std::string const & tz, bitset32_t const ptf, instrument_map & out)
{
    check_condition (! date.is_special ());
    check_condition (out.empty ());

    fs::path const in_dir = root_dir / gd::to_iso_string (date) / "asx-02";
    if (! fs::exists (in_dir))
        return false;

    LOG_info << date;

    for (int32_t pix = 0; pix < 5; ++ pix)
    {
        fs::path const in_file = in_dir / ("glimpse.recv.203.0.119.213_2180" + string_cast (1 + pix) + ".soup");
        if (! fs::exists (in_file))
            return (pix > 0);

        LOG_trace1 << "reading glimpse " << print (io::absolute_path (in_file));

        std::unique_ptr<std::istream> const in = io::stream_factory::open_input (in_file);

        process_data (* in, date, tz, pix, ptf, out);
    }

    return true;
}

void
run (vr::json const & cfg, fs::path const & root_dir, std::tuple<util::date_t, util::date_t> const & dates, std::string const & tz, bitset32_t const ptf)
{
    util::di::container app { join_as_name (sys::proc_name (), net::hostname ()) };

    app.configure ()
        ("sql", new sql_connection_factory { cfg ["sql"] })
        ("DAO", new object_DAO { { { "cfg.ro", "make" }, { "cfg.rw", "make" } } })
    ;

    app.start ();
    {
        object_DAO & dao = app ["DAO"];

        auto & instruments = dao.rw_ifc<ASX::instrument> (); // r/w

        {
            auto tx { instruments.tx_begin () }; // do all of the DAO I/O within one transaction

            std::unique_ptr<instrument_map> im_prev { };

            for (auto d = std::get<0> (dates); d <= std::get<1> (dates); d += gd::days { 1 })
            {
                std::unique_ptr<instrument_map> im { std::make_unique<instrument_map> () };
                int32_t change_count { };

                if (! process_date (root_dir, d, tz, ptf, * im))
                    continue; // skip weekends/dates with no data

                // modifications and additions:

                for (auto const & kv : * im)
                {
                    bool changed = instruments.persist_as_of (d, kv.second);
                    change_count += changed;

                    if (VR_UNLIKELY ((im_prev != nullptr) && changed))
                    {
                        LOG_trace1 << '[' << d << "]: change in " << kv.second.symbol ();
                    }
                }

                // deletions:

                if (im_prev)
                {
                    for (auto const & kv : * im_prev)
                    {
                        auto const i = im->find (kv.first);

                        if (VR_UNLIKELY (i == im->end ()))
                        {
                            LOG_info << '[' << d << "]: deleting " << kv.first;
                            bool deleted = instruments.delete_as_of (d, kv.second.symbol ());
                            check_condition (deleted);

                            ++ change_count;
                        }
                        else
                        {
                            VR_IF_DEBUG
                            (
                                if (kv.second.partition () != i->second.partition ())
                                {
                                    LOG_info << '[' << d << "]: " << kv.first << " PARTITION CHANGE " << kv.first << " " << kv.second.partition () << " -> " <<  i->second.partition ();
                                }
                            )
                        }
                    }
                }

                LOG_info << '[' << d << "]: found " << im->size () << " instrument(s), changed " << change_count;

                im_prev.swap (im);
            }

            tx.commit ();
        }
    }
    app.stop ();
}

}// end of anonymous
//----------------------------------------------------------------------------

int32_t
op_make_ref (string_vector const & av)
{
    std::string cfg_name { };
    fs::path in_root { };
    fs::path out_file { };
    std::string tz { "Australia/Sydney" };
    std::string date_range_str { };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " make-ref [options] file" };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg_name)->value_name ("FILE|NAME"), "configuration file/ID")
        ("in,i",            bpopt::value (& in_root)->value_name ("DIR")->required (), "input dir")
        ("out,o",           bpopt::value (& out_file)->value_name ("FILE")->required (), "output db file")
        ("date_range,d",    bpopt::value (& date_range_str)->value_name ("DATE-DATE")->required (), "capture date range (inclusive)")
        ("time_zone,z",     bpopt::value (& tz)->value_name ("TIMEZONE"), "tz for timestamps [default: Australia/Sydney]")
        ("product,p",       bpopt::value<string_vector> (), "product type(s) to include [default: all]")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("in", 1);     // alias 1st positional option

    int32_t rc { };
    try
    {
        VR_ARGPARSE (av, opts, popts);

        io::uri const cfg_url = rt::resolve_as_uri (cfg_name);
        vr::json cfg { };
        {
            std::unique_ptr<std::istream> const in = io::stream_factory::open_input (cfg_url);
            (* in) >> cfg;
        }

        cfg ["sql"]["make"]["db"] = out_file.native (); // HACK

        std::tuple<util::date_t, util::date_t> const dates = util::parse_date_range (date_range_str);

        bitset32_t ptf { static_cast<bitset32_t> (-1) }; // default to "all"
        if (args.count ("product"))
        {
            string_vector const products = args ["product"].as<string_vector> ();

            ptf = 0;
            for (std::string const & s : products) ptf |= (1 << ASX::ITCH_product::value (s));

            LOG_info << "[filtering for products type(s): " << print (products) << ']';
        }

        run (cfg, in_root, dates, tz, ptf);
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
