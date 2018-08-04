
#include "vr/util/backtrace.h"

#include "vr/asserts.h"

#include <unwind.h> // _Unwind_Backtrace() from libgcc

//----------------------------------------------------------------------------
extern "C"
{

struct vr_backtrace_context final
{
    vr_backtrace_context (int32_t const skip, void ** const out, int32_t const out_limit) :
        m_out { out },
        m_out_limit { out_limit },
        m_skip { skip }
    {
    }

    void ** const m_out;
    int32_t const m_out_limit;
    int32_t m_skip;
    int32_t m_position { }; // 'm_out' slot index to write to next

}; // end of struct
//............................................................................
/*
 * ::_Unwind_Trace_Fn signature:
 */
static ::_Unwind_Reason_Code
vr_backtrace_callback (struct ::_Unwind_Context * uctx, void/* vr_backtrace_capture_context */ * ctx_addr)
{
    vr_backtrace_context * const ctx = static_cast<vr_backtrace_context *> (ctx_addr);

    if (ctx->m_skip > 0)
        -- ctx->m_skip;
    else
    {
        if (ctx->m_position < ctx->m_out_limit)
            ctx->m_out [ctx->m_position ++] = reinterpret_cast<void *> (::_Unwind_GetIP (uctx)); // note side effect
        else
            return _URC_NORMAL_STOP; // try to stop further (unneeded) reentries
    }

    return _URC_NO_REASON;
}

} // end of extern
//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/*
 * this impl borrows from gperftools and cppcms
 *
 * NOTE comments about libunwind interposing libgcc's functions when loaded in
 * https://github.com/gperftools/gperftools/wiki/gperftools'-stacktrace-capturing-methods-and-their-issues
 *
 * NOTE that it now seems possible to capture the stack entirely with libdwfl, see
 *  dwfl_getthread_frames(),
 *  dwfl_thread_getframes(),
 * etc.
 */
int32_t
capture_callstack (int32_t const skip, addr_t out [], int32_t const out_limit)
{
    assert_within (skip, out_limit);

    vr_backtrace_context ctx { skip, out, out_limit };

    ::_Unwind_Backtrace (vr_backtrace_callback, & ctx);

    int32_t position = ctx.m_position;

    // if 'limit' was higher than available call depth, we will see a null ptr
    // stored as the last entry:

    if (position && (out [position - 1] == nullptr))
        -- position;

    return position;
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
