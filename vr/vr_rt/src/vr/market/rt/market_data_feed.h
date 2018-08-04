#pragma once

#include "vr/market/rt/defs.h" // recv_descriptor
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
//TODO move into ASX ns?

class market_data_feed final: public mc::steppable_<mc::rcu<_writer_>>, public util::di::component, public startable
{
    public: // ...............................................................

        using poll_descriptor   = recv_descriptor<1>;

        VR_ASSUME_COLD market_data_feed (scope_path const & cfg_path);
        ~market_data_feed ();

        // ACCESSORs:

        /**
         * @note caller must be an RCU reader that is invoking this method inside an
         *       rcu_read_lock()/rcu_read_unlock() "critical section"
         */
        VR_FORCEINLINE poll_descriptor const & poll () const;

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        // steppable:

        VR_ASSUME_HOT void step () final override;


        rt::app_cfg const * m_config { };   // [dep]
        mc::thread_pool * m_threads { };    // [dep]

        std::unique_ptr<pimpl> const m_impl;

        mc::cache_line_padded_field<poll_descriptor *> m_published { }; // [RCU-assigned]

}; // end of class
//............................................................................

inline market_data_feed::poll_descriptor const & // force-inlined
market_data_feed::poll () const
{
    return (* rcu_dereference (m_published.value ()));
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
