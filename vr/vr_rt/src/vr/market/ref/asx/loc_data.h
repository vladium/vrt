#pragma once

#include "vr/io/dao/object_DAO_fwd.h"
#include "vr/market/ref/asx/locate.h"
#include "vr/market/sources/asx/defs.h" // iid_t
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
 * @note this is used for pre-loading start-of-the-session state only (immutable
 *       after @ref start()); runtime locates are managed elsewhere
 *       (e.g. as custom book state)
 */
class loc_data final: public util::di::component, public startable
{
    public: // ...............................................................

        loc_data (scope_path const & cfg_path);
        ~loc_data ();

        // ACCESSORs:

        VR_ASSUME_HOT locate const & operator[] (iid_t const iid) const;
        VR_ASSUME_HOT locate const & operator[] (std::string const & symbol) const;

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        rt::app_cfg const * m_config { };   // [dep]
        io::object_DAO const * m_dao { };   // [dep]

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
