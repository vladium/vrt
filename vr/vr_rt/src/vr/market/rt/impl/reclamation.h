#pragma once

#include "vr/macros.h"

struct rcu_head; // forward

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
/*
 * invoked from the RCU callback thread TODO move elsewhere?
 *
 * @see mc::thread_pool
 */
extern VR_SO_LOCAL void
release_poll_descriptor (::rcu_head * const rh);

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
