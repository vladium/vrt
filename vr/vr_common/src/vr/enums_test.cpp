
#include "vr/enums.h"

#include "vr/asserts.h"
#include "vr/util/logging.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{
// test is_typesafe_enum<>:

enum NTE { A, B, C }; // old-style enums are not our 'typesafe' enums
vr_static_assert (! is_typesafe_enum<NTE>::value);

vr_static_assert(! is_typesafe_enum<int32_t>::value); // not an enum

struct NE { }; // not an enum
vr_static_assert(! is_typesafe_enum<NE>::value);

struct WRONG_enum_t { using enum_t = int32_t; }; // guard against a wrong nested 'enum_t' type
vr_static_assert(! is_typesafe_enum<WRONG_enum_t>::value);

//............................................................................

VR_ENUM (E, (A, B, C), iterable, printable, parsable);
vr_static_assert (is_typesafe_enum<E>::value);
vr_static_assert (is_enum_iterable<E>::value);
vr_static_assert (is_enum_printable<E>::value);
vr_static_assert (is_enum_parsable<E>::value);
vr_static_assert (E::first == E::A);
vr_static_assert (E::size == E::C + 1);


// no-option enum:

VR_ENUM (Ebare,  (A, B, C));
vr_static_assert (is_typesafe_enum<Ebare>::value);
vr_static_assert (! is_enum_iterable<Ebare>::value);
vr_static_assert (! is_enum_printable<Ebare>::value);
vr_static_assert (! is_enum_parsable<Ebare>::value);
vr_static_assert (Ebare::first == Ebare::A);
vr_static_assert (Ebare::size == Ebare::C + 1);


// some single-option enums:

VR_ENUM (Eiter,  (A, B, C), iterable);
vr_static_assert (is_typesafe_enum<Eiter>::value);
vr_static_assert (is_enum_iterable<Eiter>::value);

VR_ENUM (Eprint, (A, B, C), printable);
vr_static_assert (is_typesafe_enum<Eprint>::value);
vr_static_assert (is_enum_printable<Eprint>::value);

VR_ENUM (Eparse, (A, B, C), parsable);
vr_static_assert (is_typesafe_enum<Eparse>::value);
vr_static_assert (is_enum_parsable<Eparse>::value);


// nest an enum inside custom scope:

struct Ecustom
{
    VR_NESTED_ENUM ((A, B, C));

}; // end of class
vr_static_assert (is_typesafe_enum<Ecustom>::value);

//............................................................................

VR_EXPLICIT_ENUM (XE, char, ('a', xA)('b', xB)('c', xC), range, printable, parsable);
vr_static_assert (is_typesafe_enum<XE>::value);
vr_static_assert (is_enum_printable<XE>::value);
vr_static_assert (is_enum_parsable<XE>::value);
vr_static_assert (XE::xA == 'a');
vr_static_assert (XE::xC == 'c');
vr_static_assert (std::get<0> (XE::enum_range ()) == XE::xA);
vr_static_assert (std::get<1> (XE::enum_range ()) == XE::xC);


// no-option enum:

VR_EXPLICIT_ENUM (XEbare, char, ('a', xA)('b', xB)('c', xC));
vr_static_assert (is_typesafe_enum<XEbare>::value);
vr_static_assert (! is_enum_printable<XEbare>::value);
vr_static_assert (! is_enum_parsable<XEbare>::value);
vr_static_assert (XE::xA == 'a');
vr_static_assert (XE::xC == 'c');


// some single-option enums:

VR_EXPLICIT_ENUM (XErange, char, ('a', xA)('b', xB)('c', xC), range);
vr_static_assert (is_typesafe_enum<XErange>::value);

VR_EXPLICIT_ENUM (XEprint, char, ('a', xA)('b', xB)('c', xC), printable);
vr_static_assert (is_typesafe_enum<XEprint>::value);
vr_static_assert (is_enum_printable<XEprint>::value);

VR_EXPLICIT_ENUM (XEparse, char, ('a', xA)('b', xB)('c', xC), parsable);
vr_static_assert (is_typesafe_enum<XEparse>::value);
vr_static_assert (is_enum_parsable<XEparse>::value);


// nest an enum inside custom scope:

struct XEcustom
{
    VR_NESTED_EXPLICIT_ENUM (char, ('a', xA)('b', xB)('c', xC));

}; // end of class
vr_static_assert (is_typesafe_enum<XEcustom>::value);


// TODO
// - test all option subsets and permutations

} // end of 'anonymous'
//............................................................................
//............................................................................

TEST (enums, iterable)
{
    // old style for-loops:
    {
        int32_t count { };
        for (E::enum_t e = E::first; e != E::size; ++ e)
        {
            ++ count;
        }

        EXPECT_EQ (signed_cast (E::size), count);
    }
    // range for-loop:
    {
        int32_t count { };
        for (E::enum_t e : E::values ())
        {
            ++ count;
            ASSERT_TRUE (E::first <= e && e < E::size) << "invalid enum value " << static_cast<int32_t> (e);
        }

        EXPECT_EQ (signed_cast (E::size), count);
    }
}

TEST (enums, printable)
{
    // note: don't require 'iterable' in this test

    // operator<<:
    {
        std::stringstream str;
        str << E::B;

        EXPECT_EQ ("B", str.str ());
    }
    // E::name():
    {
        std::stringstream str;
        str << E::name (E::B);

        EXPECT_EQ ("B", str.str ());
    }

    // 'na' is always present and is slightly special (upper case):
    {
        {
            std::stringstream str;
            str << E::na;

            EXPECT_EQ (str.str (), VR_ENUM_NA_NAME);
        }
        {
            std::stringstream str;
            str << E::name (E::na);

            EXPECT_EQ (str.str (), VR_ENUM_NA_NAME);
        }
    }

    // I/O guards against invalid values (e.g. read from serialized data):
    {
        std::stringstream str;

        EXPECT_NO_THROW (str << E::size);
        EXPECT_NO_THROW (str << static_cast<E::enum_t> (E::size + 10));
    }
}

TEST (enums, parsable)
{
    string_literal_t const e_names [] = { "A", "B", "C" }; // note: don't require 'printable' in this test

    for (int32_t v = 0; v != length (e_names); ++ v) // note: don't require 'iterable' in this test
    {
        string_literal_t const e_name = e_names [v];
        E::enum_t const e = static_cast<E::enum_t> (v);

        E::enum_t const ee = E::value (e_name); // 'string_literal_t' overload
        ASSERT_EQ (e, ee);

        std::string const e_name_str { e_name };

        E::enum_t const eee = E::value (e_name_str); // 'std::string' overload
        ASSERT_EQ (e, eee);
    }

    const std::string empty { };

    EXPECT_THROW (E::value ("invalid.enum.name"), invalid_input);
    EXPECT_THROW (E::value (""), invalid_input);
    EXPECT_THROW (E::value (empty), invalid_input);

    // NA support:

    E::enum_t const ee = E::value (VR_ENUM_NA_NAME); // 'string_literal_t' overload
    ASSERT_EQ (ee, E::na);

    std::string const name_str { VR_ENUM_NA_NAME }; // 'std::string' overload
    E::enum_t const eee = E::value (name_str);
    ASSERT_EQ (eee, ee);
}
//............................................................................

TEST (explicit_enums, range)
{
    auto constexpr range    = XErange::enum_range ();

    vr_static_assert (std::get<0> (range) <= XErange::xA && XErange::xA <= std::get<1> (range));
    vr_static_assert (std::get<0> (range) <= XErange::xB && XErange::xB <= std::get<1> (range));
    vr_static_assert (std::get<0> (range) <= XErange::xC && XErange::xC <= std::get<1> (range));
}

TEST (explicit_enums, printable)
{
    // operator<<:
    {
        std::stringstream str;
        str << XE::xB;

        EXPECT_EQ ("xB", str.str ());
    }
    // XE::name():
    {
        std::stringstream str;
        str << XE::name (XE::xB);

        EXPECT_EQ ("xB", str.str ());
    }

    // I/O guards against invalid values (e.g. read from serialized data):
    {
        std::stringstream str;

        EXPECT_NO_THROW (str << static_cast<XE::enum_t> (XE::xB + 10));
    }
}

TEST (explicit_enums, parsable)
{
    string_literal_t const e_names [] = { "xA", "xB", "xC" }; // note: don't require 'printable' in this test
    char const e_values [] = { 'a', 'b', 'c' };

    for (int32_t i = 0; i != length (e_names); ++ i) // note: don't require 'iterable' in this test
    {
        string_literal_t const e_name = e_names [i];
        XE::enum_t const e = static_cast<XE::enum_t> (e_values [i]);

        XE::enum_t const ee = XE::value (e_name); // 'string_literal_t' overload
        ASSERT_EQ (e, ee);

        std::string const e_name_str { e_name };

        XE::enum_t const eee = XE::value (e_name_str); // 'std::string' overload
        ASSERT_EQ (e, eee);
    }

    const std::string empty { };

    EXPECT_THROW (XE::value ("invalid.enum.name"), invalid_input);
    EXPECT_THROW (XE::value (""), invalid_input);
    EXPECT_THROW (XE::value (empty), invalid_input);
}
//............................................................................

TEST (enums, to_enum)
{
    string_literal_t const e_names [] = { "A", "B", "C" }; // note: don't require 'printable' in this test

    for (int32_t v = E::first; v != E::size; ++ v) // note: don't require 'iterable' in this test
    {
        string_literal_t const e_name = e_names [v];
        E::enum_t const e = static_cast<E::enum_t> (v);

        auto const ee = to_enum<E> (e_name); // 'string_literal_t' overload
        ASSERT_EQ (e, ee);

        std::string const e_name_str { e_name };

        auto const eee = to_enum<E> (e_name_str); // 'std::string' overload
        ASSERT_EQ (e, eee);
    }

    const std::string empty { };

    EXPECT_THROW (to_enum<E> ("invalid.enum.name"), invalid_input);
    EXPECT_THROW (to_enum<E> (""), invalid_input);
    EXPECT_THROW (to_enum<E> (empty), invalid_input);
}
//............................................................................

TEST (explicit_enums, to_enum)
{
    string_literal_t const e_names [] = { "xA", "xB", "xC" }; // note: don't require 'printable' in this test
    char const e_values [] = { 'a', 'b', 'c' };

    for (int32_t i = 0; i != length (e_names); ++ i) // note: don't require 'iterable' in this test
    {
        string_literal_t const e_name = e_names [i];
        XE::enum_t const e = static_cast<XE::enum_t> (e_values [i]);

        auto const ee = to_enum<XE> (e_name); // 'string_literal_t' overload
        ASSERT_EQ (e, ee);

        std::string const e_name_str { e_name };

        auto const eee = to_enum<XE> (e_name_str); // 'std::string' overload
        ASSERT_EQ (e, eee);
    }

    const std::string empty { };

    EXPECT_THROW (to_enum<XE> ("invalid.enum.name"), invalid_input);
    EXPECT_THROW (to_enum<XE> (""), invalid_input);
    EXPECT_THROW (to_enum<XE> (empty), invalid_input);
}

} // end of namespace
//----------------------------------------------------------------------------
