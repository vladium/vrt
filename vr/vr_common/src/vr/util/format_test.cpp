
#include "vr/util/format.h"

#include "vr/util/logging.h"

#include <algorithm>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

using tested_int_types      = gt::Types<int32_t, int64_t, uint32_t, uint64_t>;

template<typename T> struct print_decimal_test: public gt::Test { };
TYPED_TEST_CASE (print_decimal_test, tested_int_types);

//............................................................................

TYPED_TEST (print_decimal_test, integral_types)
{
    using T         = TypeParam; // test parameter

    constexpr int32_t max_digits10      = 1 + std::numeric_limits<T>::digits10;

    char buf [32] = { "****" };

    // zero input:
    {
        T x { };
        auto const len = print_decimal (x, buf);
        ASSERT_EQ (1, len);
        EXPECT_EQ ('0', buf [0]);
    }

    buf [0] = '*'; // reset/overwrite
    {
        T x { 1 };

        // positive values:

        for (int32_t d = 1; d <= max_digits10; ++ d)
        {
            buf [0] = '*'; // reset/overwrite

            auto const len = print_decimal (x, buf);
            ASSERT_EQ (d, len) << "failed for d = " << d;

            std::string const buf_str { buf, unsigned_cast (len) };
            LOG_trace1 << buf_str;
            EXPECT_EQ (string_cast (x), buf_str) << "failed for d = " << d;

            // 'print_decimal_nonnegative()' will be equivalent but will not do a sign check at runtime:

            {
                auto const len2 = print_decimal_nonnegative (x, buf);
                ASSERT_EQ (d, len2) << "failed for d = " << d;

                std::string const buf_str2 { buf, unsigned_cast (len) };
                EXPECT_EQ (buf_str, buf_str2) << "failed for d = " << d;
            }

            x = 10 * x + (d + 1) % 10;
        }

        if (std::is_signed<T>::value) // compile-time guard
        {
            // negative values:

            x = -1;

            for (int32_t d = 1; d <= max_digits10; ++ d)
            {
                buf [0] = '*'; // reset/overwrite

                auto const len = print_decimal (x, buf);
                ASSERT_EQ (1 + d, len) << "failed for d = " << d;

                std::string const buf_str { buf, unsigned_cast (len) };
                LOG_trace1 << buf_str;
                EXPECT_EQ (string_cast (x), buf_str);

                x = 10 * x - (d + 1) % 10;
            }
        }
    }
}
//............................................................................

TYPED_TEST (print_decimal_test, integral_types_rjust)
{
    using T         = TypeParam; // test parameter

    constexpr int32_t max_digits10      = 1 + std::numeric_limits<T>::digits10;

    char buf [32];
    std::fill (buf, buf + length (buf), '*');

    // zero input:
    {
        T x { };
        auto const len = rjust_print_decimal_nonnegative (x, buf, length (buf));
        ASSERT_EQ (1, len);
        EXPECT_EQ ('0', buf [length (buf) - 1]);
    }

    // positive values:
    {
        T x { 1 };

        for (int32_t d = 1; d <= max_digits10; ++ d)
        {
            std::fill (buf, buf + length (buf), '*');

            auto const len = rjust_print_decimal_nonnegative (x, buf, length (buf));
            ASSERT_EQ (d, len) << "failed for d = " << d;

            std::string const buf_str { buf + length (buf) - len, unsigned_cast (len) };
            LOG_trace1 << buf_str;
            EXPECT_EQ (string_cast (x), buf_str) << "failed for d = " << d;

            x = 10 * x + (d + 1) % 10;
        }
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
