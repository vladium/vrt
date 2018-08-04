#pragma once

#include "vr/market/ref/asx/instrument.h"
#include "vr/market/sources/asx/market_data.h"
#include "vr/util/parse.h"

#include <boost/algorithm/string/trim.hpp>

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
template<typename CTX>
class ITCH_refdata_parser: public ITCH_visitor<ITCH_refdata_parser<CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<ITCH_refdata_parser<CTX> >;

        using price_traits  = typename this_source_traits::price_traits<price_si_t>;

        vr_static_assert (has_field<_partition_, CTX> ());

    public: // ...............................................................

        using instrument_map    = boost::unordered_map<iid_t, instrument>;


        ITCH_refdata_parser (arg_map const & args) :
            super (args)
        {
        }



        instrument_map const & instruments () const
        {
            if (! m_finalized)
            {
                // finalize 'm_instruments' content:

                for (auto i = m_instruments.begin (); i != m_instruments.end (); ) // possible deletion in the loop
                {
                    auto & i_tick_table = i->second.tick_table ();

                    bool erase { false };
                    do // drop test and other sketchy entries:
                    {
                        if (i->second.symbol ().empty ())
                        {
                            LOG_warn << "dropping " << print (i->second.iid ()) << ": empty symbol";
                            erase = true;
                            break;
                        }

                        if (i_tick_table.empty ())
                        {
                            LOG_warn << "dropping " << print (i->second.symbol ()) << ": empty tick size table";
                            erase = true;
                            break;
                        }

                        std::string ISIN { i->second.ISIN ().data (), i->second.ISIN ().size () };
                        boost::algorithm::trim (ISIN);
                        if (ISIN.empty ())
                        {
                            LOG_warn << "dropping " << print (i->second.symbol ()) << ": invalid ISIN";
                            erase = true;
                            break;
                        }
                    }
                    while (false);

                    if (erase)
                        i = m_instruments.erase (i);
                    else
                    {
                        std::sort (i_tick_table.begin (), i_tick_table. end ());

                        // test for no duplicate entries in the table:

                        assert_nonempty (i_tick_table); // would have been dropped above

                        tick_size_entry e_prev; e_prev.begin () = 0;

                        for (auto e = i_tick_table.begin (); e != i_tick_table.end (); )
                        {
                            if ((e_prev.begin () > 0) && (e->begin () == e_prev.begin ()))
                            {
                                if (* e == e_prev) // simply discard a duplicate entry if a complete duplicate:
                                {
                                    LOG_trace1 << "discarding duplicate tick_size table entry " << print (* e);
                                    e = i_tick_table.erase (e);
                                }
                                else
                                    throw_x (invalid_input, print (i->second.symbol ()) + ": non-equivalent tick_size entries with duplicate 'begin' values " + print (i->second));
                            }
                            else
                            {
                                e_prev = * e;
                                ++ e;
                            }
                        }

                        ++ i;
                    }
                }

                m_finalized = true;
            }

            return m_instruments;
        }

        // overridden ITCH visits:

        using super::visit;


        VR_ASSUME_HOT bool visit (itch::order_book_dir const & msg, CTX & ctx) // override
        {
            assert_condition (! m_finalized);
            auto const rc = super::visit (msg, ctx); // [chain]

            auto const i = get_or_create (msg.iid ());
            check_condition (i.second, msg);

            fill_descriptor (msg, field<_partition_> (ctx), * i.first);

            return rc;
        }

        VR_ASSUME_HOT bool visit (itch::combo_order_book_dir const & msg, CTX & ctx) // override
        {
            assert_condition (! m_finalized);
            auto const rc = super::visit (msg, ctx); // [chain]

            auto const i = get_or_create (msg.iid ());
            check_condition (i.second, msg);

            fill_descriptor (msg, field<_partition_> (ctx), * i.first); // TODO currently ignoring the combo info

            return rc;
        }

        VR_ASSUME_HOT bool visit (itch::tick_size_table_entry const & msg, CTX & ctx) // override
        {
            assert_condition (! m_finalized);
            auto const rc = super::visit (msg, ctx); // [chain]

            auto const i = m_instruments.find (msg.iid ());
            if (i != m_instruments.end ())
            {
                auto & tick_table = i->second.tick_table ();

                tick_size_entry e;
                {
                    e.tick_size () = price_traits::wire_to_book (msg.tick_size ());
                    e.begin () = price_traits::wire_to_book (msg.begin ());
                }
                tick_table.emplace_back (e);
            }

            return rc;
        }

    private: // ..............................................................

        VR_ASSUME_HOT void fill_descriptor (itch::order_book_dir const & msg, int32_t const partition, instrument & d)
        {
            // TODO product 'group' ?

            d.iid () = msg.iid ();
            d.partition () = partition;
            d.product () = msg.product ();
            d.ccy () = currency::value (msg.ccy ().data (), msg.ccy ().size ());
            assign (d.symbol (), util::rtrim (msg.symbol ()));
            assign (d.name (), util::rtrim (msg.name ()));
            d.ISIN () = msg.ISIN ();
        }

        VR_ASSUME_HOT std::pair<instrument *, bool> get_or_create (iid_t const iid)
        {
            auto i = m_instruments.find (iid);
            bool const inserted = (i == m_instruments.end ());

            if (inserted) i = m_instruments.emplace (iid, instrument { }).first;

            return { & i->second, inserted };
        }


        mutable instrument_map m_instruments { };
        mutable bool m_finalized { false };

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
