
#include "vr/data/parse/attr_parser.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
namespace parse
{

TEST (attr_parser, sniff)
{
    std::vector<attribute> const attrs = attr_parser::parse ("  A ,B,  CDEF : { X ,  Z }; \t 1234 : f8 ; ");

    ASSERT_EQ (4, signed_cast (attrs.size ()));

    attribute const & a0 = attrs [0];
    {
        EXPECT_EQ (a0.name (), "A");
        ASSERT_EQ (a0.atype (), atype::category);
        ASSERT_EQ (a0.labels ().size (), 2);
        EXPECT_EQ (a0.labels ()[0], "X");
        EXPECT_EQ (a0.labels ()[1], "Z");
    }
    {
        attribute const & a = attrs [1];
        EXPECT_EQ (a.name (), "B");
        EXPECT_EQ (a.type (), a0.type ());
    }
    {
        attribute const & a = attrs [2];
        EXPECT_EQ (a.name (), "CDEF");
        EXPECT_EQ (a.type (), a0.type ());
    }
    {
        attribute const & a = attrs [3];
        EXPECT_EQ (a.name (), "1234");
        EXPECT_EQ (a.atype (), atype::f8);
    }
}

} // end of 'parse'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
