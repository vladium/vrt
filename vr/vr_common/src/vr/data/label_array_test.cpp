
#include "vr/data/label_array.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

TEST (label_array, construction)
{
    label_array const & a = label_array::create_and_intern ({ "l0", "l1", "l2" });

    ASSERT_EQ (3, a.size ());

    int32_t count { };
    for (std::string const & l : a)
    {
        std::string const l_expected = 'l' + std::to_string (count);
        EXPECT_EQ (l_expected, l) << " failed for index " << count;

        ++ count;
    }
    EXPECT_EQ (3, count);

    for (int32_t ix = 0; ix < a.size (); ++ ix)
    {
        ASSERT_EQ (a.index (a [ix]), ix);
    }

    EXPECT_EQ (2, a.max_label_size ());

    // invalid inputs:

    ASSERT_THROW (a.index ("invalid"), invalid_input);

    string_vector empty;
    ASSERT_THROW (label_array::create_and_intern (empty), invalid_input);

    string_vector empty_label { "A", "B", "" };
    ASSERT_THROW (label_array::create_and_intern (empty_label), invalid_input);

    string_vector dups { "A", "B", "A" };
    ASSERT_THROW (label_array::create_and_intern (dups), invalid_input);
}

//............................................................................

TEST (label_array, NA_support)
{
    // "NA" is always present implicitly (and not counted in size):
    {
        label_array const & a = label_array::create_and_intern ({ "A", "B", "C" });

        ASSERT_EQ (3, a.size ());

        EXPECT_EQ (a [enum_NA ()], NA_NAME_str);
        EXPECT_EQ (a.index (NA_NAME_str), -1);
    }
    // "NA" is not allowed to be present explicitly:
    {
        EXPECT_THROW (label_array::create_and_intern ({ "A", NA_NAME_str, "C" }), invalid_input);
    }
}
//............................................................................

// TODO MT-safety test

TEST (label_array, interning)
{
    label_array const & a1 = label_array::create_and_intern ({ "A", "B", "C" });

    const string_vector labels { "A", "B", "C" };
    label_array const & a2 = label_array::create_and_intern (labels);

    EXPECT_EQ (& a1, & a2);
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
