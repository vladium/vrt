#pragma once

#include "vr/containers/util/chained_scatter_table.h"
#include "vr/enums.h"
#include "vr/market/sources/asx/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

VR_ENUM (mock_oid_scheme,
    (
        guid,       // easier to follow individual oids
        adversarial // more test stress for the impl
    ),
    printable

); // end of enum
//............................................................................
/**
 */
template<mock_oid_scheme::enum_t OID_SCHEME>
class mock_oid_counter; // master


template<>
class mock_oid_counter<mock_oid_scheme::guid>
{
    public: // ...............................................................

        mock_oid_counter (oid_t const seed = 0) :
            m_oid { seed }
        {
        }

        /*
         * count regardless of 'iid' and 's'
         */
        oid_t operator() (iid_t const iid, market::side::enum_t const s)
        {
            return (++ m_oid);
        }

    private: // ..............................................................

        oid_t m_oid { };

}; // end of specialization

template<>
class mock_oid_counter<mock_oid_scheme::adversarial>
{
    public: // ...............................................................

        mock_oid_counter (oid_t const seed = 0) :
            m_seed { seed }
        {
        }

        /*
         * count per {'iid', 's'}
         */
        oid_t operator() (iid_t const iid, market::side::enum_t const s)
        {
            auto const i = m_oid_map.emplace (std::make_tuple (iid, s), m_seed).first;

            return (++ i->second);
        }

    private: // ..............................................................

        using oid_seq_key   = std::tuple<iid_t, market::side::enum_t>;
        using oid_map       = boost::unordered_map<oid_seq_key, oid_t>;

        oid_t const m_seed;
        oid_map m_oid_map { };

}; // end of specialization

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
