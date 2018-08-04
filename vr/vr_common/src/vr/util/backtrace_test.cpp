
#include "vr/util/backtrace.h"

#include "vr/util/logging.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

VR_NOINLINE void
dump_trace ()
{
    addr_t trace [32] = { nullptr };
    int32_t const size = capture_callstack (0, trace, 10);

    LOG_info << "trace size: " << size;
    for (int32_t i = 0; i < size; ++ i)
    {
        LOG_info << "  [" << i << "] " << trace [i];
    }
}

VR_NOINLINE void
recurse (int32_t const depth)
{
    LOG_trace1 << "dummy operation to prevent gcc from inlining very short functions";

    if (depth > 0)
        recurse (depth - 1);
    else
        dump_trace ();
}
//............................................................................

TEST (backtrace, src_loc_info)
{
    recurse (4);

    // TODO flesh out
    // TODO test MT-safety
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
