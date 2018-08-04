
#include "vr/cert_tool/market/asx/itch_order_book.h"

#include "vr/market/sources/asx/market_data.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

std::ostream &
operator<< (std::ostream & os, itch_order_book::order const & e) VR_NOEXCEPT
{
    return os << e.m_side << ", " << e.m_oid << ", price: " << to_price (e.m_price)
              << ", qty: " << e.m_qty << " (traded: " << e.m_qty_traded << "), type: " << ord_attrs::describe (e.m_order_attrs)
              << ", state: " << print (e.m_state);
}
//............................................................................
//............................................................................

itch_order_book::itch_order_book (itch_ref_data::entry const & ird) :
    m_ird { & ird }
{
}
//............................................................................

int32_t
itch_order_book::list (std::ostream & out, market::side::enum_t const s) const
{
    out << print (s) << std::endl;

    for (auto const & kv : m_side [s])
    {
        out << "  " << (* kv.second) << "\r\n";
    }

    return m_side [s].size ();
}

int32_t
itch_order_book::list (std::ostream & out) const
{
    int32_t r { };
    for (side::enum_t s : side::values ())
    {
        r += list (out, s);
    }

    out << "  qty traded: " << m_ird->qty_traded () << "\r\n";

    return r;
}
//............................................................................

itch_order_book::order &
itch_order_book::update (itch::order_add const & msg)
{
    oid_t const oid = msg.oid ();
    side::enum_t const s = ord_side::to_side (msg.side ());

    order & o = lookup_or_add (s, oid);
    {
        o.m_price = msg.price ();
        o.m_qty = msg.qty ();
        o.m_order_attrs = msg.order_attrs ();
        o.m_state = ostate::active;
    }

    return o;
}

itch_order_book::order &
itch_order_book::update (itch::order_add_with_participant const & msg)
{
    return update (static_cast<itch::order_add const &> (msg));
}

itch_order_book::order &
itch_order_book::update (itch::order_fill const & msg)
{
    oid_t const oid = msg.oid ();
    side::enum_t const s = ord_side::to_side (msg.side ());

    order & o = lookup_or_add (s, oid);
    {
        o.m_qty_traded += msg.qty ();

        if (o.m_qty_traded == o.m_qty)
        {
            if (! o.is_undisclosed ())
                o.m_state = ostate::filled;
        }
    }

    add_book_qty_traded (msg.qty ());

    return o;
}

itch_order_book::order &
itch_order_book::update (itch::order_fill_with_price const & msg)
{
    oid_t const oid = msg.oid ();
    side::enum_t const s = ord_side::to_side (msg.side ());

    order & o = lookup_or_add (s, oid);
    {
        o.m_price = msg.price ();
        o.m_qty_traded += msg.qty ();

        if (o.m_qty_traded == o.m_qty)
        {
            if (! o.is_undisclosed ())
                o.m_state = ostate::filled;
        }
    }

    if (msg.printable () == itch::boolean::yes)
    {
        add_book_qty_traded (msg.qty ());
    }

    return o;
}

itch_order_book::order &
itch_order_book::update (itch::order_replace const & msg)
{
    oid_t const oid = msg.oid ();
    side::enum_t const s = ord_side::to_side (msg.side ());

    order & o = lookup_or_add (s, oid);
    {
        o.m_price = msg.price ();
        o.m_qty = msg.qty ();
        o.m_order_attrs = msg.order_attrs ();
    }

    return o;
}

itch_order_book::order &
itch_order_book::update (itch::order_delete const & msg)
{
    oid_t const oid = msg.oid ();
    side::enum_t const s = ord_side::to_side (msg.side ());

    order & o = lookup_or_add (s, oid);
    {
        o.m_state = ostate::deleted;
    }

    return o;
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
