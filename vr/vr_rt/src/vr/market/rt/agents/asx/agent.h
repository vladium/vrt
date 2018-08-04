#pragma once

#include "vr/market/ref/asx/ref_data_fwd.h"
#include "vr/market/rt/agents/asx/execution_manager.h"
#include "vr/market/rt/agents/asx/market_data_manager.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/mc/steppable.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/startable.h"
#include "vr/util/logging.h"

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
/*
 * impl note: it might have been cleaner to implement this class as a sub-container -- it would
 * have certainly made dep resolution easy;
 */
class agent_base: public mc::steppable_<mc::rcu<_reader_>>, public util::di::component, public startable,
                  public market_data_manager, public execution_manager
{
    protected: // ............................................................

        using market_data   = market_data_manager;
        using execution     = execution_manager;

        /**
         * @param cfg_path in the form '/.../<agent ID>'
         */
        agent_base (scope_path const cfg_path);


        // ACCESSORs:

        /**
         * @return a json object contained in the app config section identify by the 'cfg_path'
         *          passed into the constructor
         */
        settings const & parameters () const
        {
            return m_parameters;
        }

        VR_ASSUME_COLD rt::app_cfg const & config () const;
        ref_data const & refdata () const;

        // execution state:



        // startable:

        /**
         * @note DERIVED must chain
         */
        void start () override;
        /**
         * @note DERIVED must chain
         */
        void stop () override;

        // MUTATORs:

        VR_ASSUME_HOT void update ()
        {
            market_data::update ();
            execution::update ();
        }

    private: // ..............................................................

        VR_ASSUME_COLD agent_cfg const & agents () const;

        rt::app_cfg const * m_config { };   // [dep]
        agent_cfg const * m_agents { };     // [dep]
        ref_data const * m_ref_data { };    // [dep]

        // TODO time source
        // TODO timer event queue for strategy needs

        scope_path const m_cfg_path;
        settings m_parameters { };

}; // end of class
//............................................................................

template<typename T, typename = void>
struct has_evaluate: std::false_type
{
}; // end of master

template<typename T>
struct has_evaluate<T, util::void_t<decltype (std::declval<T> ().evaluate ())>>: std::true_type
{
}; // end of specialization

}; // end of 'impl'
//............................................................................
//............................................................................
/**
 * TODO doc DERIVED requirements and input assertions
 *
 */
template<typename DERIVED>
class agent: public impl::agent_base
{
    private: // ..............................................................

        using super         = impl::agent_base;
        using this_type     = agent<DERIVED>;

    public: // ...............................................................

        using super::super; // inherit constructors

        // execution:

        using super::order_type;

    protected: // ............................................................

        // steppable:

        VR_ASSUME_HOT void step () final override
        {
            vr_static_assert (std::is_base_of<this_type, DERIVED>::value); // 'DERIVED' needs to, well, derived from 'agent'
            vr_static_assert (impl::has_evaluate<DERIVED>::value); // there must be a 'DERIVED::evaluate ()'


            super::update (); // TODO pass results into evaluate() - or- provide accessors?

            if (VR_LIKELY (state () == state::running)) // split off the frequent case
            {
                static_cast<DERIVED *> (this)->evaluate ();
            }
            else // a trivial FSM for now, will likely get more elaborate
            {
                login_transition ();
            }
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
