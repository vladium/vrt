
#include "vr/market/sources/asx/defs.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

TEST (ASX_price_ops, sniff)
{
    using price_traits      = this_source_traits::price_traits<price_si_t>;

    EXPECT_EQ (price_traits::print_to_wire (4.15),      41500); // equivalent to 'as_price()' that's being obsoleted
    EXPECT_EQ (price_traits::print_to_wire (4.1511),    41511);

    EXPECT_EQ (price_traits::print_to_wire (0.058),     580);
    EXPECT_EQ (price_traits::print_to_wire (0.035),     350);
}
//............................................................................

TEST (ASX_price_ops, round_trip_integral)
{
    using price_traits      = this_source_traits::price_traits<price_si_t>;

    for (double px : { 0.001, 0.005, 0.01, 0.05, 0.1, 1.0, 10.05, 109.11, 99000.19 })
    {
        this_source_traits::price_type const wire_price = as_price (px);
        LOG_trace1 << " px " << std::fixed << std::setprecision (3) << px << " -> wire price " << wire_price;

        price_si_t const book_price = price_traits::wire_to_book (wire_price);

        this_source_traits::price_type const wire_price2 = price_traits::book_to_wire (book_price);
        ASSERT_EQ (wire_price2, wire_price) << " round trip failed for px " << px;

        price_si_t const book_price2 = price_traits::wire_to_book (wire_price2);
        ASSERT_EQ (book_price2, book_price) << " round trip failed for px " << px;
    }
}
//............................................................................
// TODO this is overkill, to defeat tiredness; rm after cert

TEST (ASX_ord_side, sniff)
{
    EXPECT_EQ (ord_side::value ("BUY"),       ord_side::BUY);
    EXPECT_EQ (ord_side::value ("SELL"),      ord_side::SELL);
    EXPECT_EQ (ord_side::value ("SHORT"),     ord_side::SHORT);

    EXPECT_EQ (ord_side::BUY,   'B');
    EXPECT_EQ (ord_side::SELL,  'S');
    EXPECT_EQ (ord_side::SHORT, 'T');
}

TEST (ASX_ord_type, sniff)
{
    EXPECT_EQ (ord_type::value ("LIMIT"),                 ord_type::LIMIT);

    EXPECT_EQ (ord_type::value ("CENTER_POINT"),          ord_type::CENTER_POINT);
    EXPECT_EQ (ord_type::value ("CENTER_POINT_DARK"),     ord_type::CENTER_POINT_DARK);
    EXPECT_EQ (ord_type::value ("CENTER_POINT_BLOCK"),    ord_type::CENTER_POINT_BLOCK);
    EXPECT_EQ (ord_type::value ("CENTER_POINT_DARK_MAQ"), ord_type::CENTER_POINT_DARK_MAQ);

    EXPECT_EQ (ord_type::LIMIT,                 'Y');

    EXPECT_EQ (ord_type::CENTER_POINT,          'N');
    EXPECT_EQ (ord_type::CENTER_POINT_DARK,     'D');
    EXPECT_EQ (ord_type::CENTER_POINT_BLOCK,    'B');
    EXPECT_EQ (ord_type::CENTER_POINT_DARK_MAQ, 'F');
}

TEST (ASX_ord_TIF, sniff)
{
    EXPECT_EQ (ord_TIF::value ("DAY"), ord_TIF::DAY);
    EXPECT_EQ (ord_TIF::value ("IOC"), ord_TIF::IOC);
    EXPECT_EQ (ord_TIF::value ("FOK"), ord_TIF::FOK);

    EXPECT_EQ (static_cast<int32_t> (ord_TIF::DAY), 0);
    EXPECT_EQ (static_cast<int32_t> (ord_TIF::IOC), 3);
    EXPECT_EQ (static_cast<int32_t> (ord_TIF::FOK), 4);
}

TEST (ASX_cancel_reason, sniff)
{
    EXPECT_EQ (string_cast (cancel_reason::REQUESTED_BY_USER),  "REQUESTED_BY_USER");
    EXPECT_EQ (string_cast (cancel_reason::CONNECTION_LOSS),    "CONNECTION_LOSS");
    EXPECT_EQ (string_cast (cancel_reason::INACTIVATED_BY_EOD), "INACTIVATED_BY_EOD");
}


} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
