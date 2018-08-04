
#include "vr/meta/objects.h"

#include "vr/fw_string.h"
#include "vr/util/classes.h"
#include "vr/util/logging.h"

#include "vr/test/utility.h"

#include <array>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................
//............................................................................

struct f0   { };
struct f1   { };
struct f2   { };
struct f3   { };
struct f4   { };
struct f5   { };

using f0_field  = field_def<int16_t, f0>; // a "re-usable" field def (basically, a pairing of type and name)

struct base_A
{
    int32_t m_i4;
    int64_t m_i8;

}; // end of class

VR_ENUM (fenum,
    (
        X,
        Y,
        Z
    ),
    printable, parsable

); // end of enum
//............................................................................
//............................................................................

vr_static_assert (std::is_empty<padding<0>>::value);
vr_static_assert (storage_size_of<padding<0>>::value == 0);

//............................................................................

TEST (compact_struct, construction)
{
    using schema        = make_schema_t
                        <
                            fdef_<f0_field>,
                            fdef_<std::string,  f1>, // a non-POD type ok to use in compact structs
                            fdef_<int32_t,      f2>,
                            fdef_<int64_t,      f3>,
                            fdef_<int16_t,      f4>
                        >;

    using struct_type   = make_compact_struct_t<schema>;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == sizeof (std::string) + 16); // compact due to field reordering

    vr_static_assert (has_field<f0, struct_type> ());
    vr_static_assert (has_field<f4, struct_type> ());

    struct_type s { };

    int16_t &       _f0 = field<f0> (s);
    std::string &   _f1 = field<f1> (s);
    int32_t &       _f2 = field<f2> (s);
    int64_t &       _f3 = field<f3> (s);
    int16_t &       _f4 = field<f4> (s);

    int32_t const f0_offset = (byte_ptr_cast (& _f0) - byte_ptr_cast (& s));
    int32_t const f1_offset = (byte_ptr_cast (& _f1) - byte_ptr_cast (& s));
    int32_t const f2_offset = (byte_ptr_cast (& _f2) - byte_ptr_cast (& s));
    int32_t const f3_offset = (byte_ptr_cast (& _f3) - byte_ptr_cast (& s));
    int32_t const f4_offset = (byte_ptr_cast (& _f4) - byte_ptr_cast (& s));


    LOG_trace1 << "f0 offset: " << f0_offset;
    LOG_trace1 << "f1 offset: " << f1_offset;
    LOG_trace1 << "f2 offset: " << f2_offset;
    LOG_trace1 << "f3 offset: " << f3_offset;
    LOG_trace1 << "f4 offset: " << f4_offset;

    // compact layout should be: f1, f3, f2, f0, f4

    constexpr int32_t std_string_size   = sizeof (std::string);

    ASSERT_EQ (f1_offset, 0);
    ASSERT_EQ (f3_offset, f1_offset + std_string_size);
    ASSERT_EQ (f2_offset, f3_offset + 8);
    ASSERT_EQ (f0_offset, f2_offset + 4);
    ASSERT_EQ (f4_offset, f0_offset + 2);

    ASSERT_EQ (f0_offset, (field_offset<f0, struct_type> ()));
    ASSERT_EQ (f1_offset, (field_offset<f1, struct_type> ()));
    ASSERT_EQ (f2_offset, (field_offset<f2, struct_type> ()));
    ASSERT_EQ (f3_offset, (field_offset<f3, struct_type> ()));
    ASSERT_EQ (f4_offset, (field_offset<f4, struct_type> ()));


    // mutate fields and read them back:

    EXPECT_TRUE (_f1.empty ());

    _f0 = std::numeric_limits<int16_t>::min ();
    _f2 = std::numeric_limits<int32_t>::max ();
    _f3 = std::numeric_limits<int64_t>::max ();
    _f4 = std::numeric_limits<int16_t>::max ();
    _f1 = "a string";

    EXPECT_EQ (_f1, "a string");
    EXPECT_EQ (_f0, std::numeric_limits<int16_t>::min ());
    EXPECT_EQ (_f2, std::numeric_limits<int32_t>::max ());
    EXPECT_EQ (_f3, std::numeric_limits<int64_t>::max ());
    EXPECT_EQ (_f4, std::numeric_limits<int16_t>::max ());
}
//............................................................................

TEST (compact_struct, elided_fields)
{
    // option 1: use 'make_compact_struct_t' with a vector of field defs with explicit field elision flags

    using schema        = make_schema_t
                        <
                            fdef_<f0_field>,                        // materialized by default
                            fdef_<std::string,  f1>,                // materialized by default
                            fdef_<int32_t,      f2, elide<>>,       // elided
                            fdef_<int64_t,      f3, elide<false>>   // materialized explicitly
                        >;

    using struct_type       = make_compact_struct_t<schema>;

    struct_type s;

    // option 2: use 'select_fields_t' with a (reusable) tag/type schema and a variadic list of fields chosen to be materialized:
    {
        using struct_type2  = make_compact_struct_t<select_fields_t<schema, f0, f1, f3>>;

        vr_static_assert (std::is_same<struct_type, struct_type2>::value);
    }

    // introspect 'struct_type':

    vr_static_assert (has_field<f0, struct_type> ());      // materialized field
    vr_static_assert (! (has_field<f2, struct_type> ()));    // elided field
    vr_static_assert (has_field<f3, struct_type> ());      // materialized field

    // same, but introspect type as inferred from 's' (currently requires using a macro):

    vr_static_assert (obj_has_field (f0, s));   // materialized field
    vr_static_assert (! obj_has_field(f2, s));  // elided field
    struct_type const & s_ref = s;
    vr_static_assert (obj_has_field (f0, s_ref));   // materialized field
    vr_static_assert (! obj_has_field (f2, s_ref)); // elided field

    // a field offset is always in [0, sizeof (struct)]:

    int32_t constexpr f0_offset = field_offset<f0, struct_type> ();
    int32_t constexpr f2_offset = field_offset<f2, struct_type> ();

    vr_static_assert ((0 <= f0_offset) && (f0_offset <= sizeof (s)));
    vr_static_assert ((0 <= f2_offset) && (f2_offset <= sizeof (s))); // the impl reserves the option of pushing elided fields to "appear" at the end of type with size 0

    int16_t & _f0 = field<f0> (s);  // always safe
    _f0 = 123;
    field<f0> (s) = 456;

    if (has_field<f2, struct_type> ()) // body compiles but never executes
    {
        int32_t & _f2 = field<f2> (s); // safe inside compile-time guard
        _f2 = 123;
        field<f2> (s) = 456;
    }
}
//............................................................................

TEST (compact_struct, typesafe_enum_fields)
{
    using schema        = make_schema_t
                        <
                            fdef_<fenum,    f0>,            // typesafe enum field
                            fdef_<int64_t,  f1>,
                            fdef_<fenum,    f2>,            // typesafe enum field
                            fdef_<fenum,    f3, elide<>>    // elided typesafe enum field
                        >;

    using struct_type   = make_compact_struct_t<schema>;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == 16); // compact due to field reordering

    vr_static_assert (has_field<f0, struct_type> ());
    vr_static_assert (has_field<f2, struct_type> ());
    vr_static_assert (! (has_field<f3, struct_type> ()));

    struct_type s;

    field<f0> (s) = fenum::Y;
    field<f1> (s) = -456;
    field<f2> (s) = fenum::Z;

    if (has_field<f3, struct_type> ()) // dead code elimination
    {
        field<f3> (s) = fenum::X; // safe inside compile-time guard
    }

    auto const & _f0 = field<f0> (s);
    auto const & _f1 = field<f1> (s);
    auto const & _f2 = field<f2> (s);

    EXPECT_EQ (_f0, fenum::Y);
    EXPECT_EQ (_f1, -456);
    EXPECT_EQ (_f2, fenum::Z);
}
//............................................................................

TEST (compact_struct, inheritance) // NOTE: in the latest impl, inheritance is no longer restricted to bases with the same layout
{
    using derived_schema        = make_schema_t
                                <
                                    fdef_<int32_t,  f2>,
                                    fdef_<int64_t,  f3>,
                                    fdef_<int16_t,  f4>
                                >;

    // deriving from a non-meta class:
    {
        using struct_type   = make_compact_struct_t<derived_schema, base_A>;

        // normal C++ inheritance relationship holds:

        vr_static_assert (std::is_base_of<base_A, struct_type>::value);

        vr_static_assert (is_meta_struct<struct_type>::value);

        vr_static_assert (has_field<f2, struct_type> ());
        vr_static_assert (has_field<f3, struct_type> ());
        vr_static_assert (has_field<f4, struct_type> ());

        // 'struct_type' field offsets account for 'base_A':

        vr_static_assert (field_offset<f3, struct_type> () >= sizeof (base_A));
        vr_static_assert (field_offset<f2, struct_type> () == 8 + field_offset<f3, struct_type> ());
        vr_static_assert (field_offset<f4, struct_type> () == 4 + field_offset<f2, struct_type> ());

        // mutate fields and read them back:

        struct_type s;

        s.m_i4 = std::numeric_limits<int32_t>::min ();
        s.m_i8 = std::numeric_limits<int64_t>::min ();
        field<f2> (s) = std::numeric_limits<int32_t>::max ();
        field<f3> (s) = std::numeric_limits<int64_t>::max ();
        field<f4> (s) = std::numeric_limits<int16_t>::max ();

        EXPECT_EQ (s.m_i4, std::numeric_limits<int32_t>::min ());
        EXPECT_EQ (s.m_i8, std::numeric_limits<int64_t>::min ());
        EXPECT_EQ (field<f2> (s), std::numeric_limits<int32_t>::max ());
        EXPECT_EQ (field<f3> (s), std::numeric_limits<int64_t>::max ());
        EXPECT_EQ (field<f4> (s), std::numeric_limits<int16_t>::max ());
    }

    // deriving from another meta class:
    {
        using base_schema       = make_schema_t
                                <
                                    fdef_<int16_t,      f0>,
                                    fdef_<std::string,  f1>
                                >;
        using base_struct_type  = make_compact_struct_t<base_schema>;

        using struct_type       = make_compact_struct_t<derived_schema, base_struct_type>;

        // normal C++ inheritance relationship holds:

        vr_static_assert (std::is_base_of<base_struct_type, struct_type>::value);

        vr_static_assert (is_meta_struct<struct_type>::value);

        vr_static_assert (has_field<f2, struct_type> ());
        vr_static_assert (has_field<f3, struct_type> ());
        vr_static_assert (has_field<f4, struct_type> ());

        vr_static_assert (has_field<f0, struct_type> ());
        vr_static_assert (has_field<f1, struct_type> ());

        // 'struct_type' field offsets account for 'base_A':
        // (note that we don't re-compactify base fields)

        vr_static_assert (field_offset<f1, struct_type> () == 0);
        vr_static_assert (field_offset<f0, struct_type> () == sizeof (std::string) + field_offset<f1, struct_type> ());

        vr_static_assert (field_offset<f3, struct_type> () >= sizeof (base_A));
        vr_static_assert (field_offset<f2, struct_type> () == 8 + field_offset<f3, struct_type> ());
        vr_static_assert (field_offset<f4, struct_type> () == 4 + field_offset<f2, struct_type> ());

        // mutate fields and read them back:

        struct_type s;

        field<f0> (s) = std::numeric_limits<int16_t>::min ();
        field<f1> (s) = "a string";

        field<f2> (s) = std::numeric_limits<int32_t>::max ();
        field<f3> (s) = std::numeric_limits<int64_t>::max ();
        field<f4> (s) = std::numeric_limits<int16_t>::max ();


        EXPECT_EQ (field<f0> (s), std::numeric_limits<int16_t>::min ());
        EXPECT_EQ (field<f1> (s), "a string");

        EXPECT_EQ (field<f2> (s), std::numeric_limits<int32_t>::max ());
        EXPECT_EQ (field<f3> (s), std::numeric_limits<int64_t>::max ());
        EXPECT_EQ (field<f4> (s), std::numeric_limits<int16_t>::max ());
    }
}
//............................................................................

TEST (packed_struct, construction)
{
    using schema        = make_schema_t
                        <
                            fdef_<f0_field>,
                            fdef_<fw_string8,   f1>, // a POD string type ok to use in overlay structs
                            fdef_<int32_t,      f2>,
                            fdef_<int64_t,      f3>,
                            fdef_<int16_t,      f4>
                        >;

    using struct_type   = make_packed_struct_t<schema>;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == 24); // sum of individual field sizes
    vr_static_assert (alignof (struct_type) == 1); // note: this might change in the future

    vr_static_assert (has_field<f0, struct_type> ());
    vr_static_assert (has_field<f4, struct_type> ());

    struct_type s;

    int16_t &       _f0 = field<f0> (s);
    fw_string8 &    _f1 = field<f1> (s);
    int32_t &       _f2 = field<f2> (s);
    int64_t &       _f3 = field<f3> (s);
    int16_t &       _f4 = field<f4> (s);

    int32_t const f0_offset = (byte_ptr_cast (& _f0) - byte_ptr_cast (& s));
    int32_t const f1_offset = (byte_ptr_cast (& _f1) - byte_ptr_cast (& s));
    int32_t const f2_offset = (byte_ptr_cast (& _f2) - byte_ptr_cast (& s));
    int32_t const f3_offset = (byte_ptr_cast (& _f3) - byte_ptr_cast (& s));
    int32_t const f4_offset = (byte_ptr_cast (& _f4) - byte_ptr_cast (& s));


    LOG_trace1 << "f0 offset: " << f0_offset;
    LOG_trace1 << "f1 offset: " << f1_offset;
    LOG_trace1 << "f2 offset: " << f2_offset;
    LOG_trace1 << "f3 offset: " << f3_offset;
    LOG_trace1 << "f4 offset: " << f4_offset;

    // overlay layout should be in declaration order: f0, f1, f2, f3, f4

    ASSERT_EQ (f0_offset, 0);
    ASSERT_EQ (f1_offset, 2);
    ASSERT_EQ (f2_offset, 10);
    ASSERT_EQ (f3_offset, 14);
    ASSERT_EQ (f4_offset, 22);

    ASSERT_EQ (f0_offset, (field_offset<f0, struct_type> ()));
    ASSERT_EQ (f1_offset, (field_offset<f1, struct_type> ()));
    ASSERT_EQ (f2_offset, (field_offset<f2, struct_type> ()));
    ASSERT_EQ (f3_offset, (field_offset<f3, struct_type> ()));
    ASSERT_EQ (f4_offset, (field_offset<f4, struct_type> ()));


    // mutate fields and read them back:

    _f0 = std::numeric_limits<int16_t>::min ();
    _f2 = std::numeric_limits<int32_t>::max ();
    _f3 = std::numeric_limits<int64_t>::max ();
    _f4 = std::numeric_limits<int16_t>::max ();
    _f1 = "a string";

    EXPECT_EQ (_f1, "a string");
    EXPECT_EQ (_f0, std::numeric_limits<int16_t>::min ());
    EXPECT_EQ (_f2, std::numeric_limits<int32_t>::max ());
    EXPECT_EQ (_f3, std::numeric_limits<int64_t>::max ());
    EXPECT_EQ (_f4, std::numeric_limits<int16_t>::max ());
}
//............................................................................

TEST (packed_struct, elided_fields)
{
    using schema        = make_schema_t
                        <
                            fdef_<f0_field>,                        // materialized by default
                            fdef_<fw_string8,   f1>,                // materialized by default
                            fdef_<int32_t,      f2, elide<>>,       // elided
                            fdef_<int64_t,      f3, elide<false>>   // materialized explicitly
                        >;

    using struct_type   = make_packed_struct_t<schema>;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == 18); // sum of individual (non-elided) field sizes
    vr_static_assert (alignof (struct_type) == 1); // note: this might change in the future

    struct_type s;

    // introspect 'struct_type':

    vr_static_assert (has_field<f0, struct_type> ());      // materialized field
    vr_static_assert (! (has_field<f2, struct_type> ()));    // elided field
    vr_static_assert (has_field<f3, struct_type> ());      // materialized field

    // same, but introspect type as inferred from 's' (currently requires using a macro):

    vr_static_assert (obj_has_field (f0, s));   // materialized field
    vr_static_assert (! obj_has_field (f2, s)); // elided field


    int32_t constexpr f0_offset = field_offset<f0, struct_type> ();
    int32_t constexpr f2_offset = field_offset<f2, struct_type> ();
    int32_t constexpr f3_offset = field_offset<f3, struct_type> ();

    // packed layout should be in declaration order: f0, f1, f2 (elided), f4

    vr_static_assert (f0_offset == 0);
    vr_static_assert ((0 <= f2_offset) && (f2_offset <= sizeof (s))); // the impl reserves the option of pushing elided fields to "appear" at the end of type with size 0
    vr_static_assert (f3_offset == sizeof (int16_t) + sizeof (fw_string8) + /* elided */0);

    int16_t & _f0 = field<f0> (s);  // always safe
    _f0 = 123;
    field<f0> (s) = 456;

    if (has_field<f2, struct_type> ()) // body compiles but never executes
    {
        int32_t & _f2 = field<f2> (s); // safe inside compile-time guard
        _f2 = 123;
        field<f2> (s) = 456;
    }
}
//............................................................................

TEST (packed_struct, typesafe_enum_fields)
{
    using schema        = make_schema_t
                        <
                            fdef_<fenum,    f0>,                // typesafe enum field
                            fdef_<int64_t,  f1>,
                            fdef_<fenum,    f2,     elide<>>    // elided typesafe enum field
                        >;

    using struct_type   = make_packed_struct_t<schema>;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == 12); // sum of field sizes

    vr_static_assert (has_field<f0, struct_type> ());
    vr_static_assert (! (has_field<f2, struct_type> ()));


    struct_type s;

    field<f0> (s) = fenum::Y;
    field<f1> (s) = -456;

    if (has_field<f2, struct_type> ()) // dead code elimination
    {
        field<f2> (s) = fenum::X; // safe inside compile-time guard
    }

    auto const & _f0 = field<f0> (s);
    auto const & _f1 = field<f1> (s);

    EXPECT_EQ (_f0, fenum::Y);
    EXPECT_EQ (_f1, -456);
}
//............................................................................

TEST (packed_struct, inheritance) // NOTE: in the latest impl, inheritance is no longer restricted to bases with the same layout
{
    using derived_schema    = make_schema_t
                            <
                                fdef_<int32_t, f2>,
                                fdef_<int64_t, f3>,
                                fdef_<int16_t, f4>
                            >;

    // deriving from a non-meta class:
    {
        using struct_type   = make_packed_struct_t<derived_schema, base_A>;

        // normal C++ inheritance relationship holds:

        vr_static_assert (std::is_base_of<base_A, struct_type>::value);

        vr_static_assert (is_meta_struct<struct_type>::value);

        vr_static_assert (has_field<f2, struct_type> ());
        vr_static_assert (has_field<f3, struct_type> ());
        vr_static_assert (has_field<f4, struct_type> ());

        // 'struct_type' field offsets account for 'base_A':

        vr_static_assert (field_offset<f2, struct_type> () == sizeof (base_A));
        vr_static_assert (field_offset<f3, struct_type> () == sizeof (base_A) + sizeof (int32_t));
        vr_static_assert (field_offset<f4, struct_type> () == sizeof (base_A) + sizeof (int32_t) + sizeof (int64_t));

        // mutate fields and read them back:

        struct_type s;

        s.m_i4 = std::numeric_limits<int32_t>::min ();
        s.m_i8 = std::numeric_limits<int64_t>::min ();
        field<f2> (s) = std::numeric_limits<int32_t>::max ();
        field<f3> (s) = std::numeric_limits<int64_t>::max ();
        field<f4> (s) = std::numeric_limits<int16_t>::max ();

        EXPECT_EQ (s.m_i4, std::numeric_limits<int32_t>::min ());
        EXPECT_EQ (s.m_i8, std::numeric_limits<int64_t>::min ());
        EXPECT_EQ (field<f2> (s), std::numeric_limits<int32_t>::max ());
        EXPECT_EQ (field<f3> (s), std::numeric_limits<int64_t>::max ());
        EXPECT_EQ (field<f4> (s), std::numeric_limits<int16_t>::max ());
    }

    // deriving from another meta class:
    {
        using base_schema   = make_schema_t
                            <
                                fdef_<int16_t,      f0>,
                                fdef_<fw_string8,   f1>
                            >;
        using base_struct_type  = make_packed_struct_t<base_schema>;

        using struct_type       = make_packed_struct_t<derived_schema, base_struct_type>;

        // normal C++ inheritance relationship holds:

        vr_static_assert (std::is_base_of<base_struct_type, struct_type>::value);

        vr_static_assert (is_meta_struct<struct_type>::value);

        vr_static_assert (has_field<f2, struct_type> ());
        vr_static_assert (has_field<f3, struct_type> ());
        vr_static_assert (has_field<f4, struct_type> ());

        vr_static_assert (has_field<f0, struct_type> ());
        vr_static_assert (has_field<f1, struct_type> ());

        // 'struct_type' field offsets account for 'base_A':

        vr_static_assert (field_offset<f0, struct_type> () == 0);
        vr_static_assert (field_offset<f1, struct_type> () == field_offset<f0, struct_type> () + sizeof (int16_t));

        vr_static_assert (field_offset<f2, struct_type> () == sizeof (base_struct_type));
        vr_static_assert (field_offset<f3, struct_type> () == sizeof (base_struct_type) + sizeof (int32_t));
        vr_static_assert (field_offset<f4, struct_type> () == sizeof (base_struct_type) + sizeof (int32_t) + sizeof (int64_t));

        // mutate fields and read them back:

        struct_type s;

        field<f0> (s) = std::numeric_limits<int16_t>::min ();
        field<f1> (s) = "a string";

        field<f2> (s) = std::numeric_limits<int32_t>::max ();
        field<f3> (s) = std::numeric_limits<int64_t>::max ();
        field<f4> (s) = std::numeric_limits<int16_t>::max ();


        EXPECT_EQ (field<f0> (s), std::numeric_limits<int16_t>::min ());
        EXPECT_EQ (field<f1> (s), "a string");

        EXPECT_EQ (field<f2> (s), std::numeric_limits<int32_t>::max ());
        EXPECT_EQ (field<f3> (s), std::numeric_limits<int64_t>::max ());
        EXPECT_EQ (field<f4> (s), std::numeric_limits<int16_t>::max ());
    }
}
//............................................................................
//............................................................................
namespace
{

struct sf0      { };
struct sf1      { };

struct SF
{
    int32_t m_fa;
    int32_t m_fb;

    // expose a synthetic field computed from 'm_fa' and 'm_fb':

    struct sf0_access
    {
        static constexpr auto get ()
        {
            return [](SF const & this_)
            {
                LOG_trace1 << "m_fa: " << this_.m_fa << ", m_fb: " << this_.m_fb;

                return ((static_cast<int64_t> (~ this_.m_fa) << 32) | unsigned_cast (this_.m_fb));
            };
        }
        static constexpr auto set ()
        {
            return [](SF & this_, int64_t const v)
            {
                LOG_trace1 << "v: " << v;

                this_.m_fa = ~ (v >> 32); this_.m_fb = v;
            };
        }

    }; // end of nested scope

    using _meta     = synthetic_meta_t
                    <
                        fdef_<sf0_access,   sf0>
                    >;

}; // end of class

} // end of 'anonymous'
//............................................................................
//............................................................................

TEST (synthetic_field_struct, construction)
{
    using struct_type       = SF;

    vr_static_assert (is_meta_struct<struct_type>::value);
    vr_static_assert (sizeof (struct_type) == 8);

    vr_static_assert (has_field<sf0, struct_type> ());
    vr_static_assert (! (has_field<sf1, struct_type> ()));

    struct_type s;

    int32_t constexpr sf0_offset = field_offset<f0, struct_type> ();
    vr_static_assert ((0 <= sf0_offset) && (sf0_offset <= sizeof (s)));

    // read/write 'sf0':

    field<sf0> (s) = std::numeric_limits<int64_t>::max ();
    ASSERT_EQ (field<sf0> (s), std::numeric_limits<int64_t>::max ());

    int64_t const v = field<sf0> (s);
    ASSERT_EQ (v, std::numeric_limits<int64_t>::max ());

    field<sf0> (s) = std::numeric_limits<int64_t>::min ();
    ASSERT_EQ (field<sf0> (s), std::numeric_limits<int64_t>::min ());

    if (has_field<sf1, struct_type> ()) // body compiles but never executes
    {
        int32_t _v = field<sf1> (s); // safe inside compile-time guard
        _v = 456;
        field<sf1> (s) = _v;
    }
}
//............................................................................
//............................................................................
namespace
{

template<typename S>
struct field_visitor final
{
    field_visitor (S const & s, std::ostream & os) :
        m_s { s },
        m_os { os }
    {
    }

    template<typename FIELD_DEF>
    void operator() (FIELD_DEF)
    {
        using tag           = typename FIELD_DEF::tag;

        std::string const n = print_tag<tag> ();
        auto const v = field<tag> (m_s);

        m_os << '<' << cn_<decltype (v)> () << ", " << n << "> = " << v << '\n';
    }

    S const & m_s;
    std::ostream & m_os;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
/*
 * field visitors see:
 *
 *  - all non-elided fields, including those of bases if any;
 *  - in declaration order, regardless of layout.
 */
using layouts   = gt::Types<layout::packed, layout::compact>;

template<typename T> struct meta_struct_test: gt::Test {};
TYPED_TEST_CASE (meta_struct_test, layouts);

TYPED_TEST (meta_struct_test, visitors) // @suppress("Class has a virtual method and non-virtual destructor")
{
    using layout_type   = TypeParam; // testcase parameter

    using base_schema       = make_schema_t
                            <
                                fdef_<int16_t,      f0>,
                                fdef_<fw_string8,   f1>
                            >;

    using derived_schema    = make_schema_t
                            <
                                fdef_<fenum,        f2>,
                                fdef_<int64_t,      f3>,
                                fdef_<int16_t,      f4,     elide<>>
                            >;

    using base_struct_type  = make_struct_t<layout_type, base_schema>;
    using struct_type       = make_struct_t<layout_type, derived_schema, base_struct_type>;


    struct_type s;

    field<f0> (s) = 777;
    field<f1> (s) = "fws8";
    field<f2> (s) = fenum::Y;
    field<f3> (s) = 333;

    std::stringstream ss { };
    {
        field_visitor<struct_type> v { s, ss };
        apply_visitor<struct_type> (v );
    }
    std::string const result = ss.str ();
    LOG_trace1 << "visit result string:\n" << result;

    auto const f0_i = result.find ("f0");
    auto const f1_i = result.find ("f1");
    auto const f2_i = result.find ("f2");
    auto const f3_i = result.find ("f3");
    auto const f4_i = result.find ("f4"); // should not be found

    ASSERT_NE (f0_i, std::string::npos); // non-elided base fields are visited
    ASSERT_NE (f1_i, std::string::npos);
    ASSERT_NE (f2_i, std::string::npos);
    ASSERT_NE (f3_i, std::string::npos);
    ASSERT_EQ (f4_i, std::string::npos); // elided fields are not visited

    // regardless of layout type, visits are in declaration order:

    EXPECT_LT (f0_i, f1_i);
    EXPECT_LT (f1_i, f2_i);
    EXPECT_LT (f2_i, f3_i);
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
