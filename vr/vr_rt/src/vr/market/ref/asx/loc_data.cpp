
#include "vr/market/ref/asx/loc_data.h"

#include "vr/containers/util/chained_scatter_table.h"
#include "vr/io/dao/object_DAO.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

struct loc_data::pimpl final
{
    VR_ASSUME_COLD pimpl (loc_data const & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_cfg_path { cfg_path }
    {
    }


    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_array (), cfg.type ());

        util::date_t const effective_date = config.start_date ();

        LOG_trace1 << "reading locates as of [" << effective_date << "] ...";

        auto & ifc = m_parent.m_dao->ro_ifc<locate> (); // read-only

        // pass 1: populate 'm_symbol_map'

        {
            auto const tx { ifc.tx_begin () }; // do all of the DAO read I/O within one transaction

            for (std::string const & symbol : cfg)
            {
                optional<locate> l = ifc.find_as_of (effective_date, symbol);
                if (VR_UNLIKELY (! l))
                    throw_x (invalid_input, "failed to load " + gd::to_iso_string (effective_date) + " locate for " + print (symbol));

                // TODO ASX-138: check that the effective date of 'l' is current

                m_symbol_map.emplace (symbol, std::move (* l));
            }

            // [ 'tx' rolls back, which is ok for read-only I/O ]
        }

        // pass 2: populate 'm_iid_map':

        for (auto const & kv : m_symbol_map)
        {
            locate const & l = kv.second;

            m_iid_map.put (l.iid (), & l);
        }
        check_eq (m_iid_map.size (), static_cast<iid_map::size_type> (m_symbol_map.size ())); // no duplicate iids

        m_iid_map.rehash (m_iid_map.size ()); // trim to fit

        LOG_trace1 << "loaded " << m_iid_map.size () << " locate(s)";
    }

    VR_ASSUME_COLD void stop ()
    {
        // m_iid_map.clear (); TODO ASX-47
        m_symbol_map.clear ();
    }

    using symbol_map    = boost::unordered_map<std::string, locate>; // TODO a good use case for a perfect hashmap
    using iid_map       = util::chained_scatter_table<iid_t, locate const *, util::identity_hash<iid_t> >; // TODO check iid pdf

    static constexpr iid_map::size_type initial_iid_map_capacity () { return 256; }



    loc_data const & m_parent;
    scope_path const m_cfg_path;
    symbol_map m_symbol_map { };
    iid_map m_iid_map { initial_iid_map_capacity () };

}; // end of class
//............................................................................
//............................................................................

loc_data::loc_data (scope_path const & cfg_path) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    dep (m_config) = "config";
    dep (m_dao) = "DAO";
}

loc_data::~loc_data ()    = default; // pimpl
//............................................................................

void
loc_data::start ()
{
    m_impl->start ();
}

void
loc_data::stop ()
{
    m_impl->stop ();
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
