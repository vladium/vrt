
#include "vr/market/rt/agents/asx/market_data_manager.h"

#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/rt/cfg/app_cfg.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace impl
{
namespace md
{
consume_context::consume_context (arg_map const & args) :
    m_mdv { args },
    m_visitor { { { "view", std::cref (m_mdv) } } }
{
}

} // end of 'md'
} // end of 'impl'
//............................................................................
//............................................................................

void
market_data_manager::start (rt::app_cfg const & config, agent_cfg const & agents, ref_data const & rd, agent_ID const & ID)
{
    m_consume_ctx = std::make_unique<impl::md::consume_context>
    (
        arg_map
        {
            { "agents",     std::cref (agents) },
            { "ref_data",   std::cref (rd) }
        }
    );
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
