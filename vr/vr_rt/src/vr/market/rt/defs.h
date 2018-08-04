#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/rcu.h" // rcu_head
#include "vr/io/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

struct recv_position final
{
    io::pos_t m_pos;        // cumulative count of bytes received
    addr_const_t m_end;     // points just past the available data
    timestamp_t m_ts_local; // '_ts_last_recv_' from the link that supplied the latest version

}; // end of class
//............................................................................
/*
 * base of 'recv_descriptor' used to support chaining objects into
 * reclamation slists as used with 'call_rcu()':
 */
struct recv_reclaim_context
{
    using ref_type          = int32_t;

    // [TODO actually make non-public]

        ::rcu_head m_rcu_head;  // for 'call_rcu()'
        ref_type * m_parent_reclaim_head;
        ref_type m_instance;    // ref within the parent pool
        ref_type m_next;        // negative value means 'null'

}; // end of class
//............................................................................

template<int32_t WIDTH>
struct VR_ALIGNAS_CL recv_descriptor final: public std::array<recv_position, WIDTH>,
                                            public recv_reclaim_context
{
    using data_array                    = std::array<recv_position, WIDTH>; // payload
    static constexpr int32_t width ()   { return WIDTH; }

    void clear () // payload only
    {
        __builtin_memset (this->data (), 0, sizeof (data_array));
    }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
