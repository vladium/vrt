
#include "vr/asserts.h"

#include "vr/filesystem.h"
#include "vr/util/logging.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

const std::string g_filename { fs::path {__FILE__}.filename ().string () };

} // end of anonymous
//............................................................................
//............................................................................

TEST (check, src_loc_info)
{
    int32_t const i = 123;
    int32_t line { };

    bool caught = false;
    try
    {
        line = __LINE__; check_condition (i > 1000); // plain check/assert
    }
    catch (invalid_input const & ii)
    {
        caught = true;

        LOG_trace1 << ii.what ();
        const std::string msg { ii.what () };

        EXPECT_TRUE (msg.find (g_filename) != std::string::npos);
        EXPECT_TRUE (msg.find (string_cast (line)) != std::string::npos);
        EXPECT_TRUE (msg.find ("i > 1000") != std::string::npos);
    }
    ASSERT_TRUE (caught);
}

TEST (check, context_info)
{
    int32_t i = 123;
    double d = 4.56;
    char c = 'C';
    std::string s { "hello" };

    int32_t line { };

    bool caught = false;
    try
    {
        line = __LINE__; check_condition (i > 1000, i, d, c, s); // check/assert with some additional vars added to the message
    }
    catch (invalid_input const & ii)
    {
        caught = true;

        LOG_trace1 << ii.what ();
        const std::string msg { ii.what () };

        EXPECT_TRUE (msg.find (g_filename) != std::string::npos);
        EXPECT_TRUE (msg.find (string_cast (line)) != std::string::npos);
        EXPECT_TRUE (msg.find ("i > 1000") != std::string::npos);

        // check presence of values of 'i', 'd', 'c', 's':

        EXPECT_TRUE (msg.find ("123") != std::string::npos);
        EXPECT_TRUE (msg.find ("4.56") != std::string::npos);
        EXPECT_TRUE (msg.find ("'C'") != std::string::npos);
        EXPECT_TRUE (msg.find ("\"hello\"") != std::string::npos);
    }
    ASSERT_TRUE (caught);
}
//............................................................................
#define vr_expect_throw_ii()    EXPECT_THROW (f (), invalid_input)
#define vr_expect_throw_oob()   EXPECT_THROW (f (), out_of_bounds)

TEST (check, conditions_arithmetic)
{
    int32_t i = 123;
    int8_t z { };

    // expect exceptions and also trace their messages:
    {
        bool caught { false };
        try
        {
            check_within (i, 100);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_within()";
    }
    {
        bool caught { false };
        try
        {
            check_positive (-i);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_positive()";
    }
    {
        bool caught { false };
        try
        {
            check_nonzero (z);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_nonzero()";
    }
    {
        bool caught { false };
        try
        {
            check_zero (i);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_zero()";
    }


    std::function<void ()> f;

    {
        check_within (0, 1); // 0 in [0, 1)
        check_within (1, 2); // 1 in [0, 2)

        // EXPECT_THROW() doesn't work with check_* statements, work around using lambdas

        f = { []{ check_within (0, 0); } };
        vr_expect_throw_oob ();
        f = { []{ check_within (-1, 0); } };
        vr_expect_throw_oob ();
        f = { []{ check_within (1, 0); } };
        vr_expect_throw_oob ();

        f = { []{ check_within (-1, 1); } };
        vr_expect_throw_oob ();
        f = { []{ check_within (1, 1); } };
        vr_expect_throw_oob ();
        f = { []{ check_within (2, 1); } };
        vr_expect_throw_oob ();
    }

    {
        check_in_range (0, 0, 1); // 0 in [0, 1)
        check_in_range (0, -1, 1); // 0 in [-1, 1)

        check_in_range (-1, -1, 1); // -1 in [-1, 1)
        check_in_range (-1, -2, 1); // 1 in [-2, 1)

        check_in_range (1, 0, 2); // 1 in [0, 2)
        check_in_range (1, 1, 2); // 1 in [1, 2)

        check_in_range (-2, -2, 0); // -2 in [-2, 0)
        check_in_range (-2, -3, -1); // 2 in [-3, -1)


        f = { []{ check_in_range (0, 0, 0); } };
        vr_expect_throw_oob ();
        f = { []{ check_in_range (-1, 0, 1); } };
        vr_expect_throw_oob ();
        f = { []{ check_in_range (0, -1, 0); } };
        vr_expect_throw_oob ();

        f = { []{ check_in_range (-10, -9, -7); } };
        vr_expect_throw_oob ();
        f = { []{ check_in_range (-7,  -9, -7); } };
        vr_expect_throw_oob ();

        f = { []{ check_in_range (9, 7, 9); } };
        vr_expect_throw_oob ();
        f = { []{ check_in_range (6, 7, 9); } };
        vr_expect_throw_oob ();

        f = { []{ check_in_range (-11, -10, 11); } };
        vr_expect_throw_oob ();
        f = { []{ check_in_range (11,  -10, 11); } };
        vr_expect_throw_oob ();
    }

    {
        check_is_power_of_2 (1024);
        check_is_power_of_2 (1);

        f = { []{ check_is_power_of_2 (3); } };
        vr_expect_throw_ii ();

        f = { []{ check_is_power_of_2 (0); } }; // note the edge case
        vr_expect_throw_ii ();

        f = { []{ check_is_power_of_2 (-2); } }; // negative
        vr_expect_throw_ii ();
    }

    {
        check_eq (1, 1);
        check_ne (1, 2);

        check_lt (1, 2);
        check_le (1, 1);

        check_gt (2, 1);
        check_ge (2, 2);
    }
}
//............................................................................

TEST (check, conditions_addr)
{
    std::vector<int32_t> x { 1, 2, 3, 4 };
    int32_t * const p { & x [0] };
    int8_t * const np { nullptr };

    // expect exceptions and also trace their messages:
    {
        bool caught { false };
        try
        {
            check_null (p);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_null()";
    }
    {
        bool caught { false };
        try
        {
            check_nonnull (np);
        }
        catch (invalid_input const & ii)
        {
            caught = true;
            LOG_trace1 << ii.what ();
        }
        EXPECT_TRUE (caught) << "check_nonnull()";
    }

    {
        check_addr_contained (& x [0], & x.front (), & x.back ());
        check_addr_contained (& x [x.size () - 1], & x.front (), & x.back ());

        check_addr_range_contained (& x [0], sizeof (x [0]) * x.size (), & x.front (), sizeof (x [0]) * x.size ());
    }

    std::function<void ()> f;

    {
        f = { [& x]{ check_addr_contained (& x [x.size ()], & x.front (), & x.back ()); } };
        vr_expect_throw_ii ();

        void * const np { nullptr };

        f = { [& x, np]{ check_addr_contained (np, & x.front (), & x.back ()); } };
        vr_expect_throw_ii ();


        f = { [& x]{ check_addr_range_contained (& x [1], sizeof (x [0]) * x.size (), & x.front (), sizeof (x [0]) * x.size ()); } };
        vr_expect_throw_ii ();

        f = { [& x]{ check_addr_range_contained (& x.front () - 1, 1, & x.front (), sizeof (x [0]) * x.size ()); } };
        vr_expect_throw_ii ();

        f = { [& x]{ check_addr_range_contained (& x.back (), 1 + sizeof (x [0]), & x.front (), sizeof (x [0]) * x.size ()); } };
        vr_expect_throw_ii ();
    }
}
//............................................................................

TEST (check, conditions_object)
{
    std::vector<int32_t> ev;
    std::vector<int32_t> nev { 1, 2, 3 };

    std::function<void ()> f;

    {
        f = { [& ]{ check_empty (nev); } };
        vr_expect_throw_ii ();

        f = { [& ev]{ check_nonempty (ev); } };
        vr_expect_throw_ii ();
    }
}
//............................................................................

#undef vr_expect_throw_ii
#undef vr_expect_throw_oob

//............................................................................

} // end of namespace
//----------------------------------------------------------------------------
