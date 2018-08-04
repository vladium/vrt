
#include "vr/market/sources/asx/defs.h"

#include <cmath>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

int32_t
as_price (double const x)
{
    return std::round (x * 10000);
}

double
to_price (int32_t const x)
{
    return (static_cast<double> (x) / 10000);
}
//............................................................................

std::string
ord_attrs::describe (uint32_t const attrs) VR_NOEXCEPT
{
    std::stringstream ss;

    bool not_first { false };

    ss << '{';
    {
        if (attrs & market_bid)
        {
            not_first = true;
            ss << market_bid;
        }
        if (attrs & price_stabilization)
        {
            if (not_first)
                ss << ",";
            else
                not_first = true;
            ss << price_stabilization;
        }
        if (attrs & undisclosed)
        {
            if (not_first) ss << ",";
            ss << undisclosed;
        }
    }
    ss << '}';

    return ss.str ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
