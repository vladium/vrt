
#include "vr/market/schema.h"

#include "vr/data/attributes.h"
#include "vr/io/stream_traits.h"
#include "vr/market/defs.h"
#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace schema
{
using namespace data;

//............................................................................
//............................................................................
namespace
{

template<io::format::enum_t FMT>
struct make_market_book_schema
{
    static std::vector<attribute> evaluate (int32_t const depth)
    {
        check_positive (depth);

        std::vector<attribute> r
        {
            { TS_ORIGIN,    atype::timestamp },
            { TS_LOCAL,     atype::timestamp }
        };

        atype::enum_t const price_at = atype::for_numeric<typename io::stream_traits<enum_<io::format, FMT> >::price_type> ();

        for (int32_t level = 0; level < depth; ++ level)
        {
            for (side::enum_t s : side::values ())
            {
                r.emplace_back (join_as_name<'_'> (PRICE, s, level), price_at);
            }

            for (side::enum_t s : side::values ())
            {
                r.emplace_back (join_as_name<'_'> (QTY, s, level), atype::for_numeric<qty_t> ());
            }
        }

        r.shrink_to_fit ();

        return r;
    }

}; // end of class
//............................................................................

template<io::format::enum_t FMT>
struct make_trade_schema final: public util::singleton_constructor<std::vector<attribute> >
{
    make_trade_schema (std::vector<attribute> * const obj)
    {
        new (obj) std::vector<attribute>
        {
            { TS_ORIGIN,    atype::timestamp },
            { TS_LOCAL,     atype::timestamp },
            { PRICE,        atype::for_numeric<typename io::stream_traits<enum_<io::format, FMT> >::price_type> () },
            { QTY,          atype::for_numeric<qty_t> () },
            { SIDE,         attr_type::for_enum<market::side> () }
        };

        obj->shrink_to_fit ();
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::vector<attribute>
market_book_attributes (io::format::enum_t const fmt, int32_t const depth)
{
    switch (fmt)
    {
#   define vr_CASE(r, unused, e_name) \
        case io::format:: e_name : return make_market_book_schema<io::format:: e_name >::evaluate (depth); \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_IO_FORMAT_SEQ)

#   undef vr_CASE

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch
}
//............................................................................

std::vector<attribute> const &
trade_attributes (io::format::enum_t const fmt)
{
    switch (fmt)
    {
#   define vr_CASE(r, unused, e_name) \
        case io::format:: e_name : return util::singleton<std::vector<attribute>, make_trade_schema<io::format:: e_name > >::instance (); \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_IO_FORMAT_SEQ)

#   undef vr_CASE

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

} // end of 'schema'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
