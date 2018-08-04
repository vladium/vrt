#pragma once

#include "vr/mc/steppable.h"
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

class mock_ouch_server final: public mc::steppable, public util::di::component, public startable
{
    public: // ...............................................................

        mock_ouch_server (scope_path const & cfg_path);
        ~mock_ouch_server ();

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        // steppable:

        VR_ASSUME_HOT void step () override;


        rt::app_cfg * m_config { }; // [dep]

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
