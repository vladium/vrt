#pragma once

#include "vr/enums.h"
#include "vr/util/memory.h"

#include <cstring>
#include <errno.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

VR_ENUM (hw_obj,
    (
        PU,
        core,
        socket, // package
        node    // NUMA
    ),
    iterable, printable, parsable

); // end of enum
//............................................................................

enum { L1 = 1, L2, L3 };

VR_ENUM (cache_type,
    (
        unified,
        data,
        instr
    ),
    iterable, printable

); // end of enum
//............................................................................
/*
 * requires "vr/exceptions.h"
 */
#define VR_CHECKED_SYS_CALL(exp) \
    ({ \
       int32_t rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { auto const e = errno; throw_x (sys_exception, "(" #exp ") error (" + string_cast (e) + "): " + std::strerror (e)); }; \
       rc; \
    }) \
    /* */

/*
 * #include "vr/util/logging.h"
 */
#define VR_CHECKED_SYS_CALL_noexcept(exp) \
    ({ \
       int32_t rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { auto const e = errno; LOG_error << "(" #exp ") error (" << e << "): " << std::strerror (e); }; \
       rc; \
    }) \
    /* */

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
