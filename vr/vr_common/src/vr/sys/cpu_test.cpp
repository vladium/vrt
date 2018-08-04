
#include "vr/sys/cpu.h"
#include "vr/sys/os.h"

#include "vr/test/utility.h"

#include <cmath>

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

TEST (cpu_info, object_counts)
{
    cpu_info const & ci = cpu_info::instance ();

    for (hw_obj::enum_t t : hw_obj::values ())
    {
        auto const t_count = ci.count (t);
        LOG_info << "  " << print (t) << " count:\t" << t_count;

        switch (t)
        {
            case hw_obj::PU:
            case hw_obj::core:
            {
                EXPECT_GT (t_count, 0) << "expect positive count for " << print (t);
            }
            break;

            default: break;

        } // end of switch
    }

    // PUs should not be fewer in number than cores:

    EXPECT_LE (ci.count (hw_obj::core), ci.count (hw_obj::PU));
}

TEST (cpu_info, cache_details)
{
    cpu_info const & ci = cpu_info::instance ();

    int32_t const LLC = ci.cache_info ().level_count ();
    LOG_info << "LLC: " << LLC;
    ASSERT_GT (LLC, 0);

    // sticking with the default cache type of 'data' should work across L1i,d and unified levels:

    for (int32_t lvl = L1; lvl <= LLC; ++ lvl)
    {
        cpu_info::cache::level const & cl = ci.cache_info ().level_info (lvl);
        LOG_info << "  L" << lvl << ": count " << cl.m_count << ", line size " << cl.m_line_size << ", size " << cl.m_size;

        EXPECT_GT (cl.m_count, 0);
        EXPECT_GT (cl.m_line_size, 0);
        EXPECT_GT (cl.m_size, 0UL);
    }
}
//............................................................................

TEST (affinity, sniff)
{
    bit_set const current = affinity::this_thread_PU_set ();
    LOG_info << "current: " << current;

    double r { };
    {
        affinity::scoped_thread_sched _ { bit_set { current }.reset ().set (3) };

        for (int64_t i = 0; i < 10000000L; ++ i)
        {
            r += std::cos (i);
        }

        bit_set const bound = affinity::this_thread_PU_set ();
        LOG_info << "bound: " << bound;

        EXPECT_TRUE (bound.count () == 1 && bound.test (3)) << "invalid bound PU set: " << bound;

        int32_t const pu = affinity::this_thread_last_PU ();
        EXPECT_EQ (pu, 3);
    }

    bit_set const restored = affinity::this_thread_PU_set ();
    LOG_info << "restored: " << restored;

    EXPECT_EQ (restored, current);

    LOG_trace3 << r;
}
//............................................................................

TEST (affinity, run_on_PU)
{
    int32_t const rA = affinity::run_on_PU (0, []() { return affinity::this_thread_last_PU (); });
    int32_t const rB = affinity::run_on_PU (1, []() { return affinity::this_thread_last_PU (); });

    EXPECT_EQ (rA, 0);
    EXPECT_EQ (rB, 1);
}
//............................................................................

TEST (affinity, this_thread_last_PU_vs_cpuid)
{
    std::vector<int32_t> PUs { };
    std::vector<int32_t> CPUIDs { };

    for (int32_t pu = 0, pu_limit = cpu_info::instance ().count (hw_obj::PU); pu != pu_limit; ++ pu)
    {
        affinity::run_on_PU (pu, [& PUs, & CPUIDs]()
            {
                PUs.push_back (affinity::this_thread_last_PU ());
                CPUIDs.push_back (sys::cpuid ());
            }
        );
    }

    EXPECT_EQ (PUs, CPUIDs);
}

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
