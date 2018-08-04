
#include "vr/data/NA.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................
//............................................................................
namespace
{
// from R's 'arithmetic.c':

    union ieee_double
    {
        double value;
        uint32_t word [2];
    };

    bool
    R_IsNA (double x)
    {
        if (std::isnan (x))
        {
            ieee_double y;
            y.value = x;
            return (y.word [0] == 1954);
        }
        return false;
    }

//............................................................................

VR_ENUM (E, (A, B, C), iterable, printable, parsable);

enum NTE { A, B, C }; // old-style enum

} // end of anonymous
//............................................................................
//............................................................................

// fp types:

TEST (NA_sanity, float)
{
    float const na = NA<float> ();
    EXPECT_TRUE (is_NA (na));

    float const nan = std::numeric_limits<float>::quiet_NaN ();
    EXPECT_FALSE (is_NA (nan));
}

TEST (NA_sanity, double)
{
    double const na = NA<double> ();
    EXPECT_TRUE (is_NA (na));
    EXPECT_TRUE (R_IsNA (na)); // R compatibility

    double const nan = std::numeric_limits<double>::quiet_NaN ();
    EXPECT_FALSE (is_NA (nan));
    EXPECT_FALSE (R_IsNA (nan)); // R compatibility
}

// integral types:

TEST (NA_sanity, typesafe_enum)
{
    E::enum_t const na1 = NA<E> ();
    E::enum_t const na2 = NA<E::enum_t> ();

    EXPECT_TRUE (is_NA (na1));
    EXPECT_TRUE (is_NA (na2));

    ASSERT_EQ (na1, na2);

    E::enum_t na3 = E::first;
    mark_NA (na3);

    ASSERT_EQ (na3, na1);
}

TEST (NA_sanity, plain_enum)
{
    NTE const na1 = NA<NTE> ();
    EXPECT_TRUE (is_NA (na1));

    NTE na2 = NTE::A;
    ASSERT_NE (na2, na1);

    mark_NA (na2);
    EXPECT_TRUE (is_NA (na2));
}
//............................................................................

TEST (maybe_NA, custom_repr)
{
    // default ('NA_NAME'):
    {
        std::stringstream out { };

        out << maybe_NA (NA<double> ());
        std::string const out_str = out.str ();

        EXPECT_EQ (out_str, NA_NAME_str);
    }
    // custom override:
    {
        std::stringstream out { };

        out << maybe_NA<fw_cstr ("'my NA'")> (NA<double> ());
        std::string const out_str = out.str ();

        EXPECT_EQ (out_str, "'my NA'");
    }
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
