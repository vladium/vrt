#pragma once

#include "vr/io/uri.h"
#include "vr/settings_fwd.h"
#include "vr/util/datetime.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
/**
 * @see agent_cfg
 */
class app_cfg final: public util::di::component
{
    public: // ...............................................................

        app_cfg (io::uri const & source);
        app_cfg (settings const & cfg);
        app_cfg (settings && cfg);

        ~app_cfg ();


        // ACCESSORs:

        /**
         * equivalent to 'scope("")'
         */
        settings const & root () const;

        /**
         * @param path [JSON pointer]
         */
        bool scope_exists (scope_path const & path) const;

        /**
         * @param path [JSON pointer]
         */
        settings const & scope (scope_path const & path) const;

        /**
         * @invariant 'start_time ().date () == start_date ()'
         */
        util::ptime_t const & start_time () const;

        util::date_t start_date () const;

        uint64_t const & rng_seed () const; // note: a function of 'start_time()' unless overridden with "seed"

    private: // ..............................................................

        class pimpl; // forward

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
