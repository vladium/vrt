
#include "vr/market/ref/asx/ref_data.h"

#include "vr/io/dao/object_DAO.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/datetime.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

ref_data::ref_data (settings const & cfg)
{
    dep (m_config) = "config";
    dep (m_agents) = "agents";
    dep (m_dao) = "DAO";
}
//............................................................................

void
ref_data::start ()
{
    liid_descriptor const * const liid_table = m_agents->liid_table ();
    liid_t const liid_limit = m_agents->liid_limit ();

    auto & ifc = m_dao->ro_ifc<instrument> (); // read-only

    util::date_t const effective_date = m_config->start_date ();

    LOG_trace1 << "reading instrument definitions as of [" << effective_date << "] ...";

    // pass 1: populate 'm_symbol_map'

    {
        auto const tx { ifc.tx_begin () }; // do all of the DAO read I/O within one transaction

        for (liid_t liid = 0; liid < liid_limit; ++ liid)
        {
            std::string const & symbol = liid_table [liid].m_symbol;

            optional<instrument> i = ifc.find_as_of (effective_date, symbol);
            if (VR_UNLIKELY (! i)) // for now, guard against delisted symbols TODO 'strict' mode for prod
            {
                LOG_warn << "failed to load instrument definition for " << print (symbol) << ", skipping";
                continue;
            }

            m_symbol_map.emplace (symbol, std::move (* i));
        }

        // [ 'tx' rolls back, which is ok for read-only I/O ]
    }

    // pass 2: populate 'm_iid_map':

    for (auto const & kv : m_symbol_map)
    {
        instrument const & i = kv.second;

        m_iid_map.put (i.iid (), & i);
    }
    check_eq (m_iid_map.size (), static_cast<iid_map::size_type> (m_symbol_map.size ())); // no duplicate iids

    m_iid_map.rehash (m_iid_map.size ()); // trim to fit

    LOG_trace1 << "loaded " << m_iid_map.size () << " instrument definition(s)";
}

void
ref_data::stop ()
{
    // m_iid_map.clear (); TODO ASX-47
    m_symbol_map.clear ();
}
//............................................................................

instrument const &
ref_data::operator[] (std::string const & symbol) const
{
    auto const i = m_symbol_map.find (symbol);
    if (VR_UNLIKELY (i == m_symbol_map.end ()))
        throw_x (invalid_input, "invalid symbol " + print (symbol));

    return i->second;
}
//............................................................................

ref_data::const_iterator
ref_data::begin () const
{
    return { m_symbol_map.begin () };
}

ref_data::const_iterator
ref_data::end () const
{
    return { m_symbol_map.end () };
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
