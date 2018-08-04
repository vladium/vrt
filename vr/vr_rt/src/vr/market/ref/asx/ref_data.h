#pragma once

#include "vr/containers/util/chained_scatter_table.h"
#include "vr/io/dao/object_DAO_fwd.h"
#include "vr/market/ref/asx/instrument.h"
#include "vr/market/rt/cfg/agent_cfg_fwd.h"
#include "vr/market/sources/asx/defs.h" // iid_t
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/settings_fwd.h"
#include "vr/startable.h"
#include "vr/util/di/component.h"

#include <boost/iterator/iterator_facade.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 * this pre-loads reference data that is meant to stay fixed and pinned in memory
 * for the duration of a session
 *
 * @see io::object_DAO
 */
class ref_data final: public util::di::component, public startable
{
    private: // ..............................................................

        class iterator_impl; // forward

    public: // ...............................................................

        using const_iterator    = iterator_impl;

        ref_data (settings const & cfg);

        // ACCESSORs:

        VR_ASSUME_HOT instrument const & operator[] (iid_t const iid) const;
        VR_ASSUME_HOT instrument const & operator[] (std::string const & symbol) const;

        // iteration:

        const_iterator begin () const;
        const_iterator end () const;

    private: // ..............................................................

        using symbol_map    = boost::unordered_map<std::string, instrument>; // TODO a good use case for a perfect hashmap
        using iid_map       = util::chained_scatter_table<iid_t, instrument const *, util::identity_hash<iid_t> >; // TODO check iid pdf

        struct iterator_impl final: public boost::iterator_facade<iterator_impl, instrument const, boost::forward_traversal_tag>
        {
            friend class boost::iterator_core_access;

            iterator_impl (symbol_map::const_iterator const & position) VR_NOEXCEPT :
                m_position { position }
            {
            }

            // iterator_facade:

            void increment ()
            {
                ++ m_position;
            }

            const instrument & dereference () const
            {
                return m_position->second; // TODO except for this deref of the 'instrument' part, this entire iterator impl seems almost redundant
            }

            bool equal (iterator_impl const & rhs) const
            {
                return (m_position == rhs.m_position);
            }

            symbol_map::const_iterator m_position;

        }; // end of nested class

        static constexpr iid_map::size_type initial_iid_map_capacity () { return 256; }

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override;

        // TODO use pimpl here?


        rt::app_cfg const * m_config { };   // [dep]
        agent_cfg const * m_agents { };     // [dep]
        io::object_DAO const * m_dao { };   // [dep]

        symbol_map m_symbol_map { }; // [owning]
        iid_map m_iid_map { initial_iid_map_capacity () }; // value entries reference 'm_symbol_map' slots (after it's been fully populated)

}; // end of class
//............................................................................

inline instrument const &
ref_data::operator[] (iid_t const iid) const
{
    instrument const * const i = m_iid_map.get (iid);
    if (VR_UNLIKELY (i == nullptr))
        throw_x (invalid_input, "invalid iid " + string_cast (iid));

    assert_nonnull (i);
    return (* i);
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
