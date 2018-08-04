
#include "vr/fw_string.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

char const str_2 [] = "ab";

char const str_4 [] = "abcd";
char const str_8 [] = "abcdefgh";

char const str_5 [] = "abcdQ";
char const str_9 [] = "abcdefghQ";

char const str_10 [] = "abcdefghQQ";

//............................................................................

using tested_fw_string_types    = gt::Types
<
    fw_string<4, false>,
    fw_string<4, true>,
    fw_string<8, false>,
    fw_string<8, true>
>;

template<typename T> struct fw_string_test: public gt::Test { };
TYPED_TEST_CASE (fw_string_test, tested_fw_string_types);

// TODO
// - mutation/concatenation (w/ bounds checking)
// - ops b/w different storage_type widths

} // end of anonymous
//............................................................................
//............................................................................

TYPED_TEST (fw_string_test, construction)
{
    using string_type           = TypeParam; // test parameter

    vr_static_assert (sizeof (string_type) == string_type::max_size ());

    // empty strings:
    {
        string_type const s0 { };
        string_type const s1 { "" };
        string_type const s2 { "abc", 0 };
        string_type const s3 { util::str_range { "abcd", 0 } };
        string_type const s4 { std::string { } };

        ASSERT_EQ (s0.size (), 0);
        ASSERT_EQ (s1.size (), 0);
        ASSERT_EQ (s2.size (), 0);
        ASSERT_EQ (s3.size (), 0);
        ASSERT_EQ (s4.size (), 0);

        EXPECT_EQ (s0, s1);
        EXPECT_EQ (s1, s2);
        EXPECT_EQ (s2, s3);
        EXPECT_EQ (s3, s4);
        EXPECT_EQ (s4, s0);

        // some bounds checking:
        {
            ASSERT_NO_THROW ((string_type { str_2 }));
            ASSERT_NO_THROW ((string_type { string_type::max_size () == 4 ? str_4 : str_8 }));

            ASSERT_NO_THROW ((string_type { str_10, string_type::max_size () }));
            ASSERT_NO_THROW ((string_type { util::str_range { str_10, string_type::max_size () } }));
            ASSERT_NO_THROW ((string_type { std::string { str_10, string_type::max_size () } }));

            if (string_type::bounds_checked ())
            {
                ASSERT_THROW ((string_type { string_type::max_size () == 4 ? str_5 : str_9 }), out_of_bounds);
                ASSERT_THROW ((string_type { str_10 }), out_of_bounds);

                ASSERT_THROW ((string_type { "012345678", string_type::max_size () + 1 }), out_of_bounds);
                ASSERT_THROW ((string_type { util::str_range { "012345678", string_type::max_size () + 1 } }), out_of_bounds);
                ASSERT_THROW ((string_type { std::string { "012345678" } }), out_of_bounds);

                ASSERT_THROW (s0.at (1), out_of_bounds);
            }
        }

        // mixed lhs/rhs equality:

        EXPECT_EQ (s0, "");
        EXPECT_EQ ("", s0);
        EXPECT_EQ (s0, (util::str_range { "", 0 }));
        EXPECT_EQ ((util::str_range { "", 0 }), s0);
    }
    // size 1 strings:
    {
        string_type const s0 { "a" };
        string_type const s1 { "abc", 1 };
        string_type const s2 { util::str_range { "abcd", 1 } };
        string_type const s3 { std::string { "a" } };

        ASSERT_EQ (s0.size (), 1);
        ASSERT_EQ (s1.size (), 1);
        ASSERT_EQ (s2.size (), 1);
        ASSERT_EQ (s3.size (), 1);

        EXPECT_EQ (s0, s1);
        EXPECT_EQ (s1, s2);
        EXPECT_EQ (s2, s3);

        EXPECT_EQ (hash_value (s0), hash_value (s1));
        EXPECT_EQ (hash_value (s1), hash_value (s2));
        EXPECT_EQ (hash_value (s2), hash_value (s3));

        // mixed lhs/rhs equality:

        EXPECT_EQ (s0, "a");
        EXPECT_EQ ("a", s0);
        EXPECT_EQ (s0, (util::str_range { "a", 1 }));
        EXPECT_EQ ((util::str_range { "a", 1 }), s0);

#   define vr_CHECK_S(z, i, unused) \
        { \
            string_type const & s = VR_CAT (s, i); \
            \
            EXPECT_EQ (s.data ()[0], 'a'); \
            EXPECT_EQ (s.at (0), 'a'); \
            EXPECT_EQ (s [0], 'a'); \
            std::string img = s.to_string (); \
            EXPECT_EQ (img, "a"); \
            img = string_cast (s); \
            EXPECT_EQ (img, "a"); \
            img = print (s); \
            EXPECT_EQ (img, "\"a\""); \
        } \
        /* */

        BOOST_PP_REPEAT (4, vr_CHECK_S, unused)

#   undef vr_CHECK_S

        string_type sb { "b" };
        ASSERT_NE (sb, s1);
        sb [0] = s0 [0];
        EXPECT_EQ (sb, s1);
    }

    // size 3 strings:
    {
        string_type const s0 { "abc" };
        string_type const s1 { "abc", 3 };
        string_type const s2 { util::str_range { "abcd", 3 } };
        string_type const s3 { std::string { "abc" } };

        ASSERT_EQ (s0.size (), 3);
        ASSERT_EQ (s1.size (), 3);
        ASSERT_EQ (s2.size (), 3);
        ASSERT_EQ (s3.size (), 3);

        EXPECT_EQ (s0, s1);
        EXPECT_EQ (s1, s2);
        EXPECT_EQ (s2, s3);

        EXPECT_EQ (hash_value (s0), hash_value (s1));
        EXPECT_EQ (hash_value (s1), hash_value (s2));
        EXPECT_EQ (hash_value (s2), hash_value (s3));

        // mixed lhs/rhs equality:

        EXPECT_EQ (s0, "abc");
        EXPECT_EQ ("abc", s0);
        EXPECT_EQ (s0, (util::str_range { "abc", 3 }));
        EXPECT_EQ ((util::str_range { "abc", 3 }), s0);

#   define vr_CHECK_S(z, i, unused) \
        { \
            string_type const & s = VR_CAT (s, i); \
            \
            EXPECT_EQ (s.data ()[0], 'a'); \
            EXPECT_EQ (s.data ()[1], 'b'); \
            EXPECT_EQ (s.data ()[2], 'c'); \
            EXPECT_EQ (s.at (0), 'a'); \
            EXPECT_EQ (s.at (1), 'b'); \
            EXPECT_EQ (s.at (2), 'c'); \
            EXPECT_EQ (s [0], 'a'); \
            EXPECT_EQ (s [1], 'b'); \
            EXPECT_EQ (s [2], 'c'); \
            std::string img = s.to_string (); \
            EXPECT_EQ (img, "abc"); \
            img = string_cast (s); \
            EXPECT_EQ (img, "abc"); \
            img = print (s); \
            EXPECT_EQ (img, "\"abc\""); \
        } \
        /* */

        BOOST_PP_REPEAT (4, vr_CHECK_S, unused)

#   undef vr_CHECK_S

        string_type ss { "xyz" };
        ASSERT_NE (ss, s1);
        ss [0] = s0 [0];
        ss [1] = s0 [1];
        ss [2] = s0 [2];
        EXPECT_EQ (ss, s1);

        // copy construction:
        {
            string_type const s0_copy { s0 };

            EXPECT_EQ (s0, s0_copy);
        }
        // copy assignment:
        {
            string_type s0_copy { }; s0_copy = s0;

            EXPECT_EQ (s0, s0_copy);
        }
        // move construction:
        {
            string_type s0_copy { s0 };
            string_type s0_copy_mcopy { std::move (s0) };

            EXPECT_EQ (s0, s0_copy_mcopy);
        }
        // move assignment:
        {
            string_type s0_copy { s0 };
            string_type s0_copy_mcopy { }; s0_copy_mcopy = std::move (s0);

            EXPECT_EQ (s0, s0_copy_mcopy);
        }
    }
}
//............................................................................

TYPED_TEST (fw_string_test, comparison)
{
    using string_type           = TypeParam; // test parameter

    vr_static_assert (sizeof (string_type) == string_type::max_size ());

    int32_t const max_size = string_type::max_size ();

    char lhs_buf [max_size + 1];
    char rhs_buf [max_size + 1];

    uint64_t rnd = test::env::random_seed<uint64_t> (); // note: unsigned
    int64_t const repeats = 100000;

    for (int64_t r = 0; r < repeats; ++ r)
    {
        int32_t const lhs_len = test::next_random (rnd) % max_size;
        int32_t const rhs_len = test::next_random (rnd) % max_size;
        {
            for (int32_t i = 0; i < lhs_len; ++ i) lhs_buf [i] = 1 + test::next_random (rnd) % 127; // positive chars
            lhs_buf [lhs_len] = '\0';
            for (int32_t i = 0; i < rhs_len; ++ i) rhs_buf [i] = 1 + test::next_random (rnd) % 127; // positive chars
            rhs_buf [rhs_len] = '\0';
        }

        string_type const lhs { lhs_buf, lhs_len };
        string_type const rhs { rhs_buf, rhs_len };

        bool const c = (lhs < rhs);
        bool const c_std = (std::strcmp (lhs_buf, rhs_buf) < 0);

        ASSERT_EQ (c, c_std) << "failed for lhs '" << lhs_buf << "', rhs '" << rhs_buf << '\'';

        // make sure no lhs/rhs state mutation:

        ASSERT_EQ (std::strncmp (lhs.data (), lhs_buf, lhs_len), 0);
        ASSERT_EQ (std::strncmp (rhs.data (), rhs_buf, rhs_len), 0);
    }
}

TYPED_TEST (fw_string_test, mapping)
{
    using string_type           = TypeParam; // test parameter

    using set                   = std::set<string_type>;
    using hashset               = boost::unordered_set<string_type>;

    using check_set             = std::set<std::string>;
    using check_hashset         = boost::unordered_set<std::string>;


    int32_t const max_size = string_type::max_size ();

    char fws_buf [max_size + 1];

    uint64_t rnd = test::env::random_seed<uint64_t> (); // note: unsigned
    int64_t const repeats = 100000;

    set fws_s;
    hashset fws_h;

    check_set chk_s;
    check_hashset chk_h;

    for (int64_t r = 0; r < repeats; ++ r)
    {
        int32_t const fws_len = test::next_random (rnd) % max_size;
        {
            for (int32_t i = 0; i < fws_len; ++ i) fws_buf [i] = 1 + test::next_random (rnd) % 127; // positive chars
            fws_buf [fws_len] = '\0';
        }

        string_type const fws { fws_buf };
        std::string const s   { fws_buf };

        ASSERT_EQ (fws.size (), fws_len);
        ASSERT_EQ (signed_cast (s.size ()), fws_len);

        bool const fws_s_inserted   = fws_s.insert (fws).second;
        bool const chk_s_inserted   = chk_s.insert (s).second;

        ASSERT_EQ (fws_s_inserted, chk_s_inserted) << "mapping failed for string content " << print (s);

        bool const fws_h_inserted   = fws_h.insert (fws).second;
        bool const chk_h_inserted   = chk_h.insert (s).second;

        ASSERT_EQ (fws_h_inserted, chk_h_inserted) << "hashing failed for string content " << print (s);
    }

    EXPECT_EQ (fws_h.size (), chk_h.size ());
    ASSERT_EQ (fws_s.size (), chk_s.size ());

    // validate that ordered sets contain the same strings and in the same order:

    auto fws_s_i = fws_s.begin ();
    for (std::string const & s : chk_s)
    {
        ASSERT_TRUE (fws_s_i != fws_s.end ());

        ASSERT_EQ (s, fws_s_i->to_string ());

        ++ fws_s_i;
    }
}

} // end of namespace
//----------------------------------------------------------------------------
