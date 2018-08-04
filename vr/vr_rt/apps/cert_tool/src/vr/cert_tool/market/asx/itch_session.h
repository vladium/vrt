#pragma once

#include "vr/arg_map.h"
#include "vr/filesystem.h"
#include "vr/market/ref/asx/ref_data_fwd.h"
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/startable.h"
#include "vr/market/sources/asx/defs.h" // partition_count (), order_token, etc
#include "vr/util/di/component.h"

#include <boost/thread/thread.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
class itch_session final: public util::di::component, public startable
{
    public: // ...............................................................

        itch_session (scope_path const & cfg_path, char const feed, bool const use_snapshot, fs::path const & capture_root,
                      bool const disable_nagle, std::vector<int64_t> const & sns, int32_t const PU, bool const live_run = false);



        int32_t list (arg_map && args, std::ostream & out) const;
        void qbook (arg_map && args, std::ostream & out) const;
        int32_t obook (arg_map && args, std::ostream & out) const;

        void request_rewind (arg_map && args);

    private: // ..............................................................

        class itch_session_runnable; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        rt::app_cfg const * m_config { };   // [dep]
        ref_data const * m_ref_data { };    // [dep]
        scope_path const m_cfg_path;
        fs::path const m_capture_root;
        std::unique_ptr<itch_session_runnable> const m_runnable;
        boost::thread m_worker { };
        char const m_feed;

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
