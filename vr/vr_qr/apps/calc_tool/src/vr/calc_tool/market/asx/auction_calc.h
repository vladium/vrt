#pragma once

#include "vr/calc_tool/market/asx/schema.h"
#include "vr/calc_tool/market/asx/stats_calc.h"

#include "vr/containers/util/small_vector.h"
#include "vr/data/dataframe.h"
#include "vr/data/NA.h"
#include "vr/io/files.h"
#include "vr/io/frame_streams.h"
#include "vr/io/stream_traits.h"
#include "vr/io/streams.h"
#include "vr/market/books/defs.h" // _book_
#include "vr/market/books/limit_order_book_io.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/meta/arrays.h"

#include <queue>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

static constexpr io::format::enum_t this_format     = io::format::CSV;

using price_output_t        = io::stream_traits<enum_<io::format, this_format>>::price_type;

using book_side_snapshot    = std::array<std::tuple<price_output_t, qty_t>, auction_calc_depth ()>;
using book_depth_snapshot   = std::array<book_side_snapshot, side::size>;

inline void
clear (book_depth_snapshot & bs)
{
    for (side::enum_t s : side::values ())
    {
        for (auto & lvl : bs [s])
        {
            data::mark_NA (std::get<0> (lvl));
            data::mark_NA (std::get<1> (lvl));
        }
    }
}

struct auction_metadata_base: public stats_metadata_base
{
    using update_vector     = util::small_vector<itch::auction_update const *, 2>;

    ~auction_metadata_base ()
    {
        if (m_out)
        {
            try
            {
                if (m_discard || (m_update_count_PRE_OPEN == 0)) // TODO count threshold
                {
                    assert_condition (! m_out_file.empty ());
                    m_out.reset ();

                    LOG_warn << "discarding " << print (m_out_file) << " ...";
                    fs::remove (m_out_file);
                }
                else
                {
                    if (m_buf.row_count ()) // flush
                        m_out->write (m_buf);
                }
            }
            catch (std::exception const & e)
            {
                LOG_error << "failure on destruction: " << exc_info (e);
            }
        }
    }

    void open (fs::path const & out_file)
    {
        check_condition (! m_out); // called once

        LOG_trace1 << "creating " << print (out_file) << " ...";

        io::create_dirs (out_file.parent_path ()); // TODO don't clobber in "prod" build
        m_out = io::frame_ostream::create (out_file, { { "clobber", io::clobber::trunc }, { "digits", 4 }, { "digits.secs", 6 } }, & m_out_file);
    }

    io::frame_ostream & out ()
    {
        assert_condition (!! m_out); // has been opened
        return (* m_out);
    }

    update_vector m_updates { };
    timestamp_t m_ts_OPEN { }; // timestamp of transition to 'OPEN' is partition-specific
    std::vector<price_si_t> m_VWAP { };
    std::unique_ptr<io::frame_ostream> m_out { };
    data::dataframe m_buf { 256, auction_calc_attributes (this_format) }; // overkill for 'CSV' but that may change
    std::string m_symbol { };
    fs::path m_out_file { }; // what's passed into 'open()' modified with compression extensions, etc
    int32_t m_update_count_PRE_OPEN { };
    bool m_discard { false };

}; // end of class

using auction_metadata      = meta::make_compact_struct_t<meta::make_schema_t
                                <
                                    meta::fdef_<itch::book_state::enum_t,   _state_>
                                >,
                                auction_metadata_base>;

//............................................................................
/**
 */
template<typename MARKET_DATA_VIEW, typename CTX>
class auction_calc: public ITCH_visitor<auction_calc<MARKET_DATA_VIEW, CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<auction_calc<MARKET_DATA_VIEW, CTX> >;

        vr_static_assert (has_field<_book_, CTX> ());

        static constexpr bool have_packet_index ()  { return has_field<io::net::_packet_index_, CTX> (); }
        static constexpr bool have_partition ()     { return has_field<_partition_, CTX> (); }
        static constexpr bool have_ts_local ()      { return has_field<_ts_local_, CTX> (); }

        using book_type         = typename MARKET_DATA_VIEW::book_type;

        using book_price_type   = typename book_type::price_type;
        using side_iterator     = typename book_type::side_iterator;
        using wire_price_type   = typename this_source_traits::price_type;
        using price_traits      = typename this_source_traits::price_traits<book_price_type>;

    public: // ...............................................................

        auction_calc (arg_map const & args) :
            m_date { args.get<util::date_t> ("date") },
            m_tz { args.get<std::string> ("tz") },
            m_tz_offset { util::tz_offset (m_date, m_tz) },
            m_mdv { args.get<MARKET_DATA_VIEW &> ("view") }
        {
            // we could open out contexts lazily, but for now do it eagerly
            // and save a check in event visits:

            ref_data const & rd = args.get<ref_data &> ("ref_data"); // required [but could also store a ref with the view]
            fs::path const & out_dir = args.get<fs::path &> ("out_dir"); // required

            for (auto const & e : m_mdv)
            {
                iid_t const & iid = field<_key_> (e);
                auction_metadata & md = field<_value_> (e)->user_data ();

                instrument const & i = rd [iid];

                md.m_symbol = i.symbol ();
                md.open (out_dir / join_as_name (i.symbol (), "csv"));
            }

            // set up VWAP snapshot schedule and output stream:

            std::vector<int32_t> const & unwind = args.get<std::vector<int32_t> &> ("unwind"); // [minutes]
            {
                for (auto const v : unwind)
                {
                    m_unwind_schedule.push_back (v * 60 * _1_second ());
                    check_positive (m_unwind_schedule.back ());
                }
                check_nonempty (m_unwind_schedule);

                std::sort (m_unwind_schedule.begin (), m_unwind_schedule.end ());

                // guard against duplicates:

                timestamp_t v_prev { std::numeric_limits<timestamp_t>::min () };
                for (auto const v : m_unwind_schedule)
                {
                    check_gt (v, v_prev);
                    v_prev = v;
                }

                m_summary_out.reset (new io::fd_ostream<> { out_dir / "summary.csv" });
            }
        }

        ~auction_calc ()
        {
            LOG_info << "instruments(s) with detected hidden qty: " << print (m_iids_with_hidden);

            if (! m_schedule_queue.empty ()) LOG_warn << "*** " << m_schedule_queue.size () << " future event(s) left in the queue ***";

            emit_summary ();
        }

        // message visits:

        using super::visit;


        VR_ASSUME_HOT bool visit (itch::order_book_state const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            book_type const & book = current_book (ctx);

            switch (field<_state_> (book.user_data ()))
            {
                case itch::book_state::OPEN:
                {
                    auto & md = book.user_data ();

                    if (VR_LIKELY (! md.m_ts_OPEN)) // guard against re-opens after halts, etc
                    {
                        assert_zero (md.m_ts_OPEN, md.m_symbol);

                        md.m_ts_OPEN = field<_ts_origin_> (ctx); // TODO ? _ts_local_ makes more sense, if available
                        LOG_info << print (md.m_symbol) << " book on OPEN (@ " << print_timestamp (md.m_ts_OPEN, m_tz_offset) << ')' << book;

                        // schedule VWAP reads:

                        for (timestamp_t const horizon : m_unwind_schedule)
                        {
                            m_schedule_queue.emplace (md.m_ts_OPEN + horizon, md);
                        }
                        LOG_trace1 << "schedule queue size: " << m_schedule_queue.size ();
                    }

                    // emit a row for the book state immediately after all auction executions have been sent out:

                    emit (ctx, book);
                }
                break;

                default: break;

            } // end of switch

            return rc;
        }

        VR_ASSUME_HOT bool visit (itch::auction_update const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            book_type const & book = current_book (ctx);
            auction_metadata & md = book.user_data ();

            LOG_trace2 << msg;

            switch (field<_state_> (md))
            {
                case itch::book_state::PRE_OPEN:
                {
                    // a deep copy is not necessary in our msg delivery model; store a pointer, it is valid/stable
                    // for the entire packet (until after 'post_packet'):

                    md.m_updates.push_back (& msg);
                    ++ md.m_update_count_PRE_OPEN;

                    m_updated_books.insert (& book);
                }
                break;


                case itch::book_state::PRE_CSPA:
                {
                    // TODO
                }
                break;


                case itch::book_state::TRADING_HALT:
                case itch::book_state::SUSPEND:
                {
                    LOG_trace1 << prefix (ctx) << print (md.m_symbol) << " auction update in " << field<_state_> (md) << ": " << msg;
                    md.m_discard = true;
                }
                break;


                default: break; // TODO

            } // end of switch

            return rc;
        }

        // TODO stop calc when all instruments of interest have had their OPENs

        VR_ASSUME_HOT void visit (post_packet const msg_count, CTX & ctx)
        {
            process_schedule_queue (ctx); // piggy back on each packet tick to advance event queue (TODO this ignores clock disparities across partitions)

            if (m_updated_books.empty ())
                return;

            for (book_type const * book : m_updated_books)
            {
                auction_metadata & md = book->user_data ();
                auto & msg_trace = md.m_updates;

                LOG_trace2 << "auction updates in packet: " << msg_trace.size ();

                // eval auction for the last update only, but keep the list of oids in the packet:

                itch::auction_update const & msg = * msg_trace.back ();

                {
                    price_si_t const px_auction = price_traits::wire_to_book (msg.price ());
                    bool const have_px_auction = ! data::is_NA (px_auction);

                    // note: just before OPEN, ASX can send some 'auction_update's with NA px_auction and
                    //       some (but not all) auction crosses performed
                    // note: just before transition to OPEN there will be an 'order_add' with the surplus qty (and queue rank 1)

                    LOG_trace1 << prefix (ctx) << print (md.m_symbol) << " EQ PRICE " << maybe_NA (price_book_to_print (px_auction)) << "\n*** AUCTION_UPDATE " << msg << (* book);

                    if (have_px_auction) // guard necessary because the auction may have failed to cross
                    {
                        auto const & state = field<_state_> (md);
                        check_eq (state, itch::book_state::PRE_OPEN, md.m_symbol);
                    }

                    // actual calc:

                    std::array<qty_t, side::size> qty_avail { { static_cast<qty_t> (msg.qty ()[side::BID]), static_cast<qty_t> (msg.qty ()[side::ASK]) } };
                    std::array<qty_t, side::size> qty_hidden { { 0, 0 } };

                    qty_t qty_match { data::NA<qty_t> () };
                    qty_t qty_surplus { data::NA<qty_t> () };

                    if (have_px_auction)
                    {
                        qty_match = std::min (qty_avail [side::BID], qty_avail [side::ASK]); // validity of these msg fields asserted below
                        qty_surplus = (qty_avail [side::BID] - qty_avail [side::ASK]); // validity of these msg fields asserted below
                    }

                    // pass 1: detect any hidden qty:

                    if (have_px_auction)
                    {
                        for (side::enum_t s : side::values ())
                        {
                            assert_positive (qty_avail [s], s); // note: covers NA case as well

                            auto const & b_side = book->at (s);

                            qty_t q_visible { };

                            for (side_iterator i = b_side.begin (), i_limit = b_side.find_next_worse_price (px_auction); i != i_limit; ++ i)
                            {
                                q_visible += i->qty ();
                            }

                            qty_hidden [s] = (qty_avail [s] - q_visible);

                        } // visited both sides
                    }

                    bool const has_hidden = (qty_hidden [side::BID] > 0) | (qty_hidden [side::ASK] > 0);

                    if (has_hidden) // maintain a set of symbols with hidden qty (or something equivalent) observed
                    {
                        m_iids_with_hidden.emplace (msg.iid (), md.m_symbol).second;
                        LOG_trace1 << prefix (ctx) << print (md.m_symbol) << " (" << msg.iid () << ") HIDDEN " << print (qty_hidden) << " (msg qty " << print (qty_avail) << ')' << (* book) ;
                    }

                    std::array<qty_t, side::size> qty_at_auction { { 0, 0 } }; // TODO NA ?
                    book_depth_snapshot indicated_book { }; clear (indicated_book);

                    // pass 2: populate 'qty_at_auction' and 'indicated_book':

                    if (have_px_auction)
                    {
                        for (side::enum_t s : side::values ())
                        {
                            auto const & b_side = book->at (s);
                            book_side_snapshot & ib_side = indicated_book [s];

                            int32_t i_depth { };
                            for (side_iterator i = b_side.find_eq_or_next_worse_price (px_auction); (i_depth < auction_calc_depth ()) && (i != b_side.end ()); ++ i)
                            {
                                qty_t qty_lvl = i->qty ();

                                bool const at_auction = (i->price () == px_auction);

                                if (at_auction)
                                {
                                    qty_at_auction [s] = qty_lvl;

                                    if (! i_depth)
                                    {
                                        qty_lvl = (qty_avail [s] - qty_match);
                                        assert_condition ((qty_lvl == 0) | (qty_lvl == std::abs (qty_surplus)), qty_lvl, px_auction, qty_avail, i->price (), i->qty (), md.m_symbol, (* book));

                                        if (! qty_lvl)
                                            continue; // don't increment 'i_depth'
                                        // else fall through
                                    }
                                }

                                auto & ib_lvl = ib_side [i_depth];

                                std::get<0> (ib_lvl) = price_traits::book_to_print (i->price ());
                                std::get<1> (ib_lvl) = qty_lvl;

                                ++ i_depth;
                            }

                        } // visited both sides

                        VR_IF_DEBUG // by definition the indicated book cannot be crossed:
                        (
                            price_output_t const px_i [] { std::get<0> (indicated_book [side::BID][0]), std::get<0> (indicated_book [side::ASK][0]) };

                            if (! (data::is_NA (px_i [side::BID]) | data::is_NA (px_i [side::ASK])))
                                check_lt (px_i [side::BID], px_i [side::ASK]);
                        )
                    }

                    // emit row:

                    emit (ctx, * book, px_auction, qty_at_auction, qty_match, qty_surplus, qty_avail, qty_hidden, indicated_book);

                } // end of eval

                msg_trace.clear ();
            }

            m_updated_books.clear (); // clear the set for the next packet
        }

    private: // ..............................................................

        struct schedule_event final
        {
            schedule_event (timestamp_t const ts, auction_metadata_base & md) :
                m_ts { ts },
                m_md { & md }
            {
            }

            timestamp_t m_ts;
            auction_metadata_base * m_md;

            friend VR_FORCEINLINE bool operator< (schedule_event const & lhs, schedule_event const & rhs)
            {
                return (rhs.m_ts < lhs.m_ts); // queue ordered by increasing timestamps
            }

        }; // end of nested class

        using book_set          = boost::unordered_set<book_type const *>; // TODO faster structure (use lids or ASX-31)
        using schedule_queue    = std::priority_queue<schedule_event>;


        VR_ASSUME_HOT void process_schedule_queue (CTX const & ctx)
        {
            auto const ts_origin = field<_ts_origin_> (ctx);

            bool dequeued { false };
            while (! m_schedule_queue.empty () && (m_schedule_queue.top ().m_ts <= ts_origin))
            {
                DLOG_trace2 << "[" << print_timestamp (ts_origin, m_tz_offset) << "] dequeued " << print_timestamp (m_schedule_queue.top ().m_ts, m_tz_offset);

                auction_metadata_base & md = * m_schedule_queue.top ().m_md;
                m_schedule_queue.pop (); dequeued = true;

                assert_lt (md.m_VWAP.size (), m_unwind_schedule.size ());
                md.m_VWAP.push_back (md.VWAP ());
            }

            if (dequeued) LOG_trace1 << "schedule queue size: " << m_schedule_queue.size ();
        }


        void emit (CTX & ctx, book_type const & book,
                   price_si_t const px_auction, std::array<qty_t, side::size> const & qty_at_auction, qty_t const qty_match, qty_t const qty_surplus,
                   std::array<qty_t, side::size> const & qty_avail, std::array<qty_t, side::size> const & qty_hidden,
                   book_depth_snapshot const & indicated_book)
        {
            using namespace data;

            auction_metadata & md = book.user_data ();

            io::frame_ostream & out = md.out ();
            dataframe & buf = md.m_buf;

            if (VR_UNLIKELY (buf.row_count () == buf.row_capacity ())) // flush
            {
                out.write (buf);
                buf.resize_row_count (0);
            }

            auto const & ts_origin      = field<_ts_origin_> (ctx);
            auto const & ts_local       = field<_ts_local_> (ctx);

            auto const & book_state     = field<_state_> (md);

            switch (this_format)
            {
                case io::format::CSV:
                {
                    buf.add_row
                    (
                        ts_origin + m_tz_offset,
                        ts_local + m_tz_offset,
                        book_state,
                        price_traits::book_to_print (px_auction),
                        qty_match,
                        qty_surplus,
                        qty_at_auction,
                        qty_avail,
                        qty_hidden,
                        indicated_book
                    );
                }
                break;

                default: throw_x (illegal_state, "not implemented yet");

            } // end of switch
        }

        void emit (CTX & ctx, book_type const & book) // overload for normal trading (outside of an auction)
        {
            std::array<qty_t, side::size> const zero_qties { { 0, 0 } };
            std::array<qty_t, side::size> const NA_qties { { data::NA<qty_t> (), data::NA<qty_t> () } };

            book_depth_snapshot open_book { }; clear (open_book);

            // populate 'open_book':

            for (side::enum_t s : side::values ())
            {
                auto const & b_side = book.at (s);
                book_side_snapshot & ob_side = open_book [s];

                int32_t i_depth { };
                for (side_iterator i = b_side.begin (); (i_depth < auction_calc_depth ()) && (i != b_side.end ()); ++ i, ++ i_depth)
                {
                    auto & ob_lvl = ob_side [i_depth];

                    std::get<0> (ob_lvl) = price_traits::book_to_print (i->price ());
                    std::get<1> (ob_lvl) = i->qty ();
                }
            }

            emit (ctx, book, data::NA<price_si_t> (), zero_qties, data::NA<qty_t> (), data::NA<qty_t> (), NA_qties, NA_qties, open_book);
        }

        void emit_summary () VR_NOEXCEPT
        {
            if (m_summary_out)
            {
                try
                {
                    std::ostream & os = * m_summary_out;

                    // header:
                    {
                        os << "iid,symbol";
                        for (timestamp_t const horizon_ns : m_unwind_schedule)
                        {
                            int32_t const horizon = horizon_ns / (60 * _1_second ());
                            os << "," << "px_vwap_" << horizon;
                        }
                        os << std::endl;
                    }

                    // data rows:

                    for (auto const & e : m_mdv)
                    {
                        iid_t const & iid = field<_key_> (e);
                        auto const & md = field<_value_> (e)->user_data ();

                        if (! (md.m_discard || (md.m_update_count_PRE_OPEN == 0)))
                        {
                            os << iid << ',' << print (md.m_symbol);
                            {
                                int32_t i = 0;
                                for (int32_t const i_limit = md.m_VWAP.size (); i < i_limit; ++ i)
                                {
                                    os << ',' << maybe_NA (price_book_to_print (md.m_VWAP [i]));
                                }
                                for (int32_t const i_limit = m_unwind_schedule.size (); i < i_limit; ++ i)
                                {
                                    os << ',' << maybe_NA (price_book_to_print (data::NA<price_si_t> ()));
                                }
                            }
                            os << std::endl;
                        }
                    }

                    m_summary_out.reset ();
                    LOG_info << "[VWAP summary written]";
                }
                catch (std::exception const & e)
                {
                    LOG_error << "failure writing summary: " << exc_info (e);
                }
            }
        }

        VR_ASSUME_HOT void prefix (CTX & ctx, std::ostream & out) // TODO factor out in a utility fn
        {
            out << '[';

            if (have_packet_index ()) // compile-time branch
            {
                int32_t const pkt       = field<io::net::_packet_index_> (ctx);
                out << pkt << ", ";
            }

            if (have_partition ()) // compile-time branch
            {
                int32_t const pix       = field<_partition_> (ctx);
                out << pix << ", ";
            }

            auto const ts_origin    = field<_ts_origin_> (ctx);

            out << print_timestamp (ts_origin, m_tz_offset);

            if (have_ts_local ()) // compile-time branch
            {
                auto const ts_local = field<_ts_local_> (ctx);
                auto const diff = (ts_local - ts_origin);

                out << ", " << print_timestamp (ts_local, m_tz_offset) << " (" << (diff >= 0 ? '+' : '-') << (ts_local - ts_origin) << ')';
            }

            out << "]: ";
        }

        VR_ASSUME_HOT std::string prefix (CTX & ctx) // TODO memoize
        {
            std::stringstream out { };
            prefix (ctx, out);

            return out.str ();
        }

        static VR_FORCEINLINE book_type const & current_book (CTX & ctx)
        {
            addr_const_t const book_ref = field<_book_> (ctx);
            assert_nonnull (book_ref); // relying on a selector ahead of us in the pipeline

            return (* static_cast<book_type const *> (book_ref));
        }



        util::date_t const m_date;
        std::string const m_tz;
        timestamp_t const m_tz_offset;
        MARKET_DATA_VIEW const & m_mdv;
        std::vector<timestamp_t> m_unwind_schedule { }; // relative to (partition-specific) 'OPEN' start
        schedule_queue m_schedule_queue { };
        book_set m_updated_books { };
        boost::unordered_map<iid_t, std::string> m_iids_with_hidden { };
        std::unique_ptr<std::ostream> m_summary_out { };

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
