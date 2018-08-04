
#include "vr/mc/thread_pool.h"

#include "vr/mc/atomic.h"
#include "vr/mc/rcu.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

#include <functional>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

struct thread_pool::pimpl final
{
    VR_ASSUME_COLD pimpl (thread_pool & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_cfg_path { cfg_path }
    {
        LOG_trace1 << "configured with scope path " << print (cfg_path);
    }

    VR_ASSUME_COLD ~pimpl ()
    {
        ::call_rcu_data * const rcu_cb_thread_handle = m_rcu_cb_thread_handle;
        if (rcu_cb_thread_handle != nullptr)
        {
            LOG_trace1 << "freeing rcu cb data @ " << rcu_cb_thread_handle << " ...";

            call_rcu_data_free (rcu_cb_thread_handle);
        }
    }


    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_object (), cfg.type ());

        {
            uint64_t rcu_cb_thread_flags { };   // not 'URCU_CALL_RCU_RT' by default
            int32_t rcu_cb_thread_PU { -1 };    // not affinitied by default

            if (cfg.count ("rcu"))
            {
                settings const & rcu_cfg = cfg ["rcu"];

                bool const rcu_cb_thread_RT = rcu_cfg.value ("use_RT_callback", false);
                if (rcu_cb_thread_RT) rcu_cb_thread_flags |= URCU_CALL_RCU_RT;

                rcu_cb_thread_PU = rcu_cfg.value ("callback_PU", rcu_cb_thread_PU);
            }

            // do actual callback thread creation lazily, on first attach request:

            m_create_rcu_cb_thread_closure = [this, rcu_cb_thread_flags, rcu_cb_thread_PU]()
                {
                    m_rcu_cb_thread_handle = create_call_rcu_data (rcu_cb_thread_flags, rcu_cb_thread_PU);
                };
        }
    }

    VR_ASSUME_COLD void stop ()
    {
        // [deletion of 'm_rcu_cb_thread_handle' is done in the destructor to avoid racing with container barrier]
    }


    VR_ASSUME_COLD void attach_to_rcu_callback_thread ()
    {
        LOG_trace1 << "attaching (TID " << sys::gettid () << ", pthread_t 0x" << std::hex << sys::pthreadid () << std::dec << ") to the RCU callback thread";

        // verify that the calling thread doesn't have cb handle set already:

        ::call_rcu_data * const caller_handle = get_thread_call_rcu_data ();
        check_null (caller_handle, sys::gettid (), sys::pthreadid ());

        // set 'm_rcu_cb_thread_handle' lazily on first invocation:

        ::call_rcu_data * handle = m_rcu_cb_thread_handle;

        if (handle == nullptr) // no need for double-checking, not perf-critical
        {
            m_create_rcu_cb_thread_closure ();
            handle = m_rcu_cb_thread_handle.load (std::memory_order_relaxed);

            check_nonnull (handle);
            LOG_trace1 << "created rcu cb data @ " << handle;
        }

        set_thread_call_rcu_data (handle);
    }

    VR_ASSUME_COLD void detach_from_rcu_callback_thread ()
    {
        LOG_trace1 << "detaching (TID " << sys::gettid () << ", pthread_t 0x" << std::hex << sys::pthreadid () << std::dec << ") from the RCU callback thread";

        set_thread_call_rcu_data (nullptr);

        VR_IF_DEBUG // verify:
        (
            ::call_rcu_data * const caller_handle = get_thread_call_rcu_data ();
            assert_null (caller_handle, sys::gettid (), sys::pthreadid ());
        )
    }


    thread_pool & m_parent;
    scope_path const m_cfg_path;
    std::atomic<::call_rcu_data *> m_rcu_cb_thread_handle { };
    std::function<void ()> m_create_rcu_cb_thread_closure { };

}; // end of nested class
//............................................................................
//............................................................................

thread_pool::thread_pool (scope_path const & cfg_path) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    dep (m_config) = "config";
}

thread_pool::~thread_pool ()    = default; // pimpl
//............................................................................

void
thread_pool::start ()
{
    m_impl->start ();
}

void
thread_pool::stop ()
{
    m_impl->stop ();
}
//............................................................................

void
thread_pool::attach_to_rcu_callback_thread ()
{
    m_impl->attach_to_rcu_callback_thread ();
}

void
thread_pool::detach_from_rcu_callback_thread ()
{
    m_impl->detach_from_rcu_callback_thread ();
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
