#pragma once

#include "vr/filesystem.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
/**
 */
class tracing_ctl final: public util::di::component, public startable
{
    public: // ...............................................................

        struct stats final
        {
            int64_t m_discarded_events { };
            int64_t m_lost_packets { };

        }; // end of nested class

        tracing_ctl (settings const & cfg);
        ~tracing_ctl ();

        // ACCESSORs:

        /**
         * @note these get filled by @ref start() and remain valid until destruction
         */
        std::tuple<std::string, fs::path> session () const;

        /**
         * @note these get filled by @ref stop() and remain valid until destruction
         */
        stats const & loss_stats () const;

    private: // ..............................................................

        class state; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        std::unique_ptr<state> const m_state; // pimpl pattern to avoid including lttng headers here

}; // end of class

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
