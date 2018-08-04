#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................

// NOTE these are pipeline-flushing versions:

#define VR_TSC_START()              \
    ({                              \
        int64_t vr_tsc;             \
        __asm__ __volatile__        \
        (                           \
            "rdtscp\n"              \
            "shlq $32,   %%rdx\n"   \
            "orq  %%rax, %%rdx\n"   \
            "movq %%rdx, %0\n"      \
                                    \
            "xorl %%eax, %%eax\n"   \
            "cpuid\n"               \
                : "=r" (vr_tsc)     \
                :                   \
                : "%rax", "%rbx",   \
                  "%rcx", "%rdx",   \
                  "memory"          \
        );                          \
        vr_tsc;                     \
    })                              \
    /* */

#define VR_TSC_STOP(start)          \
    __asm__ __volatile__            \
    (                               \
        "rdtscp\n"                  \
        "shlq $32,   %%rdx\n"       \
        "orq  %%rax, %%rdx\n"       \
        "subq %%rdx, %0\n"          \
        "negq %0\n"                 \
                                    \
        "xorl %%eax, %%eax\n"       \
        "cpuid\n"                   \
            : "+r" ( start )        \
            :                       \
            : "%rax", "%rbx",       \
              "%rcx", "%rdx", "cc", \
              "memory"              \
    )                               \
    /* */
//............................................................................
/**
 * @return an <em>estimate</em> of cycle overhead of a VR_TSC_START()/VR_TSC_STOP() pair
 *
 * @note this value is held in a thread-safe singleton and so should be read into a local
 *       var outside of any measurement-critical section
 */
extern int64_t const &
tsc_macro_overhead ();

//............................................................................
/**
 * @throw illegal_state if the current thread is not pinned to a single PU
 *        [debug builds only, to catch usage errors]
 *
 * @param lvl [1-based L-index, @see sys::L1]
 */
extern void
flush_cpu_cache (int32_t const lvl);

//............................................................................

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
