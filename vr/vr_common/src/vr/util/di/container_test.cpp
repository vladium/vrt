
#include "vr/util/di/component.h"

#include "vr/mc/steppable.h"
#include "vr/sys/cpu.h"
#include "vr/sys/os.h"
#include "vr/util/di/container.h"

#include "vr/test/utility.h"

#include <boost/thread/thread.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
//............................................................................
//............................................................................
namespace test1
{

class A final: public component, public startable,
               public test::instance_counter<A>
{
    private:

        // startable:

        void start () override
        {
            ++ m_start_count;
        }

        void stop () override
        {
            ++ m_stop_count;
        }

    public:

        int32_t m_start_count { };
        int32_t m_stop_count { };

}; // end of class

/*
 * uses "A"
 */
class B final: public component,
               public test::instance_counter<B>
{
    public:

        B ()
        {
            dep (m_a)   = "A";
        }

    private:

        A const * m_a { };

}; // end of class

/*
 * uses "A" and "B"
 */
class C final: public component, public startable,
               public test::instance_counter<C>
{
    public:

        C ()
        {
            dep (m_a)   = "A";
            dep (m_b)   = "B";
        }

        A & my_A () const
        {
            check_nonnull (m_a);
            return (* m_a);
        }

        B const & my_B () const
        {
            check_nonnull (m_b);
            return (* m_b);
        }

        int32_t m_start_count { };
        int32_t m_stop_count { };

    private:

        // startable:

        void start () override
        {
            ++ m_start_count;
        }

        void stop () override
        {
            ++ m_stop_count;
        }

        A * m_a { };
        B const * m_b { };

}; // end of class


class D: public component
{
}; // end of class

class DD final: public D,
                public test::instance_counter<DD>
{
}; // end of class

} // end of 'test1'
//............................................................................
//............................................................................

TEST (container, configure)
{
    using namespace test1;

    auto const iA_start     = A::instance_count ();
    auto const iB_start     = B::instance_count ();
    auto const iC_start     = C::instance_count ();
    auto const iDD_start    = DD::instance_count ();

    {
        container app { "APP" };

        app.configure () // syntactic sugar for configuration
          ("B", new B { })
          ("C", new C { })
          ("A", new A { })
        ;

        app.add ("D",  new DD { }); // configuration can be split over multiple phases
        app.add ("C2", new C { });

        // validate instance counts:

        EXPECT_EQ (A::instance_count (), iA_start + 1);
        EXPECT_EQ (B::instance_count (), iB_start + 1);
        EXPECT_EQ (C::instance_count (), iC_start + 2);
        EXPECT_EQ (DD::instance_count (), iDD_start + 1);

        // before 'container::start ()', components are retrievable (in their constructed, non-started states):
        {
            A const & a     = app ["A"];
            C const & c     = app ["C"];
            C const & c2    = app ["C2"];

            ASSERT_NE (& c, & c2); // distinct instances of the same component class

            ASSERT_EQ (a.m_start_count, 0);
            ASSERT_EQ (a.m_stop_count,  0);
            ASSERT_EQ (c.m_start_count, 0);
            ASSERT_EQ (c.m_stop_count,  0);
            ASSERT_EQ (c2.m_start_count,0);
            ASSERT_EQ (c2.m_stop_count, 0);
        }

        app.start ();
        {
            A & a           = app ["A"]; // non-const handle
            B & b           = app ["B"];
            C const & c     = app ["C"]; // const handle
            C const & c2    = app ["C2"];
            D & d           = app ["D"]; // handle of base type

            ASSERT_NE (& c, & c2); // distinct instances of the same component class

            EXPECT_EQ (& a, & c.my_A ());
            EXPECT_EQ (& b, & c.my_B ());

            DD * const dd = dynamic_cast<DD *> (& d);
            EXPECT_TRUE (!! dd);

            EXPECT_EQ (a.m_start_count, 1);
            EXPECT_EQ (a.m_stop_count,  0);
            EXPECT_EQ (c.m_start_count, 1);
            EXPECT_EQ (c.m_stop_count,  0);
            EXPECT_EQ (c2.m_start_count,1);
            EXPECT_EQ (c2.m_stop_count, 0);
        }
        app.stop ();

        // after 'container::stop ()', components are still retrievable (in their stopped states):
        {
            A & a           = app ["A"];
            B & b           = app ["B"];
            C & c           = app ["C"];
            C & c2          = app ["C2"];

            ASSERT_NE (& c, & c2); // distinct instances of the same component class

            EXPECT_EQ (& a, & c.my_A ());
            EXPECT_EQ (& b, & c.my_B ());

            EXPECT_EQ (a.m_start_count, 1);
            EXPECT_EQ (a.m_stop_count,  1);
            EXPECT_EQ (c.m_start_count, 1);
            EXPECT_EQ (c.m_stop_count,  1);
            EXPECT_EQ (c2.m_start_count,1);
            EXPECT_EQ (c2.m_stop_count, 1);
        }

        // validate instance counts:

        EXPECT_EQ (A::instance_count (), iA_start + 1);
        EXPECT_EQ (B::instance_count (), iB_start + 1);
        EXPECT_EQ (C::instance_count (), iC_start + 2);
        EXPECT_EQ (DD::instance_count (), iDD_start + 1);
    }

    // 'app' destructed, validate instance counts return to their starting values:

    EXPECT_EQ (A::instance_count (), iA_start);
    EXPECT_EQ (B::instance_count (), iB_start);
    EXPECT_EQ (C::instance_count (), iC_start);
    EXPECT_EQ (DD::instance_count (), iDD_start);
}
//............................................................................
//............................................................................
namespace test2
{

struct steppable_cpuid_tracker: public mc::steppable
{
    void track_cpuid ()
    {
        m_cpus_seen.set (sys::cpuid ());
    }

    bit_set m_cpus_seen { make_bit_set (sys::cpu_info::instance ().PU_count ()) };

}; // end of class


class A final: public component, public startable, public steppable_cpuid_tracker
{
    public:

        int64_t m_step_count { };
        int32_t m_start_count { };
        int32_t m_stop_count { };

    private:

        // startable:

        void start () override
        {
            LOG_trace1 << "A::start";
            ++ m_start_count;
        }

        void stop () override
        {
            LOG_trace1 << "A::stop";
            ++ m_stop_count;
        }

        // steppable:

        void step () override
        {
            track_cpuid ();

            ++ m_step_count;
            boost::this_thread::yield ();
        }

}; // end of class

/*
 * uses "A"
 */
class B final: public component, public steppable_cpuid_tracker // but not startable
{
    public:

        B ()
        {
            dep (m_a)   = "A";
        }

        int64_t m_step_count { };

    private:

        // steppable:

        void step () override
        {
            track_cpuid ();

            ++ m_step_count;
            boost::this_thread::yield ();
        }


        A const * m_a { };

}; // end of class

class D; // forward

/*
 * uses "A", "B", and "D"
 */
class C final: public component, public startable, public steppable_cpuid_tracker
{
    public:

        C ()
        {
            dep (m_a)   = "A"; // startable, steppable
            dep (m_b)   = "B"; //            steppable
            dep (m_d)   = "D"; // startable
        }

        A & my_A () const
        {
            check_nonnull (m_a);
            return (* m_a);
        }

        B const & my_B () const
        {
            check_nonnull (m_b);
            return (* m_b);
        }

        int64_t m_step_count { };
        int32_t m_start_count { };
        int32_t m_stop_count { };

    private:

        // startable:

        void start () override
        {
            LOG_trace1 << "C::start";
            ++ m_start_count;
        }

        void stop () override
        {
            LOG_trace1 << "C::stop";
            ++ m_stop_count;
        }

        // steppable:

        void step () override
        {
            track_cpuid ();

            ++ m_step_count;
            boost::this_thread::yield ();
        }


        A * m_a { };
        B const * m_b { };
        D * m_d { };

}; // end of class

class D: public component, public startable // but not steppable
{
    public:

        int32_t m_start_count { };
        int32_t m_stop_count { };

    private:

        // startable:

        void start () override
        {
            LOG_trace1 << "D::start";
            ++ m_start_count;
        }

        void stop () override
        {
            LOG_trace1 << "D::stop";
            ++ m_stop_count;
        }

}; // end of class

class E: public component // neither startable nor steppable
{
}; // end of class

} // end of 'test2'
//............................................................................
//............................................................................
// TODO
// - validate that threaded start()s/stop()s obey the topological dep sort order

TEST (container, affinity)
{
    using namespace test2;

    int32_t const PU_default    = 0;
    int32_t const PU_A_B        = 3;
    int32_t const PU_C2         = 2;

    container app { "APP",
        {
            { "default", PU_default },

            { "A",      PU_A_B },
            { "B",      "A" },          // "B" is in whichever PU group "A" is
            { "C",      "default" },    // "C" is in the "default" group
            { "C2",     PU_C2 }
        }
    };

    app.configure ()
        ("A",   new A { })
        ("B",   new B { })
        ("C",   new C { })
        ("D",   new D { })
        ("C2",  new C { })
        ("E",   new E { })
    ;

    app.start ();
    {
        boost::this_thread::yield ();

        // validate instances of 'startable' types ('A', 'C', 'D')

        A const & a     = app ["A"];
        C const & c     = app ["C"];
        C const & c2    = app ["C2"];
        D const & d     = app ["D"];

        // started exactly once, not stopped yet:

        EXPECT_EQ (a.m_start_count, 1);
        EXPECT_EQ (a.m_stop_count,  0);
        EXPECT_EQ (c.m_start_count, 1);
        EXPECT_EQ (c.m_stop_count,  0);
        EXPECT_EQ (c2.m_start_count,1);
        EXPECT_EQ (c2.m_stop_count, 0);
        EXPECT_EQ (d.m_start_count, 1);
        EXPECT_EQ (d.m_stop_count,  0);

        bool const force_stop = app.run_for (2 * _1_second ());
        EXPECT_FALSE (force_stop);
    }
    app.stop ();
    {
        boost::this_thread::yield ();

        A const & a     = app ["A"];
        B const & b     = app ["B"];
        C const & c     = app ["C"];
        C const & c2    = app ["C2"];
        D const & d     = app ["D"];

        // started and stopped exactly once:

        EXPECT_EQ (a.m_start_count, 1);
        EXPECT_EQ (a.m_stop_count,  1);
        EXPECT_EQ (c.m_start_count, 1);
        EXPECT_EQ (c.m_stop_count,  1);
        EXPECT_EQ (c2.m_start_count,1);
        EXPECT_EQ (c2.m_stop_count, 1);
        EXPECT_EQ (d.m_start_count, 1);
        EXPECT_EQ (d.m_stop_count,  1);

        // should have been stepped ('A', 'B', 'C'):

        ASSERT_GT (a.m_step_count,  0);
        ASSERT_GT (b.m_step_count,  0);
        ASSERT_GT (c.m_step_count,  0);
        ASSERT_GT (c2.m_step_count, 0);

        // check that steppables were strictly bound as configured:

        {
            bit_set const & cpus_seen = a.m_cpus_seen;

            EXPECT_EQ (signed_cast (cpus_seen.count ()), 1);
            EXPECT_TRUE (cpus_seen.test (PU_A_B));
        }
        {
            bit_set const & cpus_seen = b.m_cpus_seen;

            EXPECT_EQ (signed_cast (cpus_seen.count ()), 1);
            EXPECT_TRUE (cpus_seen.test (PU_A_B));
        }
        {
            bit_set const & cpus_seen = c.m_cpus_seen;

            EXPECT_EQ (signed_cast (cpus_seen.count ()), 1);
            EXPECT_TRUE (cpus_seen.test (PU_default));
        }
        {
            bit_set const & cpus_seen = c2.m_cpus_seen;

            EXPECT_EQ (signed_cast (cpus_seen.count ()), 1);
            EXPECT_TRUE (cpus_seen.test (PU_C2));
        }
    }
}
//............................................................................
//............................................................................
namespace test3
{

class ST final: public mc::steppable, public component, public startable
{
    public:

        ST (std::string const & depname = { }, bool const throw_in_start = false, bool const throw_in_stop = false, int64_t const step_throw = std::numeric_limits<int64_t>::max ()) :
            m_step_throw { step_throw },
            m_throw_in_start { throw_in_start },
            m_throw_in_stop { throw_in_stop }
        {
            LOG_trace1 << this << ": ST::constructor";

            if (! depname.empty ())
            {
                dep (m_dep) = depname;
            }
        }

        ~ST ()
        {
            LOG_trace1 << this << ": ST::destructor";
        }

        int64_t m_step_count { };
        int32_t m_start_count { };
        int32_t m_stop_count { };

    private:

        // startable:

        void start () override
        {
            LOG_trace1 << this << ": ST::start";

            ++ m_start_count;
            if (m_throw_in_start) throw_x (alloc_failure, "REQUESTED exception in start()");
        }

        void stop () override
        {
            LOG_trace1 << this << ": ST::stop";

            ++ m_stop_count;
            if (m_throw_in_stop) throw_x (alloc_failure, "REQUESTED exception in stop()");
        }

        // steppable:

        void step () override
        {
            if (++ m_step_count >= m_step_throw)
                throw_x (alloc_failure, "REQUESTED exception in step()");
        }

        ST * m_dep { }; // a (possible) dependency

        int64_t const m_step_throw;
        bool const m_throw_in_start;
        bool const m_throw_in_stop;

}; // end of class

} // end of 'test3'
//............................................................................
//............................................................................
// TODO
// - more lifecycle failure combinations

TEST (container, run_wait)
{
    using namespace test3;

    int32_t const PU_default    = 0;
    int32_t const PU_A_B        = 1;
    int32_t const PU_C          = 2;

    container app { "APP",
        {
            { "default", PU_default },

            { "A",          PU_A_B },
            { "B",          "A" },
            { "C",          PU_C }
        }
    };

    app.configure ()
        ("A",           new ST { "C" })
        ("B",           new ST { "A" })
        ("C",           new ST { })
    ;

    bool run_force_stopped { false };
    app.start ();
    {
        run_force_stopped = app.run_for (200 * _1_millisecond ());
    }
    app.stop ();

    EXPECT_FALSE (run_force_stopped);
}

TEST (container, lifecycle_exceptions)
{
    using namespace test3;

    int32_t const PU_default    = 0;
    int32_t const PU_A_B        = 1;
    int32_t const PU_C          = 2;

    // 'start()' failure(s) are reported by 'container::start()':
    {
        container app { "APP",
            {
                { "default", PU_default },

                { "A",          PU_A_B },
                { "B.start",    "A" },
                { "C.start",    PU_C },
                { "normal",     "default" }
            }
        };

        app.configure ()
            ("A",           new ST { "C.start" })
            ("B.start",     new ST { "A", true })
            ("C.start",     new ST { "", true })
        ;

        LOG_warn << "THE FOLLOWING EXCEPTION(S) IS/ARE PART OF THE TEST:";
        EXPECT_THROW (app.start (), lifecycle_exception) << "start() failure reporting";
    }

    // 'step()' failure(s) are reported by 'container::stop()' if 'stop()' is explicit, not part of the destructor call:
    // (and they could be the original types, not necessarily wrapped in 'lifecycle_exception')
    {
        container app { "APP",
            {
                { "default", PU_default },

                { "A",          PU_A_B },
                { "B.step",     "A" },
                { "C.step",     PU_C },
                { "normal",     "default" }
            }
        };

        app.configure ()
            ("A",           new ST { "C.step" })
            ("B.step",      new ST { "A", false, false, 3 })
            ("C.step",      new ST { "", false, false, 10 })
            ("normal",      new ST { })
        ;

        app.start ();
        {
            app.run_for (_1_second ()); // this is not a required call for this particular test
        }
        LOG_warn << "THE FOLLOWING EXCEPTION(S) IS/ARE PART OF THE TEST:";
        EXPECT_THROW (app.stop (), std::exception) << "stop() failure reporting";
    }
    // ... but only logged if invoked by the container destructor:
    {
        {
            container app { "APP",
                {
                    { "default", PU_default },

                    { "A",          PU_A_B },
                    { "B.step",     "A" },
                    { "C.step",     PU_C },
                    { "normal",     "default" }
                }
            };

            app.configure ()
                ("A",           new ST { "C.step" })
                ("B.step",      new ST { "A", false, false, 3 })
                ("C.step",      new ST { "", false, false, 10 })
                ("normal",      new ST { })
            ;

            app.start ();
            {
                app.run_for (_1_second ()); // this is not a required call for this particular test
            }
            LOG_warn << "THE FOLLOWING EXCEPTION(S) IS/ARE PART OF THE TEST:";
        }
        // [to avoid 'std::terminate()', if 'app()' is destroyed without a user call to 'stop()',
        // the exceptions are only LOG_error-logged]
    }
}

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
