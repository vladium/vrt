
#include "vr/calc_tool/market/asx/schema.h"

#include "vr/data/attributes.h"
#include "vr/io/stream_traits.h"
#include "vr/market/schema.h"
#include "vr/market/sources/asx/itch/messages.h" // itch::book_state
#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace
{
using namespace data;
using namespace schema;

template<io::format::enum_t FMT>
struct make_auction_calc_schema final: public util::singleton_constructor<std::vector<attribute> >
{
    static constexpr atype::enum_t price_at ()  { return atype::for_numeric<typename io::stream_traits<enum_<io::format, FMT> >::price_type> (); }

    make_auction_calc_schema (std::vector<data::attribute> * const obj)
    {
        new (obj) std::vector<attribute>
        {
            { TS_ORIGIN,    atype::timestamp },
            { TS_LOCAL,     atype::timestamp },
//            { "oid",        atype::for_numeric<oid_t> () },
            { STATE,        attr_type::for_enum<itch::book_state> () },
            { join_as_name<'_'> (PRICE, "auction"),     price_at () },
            { join_as_name<'_'> (QTY, "match"),         atype::for_numeric<qty_t> () },
            { join_as_name<'_'> (QTY, "surplus"),       atype::for_numeric<qty_t> () }
        };

        for (side::enum_t s : side::values ())
        {
            obj->emplace_back (join_as_name<'_'> (QTY, s, "auction"),   atype::for_numeric<qty_t> ());
        }
        for (side::enum_t s : side::values ())
        {
            obj->emplace_back (join_as_name<'_'> (QTY, s, "avail"),     atype::for_numeric<qty_t> ());
        }
        for (side::enum_t s : side::values ())
        {
            obj->emplace_back (join_as_name<'_'> (QTY, s, "hidden"),    atype::for_numeric<qty_t> ());
        }

        for (side::enum_t s : side::values ())
        {
            for (int32_t depth = 0; depth < auction_calc_depth (); ++ depth)
            {
                obj->emplace_back (join_as_name<'_'> (PRICE, s, "i", depth),   price_at ());
                obj->emplace_back (join_as_name<'_'> (QTY, s, "i", depth),     atype::for_numeric<qty_t> ());
            }
        }

        obj->shrink_to_fit ();
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::vector<data::attribute> const &
auction_calc_attributes (io::format::enum_t const fmt)
{
    switch (fmt)
    {
#   define vr_CASE(r, unused, e_name) \
        case io::format:: e_name : return util::singleton<std::vector<attribute>, make_auction_calc_schema<io::format:: e_name > >::instance (); \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_IO_FORMAT_SEQ)

#   undef vr_CASE

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
