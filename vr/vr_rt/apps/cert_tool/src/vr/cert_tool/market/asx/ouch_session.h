#pragma once

#include "vr/arg_map.h"
#include "vr/filesystem.h"
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
class ouch_session final: public util::di::component, public startable
{
    public: // ...............................................................

        VR_NESTED_ENUM (
            (
                op_submit,
                op_replace,
                op_cancel
            ),
            printable, parsable

        ); // end of nested enum


        ouch_session (scope_path const & cfg_path, fs::path const & capture_root,
                      bool const disable_nagle, std::vector<int64_t> const & seqnums, int32_t const PU, bool const live_run = false);


        int32_t list_book (std::ostream & out) const;


        order_token submit (arg_map && args);
        order_token replace (order_token const otk, arg_map && args);
        void cancel (order_token const otk, arg_map && args);

    private: // ..............................................................

        class ouch_session_runnable; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        rt::app_cfg const * m_config { }; // [dep]
        scope_path const m_cfg_path;
        fs::path const m_capture_root;
        std::unique_ptr<ouch_session_runnable> const m_runnable;
        boost::thread m_worker { };

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
