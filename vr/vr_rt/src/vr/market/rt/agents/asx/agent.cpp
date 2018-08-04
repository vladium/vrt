
#include "vr/market/rt/agents/asx/agent.h"

#include "vr/market/ref/asx/ref_data.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

static agent_ID
extract_ID (scope_path const cfg_path)
{
    string_vector const tokens = util::split (cfg_path, "/");
    check_nonempty (tokens);

    agent_ID const r { tokens.back () };
    check_nonempty (r);

    return r;
}

//............................................................................
//............................................................................
namespace impl
{

agent_base::agent_base (scope_path const cfg_path) :
    market_data_manager (),
    execution_manager (extract_ID (cfg_path)), // note: this base constructor makes 'ID ()' accessor valid
    m_cfg_path { cfg_path }
{
    dep (m_mdf) = "mdf";    // note: dep inherited from 'market_data_manager' but bound here
    dep (m_xl)  = "xl";     // note: dep inherited from 'execution_manager' but bound here

    dep (m_config) = "config";
    dep (m_agents) = "agents";
    dep (m_ref_data) = "ref_data";
}
//............................................................................

void
agent_base::start ()
{
    // make a copy of this agent's config settings:
    // (these could be interpreted as class name + constructor args once ASX-137) is in full swing)
    {
        settings const & cfg = config ().scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_array () && (cfg.size () == 2), cfg.type (), cfg);

        m_parameters = cfg [1];
        check_condition (m_parameters.is_object (), m_parameters);
    }

    market_data_manager::start (config (), agents (), refdata (), ID ());
    execution_manager::start (config (), agents ());
}

void
agent_base::stop ()
{
    // TODO this will likely need to hook into a graceful shutdown protocol
}
//............................................................................

rt::app_cfg const &
agent_base::config () const
{
    assert_nonnull (m_config, ID ());

    return (* m_config);
}

agent_cfg const &
agent_base::agents () const
{
    assert_nonnull (m_agents, ID ());

    return (* m_agents);
}

ref_data const &
agent_base::refdata () const
{
    assert_nonnull (m_ref_data, ID ());

    return (* m_ref_data);
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
