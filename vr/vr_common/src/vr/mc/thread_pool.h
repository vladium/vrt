#pragma once

#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
/**
 */
class thread_pool final: public util::di::component, public startable
{
    public: // ...............................................................

        thread_pool (scope_path const & cfg_path);
        ~thread_pool ();

        /**
         *
         */
        VR_ASSUME_COLD void attach_to_rcu_callback_thread (); // TAKE 'rcu_market &' requestor arg?
        VR_ASSUME_COLD void detach_from_rcu_callback_thread ();

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        rt::app_cfg * m_config { }; // [dep]

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
