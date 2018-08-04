#pragma once

#include "vr/io/sql/sqlite/sqlite_connection.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 */
class sql_connection_factory final: public util::di::component, public startable
{
    public: // ...............................................................

        using connection    = sqlite_connection;

        VR_ASSUME_COLD sql_connection_factory (settings const & named_cfgs);
        ~sql_connection_factory ();


        connection & acquire (std::string const & name);
        void release (connection const & c);

    private: // ..............................................................

        class pimpl; // forward

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
