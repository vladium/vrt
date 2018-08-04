#pragma once

#include "vr/market/defs.h" // agent_ID
#include "vr/market/ref/asx/ref_data_fwd.h"
#include "vr/market/rt/asx/xl_request.h"
#include "vr/market/rt/cfg/agent_cfg_fwd.h"
#include "vr/market/rt/defs.h" // recv_descriptor
#include "vr/market/rt/execution_link_ifc.h"
#include "vr/mc/cache_aware.h"
#include "vr/mc/defs.h"
#include "vr/mc/rcu.h"
#include "vr/mc/steppable.h"
#include "vr/mc/thread_pool_fwd.h"
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
class execution_link final: public mc::steppable_<mc::rcu<_writer_>>, public util::di::component, public startable
{
    public: // ...............................................................

        using ifc                   = execution_link_ifc<xl_request_storage>;
        using poll_descriptor       = recv_descriptor<4>; // TODO ASX-specific; can't use a constexpr here due to std defect


        VR_ASSUME_COLD execution_link (scope_path const & cfg_path, std::string const & mock = { });
        ~execution_link ();


        // ACCESSORs:

        /**
         * @note caller must be an RCU reader that is invoking this method inside an
         *       rcu_read_lock()/rcu_read_unlock() "critical section"
         */
        VR_FORCEINLINE poll_descriptor const & poll () const;

        // MUTATORs:

        /**
         * @note valid to invoke (at most once per agent ID) after @ref start()
         */
        VR_ASSUME_COLD ifc connect (call_traits<agent_ID>::param aid);

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        // steppable:

        VR_ASSUME_HOT void step () final override;


        rt::app_cfg const * m_config { };   // [dep]
        mc::thread_pool * m_threads { };    // [dep]
        agent_cfg const * m_agents { };     // [dep]
        ref_data const * m_ref_data { };    // [dep]
        util::di::component const * m_mock { }; // [dep, a dummy one to force start() ordering wrt mock components]

        std::unique_ptr<pimpl> const m_impl;

        mc::cache_line_padded_field<poll_descriptor *> m_published { }; // [RCU-assigned]

}; // end of class
//............................................................................

inline execution_link::poll_descriptor const & // force-inlined
execution_link::poll () const
{
    return (* rcu_dereference (m_published.value ()));
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
