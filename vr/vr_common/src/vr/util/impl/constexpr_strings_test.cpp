
#include "vr/util/impl/constexpr_strings.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

TEST (path_stem, correctness)
{
    {
        constexpr string_literal_t s = path_stem ("/a/b/cdef");
        const std::string s_expected { "cdef" };

        EXPECT_EQ (s_expected, s);
    }
    {
        constexpr string_literal_t s = path_stem ("/abcdef");
        const std::string s_expected { "abcdef" };

        EXPECT_EQ (s_expected, s);
    }
    // edge case inputs:
    {
        constexpr string_literal_t s = path_stem ("abc");
        const std::string s_expected { "abc" };

        EXPECT_EQ (s_expected, s);
    }
    {
        constexpr string_literal_t s = path_stem ("abc/");
        const std::string s_expected { };

        EXPECT_EQ (s_expected, s);
    }
    {
        constexpr string_literal_t s = path_stem ("/");
        const std::string s_expected { };

        EXPECT_EQ (s_expected, s);
    }
    {
        constexpr string_literal_t s = path_stem ("");
        const std::string s_expected { };

        EXPECT_EQ (s_expected, s);
    }
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
