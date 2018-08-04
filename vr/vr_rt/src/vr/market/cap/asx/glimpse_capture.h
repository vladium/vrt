#pragma once

#include "vr/filesystem.h"
#include "vr/rt/timer_queue/timer_queue_fwd.h"
#include "vr/runnable.h"
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
class glimpse_capture final: public util::di::component, public runnable
{
    public: // ...............................................................

        glimpse_capture (std::string const & server, uint16_t const port_base, bool const disable_nagle, std::vector<int64_t> const & login_seqnums,
                         fs::path const & out_dir, std::string const & capture_ID,
                         int32_t const PU);

        ~glimpse_capture ();


        // runnable:

        VR_ASSUME_HOT void operator() () final override;

    private: // ..............................................................

        class pimpl; // forward


        rt::timer_queue const * m_timer_queue { };  // [dep]

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
