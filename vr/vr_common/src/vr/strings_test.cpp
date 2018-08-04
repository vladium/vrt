
#include "vr/filesystem.h"
#include "vr/strings.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

TEST (quote_string, char_array)
{
    {
        std::array<char, 10> const a { { 'a', 'b', 'c' } }; // 'N' > 'size ()'
        std::string const a_qstr = quote_string<'"'> (a);
        EXPECT_EQ (a_qstr, "\"abc\"");
    }
    {
        std::array<char, 3> const a { { 'a', 'b', 'c' } }; // 'N' == 'size ()'
        std::string const a_qstr = quote_string<'"'> (a);
        EXPECT_EQ (a_qstr, "\"abc\"");
    }
    // defense against a null char in the data:
    {
        std::array<char, 10> const a { { 'a', 'b', 'c', 0, 'd' } };
        std::string const a_qstr = quote_string<'"'> (a);
        EXPECT_EQ (a_qstr, "\"abc\"");
    }
}
//............................................................................

TEST (string_cast, char_array)
{
    {
        std::array<char, 10> const a { { 'a', 'b', 'c' } }; // 'N' > 'size ()'
        std::string const a_str = string_cast (a);
        EXPECT_EQ (a_str, "abc");
    }
    {
        std::array<char, 3> const a { { 'a', 'b', 'c' } }; // 'N' == 'size ()'
        std::string const a_str = string_cast (a);
        EXPECT_EQ (a_str, "abc");
    }
    // defense against a null char in the data:
    {
        std::array<char, 10> const a { { 'a', 'b', 'c', 0, 'd' } };
        std::string const a_str = string_cast (a);
        EXPECT_EQ (a_str, "abc");
    }
}
//............................................................................

TEST (print, scalars)
{
    {
        int64_t x { -123456 };
        EXPECT_EQ ("-123456", print (x)); // value

        std::string const s = print (& x); // pointer
        std::stringstream os; os << (& x);
        EXPECT_EQ (os.str (), s);
    }

    // specializations:
    {
        std::string const s = print ('X');
        EXPECT_EQ ("'X'", s); // chars quoted
    }
    {
        std::string const s = print (std::string { "a string" });
        EXPECT_EQ ("\"a string\"", s); // strings quoted
    }
    {
        std::array<char, 20> a { { 'a', 'n', ' ', 'a', 'r', 'r', 'a', 'y' } };
        std::string const s = print (a);
        EXPECT_EQ ("\"an array\"", s); // char arrays quoted
    }
    {
        string_literal_t const c_str { "a C-string" };
        std::string const s = print (c_str);
        EXPECT_EQ ("\"a C-string\"", s); // C-strings quoted
    }
    {
        std::string const s = print ("a C-string literal");
        EXPECT_EQ ("\"a C-string literal\"", s); // string literals quoted
    }
}

/*
 * bytes are tricky due to possible 'char' type equivalencies:
 *
 */
using tested_byte_types     = gt::Types<int8_t, uint8_t>;

template<typename T> struct print_test: public gt::Test { };
TYPED_TEST_CASE (print_test, tested_byte_types);

TYPED_TEST (print_test, scalars_bytes)
{
    using type      = TypeParam; // test parameter

    type x { 1 };
    EXPECT_EQ ("1", print (x)); // numeric value, not a char image

    // byte pointers are unsafe to print (will be interpreted as 0-terminated strings),
    // so the safe usage paradigm is a void pointer cast:

    std::string const s = print (static_cast<void *> (& x));
    std::stringstream os; os << (static_cast<void *> (& x));
    EXPECT_EQ (os.str (), s);
}
//............................................................................

TEST (print, arrays)
{
    {
        int32_t a [] = { 1, 2, 3 };
        std::string const s = print (a);

        EXPECT_EQ (s, "{1, 2, 3}");
    }
    {
        int32_t const a [] = { 1, 2, 3 };
        std::string const s = print (a);

        EXPECT_EQ (s, "{1, 2, 3}");
    }
    {
        std::string const a [] = { "A", "B", "DDD" };
        std::string const s = print (a);

        EXPECT_EQ (s, "{\"A\", \"B\", \"DDD\"}");
    }
}
//............................................................................

TEST (print, containers)
{
    {
        string_vector const c; // empty
        std::string const s = print (c);

        EXPECT_EQ ("{}", s);
    }
    {
        std::vector<char> const c { 'A', 'B', 'C' };
        std::string const s = print (c);

        EXPECT_EQ ("{'A', 'B', 'C'}", s);
    }
    // nested containers:
    {
        std::vector<std::vector<char> > const c { { 'A', 'B', 'C' }, { 'D' } };
        std::string const s = print (c);

        EXPECT_EQ ("{{'A', 'B', 'C'}, {'D'}}", s);
    }

}
//............................................................................
/*
 * - check the infinite recursion guard
 * - check fs::path specialization
 */
TEST (print, fs_path)
{
    {
        fs::path const p { "foo" };
        std::string const s = print (p);
        EXPECT_EQ ("[foo]", s);
    }
    {
        fs::path p { "foo" };
        std::string const s = print (p);
        EXPECT_EQ ("[foo]", s);
    }
}
//............................................................................

TEST (join_as_name, sniff)
{
    {
        std::string const n = join_as_name ();
        EXPECT_EQ ("", n);
    }
    {
        std::string const n = join_as_name ("A");
        EXPECT_EQ ("A", n);
    }
    {
        std::string const n = join_as_name ("A", "B");
        EXPECT_EQ ("A.B", n);
    }
    {
        std::string const n = join_as_name ("A", "B", "C");
        EXPECT_EQ ("A.B.C", n);
    }

    // empty tokens detected:
    {
        std::string const n = join_as_name ("A", "", "C");
        EXPECT_EQ ("A.C", n);
    }
    {
        std::string const n = join_as_name ("", "", "C");
        EXPECT_EQ ("C", n);
    }
    {
        std::string const n = join_as_name ("", "B", "");
        EXPECT_EQ ("B", n);
    }
    {
        std::string const n = join_as_name ("A", "", "");
        EXPECT_EQ ("A", n);
    }
    {
        std::string const n = join_as_name ("", "", "");
        EXPECT_EQ ("", n);
    }

    // non-default separator and non-string values:
    {
        std::string const n = join_as_name<':'> ('A', 1234);
        EXPECT_EQ ("A:1234", n);
    }
}
//............................................................................

TEST (name_filter, set)
{
    name_filter const nf { set_name_filter { string_vector { "A", "B", "C", "A" } } };

    EXPECT_TRUE (nf ("A"));
    EXPECT_TRUE (nf ("B"));
    EXPECT_TRUE (nf ("C"));

    EXPECT_FALSE (nf ("D"));
    EXPECT_FALSE (nf ("AA"));
}

} // end of namespace
//----------------------------------------------------------------------------
