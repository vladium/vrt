
#include "vr/util/ops_int.h"

#include "vr/data/NA.h"
#include "vr/util/logging.h"

#include "vr/test/utility.h"

#include <boost/math/special_functions/pow.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
/*
 * note: this nlz() implementation has a defined behavior for zero input
 */
TEST (ops_int_test, vr_nlz)
{
#define vr_nlz_TEST(r, type, test) \
    { \
        const type x_input = BOOST_PP_TUPLE_ELEM (2, 0, test); \
        const int32_t n_expected = BOOST_PP_TUPLE_ELEM (2, 1, test); \
        auto x = x_input; \
        const int32_t n = vr_nlz_##type (x); \
        EXPECT_EQ (n_expected, n) << "x: " << x; \
        ASSERT_EQ (x_input, x); /* catch ABI bugs */ \
    } \
    /* */

    // 32 bits:

#define vr_nlz_TESTCASES (0, 32)(1, 31)(3, 30)(4, 29)(-1, 0)

    BOOST_PP_SEQ_FOR_EACH (vr_nlz_TEST, int32_t, VR_DOUBLE_PARENTHESIZE_2 (vr_nlz_TESTCASES))

#undef vr_nlz_TESTCASES
#define vr_nlz_TESTCASES (0, 64)(1, 63)(3, 62)(4, 61)(-1, 0)

    // 64 bits:

    BOOST_PP_SEQ_FOR_EACH (vr_nlz_TEST, int64_t, VR_DOUBLE_PARENTHESIZE_2 (vr_nlz_TESTCASES))

#undef vr_nlz_TEST
}
//............................................................................

using supported_int_types   = gt::Types<int32_t, int64_t>;

template<typename T> struct ops_int_test: public gt::Test { };
TYPED_TEST_CASE (ops_int_test, supported_int_types);

//............................................................................
//............................................................................
namespace
{

template<typename T, int32_t ZERO_RETURN>
void ilog10_floor_test ()
{
    using value_type    = T;

    using zero_policy   = arg_policy<zero_arg_policy::return_special, ZERO_RETURN>;

    using ops_fast      = ops_int<zero_policy, false>;
    using ops_checked   = ops_int<zero_policy, true>;

    // input validation:

    EXPECT_NO_THROW (ops_fast::ilog10_floor (-2));
    EXPECT_THROW (ops_checked::ilog10_floor (-2), invalid_input);

    // test zero input:
    {
        value_type x = 0; // note: special value (return controlled by policy)
        const int32_t l = ops_checked::ilog10_floor (x);
        const int32_t l_expected = zero_policy::zero_return;
        ASSERT_EQ (l, l_expected) << "failed for zero input ";
    }

    {
        value_type x = 9;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, 0) << "x: " << x;
    }
    {
        value_type x = 10;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, 1) << "x: " << x;
    }
    {
        value_type x = 11;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, 1) << "x: " << x;
    }

    // test a large input:

    constexpr int32_t max_pow10     = std::numeric_limits<value_type>::digits10;
    const value_type x_big = boost::math::pow<max_pow10> (static_cast<value_type> (10));

    LOG_trace1 << "max pow10: " << max_pow10 << ", x_big: " << x_big;

    {
        value_type x = x_big - 1;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, max_pow10 - 1) << "x: " << x;
    }
    {
        value_type x = x_big;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, max_pow10) << "x: " << x;
    }
    {
        value_type x = x_big + 1;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, max_pow10) << "x: " << x;
    }

    // test max input:

    const value_type x_max = std::numeric_limits<value_type>::max ();

    LOG_trace1 << "x_max: " << x_max;

    {
        value_type x = x_max;
        const int32_t l = ops_checked::ilog10_floor (x);
        EXPECT_EQ (l, max_pow10) << "x: " << x;
    }
}

} // end of anonymous
//............................................................................
//............................................................................
// TODO
// - log2 tests

TYPED_TEST (ops_int_test, ilog10_floor_zero_0)
{
    using value_type    = TypeParam; // test parameter

    ilog10_floor_test<value_type, 0> ();
}

TYPED_TEST (ops_int_test, ilog10_floor_zero_m1)
{
    using value_type    = TypeParam; // test parameter

    ilog10_floor_test<value_type, -1> ();
}
//............................................................................

TYPED_TEST (ops_int_test, popcount)
{
    using value_type    = TypeParam; // test parameter

    value_type const zero       = 0;
    EXPECT_EQ (popcount (zero), 0);

    value_type const all_ones   = -1;
    EXPECT_EQ (popcount (all_ones), signed_cast (8 * sizeof (value_type)));
}
//............................................................................

TYPED_TEST (ops_int_test, ls_zero_index)
{
    using value_type    = TypeParam; // test parameter

    for (int32_t i = 0; i < signed_cast (sizeof (value_type)); ++ i)
    {
        value_type const x      = -1 & ~(static_cast<value_type> (0xFF) << (i * 8));
        int32_t const lszix = ls_zero_index (x);

        EXPECT_EQ (lszix, i);
    }

    // some edge cases:
    {
        // no zeros:
        {
            value_type const x  = -1;
            int32_t const lszix = ls_zero_index (x);

            EXPECT_EQ (lszix, signed_cast (sizeof (value_type)));
        }

        // several zeros:
        {
            value_type x        = 0; // all zeros
            int32_t lszix = ls_zero_index (x);

            EXPECT_EQ (lszix, 0);

            x                   = 0xFF00FF; // all but 2 zeros
            lszix = ls_zero_index (x);
            EXPECT_EQ (lszix, 1);
        }
    }
}
//............................................................................

TYPED_TEST (ops_int_test, net_int)
{
    using value_type    = TypeParam; // test parameter

    const value_type inputs [] { std::numeric_limits<value_type>::min (), -1000, -1, 0, 1, 1000, std::numeric_limits<value_type>::max () };

    {
        using nint_type     = net_int<value_type, false>;

        nint_type ni;

        for (value_type i : inputs)
        {
            ni = i;
            value_type const ii = ni;

            ASSERT_EQ (ii, i) << "failed for input " << i;

            // copy construction:
            {
                nint_type const ni_cc { ni };
                value_type const ii_cc = ni_cc;

                ASSERT_EQ (ii_cc, ii);
            }
            // copy assignment:
            {
                nint_type ni_ca; ni_ca = ni;
                value_type const ii_ca = ni_ca;

                ASSERT_EQ (ii_ca, ii);
            }
        }
    }
    {
        using nint_type     = net_int<value_type, true>;

        nint_type ni;

        for (value_type i : inputs)
        {
            ni = i;
            value_type const ii = ni;

            ASSERT_EQ (ii, i) << "failed for input " << i;

            // copy construction:
            {
                nint_type const ni_cc { ni };
                value_type const ii_cc = ni_cc;

                ASSERT_EQ (ii_cc, ii);
            }
            // copy assignment:
            {
                nint_type ni_ca; ni_ca = ni;
                value_type const ii_ca = ni_ca;

                ASSERT_EQ (ii_ca, ii);
            }
        }
    }
}
//............................................................................

TEST (ops_int_test, mux)
{
    EXPECT_TRUE (0          == mux (0));
    EXPECT_TRUE (  0x0102FF == mux (0, 1, 2, 255));
    EXPECT_TRUE (0x01020304 == mux (1, 2, 3, 4));
}
//............................................................................

TEST (bit_cast, fp)
{
    {
        auto const na = data::NA<float> ();
        LOG_trace1 << "bit image of float NA: " << hex_string_cast (bit_cast (na));

        auto const na_image = bit_cast (na);
        EXPECT_EQ (signed_cast (na_image & 0xFFFF), VR_R_NA_MARKER);
    }
    {
        auto const na = data::NA<double> ();
        LOG_trace1 << "bit image of double NA: " << hex_string_cast (bit_cast (na));

        auto const na_image = bit_cast (na);
        EXPECT_EQ (signed_cast (na_image & 0xFFFF), VR_R_NA_MARKER);
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
