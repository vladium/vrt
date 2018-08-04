
#include "vr/util/any_map.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (any_map, construction)
{
    // curly brace init works:

    any_map const am { { "a", 123 }, { "c", true }, { "b", "a c-string" } };
    ASSERT_EQ (3, signed_cast (am.size ()));

    // the map is ordered:
    {
        auto i = am.begin ();

        EXPECT_EQ (i->first, "a");
        EXPECT_EQ (any_get<int32_t> (i->second), 123);

        ++ i;
        EXPECT_EQ (i->first, "b");
        EXPECT_EQ (any_get<std::string> (i->second), "a c-string");

        ++ i;
        EXPECT_EQ (i->first, "c");
        EXPECT_EQ (any_get<bool> (i->second), true);
    }

    // get ():

    bool const & c_value = am.get<bool> ("c");
    EXPECT_EQ (c_value, true);

    EXPECT_THROW (am.get<int32_t> ("nonexistent"), invalid_input);

    int32_t const i_value = am.get<int32_t> ("nonexistent", 123);
    EXPECT_EQ (i_value, 123);
}
//............................................................................

TEST (any_map, mutation)
{
    // curly brace init works:

    any_map am { { "a", 123 }, { "c", true }, { "b", 'B' } };

    {
        auto const i = am.find ("b");
        ASSERT_TRUE (i != am.end ());
        ASSERT_EQ (any_get<char> (i->second), 'B');

        i->second = "a string";
    }
    {
        auto const i = am.find ("b");
        ASSERT_TRUE (i != am.end ());

        std::string const b_value = any_get<std::string> (i->second);
        EXPECT_EQ (b_value, "a string");
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
