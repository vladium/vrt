#pragma once

#include "vr/macros.h"

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

//----------------------------------------------------------------------------

// logging that is never elided (with verbosity controlled at runtime):

#define LOG_trace_enabled(verbosity)    VLOG_IS_ON(verbosity)

#define LOG_trace1                  VLOG(1)
#define LOG_trace2                  VLOG(2)
#define LOG_trace3                  VLOG(3)

#define LOG_trace(verbosity)        VLOG(verbosity)

#define LOG_info                    LOG(INFO)
#define LOG_warn                    LOG(WARNING)
#define LOG_error                   LOG(ERROR)
#define LOG_fatal                   LOG(DFATAL)

// logging that is elided in release builds:

#define DLOG_trace1                 DVLOG(1)
#define DLOG_trace2                 DVLOG(2)
#define DLOG_trace3                 DVLOG(3)

#define DLOG_trace(verbosity)       DVLOG(verbosity)

#define DLOG_info                   DLOG(INFO)
#define DLOG_warn                   DLOG(WARNING)
#define DLOG_error                  DLOG(ERROR)
#define DLOG_fatal                  DLOG(DFATAL)

//............................................................................
//............................................................................

namespace vr
{
namespace util
{
/*
 * this function guarantees that logging will be initialized once and MT-safely.
 */
extern VR_ASSUME_COLD bool log_initialize ();

} // end of 'util'
} // end of namespace
//............................................................................
//............................................................................
namespace
{
/*
 * trigger logging initialization from any compilation unit that includes this header
 * (the actual setup logic will be executed once only).
 */
bool const vr_log_initialize_done VR_UNUSED { vr::util::log_initialize () };

} // end of anonymous
//----------------------------------------------------------------------------
