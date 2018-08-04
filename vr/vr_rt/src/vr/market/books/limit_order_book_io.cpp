
#include "vr/market/books/limit_order_book_io.h"

#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace md
{
//............................................................................
//............................................................................
namespace
{

struct format_holder
{
    std::array<boost::format, 8> const m_formats
        { {
            boost::format { "%|13s|%|6d| |%|10.4f| |\n" },              // BID only, no offer
            boost::format { "%|20T ||%|10.4f| |%|6d| (%|d|)\n" },       // ASK only, no bid
            boost::format { "%|13s|%|6d| |%|10.4f| |%|6d| (%|d|)\n" },  // BID and ASK

            boost::format { " %|20T=||%|32T=||%|53T=|\n" },             // spread divider

            boost::format { "%|13s|%|6d| |%|10.4f| |%|53T-|\n" },       // BID only, ASK not started
            boost::format { " %|20T-||%|10.4f| |%|6d| (%|d|)\n" },      // ASK only, BID not started

            boost::format { " %|20T-||%|32T ||\n" },
            boost::format { " %|20T ||%|32T ||%|53T-|\n" }
        } };

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
namespace impl
{

std::array<boost::format, 8> const &
book_formats ()
{
    return util::singleton<format_holder>::instance ().m_formats;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'md'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
