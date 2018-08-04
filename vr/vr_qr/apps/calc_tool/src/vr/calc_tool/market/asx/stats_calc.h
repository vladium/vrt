#pragma once

#include "vr/io/frame_streams.h"
#include "vr/market/books/defs.h" // _book_
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/util/multiprecision_io.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

struct stats_metadata_base
{
    price_si_t VWAP () const
    {
        if (VR_LIKELY (m_trade_qty > 0))
            return static_cast<price_si_t> (m_trade_value / m_trade_qty); // TODO could be done with smaller precision loss with an fp div

        return data::NA<price_si_t> ();
    }

    std::string m_symbol { };
    std::size_t m_trade_qty { };
    native_uint128_t m_trade_value { };

}; // end of class

using stats_metadata        = meta::make_compact_struct_t<meta::make_schema_t
                                <
                                    meta::fdef_<itch::book_state::enum_t,   _state_>
                                >,
                                stats_metadata_base>;

//............................................................................
/**
 * qty accounting (c.f ASX ITCH 2.10):
 *
 *  - 'order_fill's on one side (avoid double-counting);
 *  - 'order_fill_with_price's that are 'printable'==Y;
 *  - 'trade's that are 'printable'==Y;
 *
 * value accounting:
 *
 *  - in parallel with qty;
 *  - for 'order_fill's look up the orders' current prices in the book (this must run before the listener can possibly delete an order)
 *
 */
template<typename MARKET_DATA_VIEW, typename CTX>
class stats_calc: public ITCH_visitor<stats_calc<MARKET_DATA_VIEW, CTX> >
{
    using super         = ITCH_visitor<stats_calc<MARKET_DATA_VIEW, CTX> >;

        vr_static_assert (has_field<_book_, CTX> ());

        using book_type         = typename MARKET_DATA_VIEW::book_type;

        using book_price_type   = typename book_type::price_type;
        using side_iterator     = typename book_type::side_iterator;
        using wire_price_type   = typename this_source_traits::price_type;
        using price_traits      = typename this_source_traits::price_traits<book_price_type>;

        static constexpr side::enum_t counting_side ()  { return side::BID; }

    public: // ...............................................................

        stats_calc (arg_map const & args) :
            m_mdv { args.get<MARKET_DATA_VIEW &> ("view") }
        {
            ref_data const & rd = args.get<ref_data &> ("ref_data"); // required [but could also store a ref with the view]

            for (auto const & e : m_mdv)
            {
                iid_t const & iid = field<_key_> (e);
                auto & md = field<_value_> (e)->user_data ();

                instrument const & i = rd [iid];

                md.m_symbol = i.symbol ();
            }
        }


        /*
         * used by the op driver
         */
        void emit (std::ostream & os)
        {
            os << "iid,symbol,px_vwap,qty_trade,value_trade" << std::endl;

            for (auto const & e : m_mdv)
            {
                iid_t const & iid = field<_key_> (e);
                auto & md = field<_value_> (e)->user_data ();

                os << iid << ',' << print (md.m_symbol)
                   << ',' << price_book_to_print ((md.m_trade_qty > 0) ? md.VWAP () : data::NA<price_si_t> ())
                   << ',' << md.m_trade_qty
                   << ',' << (md.m_trade_value / price_si_scale ())
                   << std::endl;
            }
        }

        void emit (io::frame_ostream & out)
        {

        }



        // overridden ITCH visits:

        using super::visit;

        VR_ASSUME_HOT bool visit (itch::order_fill const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            if (counting_side () == ord_side::to_side (msg.side ()))
            {
                book_type const & book = current_book (ctx);
                auto & md = book.user_data ();

                if (field<_state_> (md) == itch::book_state::OPEN) // track VWAP stats only during the regular session
                {
                    auto const * const o = book.find_order (counting_side (), msg.oid ());
                    if (VR_UNLIKELY (o == nullptr))
                        LOG_warn << "oid " << msg.oid () << " not in the current book";
                    else
                    {
                        auto const & lvl = book.level_of (* o);

                        auto const qty = msg.qty ();

                        md.m_trade_qty += qty;
                        md.m_trade_value += qty * field<_price_> (lvl);
                    }
                }
            }

            return rc;
        }

        VR_ASSUME_HOT bool visit (itch::order_fill_with_price const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            if (msg.printable () == itch::boolean::yes)
            {
                book_type const & book = current_book (ctx);
                auto & md = book.user_data ();

                if (field<_state_> (md) == itch::book_state::OPEN) // track VWAP stats only during the regular session
                {
                    auto const qty = msg.qty ();
                    price_si_t const price = price_traits::wire_to_book (msg.price ());

                    md.m_trade_qty += qty;
                    md.m_trade_value += qty * price;
                }
            }

            return rc;
        }

        VR_ASSUME_HOT bool visit (itch::trade const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx);

            if (msg.printable () == itch::boolean::yes)
            {
                book_type const & book = current_book (ctx);
                auto & md = book.user_data ();

                if (field<_state_> (md) == itch::book_state::OPEN) // track VWAP stats only during the regular session
                {
                    auto const qty = msg.qty ();
                    price_si_t const price = price_traits::wire_to_book (msg.price ());

                    md.m_trade_qty += qty;
                    md.m_trade_value += qty * price;
                }
            }

            return rc;
        }

    private: // ..............................................................

        static VR_FORCEINLINE book_type const & current_book (CTX & ctx)
        {
            addr_const_t const book_ref = field<_book_> (ctx);
            assert_nonnull (book_ref); // relying on a selector ahead of us in the pipeline

            return (* static_cast<book_type const *> (book_ref));
        }


        MARKET_DATA_VIEW const & m_mdv;

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
