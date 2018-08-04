
#include "vr/util/parse.h"

#include "vr/io/streams.h"
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

template<typename T> struct parse_decimal_test: public gt::Test { };
TYPED_TEST_CASE (parse_decimal_test, tested_int_types);

//............................................................................

TYPED_TEST (parse_decimal_test, integral_types)
{
    using T         = TypeParam; // test parameter

    constexpr int32_t max_digits10      = 1 + std::numeric_limits<T>::digits10;

    char buf [32];

    // zero input:
    {
        buf [0] = '0';
        T const x = parse_decimal<T> (buf, 1);
        ASSERT_EQ (x, static_cast<T> (0));
    }

    {
        T x { 1 };

        // positive values:

        for (int32_t d = 1; d <= max_digits10; ++ d)
        {
            {
                io::array_ostream<> out { buf };
                out << x;
            }

            T const y = parse_decimal<T> (buf, d);
            ASSERT_EQ (y, x) << "failed for d = " << d;

            // 'parse_decimal_nonnegative()' will be equivalent but will not do a sign check at runtime:
            {
                T const y2 = parse_decimal_nonnegative<T> (buf, d);
                ASSERT_EQ (y2, x) << "failed for d = " << d;
            }

            x = 10 * x + (d + 1) % 10;
        }

        if (std::is_signed<T>::value) // compile-time guard
        {
            // negative values:

            x = -1;

            for (int32_t d = 1; d <= max_digits10; ++ d)
            {
                {
                    io::array_ostream<> out { buf };
                    out << x;
                }

                T const y = parse_decimal<T> (buf, d + /* sign */1);
                ASSERT_EQ (y, x) << "failed for d = " << d;

                x = 10 * x - (d + 1) % 10;
            }
        }
    }
}
//............................................................................

TYPED_TEST (parse_decimal_test, round_trip)
{
    using T         = TypeParam; // test parameter

    const int32_t repeats = 100000;

    char buf [32];

    T rnd = test::env::random_seed<T> ();

    for (int32_t repeat = 0; repeat < repeats; ++ repeat)
    {
        auto const sz = print_decimal (rnd, buf);
        T const x = parse_decimal<T> (buf, sz);

        ASSERT_EQ (x, rnd) << "[repeat " << repeat << "] failed for sz " << sz << ", image: [" << std::string { buf, static_cast<std::size_t> (sz) } << ']';

        test::next_random (rnd);
    }
}
//............................................................................

TEST (split, sniff)
{
    {
        string_vector const tokens = split ("A, B, C", ", ", /* keep_empty_tokens */false);
        ASSERT_EQ (signed_cast (tokens.size ()), 3);

        EXPECT_EQ ("A", tokens [0]);
        EXPECT_EQ ("B", tokens [1]);
        EXPECT_EQ ("C", tokens [2]);
    }
    {
        string_vector const tokens = split ("A, B, C", ", ", /* keep_empty_tokens */true);
        ASSERT_EQ (signed_cast (tokens.size ()), 5);

        EXPECT_EQ ("A", tokens [0]);
        EXPECT_EQ ("B", tokens [2]);
        EXPECT_EQ ("C", tokens [4]);

        EXPECT_TRUE (tokens [1].empty ());
        EXPECT_TRUE (tokens [3].empty ());
    }

    // some edge cases:

    {
        string_vector const tokens = split ("A", ", ");
        ASSERT_EQ (signed_cast (tokens.size ()), 1);

        EXPECT_EQ ("A", tokens [0]);
    }
    {
        string_vector const tokens = split ("", ", ");
        ASSERT_EQ (signed_cast (tokens.size ()), 1);

        EXPECT_EQ ("", tokens [0]);
    }
}
//............................................................................

TEST (ltrim, sniff)
{
    {
        std::array<char, 5> const a { ' ', ' ', ' ', 'a', 'b' };
        auto const at = ltrim (a, ' ');

        EXPECT_EQ (at, "ab");
    }
    {
        std::array<char, 5> const a { 'a', 'b', '_', 'c', 'd' }; // no delimiter in the array
        auto const at = ltrim (a, ' ');

        EXPECT_EQ (at, "ab_cd");
    }
    {
        std::array<char, 5> const a { ' ', ' ', ' ', ' ', ' ' };
        auto const at = ltrim (a, ' ');

        EXPECT_TRUE (at.empty ());
    }
}

TEST (rtrim, sniff)
{
    {
        std::array<char, 5> const a { 'a', 'b', ' ', ' ', ' ' };
        auto const at = rtrim (a, ' ');

        EXPECT_EQ (at, "ab");
    }
    {
        std::array<char, 5> const a { 'a', 'b', '_', 'c', 'd' }; // no delimiter in the array
        auto const at = rtrim (a, ' ');

        EXPECT_EQ (at, "ab_cd");
    }
    {
        std::array<char, 5> const a { ' ', ' ', ' ', ' ', ' ' };
        auto const at = rtrim (a, ' ');

        EXPECT_TRUE (at.empty ());
    }
}
//............................................................................

TEST (parse_int_list, sniff)
{
    std::vector<int64_t> const is = parse_int_list<int64_t> ("1, 2, 3,333");
    ASSERT_EQ (signed_cast (is.size ()), 4);

    EXPECT_EQ (is [0], 1);
    EXPECT_EQ (is [1], 2);
    EXPECT_EQ (is [2], 3);
    EXPECT_EQ (is [3], 333);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
