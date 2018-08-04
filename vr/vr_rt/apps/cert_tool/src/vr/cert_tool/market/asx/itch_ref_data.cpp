
#include "vr/cert_tool/market/asx/itch_ref_data.h"

#include "vr/market/sources/asx/itch/messages.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

#define vr_EOL  "\r\n"

//............................................................................

std::ostream &
operator<< (std::ostream & os, itch_ref_data::entry const & e) VR_NOEXCEPT
{
    return os << "[p #" << e.m_pix << "] " << e.m_product << '\t' << e.m_iid << '\t' << print (e.m_symbol) << '\t' << print (e.m_name)
              << "\t(" << e.m_book_state << "), total traded: " << e.m_qty_traded;

}
//............................................................................
//............................................................................

itch_ref_data::entry const &
itch_ref_data::operator[] (std::string const & symbol) const
{
    auto const i = m_symbol_map.find (symbol);
    if (VR_UNLIKELY (i == m_symbol_map.end ()))
        throw_x (invalid_input, "unknown symbol " + print (symbol));

    assert_nonnull (i->second, symbol);
    return (* i->second);
}

itch_ref_data::entry const &
itch_ref_data::operator[] (iid_t const & iid) const
{
    auto const i = m_iid_map.find (iid);
    if (VR_UNLIKELY (i == m_iid_map.end ()))
        throw_x (invalid_input, "unknown iid " + print (iid));

    assert_nonnull (i->second, iid);
    return (* i->second);
}
//............................................................................

int32_t
itch_ref_data::list_symbols (name_filter const & nf, std::ostream & out) const
{
    int32_t r { };
    for (auto const & kv : m_symbol_map)
    {
        if (nf (kv.first))
        {
            assert_nonnull (kv.second);
            ++ r;
            out << "  " << (* kv.second) << vr_EOL;
        }
    }

    return r;
}

int32_t
itch_ref_data::list_names (name_filter const & nf, std::ostream & out) const
{
    int32_t r { };
    for (auto const & kv : m_symbol_map)
    {
        assert_nonnull (kv.second);
        entry const & e = * kv.second;

        if (nf (e.m_name))
        {
            ++ r;
            out << "  " << e << vr_EOL;
        }
    }

    return r;
}
//............................................................................

void
itch_ref_data::add (itch::order_book_dir const & def, int32_t const pix)
{
    // this is not very "transactional":

    iid_t const iid = def.iid ();
    auto const irc = m_iid_map.emplace (iid, nullptr);
    if (VR_UNLIKELY (! irc.second))
        throw_x (invalid_input, "duplicate iid: " + print (def));

    std::string symbol = util::rtrim (def.symbol ()).to_string ();
    auto const src = m_symbol_map.emplace (symbol, nullptr);
    if (VR_UNLIKELY (! src.second))
        throw_x (invalid_input, "duplicate symbol: " + print (def));

    std::string name = util::rtrim (def.name ()).to_string ();

    LOG_trace3 << "adding ref data entry for " << print (iid) << ", " << print (symbol);

    m_entries.emplace_back (std::move (symbol), std::move (name), iid, def.product (), pix); // last use of 'symbol' and 'name'
    entry * const e = & m_entries.back ();

    irc.first->second = e;
    src.first->second = e;

    assert_eq (m_iid_map.size (), m_symbol_map.size ()); // invariant
}

void
itch_ref_data::add (itch::combo_order_book_dir const & def, int32_t const pix)
{
    add (def, pix);
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
