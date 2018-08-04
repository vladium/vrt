
#include "vr/market/rt/impl/reclamation.h"

#include "vr/market/rt/defs.h" // recv_*
#include "vr/mc/atomic.h"
#include "vr/mc/mc.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

void
release_poll_descriptor (::rcu_head * const rh)
{
    recv_reclaim_context * const rcl_ctx = reinterpret_cast<recv_reclaim_context *> (rh); // std layout asserted

    // push parent of 'rcl_ctx' [lock-free]:
    {
        std::atomic<recv_reclaim_context::ref_type> & pd_reclaim_head { * reinterpret_cast<std::atomic<recv_reclaim_context::ref_type> *> (rcl_ctx->m_parent_reclaim_head) };

        recv_reclaim_context::ref_type const new_head = rcl_ctx->m_instance;
        DLOG_trace2 << "pushing new reclaim head " << new_head;
        assert_nonnegative (new_head);

        mc::volatile_cast (rcl_ctx->m_next) = pd_reclaim_head.load (std::memory_order_relaxed);

        while (! pd_reclaim_head.compare_exchange_weak (rcl_ctx->m_next, new_head, std::memory_order_release, std::memory_order_relaxed))
        ;
    }
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
