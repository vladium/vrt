
#include "vr/io/cap/cap_reader.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/io/streams.h"
#include "vr/market/books/asx/market_data_listener.h"
#include "vr/market/books/asx/market_data_view.h"
#include "vr/market/books/book_event_context.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/sources/asx/itch/ITCH_filters.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/rt/cfg/resources.h"
#include "vr/util/di/container.h"

#include "vr/test/configure.h"
#include "vr/test/files.h"
#include "vr/test/utility.h"

#include <boost/preprocessor/logical/or.hpp>

//----------------------------------------------------------------------------
namespace vr
{
using namespace io;
using namespace io::net;

namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace
{

template<typename LIMIT_ORDER_BOOK, typename CTX>
class book_checker: public ITCH_visitor<book_checker<LIMIT_ORDER_BOOK, CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<book_checker<LIMIT_ORDER_BOOK, CTX> >;

        vr_static_assert (has_field<_book_, CTX> ()); // how the book reference is communicated

    public: // ...............................................................

        book_checker ()     = default;

        book_checker (arg_map const & args) :
            book_checker { }
        {
        }

        // overridden ITCH visits:

        using super::visit;

        // run the books internal consistency checks following any visit that may have modified them:

        VR_ASSUME_HOT void visit (post_message const msg_type, CTX & ctx) // override
        {
            super::visit (msg_type, ctx); // [chain]

            LIMIT_ORDER_BOOK const * const book = static_cast<LIMIT_ORDER_BOOK const *> (field<_book_> (ctx));
            if (book) book->check ();
        }

}; // end of class
//............................................................................

string_vector
reservoir_sample (string_vector const data, int32_t const size, uint32_t rnd)
{
    assert_positive (size);

    string_vector r { };

    int32_t const di_limit = data.size ();

    int32_t i = 0;
    int32_t di = 0;

    for ( ; (i < size) && (di < di_limit); ++ i, ++ di)
    {
        r.push_back (data [di]);
    }

    for ( ; (di < di_limit); ++ i, ++ di)
    {
        int32_t const j = (test::next_random (rnd) % i);
        if (j < size)
            r [j] = data [di];
    }

    return r;
}

} // end of anonymous
//............................................................................
//............................................................................

using scenarios     = gt::Types
<
    util::int32_<1>, // tests ASX-55 fix
    util::int32_<VR_IF_THEN_ELSE (BOOST_PP_OR (VR_FULL_TESTS, VR_RELEASE))(100, 10)> // very time-consuming when many books are checked on every event
>;

template<typename T> struct ASX_market_data_view_test: public gt::Test { };
TYPED_TEST_CASE (ASX_market_data_view_test, scenarios);

/*
 * run captured data through a pipeline with
 *  - instrument selector
 *  - book listener as supplied by a 'market_data_view'
 *  - view/book verifier
 */
TYPED_TEST (ASX_market_data_view_test, capture_replay)
{
    using scenario      = TypeParam; // test parameter

    constexpr int32_t view_size_limit   = scenario::value;

    uint32_t rnd { test::env::random_seed<uint32_t> () };

    fs::path const test_input = test::find_capture (source::ASX, "<"_rop, util::current_date_in ("Australia/Sydney"));
    LOG_info << "using test data in " << print (test_input);

    string_vector const universe = io::read_json (rt::resolve_as_uri ("asx/symbols.asx300.json"));
    string_vector const symbols = reservoir_sample (universe, view_size_limit, rnd);

    util::di::container app { join_as_name ("APP", test::current_test_name ()) };
    {
        test::configure_app_ref_data (app, symbols);
    }

    app.start ();
    {
        ref_data const & rd = app ["ref_data"];
        agent_cfg const & ac = app ["agents"];

        int32_t const view_size     = symbols.size ();
        int64_t const record_limit  = ((VR_FULL_TESTS || (view_size == 1)) ? std::numeric_limits<int64_t>::max () : 5000000);

        LOG_info << "creating view with " << view_size << " instrument(s), record limit: " << record_limit;
        LOG_info << "test symbol(s): " << print (symbols);


        using book_type         = limit_order_book<price_si_t, oid_t, level<_qty_, _order_count_>>;
        using view              = market_data_view<book_type>;

        view mdv // note that the same book instances owned by 'mdv' live through both glimpse and mcast eval runs
        {
            {
                { "ref_data",   std::cref (rd) },
                { "agents",     std::cref (ac) }
            }
        };

        mdv.check (); // initial consistency after construction

        using reader            = cap_reader;

        // start with glimpse snapshot state:
        {
            using visit_ctx         = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_>;

            using selector          = view::instrument_selector<visit_ctx>;
            using listener          = market_data_listener<this_source (), book_type, visit_ctx>;
            using checker           = book_checker<book_type, visit_ctx>;

            using pipeline          = ITCH_pipeline
                                    <
                                        selector,
                                        listener,
                                        checker
                                    >;

            using visitor           = Soup_frame_<io::mode::recv, pipeline>;

            for (int32_t pix = 0; pix < partition_count (); ++ pix) // consume all glimpse data
            {
                visitor v
                {
                    {
                        { "view", std::cref (mdv) },
                    }
                };

                visit_ctx ctx { };

                std::string const filename = "glimpse.recv.203.0.119.213_2180" + string_cast (pix + 1) + ".soup";

                std::unique_ptr<std::istream> const in = stream_factory::open_input (test_input / filename);

                reader r { * in, cap_format::wire };

                r.evaluate (ctx, v);
            }
        }

        mdv.check ();

        // continue by consuming mcast data:
        {
            using visit_ctx         = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_, _ts_local_, _ts_local_delta_, _seqnum_,  _dst_port_>;

            using selector          = view::instrument_selector<visit_ctx>;
            using listener          = market_data_listener<this_source (), book_type, visit_ctx>;
            using checker           = book_checker<book_type, visit_ctx>;

            using pipeline          = ITCH_pipeline
                                    <
                                        selector,
                                        listener,
                                        checker
                                    >;

            using visitor           = pcap_<IP_<UDP_<Mold_frame_<pipeline>>>>;

            visitor v
            {
                {
                    { "view", std::cref (mdv) },
                }
            };

            visit_ctx ctx { };

            std::unique_ptr<std::istream> const in = stream_factory::open_input (test_input / "mcast.recv.p1p2.pcap.zst");

            reader r { * in, cap_format::pcap };

            r.evaluate (ctx, v, record_limit);
        }

        mdv.check ();
    }
    app.stop ();
}

} // end of 'ASX
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
