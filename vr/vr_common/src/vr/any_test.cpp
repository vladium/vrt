
#include "vr/any.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

any
return_any ()
{
    return { "some string" };
}

VR_ENUM (E, (A, B, C), printable, parsable);

class NC: noncopyable
{
}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

TEST (any, any_get)
{
    any a { 123L };
    any const & a_const = a;

    auto const v_const = any_get<int64_t> (a_const);
    ASSERT_EQ (v_const, 123L);

    auto v = any_get<int64_t> (a);
    ASSERT_EQ (v, 123L);

    // check our more informative exception throwing:

    EXPECT_THROW (any_get<std::string> (a_const), type_mismatch);
    EXPECT_THROW (any_get<char> (return_any ()), type_mismatch); // lvalue
}

TEST (any, any_get_numeric)
{
    {
        any const a { -123L };

        auto const i = any_get<int32_t> (a); // numeric cast ok
        ASSERT_EQ (i, -123);

        auto const us = any_get<int16_t> (a); // numeric cast ok
        ASSERT_EQ (us, static_cast<int16_t> (-123));

        EXPECT_THROW (any_get<uint64_t> (a), type_mismatch); // negative overflow
        EXPECT_THROW (any_get<uint16_t> (a), type_mismatch); // negative overflow
    }
}

TEST (any, any_get_enum)
{
    {
        any const a { E::B };

        auto const v = any_get<E> (a);
        ASSERT_EQ (v, E::B);
    }
    {
        any const a { E::name (E::C) };

        auto const v = any_get<E> (a);
        ASSERT_EQ (v, E::C);
    }
}

TEST (any, any_get_ref)
{
    std::string s { "a string" };
    std::string const & s_const_ref = s;

    // pass a copy of value:
    {
        any const a { s_const_ref }; // deep copy [note: 'a' is const, the only supported type for any_get]

        // get string value as const string ref:

        std::string const & v_const = any_get<std::string const &> (a);
        ASSERT_EQ (v_const, s);
        ASSERT_NE (& v_const, & s); // a copy of the orignal string has been stored in 'a'

        // getting string value as non-const string ref will still return const ref:
        // [note: this is a usability feature]

        std::string const & v_const2 = any_get<std::string &> (a);
        ASSERT_EQ (& v_const2, & v_const); // same value object, no intermediaries created
    }

    // pass a const ref to 's':
    {
        any const a { std::cref (s) }; // no copy, const ref [note: 'a' is still const]

        std::string const & v_const = any_get<std::string const &> (a);
        ASSERT_EQ (& v_const, & s); // obtained const ref to 's'

        // getting ref as non-const ref will still return const ref:
        // [note: this is a usability feature]

        std::string const & v_const2 = any_get<std::string &> (a);
        ASSERT_EQ (& v_const2, & v_const); // same value object, no intermediaries created
    }

    // pass a const ref to a manifestly non-copyable value:
    {
        NC nc { };

        any const a { std::cref (nc) };

        NC const & v_const = any_get<NC const &> (a);
        ASSERT_EQ (& v_const, & nc); // obtained const ref to 'nc'

        // again, any_get can omit 'const' for convenience:
        NC const & v_const2 = any_get<NC &> (a);
        ASSERT_EQ (& v_const2, & v_const); // same object ref
    }
}

} // end of namespace
//----------------------------------------------------------------------------
