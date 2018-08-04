#pragma once

#include "vr/types.h"

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

// TODO move outside 'ASX' ns if sizing logic is generic:

template<typename OBJECT_POOLS>
struct market_data_view_pool_arena: private noncopyable
{
    // TODO clever logic for initial pool sizing specific to market data

    OBJECT_POOLS const & object_pools () const
    {
        return m_object_pools;
    }

    OBJECT_POOLS & object_pools ()
    {
        return m_object_pools;
    }

    OBJECT_POOLS m_object_pools { };

}; // end of class

template<typename OBJECT_POOLS>
struct execution_view_pool_arena: private noncopyable
{
    // TODO clever logic for initial pool sizing specific to execution

    OBJECT_POOLS const & object_pools () const
    {
        return m_object_pools;
    }

    OBJECT_POOLS & object_pools ()
    {
        return m_object_pools;
    }

    OBJECT_POOLS m_object_pools { };

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
