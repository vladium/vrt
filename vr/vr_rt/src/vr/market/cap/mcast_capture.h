#pragma once

#include "vr/filesystem.h"
#include "vr/io/net/mcast_source.h"
#include "vr/rt/timer_queue/timer_queue_fwd.h"
#include "vr/util/di/component.h"
#include "vr/mc/bound_runnable.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
/**
 */
class mcast_capture final: public util::di::component, public mc::bound_runnable
{
    public: // ...............................................................

        struct stats final
        {
            int64_t m_total { };
            int64_t m_dropped { };
            int64_t m_filtered { };

        }; // end of nested class

        mcast_capture (std::string const & ifc, std::vector<io::net::mcast_source> const & sources, io::net::ts_policy::enum_t const tsp,
                       fs::path const & out_dir, std::string const & capture_ID,
                       int32_t const PU);


        /**
         * @return [valid after run() completes]
         */
        stats const & capture_stats () const;

        // runnable:

        VR_ASSUME_HOT void operator() () final override;

    private: // ..............................................................

        rt::timer_queue const * m_timer_queue { };  // [dep]

        std::string const m_ifc;
        std::vector<io::net::mcast_source> const m_sources;
        fs::path const m_out_file;
        io::net::ts_policy::enum_t const m_tsp;
        stats m_stats { };

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
