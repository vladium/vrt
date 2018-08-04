#pragma once

#include "vr/util/di/dep_injection_fwd.h" // container_barrier
#include "vr/types.h"

#include <exception> // std::exception_ptr

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
class step_runnable; // forward
class steppable; // forward
/**
 */
class step_ctl
{
    public: // ...............................................................

        virtual ~step_ctl ()   = default;


        /**
         * ask whoever can notify *all* running compnents, not just the caller
         * 'step_runnable' instance, etc
         */
        virtual VR_ASSUME_COLD void request_stop (steppable const * const requestor) VR_NOEXCEPT    = 0;

        /**
         * @note this does *not* imply stop will be requested
         */
        virtual VR_ASSUME_COLD void report_failure (std::exception_ptr const & eptr) VR_NOEXCEPT    = 0;

    protected: // ............................................................

        friend class step_runnable; // grant access to 'sequencer()'

        virtual util::di::container_barrier & sequencer ()   = 0;

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
