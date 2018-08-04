#pragma once

#include "vr/asserts.h"
#include "vr/market/rt/cfg/defs.h" // liid_descriptor
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
/**
 * complements @ref rt::app_cfg with market/strategy-specific derived info
 */
/*
 * TODO at the moment, this doesn't need to be 'startable' because it
 * only uses 'app_cfg'; but keeping the impl as is for when need to look
 * at ref/locate data
 */
class agent_cfg final: public util::di::component, public startable // TODO rename
{
    private: // ..............................................................

        using agent_map = boost::unordered_map<agent_ID, agent_descriptor>;

        struct agent_scope final
        {
            using const_iterator    = agent_map::const_iterator;

            int32_t size () const;

            // lookup:

            agent_descriptor const & operator[] (call_traits<agent_ID>::param ID) const;

            // iteration:

            const_iterator begin () const;
            const_iterator end () const;

            private: friend class agent_cfg;

                agent_scope (agent_cfg const & parent) : m_parent { parent } { };
                agent_cfg const & m_parent;

        }; // end of nested class

    public: // ...............................................................

        agent_cfg (scope_path const & cfg_path);

        // ACCESSORs:

        // symbols/instruments:

        VR_ASSUME_HOT liid_t const & liid_limit () const;

        /**
         * @return C-array of size @ref liid_limit()
         */
        VR_FORCEINLINE liid_descriptor const * VR_RESTRICT liid_table () const;

        // agents:

        agent_scope agents () const;

    private: // ..............................................................

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;


        rt::app_cfg const * m_config { };       // [dep]

        std::unique_ptr<liid_descriptor [/* liid */]> m_liid_table { };
        liid_t m_liid_limit { };
        int32_t m_agent_count { };
        agent_map m_agent_descriptors { };
        scope_path const m_cfg_path;

}; // end of class
//............................................................................

inline liid_t const &
agent_cfg::liid_limit () const
{
    return m_liid_limit;
}
//............................................................................

inline liid_descriptor const * VR_RESTRICT // force-inlined
agent_cfg::liid_table () const
{
    assert_nonnull (m_liid_table);

    return m_liid_table.get ();
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
