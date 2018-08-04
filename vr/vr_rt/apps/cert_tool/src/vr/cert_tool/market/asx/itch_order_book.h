#pragma once

#include "vr/cert_tool/market/asx/itch_ref_data.h"
#include "vr/containers/util/stable_vector.h"
#include "vr/market/defs.h"
#include "vr/market/sources/asx/defs.h"
#include "vr/strings.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

namespace itch {
class order_add; // forward
class order_add_with_participant; // forward
class order_fill; // forward
class order_fill_with_price; // forward
class order_replace; // forward
class order_delete; // forwad
}

// TODO 'trade' ?

VR_ENUM (ostate,
    (
        active,
        filled,
        deleted
    ),
    printable

); // end of enum

/**
 * this is per instrument
 */
class itch_order_book final: noncopyable
{
    public: // ...............................................................

        struct order final
        {
            order (oid_t const oid, market::side::enum_t const s) :
                m_oid { oid },
                m_side { s }
            {
            }

            bool is_undisclosed () const
            {
                return (m_order_attrs & ord_attrs::undisclosed);
            };


            oid_t const m_oid;
            market::side::enum_t const m_side;
            int32_t m_price { };
            int64_t m_qty { };
            int64_t m_qty_traded { };
            uint16_t m_order_attrs { };
            ostate::enum_t m_state { ostate::active };

            friend std::ostream & operator<< (std::ostream & os, order const & e) VR_NOEXCEPT;

        }; // end of nested class


        itch_order_book (itch_ref_data::entry const & ird);


        // ACCESSORs:

        int32_t list (std::ostream & out, market::side::enum_t const s) const;
        int32_t list (std::ostream & out) const;

        // MUTATORs:

        VR_ASSUME_HOT order & update (itch::order_add const & msg);
        VR_ASSUME_HOT order & update (itch::order_add_with_participant const & msg);
        VR_ASSUME_HOT order & update (itch::order_fill const & msg);
        VR_ASSUME_HOT order & update (itch::order_fill_with_price const & msg);
        VR_ASSUME_HOT order & update (itch::order_replace const & msg);
        VR_ASSUME_HOT order & update (itch::order_delete const & msg);

        VR_FORCEINLINE void add_book_qty_traded (int64_t const qty_traded)
        {
            m_ird->qty_traded () += qty_traded;
        }

    private: // ..............................................................

        using order_pool        = util::stable_vector<order>;
        using oid_map           = boost::unordered_map<oid_t, order *>;

        /*
         * return nullptr on lookup failure
         */
        VR_FORCEINLINE order * lookup (side::enum_t const s, oid_t const oid) const
        {
            auto const i = m_side [s].find (oid);
            if (i == m_side [s].end ())
                return nullptr;

            assert_nonnull (i->second, s, oid);
            return i->second;
        }

        VR_FORCEINLINE order & lookup_or_add (side::enum_t const s, oid_t const oid)
        {
            auto const i = m_side [s].find (oid);
            order * o { nullptr };

            if (i == m_side [s].end ())
            {
                DLOG_trace3 << "adding order " << print (s) << ", " << oid;

                m_orders.emplace_back (oid, s);
                o = & m_orders.back ();

                m_side [s].emplace (oid, o);
            }
            else
            {
                o = i->second;
            }

            assert_nonnull (o, s, oid);
            return (* o);
        }


        itch_ref_data::entry const * const m_ird;
        order_pool m_orders { };
        oid_map m_side [side::size] { { }, { } };

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
