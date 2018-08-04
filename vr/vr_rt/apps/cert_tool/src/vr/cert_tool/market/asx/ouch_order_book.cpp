
#include "vr/cert_tool/market/asx/ouch_order_book.h"

#include "vr/market/sources/asx/ouch/error_codes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

std::ostream &
operator<< (std::ostream & os, order_descriptor const & obj) VR_NOEXCEPT
{
    os << "  " << print (obj.m_otk_current) << " [" << (obj.m_partition +/* ! */1)  << "]:\t"
       << obj.m_state << ", " << obj.m_qty_executed << ", "
       << obj.m_cancel_code << ", " << print_reject_code (obj.m_reject_code);

    return os;
}
//............................................................................
//............................................................................

int32_t
ouch_order_book::list (std::ostream & out) const
{
    boost::unordered_set<addr_const_t> uniq { };

    int32_t r { };

    for (auto const & kv : m_otk_map)
    {
        if (uniq.insert (kv.second).second)
        {
            ++ r;
            out << "\r\n" << (* kv.second);
        }
    }

    return r;
}
//............................................................................

order_descriptor &
ouch_order_book::add (order_token const otk, int32_t const partition)
{
    auto i = m_otk_map.find (otk);
    if (VR_UNLIKELY (i != m_otk_map.end ()))
        throw_x (invalid_input, "order " + print (otk) + " already in the book");

    m_odv.emplace_back (otk, partition);
    order_descriptor * const o = & m_odv.back ();

    m_otk_map.emplace (otk, o);

    return (* o);
}

order_descriptor &
ouch_order_book::add_alias (order_token const otk, order_token const new_otk)
{
    auto i = m_otk_map.find (otk);
    if (VR_UNLIKELY (i == m_otk_map.end ()))
        throw_x (invalid_input, "order " + print (otk) + " not in the book");

    assert_nonnull (i->second);
    m_otk_map.emplace (new_otk, i->second);

    return (* i->second);
}

order_descriptor &
ouch_order_book::lookup (order_token const otk, bool const add_if_missing)
{
    auto i = m_otk_map.find (otk);
    if (VR_UNLIKELY (i == m_otk_map.end ()))
    {
        if (! add_if_missing)
            throw_x (invalid_input, "order " + print (otk) + " not in the book");
        else
        {
            m_odv.emplace_back (otk, /* TODO: unknown */-1);
            order_descriptor * const o = & m_odv.back ();

            i = m_otk_map.emplace (otk, o).first;
        }
    }

    assert_nonnull (i->second);
    return (* i->second);
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
