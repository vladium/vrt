
#include "vr/mc/step_runnable.h"

#include "vr/startable.h"
#include "vr/util/di/container_barrier.h"

#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

std::unique_ptr<step_runnable>
step_runnable::create (int32_t const PU, std::string const & name, step_ctl & ctl, std::vector<steppable *> const & steps)
{
    switch (steps.size ())
    {
        case 0: throw_x (invalid_input, "a non-zero number of steps are required for the composite");

#   define vr_CASE(z, LENGTH, data)  \
        case LENGTH : \
        { \
            std::array<steppable *, LENGTH > a { }; \
            std::copy (steps.begin (), steps.end (), a.begin ()); \
            \
            return std::unique_ptr<step_runnable> { new step_runnable_< LENGTH > { PU, name, ctl, std::move (a) } }; /* last use of 'a' */ \
        } \
        /* */

        BOOST_PP_REPEAT_FROM_TO (1, 7, vr_CASE, unused)

#   undef vr_CASE

        default: throw_x (invalid_input, "number of steps (" + string_cast (steps.size ()) + ") exceeds current impl limit");

    } // end of switch
}
//............................................................................

step_runnable::step_runnable (int32_t const PU, std::string const & name, step_ctl & ctl) :
    bound_runnable (PU, name),
    m_external_ctl { ctl }
{
    LOG_trace1 << print (bound_runnable::name ()) << ": configured to bind to PU " << PU;
}
//............................................................................

bool
step_runnable::rcu_register (steppable * const steps [], int32_t const length)
{
    bool rc { false };
    int32_t mode { };

    for (int32_t i = 0; i < length; ++ i)
    {
        steppable const * const s = steps [i];
        assert_nonnull (s, i);

        rcu_marker const * m = dynamic_cast<rcu_marker const *> (s);
        if (m)
        {
            rcu_::enum_t const mm = m->rcu_mode ();
            LOG_trace1 << print (name ()) << ": step " << i << " (" << util::instance_name (s) << ") is RCU " << mm;

            mode |= mm;
        }
    }

    if (VR_UNLIKELY (mode == (rcu_::reader | rcu_::writer)))
        throw_x (invalid_input, print (name ()) + " can't be both RCU reader and writer");

    m_rcu_mode = static_cast<rcu_::enum_t> (mode);
    if (m_rcu_mode != rcu_::none)
    {
        LOG_trace1 << print (name ()) << ": registering as RCU " << m_rcu_mode;

        rcu_register_thread ();
        rc = true;
    }

    return rc;
}

void
step_runnable::rcu_unregister ()
{
    if (m_rcu_mode != rcu_::none)
    {
        LOG_trace1 << print (name ()) << ": unregistering as RCU " << m_rcu_mode;

        rcu_unregister_thread();
    }
}
//............................................................................

bool
step_runnable::start_steps (steppable * const steps [], int32_t const length) VR_NOEXCEPT_IF (VR_RELEASE)
{
    bool ok { true };

    util::di::container_barrier & seq = sequencer ();

    for (int32_t i = 0; i < length; ++ i)
    {
        steppable * const s = steps [i];
        assert_nonnull (s, i);

        startable * const s_startable = dynamic_cast<startable *> (s);
        if (s_startable == nullptr) continue; // 's' is not a 'startable and hence has no turn allocated for it

        LOG_trace2 << "  PU [" << PU () << "]: starting " << util::instance_name (s_startable);

        // [note: the sequencer will catch any failures and report to the container]

        ok &= seq.start (* s_startable); // noexcept
    }

    seq.start_end_wait (); // join the start latch

    return ok;
}

void
step_runnable::stop_steps  (steppable * const steps [], int32_t const length) VR_NOEXCEPT_IF (VR_RELEASE)
{
    util::di::container_barrier & seq = sequencer ();

    seq.run_end_wait (); // join the end-of-run latch

    for (int32_t i = length; -- i >= 0; ) // note: stop in inverse order of starting
    {
        steppable * const s = steps [i];
        assert_nonnull (s, i);

        startable * const s_startable = dynamic_cast<startable *> (s);
        if (s_startable == nullptr) continue; // 's' is not a 'startable' and hence has no turn allocated for it

        LOG_trace2 << "  PU [" << PU () << "]: stopping " << util::instance_name (s_startable);
        seq.stop (* s_startable);
    }
}
//............................................................................

void
step_runnable::request_stop ()
{
    LOG_trace1 << "stop requested externally";

    request_stop (nullptr); // broadcast
}
//............................................................................

void
step_runnable::request_stop (steppable const * const requestor) VR_NOEXCEPT
{
    if (requestor) LOG_trace1 << "stop request from " << util::instance_name (requestor);

    bound_runnable::stop_flag ().raise ();

    m_external_ctl.request_stop (requestor); // delegate
}

void
step_runnable::report_failure (std::exception_ptr const & eptr) VR_NOEXCEPT
{
    m_external_ctl.report_failure (eptr); // delegate
}

util::di::container_barrier &
step_runnable::sequencer ()
{
    return m_external_ctl.sequencer (); // delegate
}
//............................................................................

void
step_runnable::set_step_ctl (steppable * const steps [], int32_t const length, step_ctl & ctl)
{
    for (int32_t s = 0; s < length; ++ s)
    {
        check_nonnull (steps [s], s);
        steps [s]->m_step_ctl = & ctl;
    }
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
