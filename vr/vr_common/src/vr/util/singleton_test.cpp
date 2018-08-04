
#include "vr/util/singleton.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace
{

struct S final: public virtual test::instance_counter<S>
{
    int32_t m_f0 { 333 };
    std::string m_f1 { "some string" };

}; // end of class

struct S_leaking_factory final: public singleton_constructor<S>
{
    S_leaking_factory (S * const s)
    {
        new (s) S { }; // behavior that was incorrect prior to fixing ASX-2

        s->m_f0 = 777;
        s->m_f1 = "another string";
    }

}; // end of class

/*
 * something that used to be hard to use with the singleton pattern in c++03:
 *  - no no-arg constructor
 *  - const fields
 *  - [final]
 */
struct F final: public virtual test::instance_counter<F>
{
    F (int32_t const a0, std::string const & a1) :
        m_f0 { a0 },
        m_f1 { a1 }
    {
    }

    int32_t const m_f0;
    std::string const m_f1;

}; // end of class

struct F_factory: public singleton_constructor<F>
{
    F_factory (F * const mem)
    {
        new (mem) F { 333, "another string" };
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

TEST (singleton, default_constructor)
{
    using S_singleton       = singleton<S>;

    auto const S_i_count    = S::instance_count ();

    S const & s1 = S_singleton::instance ();
    EXPECT_EQ (S::instance_count (), S_i_count + 1);

    // validate proper 's1' construction:

    EXPECT_EQ (333, s1.m_f0);
    EXPECT_EQ ("some string", s1.m_f1);

    S const & s2 = S_singleton::instance ();
    EXPECT_EQ (& s1, & s2);

    EXPECT_EQ (S::instance_count (), S_i_count + 1);
}

TEST (singleton, fix_ASX_2)
{
    using S_singleton       = singleton<S, S_leaking_factory>;

    auto const S_i_count    = S::instance_count ();

    S const & s1 = S_singleton::instance ();
    EXPECT_EQ (S::instance_count (), S_i_count + 1);

    // validate proper 's1' construction:

    EXPECT_EQ (777, s1.m_f0);
    EXPECT_EQ ("another string", s1.m_f1);

}

TEST (singleton, custom_factory)
{
    using F_singleton       = singleton<F, F_factory>;

    F const & f1 = F_singleton::instance ();
    EXPECT_EQ (1, F::instance_count ());

    // validate proper 'f1' construction:

    EXPECT_EQ (333, f1.m_f0);
    EXPECT_EQ ("another string", f1.m_f1);

    F const & f2 = F_singleton::instance ();
    EXPECT_EQ (& f1, & f2);

    EXPECT_EQ (1, F::instance_count ());
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
