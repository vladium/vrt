#pragma once

#include "vr/util/object_pools.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

template<std::size_t SIZE, std::size_t ALIGNMENT, typename OPTIONS>
void
fixed_size_allocator<SIZE, ALIGNMENT, OPTIONS>::check () const
{
    using ref_set   = boost::unordered_set<pointer_type>;
    using addr_set  = boost::unordered_set<addr_const_t>;

    // walk the free list and check the structure is sound:

    int64_t const free_node_upper_bound = m_chunk_count * options::chunk_capacity ();
    check_le (free_node_upper_bound, std::numeric_limits<pointer_type>::max ()); // no 'pointer_type' overflow

    fast_pointer_type ref = m_free_list;

    ref_set uniq_refs { };
    addr_set uniq_addrs { };
    bool eol_seen { false };

    for (int64_t count = 0; count /* ! */<= free_node_upper_bound; ++ count)
    {
        if (ref == null_ref ())
        {
            eol_seen = true;
            break;
        }

        addr_const_t const ref_node = dereference (ref);

        // no duplicate refs or node addresses (hence no cycles):
        {
            auto const rc = uniq_refs.insert (ref);
            check_condition (rc.second);
        }
        {
            auto const rc = uniq_addrs.insert (ref_node);
            check_condition (rc.second);
        }

        ref = * static_cast<pointer_type const *> (ref_node);
    }

    check_condition (eol_seen);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
