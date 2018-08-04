
#include "vr/sys/os.h"
#include "vr/mc/mc.h"

#include "vr/test/mc.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................
//............................................................................
namespace
{

struct do_A final
{
    void operator() ()
    {
        for (int32_t i = 0; i < 3; ++ i)
        {
            ++ m_count;
            sys::short_sleep_for ((i + 1) * 50 * _1_millisecond ());
        }
    }

    int32_t m_count { };

}; // end of

struct do_B final
{
    void operator() ()
    {
        for (int32_t i = 0; i < 5; ++ i)
        {
            ++ m_count;
            sys::short_sleep_for ((i + 1) * 20 * _1_millisecond ());
        }
    }

    int32_t m_count { };

}; // end of

} // end of anonymous
//............................................................................
//............................................................................

TEST (task_container, unnamed_unbound)
{
    test::task_container tasks { };

    tasks.add ({ do_A { } });
    tasks.add ({ do_B { } });

    do_A const & a = tasks [0]; // indexed in addition order
    do_B const & b = tasks [1]; // indexed in addition order

    tasks.start ();
    tasks.stop ();

    EXPECT_EQ (a.m_count, 3);
    EXPECT_EQ (b.m_count, 5);
}

TEST (task_container, bound)
{
    test::task_container tasks { };

    tasks.add ({ do_A { }, 0 }, "A");
    tasks.add ({ do_B { }, 1 }, "B");

    do_A const & a = tasks ["A"];
    do_B const & b = tasks ["B"];

    do_A const & a2 = tasks [0];
    ASSERT_EQ (& a2, & a);
    do_B const & b2 = tasks [1];
    ASSERT_EQ (& b2, & b);

    tasks.start ();
    tasks.stop ();

    EXPECT_EQ (a.m_count, 3);
    EXPECT_EQ (b.m_count, 5);
}

TEST (task_container, unbound)
{
    test::task_container tasks { };

    tasks.add ({ do_A { } }, "A");
    tasks.add ({ do_B { } }, "B");

    do_A const & a = tasks ["A"];
    do_B const & b = tasks ["B"];

    do_A const & a2 = tasks [0];
    ASSERT_EQ (& a2, & a);
    do_B const & b2 = tasks [1];
    ASSERT_EQ (& b2, & b);

    tasks.start ();
    tasks.stop ();

    EXPECT_EQ (a.m_count, 3);
    EXPECT_EQ (b.m_count, 5);
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
