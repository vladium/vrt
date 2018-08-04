#pragma once

#include "vr/calc_tool/market/asx/schema.h"
#include "vr/calc_tool/market/asx/stats_calc.h"

#include "vr/containers/util/small_vector.h"
#include "vr/data/dataframe.h"
#include "vr/data/NA.h"
#include "vr/io/files.h"
#include "vr/io/frame_streams.h"
#include "vr/market/books/defs.h" // _book_
#include "vr/market/books/limit_order_book_io.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/settings.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

struct sim_metadata_base: public stats_metadata_base
{
    using update_vector     = util::small_vector<itch::auction_update const *, 2>;


    update_vector m_updates { };
    timestamp_t m_ts_OPEN { };
    std::vector<int64_t> m_pnl { }; // indexed by unwind
    price_si_t m_px_entry { };
    qty_t m_qty_entry { }; // sign indicates long/short
    int32_t m_next_unwind { }; // indexes into 'm_unwind_schedule'

    bool m_discard { true };

}; // end of class

using sim_metadata          = meta::make_compact_struct_t<meta::make_schema_t
                                <
                                    meta::fdef_<itch::book_state::enum_t,   _state_>
                                >,
                                sim_metadata_base>;

//............................................................................
/**
 */
template<typename MARKET_DATA_VIEW, typename CTX>
class sim_calc: public ITCH_visitor<sim_calc<MARKET_DATA_VIEW, CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<sim_calc<MARKET_DATA_VIEW, CTX> >;

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

        sim_calc (arg_map const & args) :
            m_date { args.get<util::date_t> ("date") },
            m_tz { args.get<std::string> ("tz") },
            m_tz_offset { util::tz_offset (m_date, m_tz) },
            m_mdv { args.get<MARKET_DATA_VIEW &> ("view") }
        {
            settings const & unwind = args.get<settings &> ("unwind");
            ref_data const & rd = args.get<ref_data &> ("ref_data"); // required [but could also store a ref with the view]

            {
                for (auto const & v : unwind)
                {
                    m_unwind_schedule.push_back (static_cast<int32_t> (v) * _1_second ());
                    check_positive (m_unwind_schedule.back ());
                }
                check_nonempty (m_unwind_schedule); // TODO also guard against duplicates

                std::sort (m_unwind_schedule.begin (), m_unwind_schedule.end ());
            }
            {
                int32_t sim_count { };

                for (auto const & e : m_mdv)
                {
                    iid_t const & iid = field<_key_> (e);
                    auto & md = field<_value_> (e)->user_data ();

                    instrument const & i = rd [iid];

                    md.m_symbol = i.symbol ();
                    md.m_pnl.resize (m_unwind_schedule.size ());

                    ++ sim_count;
                }

                m_sim_count = sim_count;
                check_positive (m_sim_count);
            }

            LOG_trace1 << "simulating " << m_sim_count << " instrument(s) with unwind schedule " << print (m_unwind_schedule);
        }


        void emit (std::ostream & os, int32_t const sim_day)
        {
//            os << "sim_day,date,window,PL" << std::endl;

            std::vector<int64_t> pnl_total (m_unwind_schedule.size ());

            for (auto const & e : m_mdv)
            {
                auto & md = field<_value_> (e)->user_data ();

                if (! md.m_discard)
                {
                    check_eq (md.m_pnl.size (), m_unwind_schedule.size ());

                    for (int32_t u = 0, u_limit = m_unwind_schedule.size (); u != u_limit; ++ u)
                    {
                        pnl_total [u] += md.m_pnl [u];
                    }
                }
            }

            for (int32_t u = 0, u_limit = m_unwind_schedule.size (); u != u_limit; ++ u)
            {
                os << sim_day << ',' << gd::to_iso_string (m_date) << ',' << u
                   << ',' << static_cast<int64_t> (price_book_to_print (pnl_total [u]))
                   << std::endl;
            }
        }


        // ------------------------ pre-visits: ------------------------

        using super::visit;

        VR_ASSUME_HOT bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg_type, msg, ctx);

            addr_const_t const book_ref = field<_book_> (ctx);
            if (! book_ref) // check needed since the callback is not iid-specific
                return false;


            if (VR_UNLIKELY (m_sim_count <= 0)) // entire sim is done
                throw int32_t { 0 };

            // OPEN logic:
            {
                book_type const & book = (* static_cast<book_type const *> (book_ref));
                auto & md = book.user_data ();

                if (md.m_ts_OPEN && ! md.m_discard) // this instrument is being sim-traded (first check is equivalent to 'OPEN' state check)
                {
                    assert_eq (field<_state_> (md), itch::book_state::OPEN);

                    int32_t next_unwind = md.m_next_unwind;
                    int32_t const unwind_limit = m_unwind_schedule.size ();

                    if (next_unwind < unwind_limit)
                    {
                        // [still simulating this instrument's unwinds]

                        auto const ts_origin = field<_ts_origin_> (ctx);

                        if (ts_origin >= md.m_ts_OPEN + m_unwind_schedule [next_unwind]) // TODO more accurate would be to do a loop
                        {
                            price_si_t const px_exit = md.VWAP ();
                            if (VR_LIKELY (! data::is_NA (px_exit)))
                            {
                                int64_t const pnl = md.m_qty_entry * (px_exit - md.m_px_entry);
                                md.m_pnl [next_unwind] = pnl;

                                LOG_trace1 << "  [" << next_unwind << "] " << print_timestamp (ts_origin, m_tz_offset) << ": " << print (md.m_symbol) << " exit at VWAP " << price_book_to_print (px_exit) << ", P/L " << price_book_to_print (pnl);

                                md.m_next_unwind = ++ next_unwind;
                                m_sim_count -= (next_unwind == unwind_limit);
                            }
                        }
                    }
                }
            }

            return rc;
        }

        // ------------------------ message visits: ------------------------

        bool visit (itch::order_book_state const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            book_type const & book = current_book (ctx);
            auto & md = book.user_data ();

            switch (field<_state_> (md))
            {
                case itch::book_state::OPEN:
                {
                    if (md.m_qty_entry)
                    {
                        // capture start of OPEN TODO actually more accurate to use end-of-auction update and/or order_fills/trades

                        md.m_ts_OPEN = field<_ts_origin_> (ctx); // TODO _ts_local_ makes more sense, if available

                        LOG_trace1 << print (md.m_symbol) << " -> OPEN @ " << print_timestamp (md.m_ts_OPEN, m_tz_offset) << ", entry " << price_book_to_print (md.m_px_entry) << '/' << md.m_qty_entry << book;
                    }
                    else // not putting on a trade
                    {
                        -- m_sim_count;
                        md.m_discard = true;
                    }
                }
                break;


                case itch::book_state::TRADING_HALT:
                case itch::book_state::SUSPEND:
                case itch::book_state::PRE_NR: // TODO ???
                {
                    -- m_sim_count;
                    md.m_discard = true;
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
            auto & md = book.user_data ();

            LOG_trace2 << msg;

            switch (field<_state_> (md))
            {
                case itch::book_state::PRE_OPEN:
                {
                    md.m_updates.push_back (& msg);
                    m_auction_updated_books.insert (& book);
                }
                break;

                case itch::book_state::TRADING_HALT:
                case itch::book_state::SUSPEND:
                {
                    LOG_trace1 << print (md.m_symbol) << " auction update in " << field<_state_> (md) << ": " << msg;
                    md.m_discard = true;
                }
                break;


                default: break; // TODO

            } // end of switch

            return rc;
        }

        // ------------------------ post-visits: ------------------------

        VR_ASSUME_HOT void visit (post_packet const msg_count, CTX & ctx)
        {
            // PRE_OPEN logic:
            {
                for (book_type const * book : m_auction_updated_books)
                {
                    auto & md = book->user_data ();

                    auto const & state = field<_state_> (md);
                    if (VR_UNLIKELY (state == itch::book_state::OPEN)) // TODO handle if 'OPEN' transition happened within the same packet as some 'auction_update's
                    {
                        LOG_warn << print (md.m_symbol) << "already opened";
                        continue;
                    }

                    auto & msg_trace = md.m_updates;

                    itch::auction_update const & msg = * msg_trace.back (); // eval for the last update only
                    msg_trace.clear ();

                    {
                        price_si_t const px_auction = price_traits::wire_to_book (msg.price ());
                        bool const have_px_auction = ! data::is_NA (px_auction);

                        if (! have_px_auction) // guard necessary because the auction may have failed to cross
                            continue;

                        md.m_discard = false; // have seen at least one 'auction_update'

                        // track sim trade:

                        md.m_px_entry = px_auction;

                        qty_t const qty_surplus = (msg.qty ()[side::BID] - msg.qty ()[side::ASK]);
                        if (VR_UNLIKELY (! qty_surplus))
                            md.m_qty_entry = { };
                        else
                        {
                            side::enum_t const entry_side = (qty_surplus > 0 ? side::BID : side::ASK); // TODO operator/friend for this in 'market::side'

                            auto const * const entry_lvl = book->at (entry_side).eq_price (px_auction);
                            qty_t const entry_side_qty_at = (entry_lvl ? field<_qty_> (* entry_lvl) : 0);

                            qty_t const trade_size = (msg.qty ()[~ entry_side] - entry_side_qty_at); // abs value
                            if (trade_size <= 0)
                                md.m_qty_entry = { };
                            else
                                md.m_qty_entry = sign (entry_side) * trade_size; // TODO sans imul
                        }

                        LOG_trace2 << print (md.m_symbol) << " entry " << price_book_to_print (md.m_px_entry) << '/' << md.m_qty_entry;
                    }
                }

                m_auction_updated_books.clear (); // clear the set for the next packet
            }
        }

    private: // ..............................................................

        using book_set          = boost::unordered_set<book_type const *>; // TODO faster structure (use lids or ASX-31)


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
        book_set m_auction_updated_books { };
        std::vector<timestamp_t> m_unwind_schedule { }; // relative to 'OPEN' start
        int32_t m_sim_count { }; // count of instruments still being processed

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
