
#include "vr/ref_tool/ops.h"

#include "vr/enums.h"
#include "vr/io/dao/object_DAO.h"
#include "vr/io/files.h"
#include "vr/io/net/utility.h" // hostname()
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/io/stream_factory.h"
#include "vr/market/ref/asx/instrument.h"
#include "vr/market/ref/asx/locate.h"
#include "vr/rt/cfg/resources.h"
#include "vr/sys/os.h"
#include "vr/util/argparse.h"
#include "vr/util/datetime.h"
#include "vr/util/di/container.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace market;

//............................................................................
//............................................................................
namespace
{
using namespace ASX;

using instrument_map        = boost::unordered_map<std::string, locate>;

VR_ENUM (loc_line,
    (
        date_header,
        col_header,
        data_row,
        trailer
    ),
    iterable

); // end of enum

string_vector const COL_HEADER { "TICKER", "SEC_ID", "REQ QTY", "RATE" , "AVAIL QTY", "RATE DESC" };
std::string const UTRL { "UTRL" };

boost::regex const RESTRICTED_RE { R"(.*restricted.*)", boost::regex::icase };

//............................................................................

void
process_data (std::istream & in, util::date_t const & date, std::string const & tz, instrument_map & out)
{
    using locate_qty_value_type     = meta::find_field_def_t<_qty_, locate>::value_type;

    int32_t lc { };
    loc_line::enum_t state { loc_line::date_header };

    for (std::string line; std::getline (in, line); ++ lc)
    {
        boost::trim (line); // in-place

        switch (state)
        {
            case loc_line::date_header:
            {
                util::date_t const uhdr_date { util::extract_date (line) };
                check_condition (! uhdr_date.is_special (), line);
                check_eq (uhdr_date + gd::days { 1 }, date, line);

                ++ state;
            }
            break;

            case loc_line::col_header:
            {
                string_vector const cols = util::split (line, ",", /* keep_empty_tokens */true);
                check_eq (cols.size (), COL_HEADER.size ());
                check_eq (cols, COL_HEADER);

                ++ state;
            }
            break;

            case loc_line::data_row:
            {
                string_vector const tokens = util::split (line, ",", /* keep_empty_tokens */true);

                if (VR_LIKELY (tokens.size () == COL_HEADER.size ()))
                {
                    // populate a new 'locate' instance:
                    {
                        std::string symbol = tokens [0];
                        check_condition (boost::ends_with (symbol, " AU"), symbol);
                        symbol.resize (symbol.size () - 3);

                        std::string const & rate_key_tok = tokens [5];
                        if (VR_UNLIKELY (boost::regex_match (rate_key_tok, RESTRICTED_RE)))
                        {
                            LOG_warn << "skipping restricted locate for " << print (symbol) << ": [" << line << ']';
                            break;
                        }

                        auto const i = out.emplace (symbol, locate { });
                        if (VR_UNLIKELY (! i.second))
                            throw_x (invalid_input, "duplicate locate symbol " + print (symbol));

                        locate & l = i.first->second;

                        l.symbol () = symbol;
                        // [l.iid () is set by the caller]

                        std::string const & rate_tok = tokens [3];
                        std::string const & qty_avail_tok = tokens [4];

                        LOG_trace2 << print (symbol) << ": qty_avail " << qty_avail_tok << ", rate " << rate_tok << ", rate key " << print (rate_key_tok);

                        locate_qty_value_type const qty = util::parse_decimal_nonnegative<int64_t> (qty_avail_tok.data (), qty_avail_tok.size ());
                        check_in_range (qty, 0, std::numeric_limits<locate_qty_value_type>::max ());

                        ratio_si_t rate { };

                        if (VR_UNLIKELY (rate_tok.empty ()))
                        {
                            check_zero (qty, line);
                        }
                        else
                        {
                            rate = static_cast<ratio_si_t> (boost::lexical_cast<double> (rate_tok) * (ratio_si_scale () / 100));
                            check_lt (std::abs (static_cast<double> (rate * 100) / ratio_si_scale () - boost::lexical_cast<double> (rate_tok)), 1.0e-7);
                        }

                        l.qty () = qty;
                        l.rate () = rate;
                        l.key () = rate_key_tok;

                        LOG_trace3 << "filled locate intance: " << l;
                    }

                    break;
                }
                else if (tokens.size () == 1)
                {
                    ++ state;

                    // [fall through]
                }
                else
                {
                    throw_x (invalid_input, "unexpected line " + string_cast (lc + 1) + " content: [" + line + ']');
                }
            }
            // !!! FALL THROUGH !!!
            /* no break */

            case loc_line::trailer:
            {
                string_vector const tokens = util::split (line, " ");
                check_eq (tokens.size (), 2);
                check_eq (tokens [0], UTRL);

                std::string const & lc_tok = tokens [1];
                int64_t const trl_lc = util::parse_decimal_nonnegative<int64_t> (lc_tok.data (), lc_tok.size ());
                LOG_trace1 << "lc: " << lc << ", trl_lc: " << trl_lc;

                check_eq (trl_lc, lc - 2); // number of locate data lines matches what the trailer says
            }
            goto done;

            default: VR_ASSUME_UNREACHABLE (state);

        } // end of switch
    }

done:

    LOG_info << "read " << lc << " line(s), found " << out.size () << " locate(s)";
}
//............................................................................

bool
process_date (fs::path const & root_dir, util::date_t const & date, std::string const & tz, instrument_map & out)
{
    check_condition (! date.is_special ());
    check_condition (out.empty ());

    boost::regex const loc_re { R"(.*standing_loc_res_)" + gd::to_iso_string (date) + R"([^.]*\.csv$)" };
    optional<fs::path> const in_file_opt = io::find_file (loc_re, root_dir);

    if (! in_file_opt)
    {
        LOG_warn << '[' << date << "]: no file found matching '" << loc_re.str () << '\'';
        return false;
    }

    fs::path const & in_file = (* in_file_opt);
    assert_condition (in_file.is_absolute ()); // find_file() guarantee

    LOG_info << '[' << date << "]: processing " << print (in_file);

    std::unique_ptr<std::istream> const in = io::stream_factory::open_input (in_file);

    process_data (* in, date, tz, out);

    return true;
}
//............................................................................

void
run (vr::json const & cfg, fs::path const & root_dir, std::tuple<util::date_t, util::date_t> const & dates, std::string const & tz)
{
    util::di::container app { join_as_name (sys::proc_name (), net::hostname ()) };

    app.configure ()
        ("sql", new sql_connection_factory { cfg ["sql"] })
        ("DAO", new object_DAO { { { "cfg.ro", "read.ref" }, { "cfg.rw", "make" } } })
    ;

    app.start ();
    {
        object_DAO & dao = app ["DAO"];

        auto & instruments = dao.ro_ifc<instrument> (); // ro
        auto & locates = dao.rw_ifc<locate> (); // r/w

        {
            auto tx { locates.tx_begin () }; // do all of the DAO write I/O within one transaction

            std::unique_ptr<instrument_map> im_prev { };

            // TODO AND this loop with ASX calendar to avoid spurious locate download alarms

            for (auto d = std::get<0> (dates); d <= std::get<1> (dates); d += gd::days { 1 })
            {
                std::unique_ptr<instrument_map> im { std::make_unique<instrument_map> () };

                if (! process_date (root_dir, d, tz, * im))
                    continue; // skip weekends/dates with no data

                // join locate symbols to instrument iids:

                for (auto & kv : * im)
                {
                    optional<instrument> const i = instruments.find_as_of (d, kv.second.symbol ());
                    if (VR_UNLIKELY (! i))
                        throw_x (illegal_state, '[' + gd::to_iso_string(d) + "]: no ref data for symbol " + print (kv.second));

                    kv.second.iid () = i->iid ();

                    LOG_trace2 << "completed locate intance: " << kv.second;
                }

                // persist (and close) 'im':

                for (auto const & kv : * im)
                {
                    bool rc = locates.persist_as_of (d, kv.second);
                    check_condition (rc, d, kv.second); // locates always close current versions (see next comment)

                    rc = locates.delete_as_of (d + gd::days { 1 }, kv.second.symbol ());
                    check_condition (rc, d, kv.second); // locates always valid for a single date
                }

                LOG_info << '[' << d << "]: added " << im->size () << " locate(s)";

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
op_make_loc (string_vector const & av)
{
    std::string cfg_name { };
    fs::path in_root { };
    fs::path out_file { };
    std::string tz { "Australia/Sydney" };
    std::string date_range_str { };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " make-loc [options] file" };
    opts.add_options ()
        ("config,c",        bpopt::value (& cfg_name)->value_name ("FILE|NAME"), "configuration file/ID")
        ("in,i",            bpopt::value (& in_root)->value_name ("DIR")->required (), "input dir")
        ("out,o",           bpopt::value (& out_file)->value_name ("FILE")->required (), "output db file")
        ("date_range,d",    bpopt::value (& date_range_str)->value_name ("DATE-DATE")->required (), "locate date range (inclusive)")
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

        io::uri const cfg_url = rt::resolve_as_uri (cfg_name);
        vr::json cfg { };
        {
            std::unique_ptr<std::istream> const in = io::stream_factory::open_input (cfg_url);
            (* in) >> cfg;
        }

        cfg ["sql"]["read.ref"]["db"] = out_file.native (); // HACK
        cfg ["sql"]["make"]["db"] = out_file.native (); // HACK

        std::tuple<util::date_t, util::date_t> const dates = util::parse_date_range (date_range_str);

        run (cfg, in_root, dates, tz);
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
