#pragma once

#include "vr/rt/timer_queue/defs.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"
#include "vr/util/memory.h"

#include <boost/thread/thread.hpp>

#include <map>

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{

class timer_queue final: public util::di::component, public startable
{
    public: // ...............................................................

        using schedule_spec     = boost::unordered_map<std::string, std::string>;

        timer_queue (schedule_spec const & schedule, int32_t const PU);

        // ACCESSORs:

        timer const & operator[] (std::string const & name) const;

    private: // ..............................................................

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        using timer_ref_set     = std::vector<timer *>;
        using schedule_map      = std::map<timestamp_t, timer_ref_set>;

        class timer_expiration_runnable; // forward


        std::vector<timer> m_timers; // note: storage reserved before 'm_name_map' is populated
        schedule_map m_schedule; // references 'm_timers' slots
        boost::unordered_map<std::string, timer const *> m_name_map; // references 'm_timers' slots
        std::unique_ptr<timer_expiration_runnable> m_runnable;
        boost::thread m_worker { };

}; // end of class

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
