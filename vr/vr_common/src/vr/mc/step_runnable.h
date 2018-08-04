#pragma once

#include "vr/asserts.h"
#include "vr/mc/bound_runnable.h"
#include "vr/mc/defs.h"
#include "vr/mc/rcu.h"
#include "vr/mc/step_ctl.h"
#include "vr/mc/steppable.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................
//............................................................................
namespace impl
{

template<int32_t I, int32_t I_LIMIT>
struct step_invoker
{
    vr_static_assert (I_LIMIT > 0);

    static VR_FORCEINLINE void evaluate (std::array<steppable *, I_LIMIT> const & steps)
    {
        steps [I]->step ();
        step_invoker<(I + 1), I_LIMIT>::evaluate (steps); // recurse
    }

}; // end of master

template<int32_t I_LIMIT> // end of recursion
struct step_invoker<I_LIMIT, I_LIMIT>
{
    static VR_FORCEINLINE void evaluate (std::array<steppable *, I_LIMIT> const &)  { }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

class step_runnable: public bound_runnable, public step_ctl
{
    public: // ...............................................................

        static VR_ASSUME_COLD std::unique_ptr<step_runnable> create (int32_t const PU, std::string const & name, step_ctl & ctl,
                                                                     std::vector<steppable *> const & steps);


        VR_ASSUME_COLD void request_stop (); // used by the container/external ctl

    protected: // ............................................................

        VR_ASSUME_COLD step_runnable (int32_t const PU, std::string const & name, step_ctl & ctl);

        // step_ctl:

        VR_ASSUME_COLD void request_stop (steppable const * const requestor) VR_NOEXCEPT final override;
        VR_ASSUME_COLD void report_failure (std::exception_ptr const & eptr) VR_NOEXCEPT final override;
        util::di::container_barrier & sequencer () final override;


        VR_ASSUME_COLD bool rcu_register (steppable * const steps [], int32_t const length);
        VR_ASSUME_COLD void rcu_unregister ();

        VR_ASSUME_COLD bool start_steps (steppable * const steps [], int32_t const length) VR_NOEXCEPT_IF (VR_RELEASE);
        VR_ASSUME_COLD void stop_steps  (steppable * const steps [], int32_t const length) VR_NOEXCEPT_IF (VR_RELEASE);

        static VR_ASSUME_COLD void set_step_ctl (steppable * const steps [], int32_t const length, step_ctl & ctl);


        step_ctl & m_external_ctl;
        rcu_::enum_t m_rcu_mode { rcu_::none };

}; // end of class
//............................................................................
// TODO
// - option to raise thread priority
// - mlock here or where it can be managed appropriately
/**
 * reasons to compose multiple 'steppable's into one:
 *
 *  - have start()/stop() lifecycle methods invoked thread-locally (useful for
 *    thread-local data init) and in the topological dep sort order as determined
 *    by @ref util::di::container;
 *
 *  - be able to strictly bind a single posix/native thread to a PU instead of
 *    relying on the kernel scheduler having to time slice (esp. in tickless setups);
 *
 *  - automatically provide per-thread, per-PU, RCU thread registration and quiescent
 *    period marking;
 *
 *  - eliminate branch mispredicts when individual steps are called (distinct
 *    call sites created with forced loop unrolling will leverage BTB);
 *
 *  - simplify NUMA memory binding policies;
 */
template<int32_t LENGTH>
class step_runnable_ final: public step_runnable
{
    private: // ..............................................................

        vr_static_assert (LENGTH > 0);

        using super         = step_runnable;

    public: // ...............................................................

        VR_ASSUME_COLD step_runnable_ (int32_t const PU, std::string const & name, step_ctl & ctl, std::array<steppable *, LENGTH> && steps) :
            step_runnable (PU, name, ctl),
            m_steps { std::move (steps) }
        {
            super::set_step_ctl (& m_steps [0], LENGTH, * this); // set ourselves as the step controller for 'steps' to be able to interpose
        }

        // runnable:

        VR_ASSUME_HOT void operator() () final override
        {
            super::bind ();
            bool const use_rcu = super::rcu_register (& m_steps [0], LENGTH);
            {
                LOG_info << "PU " << sys::cpuid () << " running " << print (super::name ());

                if (super::start_steps (& m_steps [0], LENGTH)) // after binding
                {
                    LOG_info << "RUNNING: " << print (super::name ());

                    // [run steps only if no startup failures]

                    try
                    {
                        while (VR_UNLIKELY (! stop_flag ().is_raised ())) // spin on a cache line that's local to this PU (and only shared with at most one other)
                        {
                            impl::step_invoker<0, LENGTH>::evaluate (m_steps);

                            if (use_rcu) rcu_quiescent_state (); // we're using QSBR flavor of RCU
                        }

                        LOG_info << "DONE: " << print (super::name ());
                    }
                    catch (...)
                    {
                        if (use_rcu) rcu_quiescent_state (); // we're using QSBR flavor of RCU

                        LOG_error << "*** ABORTED: " << print (super::name ());

                        super::report_failure (std::current_exception ());
                        super::request_stop (); // *after* reporting the failure
                    }
                }

                if (use_rcu) rcu_quiescent_state (); // we're using QSBR flavor of RCU
                super::stop_steps (& m_steps [0], LENGTH); // while still bound [note: this will only stop() 'startable's that succeeded in their start()s]
            }
            super::rcu_unregister ();
            super::unbind ();
        }

    private: // ..............................................................

        std::array<steppable *, LENGTH> const m_steps; // [non-owning]

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
