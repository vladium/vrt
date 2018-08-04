
#include "vr/data/attributes.h"

#include <boost/algorithm/string.hpp>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

TEST (attr_type, construction_numeric)
{
    attr_type const at1 { atype::i4 };
    attr_type const at2 { atype::i4 };

    EXPECT_EQ (atype::i4, at1.atype ());
    EXPECT_EQ (atype::i4, at2.atype ());

    EXPECT_EQ (at1, at2);
    EXPECT_EQ (hash_value (at1), hash_value (at2));

    attr_type const at3 { atype::f8 };

    EXPECT_EQ (atype::f8, at3.atype ());

    EXPECT_NE (at1, at3);
    EXPECT_NE (at2, at3);

    // copy construction:
    {
        attr_type const at3c { at3 };
        EXPECT_EQ (at3, at3c);
    }
    // move construction:
    {
        attr_type at { atype::timestamp };
        attr_type const atc { std::move (at) };

        EXPECT_EQ (atype::timestamp, atc.atype ());
    }

    // copy assignment:
    {
        attr_type at3c { atype::i4 };
        at3c = at3;
        EXPECT_EQ (at3, at3c);
    }
    // move assignment:
    {
        attr_type at { atype::timestamp };
        attr_type atc { atype::i8 };
        atc = std::move (at);

        EXPECT_EQ (atype::timestamp, atc.atype ());
    }


    // invalid input:

    EXPECT_THROW (attr_type { atype::category }, invalid_input);

    // invalid access:

    EXPECT_THROW (at1.labels<true> (), type_mismatch);
}

TEST (attr_type, construction_categorical)
{
    attr_type const at1 { label_array::create_and_intern ({ "X", "Y", "Z" }) };
    label_array const * const at1_la_addr = & at1.labels (); // capture for later stability check
    attr_type const at2 { at1.labels () };

    EXPECT_EQ (atype::category, at1.atype ());
    EXPECT_EQ (atype::category, at2.atype ());

    ASSERT_EQ ("X", at1.labels ()[0]);
    ASSERT_EQ ("Y", at1.labels ()[1]);
    ASSERT_EQ ("Z", at1.labels ()[2]);

    ASSERT_EQ (& at1.labels (), & at2.labels ());

    EXPECT_EQ (at1, at2);
    EXPECT_EQ (hash_value (at1), hash_value (at2));

    attr_type const at3 { label_array::create_and_intern ({ "X", "Y" }) };

    EXPECT_EQ (atype::category, at3.atype ());

    EXPECT_NE (at1, at3);
    EXPECT_NE (at2, at3);

    // copy construction:
    {
        attr_type const at3c { at3 };

        EXPECT_EQ (at3, at3c);
        EXPECT_EQ ("X", at3c.labels ()[0]);
        EXPECT_EQ ("Y", at3c.labels ()[1]);
    }
    // move construction:
    {
        attr_type at { label_array::create_and_intern ({ "L0", "L1" }) };
        attr_type const atc { std::move (at) };

        EXPECT_EQ (atype::category, atc.atype ());
        EXPECT_EQ ("L0", atc.labels ()[0]);
        EXPECT_EQ ("L1", atc.labels ()[1]);
    }

    // copy assignment:
    {
        attr_type at3c { atype::i4 };
        at3c = at3;

        EXPECT_EQ (at3, at3c);
        EXPECT_EQ ("X", at3c.labels ()[0]);
        EXPECT_EQ ("Y", at3c.labels ()[1]);
    }
    // move assignment
    {
        attr_type at { label_array::create_and_intern ({ "L0", "L1" }) };
        attr_type atc { atype::i8 };
        atc = std::move (at);

        EXPECT_EQ (atype::category, atc.atype ());
        EXPECT_EQ ("L0", atc.labels ()[0]);
        EXPECT_EQ ("L1", atc.labels ()[1]);
    }


    label_array const * const la_addr = & label_array::create_and_intern ({ "X", "Y", "Z" });
    ASSERT_EQ (at1_la_addr, la_addr); // stable addr

    EXPECT_EQ (at1_la_addr, & at1.labels ());
    EXPECT_EQ (at1_la_addr, & at2.labels ());
}


TEST (attr_type, construction_typesafe_enum)
{
    attr_type const at1 { attr_type::for_enum<atype> () };
    attr_type const at2 { attr_type::for_enum<atype> () };

    EXPECT_EQ (atype::category, at1.atype ());
    EXPECT_EQ (atype::category, at2.atype ());

    ASSERT_EQ ("category", at1.labels ()[0]);
    ASSERT_EQ ("timestamp", at1.labels ()[1]);

    ASSERT_EQ (& at1.labels (), & at2.labels ());

    EXPECT_EQ (at1, at2);
    EXPECT_EQ (hash_value (at1), hash_value (at2));
}
//............................................................................

TEST (attribute, construction)
{
    attribute const a1 { "a", atype::i4 };
    attribute const a2 { "a", atype::i4 };

    EXPECT_EQ (atype::i4, a1.type ().atype ());
    EXPECT_EQ (atype::i4, a2.type ().atype ());

    EXPECT_EQ ("a", a1.name ());
    EXPECT_EQ ("a", a2.name ());

    EXPECT_EQ (a1, a2);
    EXPECT_EQ (hash_value (a1), hash_value (a2));

    attribute const a3 { "a", label_array::create_and_intern ({ "x", "y" }) }; // change atype
    attribute const a4 { "b", atype::i4 }; // change name

    EXPECT_NE (a1, a3);
    EXPECT_NE (a1, a4);

    // copy construction:
    {
        attribute const a3c { a3 };

        EXPECT_EQ (a3, a3c);
        EXPECT_EQ ("x", a3c.type ().labels ()[0]);
        EXPECT_EQ ("y", a3c.type ().labels ()[1]);
    }
    // move construction:
    {
        attribute a { "c", label_array::create_and_intern ({ "l0", "l1" }) };
        attribute const ac { std::move (a) };

        EXPECT_EQ ("c", ac.name ());
        EXPECT_EQ (atype::category, ac.type ().atype ());
        EXPECT_EQ ("l0", ac.type ().labels ()[0]);
        EXPECT_EQ ("l1", ac.type ().labels ()[1]);
    }

    // copy assignment:
    {
        attribute a3c { "asdfasdf", atype::i4 };
        a3c = a3;

        EXPECT_EQ (a3, a3c);
        EXPECT_EQ ("a", a3c.name ());
        EXPECT_EQ ("x", a3c.type ().labels ()[0]);
        EXPECT_EQ ("y", a3c.type ().labels ()[1]);
    }
    // move assignment
    {
        attribute a { "d", label_array::create_and_intern ({ "l0", "l1" }) };
        attribute ac { "ac", atype::i8 };
        ac = std::move (a);

        EXPECT_EQ ("d", ac.name ());
        EXPECT_EQ (atype::category, ac.type ().atype ());
        EXPECT_EQ ("l0", ac.type ().labels ()[0]);
        EXPECT_EQ ("l1", ac.type ().labels ()[1]);
    }
}
//............................................................................

TEST (attr_schema, construction)
{
    std::vector<attribute> const attrs { { "a0", atype::i4 }, { "a1", label_array::create_and_intern ({ "X", "Y" }) } };
    attr_schema const as1 { attrs };
    attr_schema const as2 { std::vector<attribute> { { "a0", atype::i4 }, { "a1", label_array::create_and_intern ({ "X", "Y" }) } } };

    ASSERT_EQ (signed_cast (attrs.size ()), as1.size ());
    ASSERT_EQ (signed_cast (attrs.size ()), as2.size ());

    EXPECT_EQ (as1, as2);
    EXPECT_EQ (hash_value (as1), hash_value (as2));

    // move construction:
    {
        std::vector<attribute> const attrs { { "a0", atype::i4 }, { "a1", atype::timestamp }, { "a2", label_array::create_and_intern ({ "X" }) } };

        attr_schema as { attrs };
        auto const h = hash_value (as);

        attr_schema const asc { std::move (as) };
        ASSERT_EQ (3, asc.size ());

        EXPECT_EQ (asc [0], attrs [0]);
        EXPECT_EQ (asc [1], attrs [1]);
        EXPECT_EQ (asc [2], attrs [2]);

        EXPECT_EQ (std::get<0> (asc ["a0"]), attrs [0]);
        EXPECT_EQ (std::get<0> (asc ["a1"]), attrs [1]);
        EXPECT_EQ (std::get<0> (asc ["a2"]), attrs [2]);

        EXPECT_EQ (hash_value (asc), h);
    }


    // invalid input:

    EXPECT_THROW ((attr_schema { std::vector<attribute> { { "a", atype::i4 }, { "B", atype::f4 }, { "a", atype::i8 } } }), invalid_input); // dup names
}

TEST (attr_schema, indexing)
{
    std::vector<attribute> const attrs { { "a0", atype::i4 }, { "a1", label_array::create_and_intern ({ "X", "Y" }) } };
    attr_schema const as1 { attrs };
    attr_schema const as2 { std::vector<attribute> { { "a0", atype::i4 }, { "a1", label_array::create_and_intern ({ "X", "Y" }) } } };

    ASSERT_EQ (signed_cast (attrs.size ()), as1.size ());
    ASSERT_EQ (signed_cast (attrs.size ()), as2.size ());

    // lookup by index:

    for (int32_t i = 0, i_limit = attrs.size (); i < i_limit; ++ i)
    {
        ASSERT_EQ (attrs [i], as1.at<true> (i));
        ASSERT_EQ (attrs [i], as2.at<true> (i));

        EXPECT_EQ (attrs [i], as1 [i]);
        EXPECT_EQ (attrs [i], as2 [i]);
    }

    // lookup by name:

    for (int32_t i = 0; i < as1.size (); ++ i)
    {
        {
            auto const ap1 = as1.at<true> ('a' + std::to_string (i));

            ASSERT_EQ (& as1 [i], & std::get<0> (ap1));
            ASSERT_EQ (i, std::get<1> (ap1));

            auto const ap2 = as2.at<true> ('a' + std::to_string (i));

            ASSERT_EQ (& as2 [i], & std::get<0> (ap2));
            ASSERT_EQ (i, std::get<1> (ap2));
        }
        {
            auto const ap1 = as1 ['a' + std::to_string (i)];

            ASSERT_EQ (& as1 [i], & std::get<0> (ap1));
            ASSERT_EQ (i, std::get<1> (ap1));

            auto const ap2 = as2 ['a' + std::to_string (i)];

            ASSERT_EQ (& as2 [i], & std::get<0> (ap2));
            ASSERT_EQ (i, std::get<1> (ap2));
        }
    }

    // iteration:

    int32_t count { };
    for (attribute const & a : as1)
    {
        std::string const n_expected = 'a' + std::to_string (count);
        EXPECT_EQ (n_expected, a.name ()) << " failed for index " << count;

        EXPECT_EQ (attrs [count], a) << " wrong attribute at index " << count << ": " << a;

        ++ count;
    }
    EXPECT_EQ (signed_cast (attrs.size ()), count);
}
//............................................................................
//............................................................................
namespace
{
/*
 * a0, a1, A2:  i4;
 * a3, a4:      {l_q, l.r, l_S};
 * a.5:         f8;
 * A6:          {l_q, l.r, l_S};
 * 7_a:         timestamp;
 */
void
check_expected_schema (attr_schema const & as)
{
    ASSERT_EQ (8, as.size ());

    {
        attribute const & a = as [0];
        EXPECT_EQ (a.name (), "a0");
        EXPECT_EQ (a.atype (), atype::i4);
    }
    {
        attribute const & a = as [1];
        EXPECT_EQ (a.name (), "a1");
        EXPECT_EQ (a.atype (), atype::i4);
    }
    {
        attribute const & a = as [2];
        EXPECT_EQ (a.name (), "A2");
        EXPECT_EQ (a.atype (), atype::i4);
    }
    {
        attribute const & a = as [3];
        EXPECT_EQ (a.name (), "a3");
        ASSERT_EQ (a.atype (), atype::category);
        ASSERT_EQ (a.labels ().size (), 3);
        EXPECT_EQ (a.labels ()[0], "l_q");
        EXPECT_EQ (a.labels ()[1], "l.r");
        EXPECT_EQ (a.labels ()[2], "l_S");
    }
    {
        attribute const & a = as [4];
        EXPECT_EQ (a.name (), "a4");
        EXPECT_EQ (a.type (), as [3].type ());
    }
    {
        attribute const & a = as [5];
        EXPECT_EQ (a.name (), "a.5");
        EXPECT_EQ (a.atype (), atype::f8);
    }
    {
        attribute const & a = as [6];
        EXPECT_EQ (a.name (), "A6");
        EXPECT_EQ (a.type (), as [3].type ());
    }
    {
        attribute const & a = as [7];
        EXPECT_EQ (a.name (), "7_a");
        EXPECT_EQ (a.atype (), atype::timestamp);
    }
}

} // end of anonymous
//............................................................................
//............................................................................

TEST (attr_schema, parse_valid_input)
{
    {
        attr_schema const as { "a0, a1, A2: i4; a3, a4: {l_q, l.r, l_S}; a.5: f8; A6: {l_q, l.r, l_S}; 7_a: ts;" };
        check_expected_schema (as);
    }
    {
        attr_schema const as { "a0, a1, A2: i4;\n"
                               "a3, a4: {l_q, l.r, l_S};\n"
                               "a.5: f8;\tA6: {l_q, l.r, l_S};\n"
                               "7_a:\tts;" };
        check_expected_schema (as);
    }
    // trailing ';' is optional:
    {
        attr_schema const as { "a0, a1, A2: i4; a3, a4: {l_q, l.r, l_S}; a.5: f8; A6: {l_q, l.r, l_S}; 7_a: ts" };
        check_expected_schema (as);
    }
    // leading, trailing whitespace:
    {
        attr_schema const as { " a0, a1, A2: i4; a3, a4: {l_q, l.r, l_S}; a.5: f8; A6: { l_q , l.r , l_S }; 7_a: ts " };
        check_expected_schema (as);
    }
    // 'ts'/'time'/'timestamp' are all accepted as synonyms:
    {
        attr_schema const as { "a0, a1, A2: i4; a3, a4: {l_q, l.r, l_S}; a.5: f8; A6: {l_q, l.r, l_S}; 7_a: time" };
        check_expected_schema (as);
    }
    {
        attr_schema const as { "a0, a1, A2: i4; a3, a4: {l_q, l.r, l_S}; a.5: f8; A6: {l_q, l.r, l_S}; 7_a: timestamp" };
        check_expected_schema (as);
    }
}

TEST (attr_schema, parse_invalid_input)
{
    EXPECT_THROW (attr_schema::parse ("a$0 : i4;"), parse_failure); // lexing error
    EXPECT_THROW (attr_schema::parse ("a0 : i$4;"), parse_failure); // lexing error
    EXPECT_THROW (attr_schema::parse ("a0 : i4; a1 : i$8;"), parse_failure); // lexing error

    EXPECT_THROW (attr_schema::parse ("a0 : i4; ;"), parse_failure); // trailing garbage
    EXPECT_THROW (attr_schema::parse ("a0 : i4; $"), parse_failure); // trailing garbage
    EXPECT_THROW (attr_schema::parse ("a0 : i4; a :"), parse_failure); // unfinished decl
    EXPECT_THROW (attr_schema::parse ("a0 : i4; a : i32"), parse_failure); // invalid atype
}
//............................................................................

TEST (attr_schema, print)
{
    attr_schema const as { std::vector<attribute>
    {
        { "a0", atype::i4 },
        { "a1", label_array::create_and_intern ({ "u", "v", "x", "y", "z" }) },
        { "a2", atype::i8 },
        { "a3", atype::f4 },
        { "a4", atype::f8 },
        { "a5", atype::timestamp },
        { "a6", label_array::create_and_intern ({ "u", "v", "x", "y", "z" }) }
    } };

    std::stringstream os;
    os << print (as);

    std::string const as_str = os.str ();

    // names:

    EXPECT_TRUE (boost::contains (as_str, "a0"));
    EXPECT_TRUE (boost::contains (as_str, "a1"));
    EXPECT_TRUE (boost::contains (as_str, "a2"));
    EXPECT_TRUE (boost::contains (as_str, "a3"));
    EXPECT_TRUE (boost::contains (as_str, "a4"));
    EXPECT_TRUE (boost::contains (as_str, "a5"));
    EXPECT_TRUE (boost::contains (as_str, "a6"));

    // types:

    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::i4)));
    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::category)));
    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::i8)));
    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::f4)));
    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::f8)));
    EXPECT_TRUE (boost::contains (as_str, atype::name (atype::timestamp)));

    // [category labels may be shown in abbreviated form]
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
