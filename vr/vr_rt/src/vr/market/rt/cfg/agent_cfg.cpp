
#include "vr/market/rt/cfg/agent_cfg.h"

#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/logging.h"
#include "vr/util/random.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

int32_t
agent_cfg::agent_scope::size () const
{
    return m_parent.m_agent_descriptors.size ();
}
//............................................................................

agent_descriptor const &
agent_cfg::agent_scope::operator[] (call_traits<agent_ID>::param ID) const
{
    auto const i = m_parent.m_agent_descriptors.find (ID);
    if (VR_UNLIKELY (i == m_parent.m_agent_descriptors.end ()))
        throw_x (invalid_input, "invalid agent ID " + print (ID));

    return i->second;
}
//............................................................................

agent_cfg::agent_scope::const_iterator
agent_cfg::agent_scope::begin () const
{
    return m_parent.m_agent_descriptors.begin ();
}

agent_cfg::agent_scope::const_iterator
agent_cfg::agent_scope::end () const
{
    return m_parent.m_agent_descriptors.end ();
}
//............................................................................
//............................................................................

agent_cfg::agent_cfg (scope_path const & cfg_path) :
    m_cfg_path { cfg_path }
{
    dep (m_config) = "config";
}
//............................................................................

void
agent_cfg::start ()
{
    rt::app_cfg const & config = (* m_config);

    uint64_t const rng_seed = config.rng_seed ();
    LOG_trace1 << "[seed = " << rng_seed << ']';

    // parse agent cfg(s):

    auto const & agents = config.scope (m_cfg_path);
    check_condition (agents.is_object (), agents.type ()); // a map of { aid -> { json ptr to strategy def, parms }

    // pass 1: all agents have traded instruments defined

    boost::unordered_set<std::string> all_symbols { };
    {
        for (auto ai = agents.begin (); ai != agents.end (); ++ ai)
        {
            std::string const aid = ai.key ();

            auto const & agent_cfg = ai.value ();
            check_condition (agent_cfg.is_array () && (agent_cfg.size () == 2), agent_cfg);

            // note: it is ok for different agent IDs to share/point to the same strategy
            // but traded instrument overlap is currently an error

            // get instruments traded by 'aid' as a standard agent cfg parm:
            {
                settings const & parms = agent_cfg.at (1);
                check_condition (parms.is_object (), parms);

                auto const & instruments = parms.at ("instruments");
                check_nonempty (instruments);

                for (std::string const & s : instruments)
                {
                    if (VR_UNLIKELY (! all_symbols.insert (s).second))
                        throw_x (invalid_input, "duplicate traded instrument " + print (s));
                }
            }
        }
    }
    check_nonempty (all_symbols);

    m_liid_limit = all_symbols.size ();

    m_liid_table = std::make_unique<liid_descriptor []> (m_liid_limit); // filled in the following loop
    boost::unordered_map<std::string, liid_t> mapping { }; // filled in the following loop

    // pass 2:
    {
        std::unique_ptr<liid_t []> const liids_shuffled { std::make_unique<liid_t []> (m_liid_limit) };
        {
            for (liid_t j = 0; j < m_liid_limit; ++ j) liids_shuffled [j] = j;

            util::random_shuffle (& liids_shuffled [0], all_symbols.size (), rng_seed);
        }

        int32_t k = 0;
        for (std::string const & symbol : all_symbols)
        {
            liid_t const liid = liids_shuffled [k ++]; // note: side-effect

            mapping.emplace (symbol, liid);
            m_liid_table [liid].m_symbol = symbol;
        }
    }

    // pass 3: populate 'm_agent_IDs' (this also checks their ID uniqueness) and 'm_agent_descriptors'
    {
        agent_map agent_descriptors { };

        for (auto ai = agents.begin (); ai != agents.end (); ++ ai)
        {
            std::string const id = ai.key ();

            auto const & agent_cfg = ai.value ();
            assert_condition (agent_cfg.is_array () && (agent_cfg.size () == 2), agent_cfg);

            agent_ID const aid { id };

            auto aj = agent_descriptors.emplace (aid, agent_descriptor { }).first;
            if (VR_UNLIKELY (aj == agent_descriptors.end ()))
                throw_x (invalid_input, "duplicate agent ID " + print (aid));

            agent_descriptor & a_desc = aj->second;
            symbol_liid_relation & a_symbol_mapping = a_desc.m_symbol_mapping;

            {
                settings const & parms = agent_cfg.at (1);
                assert_condition (parms.is_object (), parms);

                auto const & instruments = parms.at ("instruments");

                for (std::string const & s : instruments)
                {
                    liid_t const & liid = mapping.find (s)->second;

                    a_symbol_mapping.insert (symbol_liid_relation::value_type { s, liid });
                }
            }

            LOG_trace1 << "agent " << print (aid) << " uses " << a_symbol_mapping.size () << " instrument(s)";
        }

        m_agent_descriptors = std::move (agent_descriptors); // note: last use of 'agent_descriptors'
    }
}

void
agent_cfg::stop ()
{
    m_agent_descriptors.clear ();

    // [keep 'm_liid_table' around just to be lenient]
}
//............................................................................

agent_cfg::agent_scope
agent_cfg::agents () const
{
    return { * this };
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
