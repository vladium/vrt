#pragma once

#include <errno.h>

#include <sqlite3.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/*
 * requires "vr/exceptions.h"
 */
#define VR_CHECKED_SQLITE_CALL(exp) \
    { \
       int32_t const rc = (exp); \
       if (VR_UNLIKELY (rc != SQLITE_OK)) \
           { throw_x (io_exception, "(" #exp ") error (" + string_cast (rc) + "): " + ::sqlite3_errstr (rc)); }; \
    } \
    /* */

/*
 * #include "vr/util/logging.h"
 */
#define VR_CHECKED_SQLITE_CALL_noexcept(exp) \
    ({ \
        int32_t const rc = (exp); \
       if (VR_UNLIKELY (rc != SQLITE_OK)) \
           { LOG_error << "(" #exp ") error (" << rc << "): " << ::sqlite3_errstr (rc); }; \
    }) \
    /* */

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
