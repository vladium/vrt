
#include "vr/market/rt/asx/utility.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

TEST (order_token_generator, sniff)
{
    {
        order_token const otk { order_token_generator::make_with_prefix<1> (1, 0xA) };
        EXPECT_EQ (otk, order_token { "A0000001" });
    }
    {
        order_token const otk { order_token_generator::make_with_prefix<2> (1, 0xAB) }; // two prefix digits
        EXPECT_EQ (otk, order_token { "AB000001" });
    }
    {
        order_token const otk { order_token_generator::make_with_prefix<1> (0x12543AF, 0xB) };
        EXPECT_EQ (otk, order_token { "B12543AF" });
    }

    // some extreme values:
    {
        order_token const otk { order_token_generator::make_with_prefix<1> (0, 0x0) };
        EXPECT_EQ (otk, order_token { "00000000" });
    }
    {
        order_token const otk { order_token_generator::make_with_prefix<1> (1, 0x0) };
        EXPECT_EQ (otk, order_token { "00000001" });
    }
    {
        order_token const otk { order_token_generator::make_with_prefix<1> (0xFFFFFFF, 0xF) };
        EXPECT_EQ (otk, order_token { "FFFFFFFF" });
    }

    // edge cases:
    {
        order_token const otk { order_token_generator::make_with_prefix<0> (0x01234567, 0xF) }; // 'prefix' value is ignore (empty prefix)
        EXPECT_EQ (otk, order_token { "01234567" }); // note that this still produces leading zeroes
    }
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
