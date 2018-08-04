
#include "vr/util/str_range.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

TEST (str_range, comparison_homogeneous)
{
    char a [] = "asdfgh";
    char b [] = "asdfghwerwe";

    {
        str_range const lhs { a,  6 };
        str_range const rhs { a,  6 }; // same 'm_start'

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (lhs < rhs);
        ASSERT_EQ (hash_value (lhs), hash_value (rhs));

        ASSERT_EQ (signed_cast (lhs.to_string ().size ()), lhs.m_size);
        ASSERT_EQ (signed_cast (rhs.to_string ().size ()), rhs.m_size);

        // confirm deep data copy:

        EXPECT_NE (lhs.m_start, & lhs.to_string ().data ()[0]);
        EXPECT_NE (rhs.m_start, & rhs.to_string ().data ()[0]);
    }
    {
        str_range const lhs { a,  6 };
        str_range const rhs { b,  6 };

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (lhs < rhs);
        EXPECT_EQ (hash_value (lhs), hash_value (rhs));
    }
    {
        str_range const lhs { a,  6 };
        str_range const rhs { b,  7 };

        ASSERT_NE (lhs, rhs);
        ASSERT_LT (lhs, rhs);
    }
    {
        str_range const lhs { a,  5 };
        str_range const rhs { b,  6 };

        ASSERT_NE (lhs, rhs);
        ASSERT_LT (lhs, rhs);
    }
    {
        str_range const lhs { a,  5 };
        str_range const rhs { b,  4 };

        ASSERT_NE (lhs, rhs);
        ASSERT_LT (rhs, lhs);
    }

    // zero size is valid:
    {
        str_range const lhs { a,  0 };
        str_range const rhs { b,  0 };

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (rhs < lhs);
        EXPECT_EQ (hash_value (lhs), hash_value (rhs));
    }
}

TEST (str_range, comparison_mixed)
{
    char a [] = "asdfgh";
    char b [] = "asdfghwerwe";

    {
        string_literal_t const lhs = a;
        str_range const rhs { a,  6 }; // same 'm_start'

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (lhs < rhs);
    }
    {
        str_range const lhs { a,  6 };
        string_literal_t const rhs = a; // same 'm_start'

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (lhs < rhs);
    }

    {
        string_literal_t const lhs = a;
        str_range const rhs { b,  6 };

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (lhs < rhs);
    }
    {
        str_range const lhs { a,  6 };
        string_literal_t const rhs = b;

        ASSERT_FALSE (lhs == rhs);
        ASSERT_LT (lhs, rhs);
    }

    // zero size is valid:
    {
        string_literal_t const lhs = "";
        str_range const rhs { b,  0 };

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (rhs < lhs);
    }
    {
        str_range const lhs { a,  0 };
        string_literal_t const rhs = "";

        ASSERT_EQ (lhs, rhs);
        ASSERT_FALSE (rhs < lhs);
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
