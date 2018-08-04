
#include "vr/meta/structs.h"

#include "vr/util/logging.h"
#include "vr/util/ops_int.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................
//............................................................................
namespace test
{

VR_META_TAG (f0);
VR_META_TAG (f1);
VR_META_TAG (f2);
VR_META_TAG (f3);
VR_META_TAG (f4);
VR_META_TAG (f5);

//............................................................................

VR_META_PACKED_STRUCT
(A,
    (char,          f0)
    (int64_t,       f1)
    (fw_string4,    f2)

); // end of class

// static tests:

vr_static_assert (sizeof (A) == sizeof (char) + sizeof (int64_t) + sizeof (fw_string4));
vr_static_assert (util::has_print<A>::value);
vr_static_assert (util::is_streamable<A, std::ostream>::value);

//............................................................................

VR_META_TAG (fA);

VR_META_PACKED_STRUCT
(B,
    (A,             fA) // aggregate an 'A' subobject
    (int32_t,       f3)

); // end of class

// static tests:

vr_static_assert (sizeof (B) == sizeof (A) + sizeof (int32_t));
vr_static_assert (util::has_print<B>::value);
vr_static_assert (util::is_streamable<B, std::ostream>::value);


VR_META_PACKED_STRUCT
(BD, A, // derive from 'A'
    (int32_t,       f3)

); // end of class

// static tests:

vr_static_assert (sizeof (BD) == sizeof (A) + sizeof (int32_t));
vr_static_assert (util::has_print<BD>::value);
vr_static_assert (util::is_streamable<BD, std::ostream>::value);

VR_META_PACKED_STRUCT
(BDD, BD, // derive from 'BD' (2nd level of inheritance)
    (int64_t,       f4)

); // end of class

// static tests:

vr_static_assert (util::has_print<BDD>::value);
vr_static_assert (util::is_streamable<BDD, std::ostream>::value);

//............................................................................

using be_int32_t    = util::net_int<int32_t, false>; // big-endian
using be_int64_t    = util::net_int<int64_t, false>; // big-endian

VR_META_PACKED_STRUCT
(C,
    (be_int32_t,    f4)
    (be_int64_t,    f5)

); // end of class

// static tests:

vr_static_assert (sizeof (C) == sizeof (int32_t) + sizeof (int64_t));
vr_static_assert (util::has_print<C>::value);
vr_static_assert (util::is_streamable<C, std::ostream>::value);

} // end of 'test'
//............................................................................
//............................................................................
// TODO
// - copy/assign semantics
// - equality, hashable

TEST (structs, construct)
{
    using namespace test;

    A a;

    // validate two different styles of field access:
    {
        auto const & f0_a = a.f0 ();         // "human-friendly" field access
        auto const & f0_f = field<_f0_> (a); // metaprogramming-friendly field access

        ASSERT_EQ (& f0_a, & f0_f);

        auto const & f1_a = a.f1 ();
        auto const & f1_f = field<_f1_> (a);

        ASSERT_EQ (& f1_a, & f1_f);
    }

    // set 'a' to a known state:
    {
        a.f0 () = 'F';
        a.f1 () = std::numeric_limits<std::decay_t<decltype (a.f1 ())>>::max ();
        a.f2 () = "@XYZ";
    }

    // validate basic printable/streamable expectations:

    std::string const a_str = print (a);

    EXPECT_TRUE (a_str.find ("f0") != std::string::npos);
    EXPECT_TRUE (a_str.find ("f1") != std::string::npos);
    EXPECT_TRUE (a_str.find ("f2") != std::string::npos);

    LOG_trace1 << a;
}
//............................................................................

TEST (structs, aggregate)
{
    using namespace test;

    B b;

    {
        auto const & fA_a = b.fA ();
        auto const & fA_f = field<_fA_> (b);

        ASSERT_EQ (& fA_a, & fA_f);

        auto const & f3_a = b.f3 ();
        auto const & f3_f = field<_f3_> (b);

        ASSERT_EQ (& f3_a, & f3_f);
    }

    // set 'b' to a known state:
    {
        b.fA ().f0 () = 'F';
        b.fA ().f1 () = std::numeric_limits<std::decay_t<decltype (b.fA ().f1 ())>>::max ();
        b.fA ().f2 () = "@XYZ";
        b.f3 () = std::numeric_limits<std::decay_t<decltype (b.f3 ())>>::max ();
    }

    std::string const b_str = print (b);

    EXPECT_TRUE (b_str.find ("f0") != std::string::npos);
    EXPECT_TRUE (b_str.find ("f3") != std::string::npos);

    LOG_trace1 << b;
}
//............................................................................

TEST (structs, inherit) // also ASX-21
{
    using namespace test;

    BDD b;

    {
        A const & A_b = b;
        ASSERT_EQ (& A_b, & b);
    }
    {
        BD const & BD_b = b;
        ASSERT_EQ (& BD_b, & b);
    }

    // set 'b' to a known state:
    {
        b.f0 () = 'F';
        b.f1 () = std::numeric_limits<std::decay_t<decltype (b.f1 ())>>::max ();
        b.f2 () = "@XYZ";
        b.f3 () = std::numeric_limits<std::decay_t<decltype (b.f3 ())>>::max ();
        b.f4 () = -1;
    }

    std::string const b_str = print (b);

    EXPECT_TRUE (b_str.find ("f0") != std::string::npos); // ASX-25
    EXPECT_TRUE (b_str.find ("f3") != std::string::npos); // ASX-25
    EXPECT_TRUE (b_str.find ("f4") != std::string::npos);

    LOG_trace1 << b;
}
//............................................................................

TEST (structs, types_with_user_conversion)
{
    using namespace test;

    C c;

    // set 'c' to a known state:
    {
        c.f4 () = 10000;
        c.f5 () = std::numeric_limits<int64_t>::max ();
    }
    // validate 'c' state:
    {
        EXPECT_EQ (c.f4 (), 10000);
        EXPECT_EQ (c.f5 (), std::numeric_limits<int64_t>::max ());
    }

    std::string const c_str = print (c);

    EXPECT_TRUE (c_str.find ("f4") != std::string::npos);
    EXPECT_TRUE (c_str.find ("f5") != std::string::npos);

    LOG_trace1 << c;
}

} // end of meta
} // end of namespace
//----------------------------------------------------------------------------
