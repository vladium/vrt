
#include "vr/util/di/container_barrier.h"

#include "vr/mc/mc.h"
#include "vr/startable.h"
#include "vr/util/classes.h"
#include "vr/util/di/container.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
//............................................................................

container_barrier::container_barrier (container & parent, std::size_t const latch_count, turn_map && turns) :
    m_parent { parent },
    m_turns { std::move (turns) },
    m_start_latch { latch_count },
    m_stop_latch { latch_count }
{
}
//............................................................................

void
container_barrier::start_begin ()
{
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        mc::volatile_cast (m_turn) = 0;
    }
    m_cv.notify_all ();
}

void
container_barrier::start_end_wait ()
{
    m_start_latch.count_down_and_wait ();
}
//............................................................................

void
container_barrier::run_end_wait ()
{
    m_stop_latch.count_down_and_wait ();
}
//............................................................................

void
container_barrier::stop_begin ()
{
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        mc::volatile_cast (m_turn) = 1 - mc::volatile_cast (m_turn);
    }
    m_cv.notify_all ();
}

void
container_barrier::stop_end ()
{
}
//............................................................................

bool
container_barrier::start (startable & cs) VR_NOEXCEPT_IF (VR_RELEASE)
{
    bool ok { false };

    component * const c = dynamic_cast<component *> (& cs);
    assert_nonnull (c);

    auto const i = m_turns.find (& cs);
    assert_condition (i != m_turns.end (), util::instance_name (& cs));

    int32_t const cs_turn = i->second;
    std::string const c_name = m_parent.name_of (c); // copy of name str

    LOG_trace3 << print (c_name) << ": waiting for start turn " << cs_turn;
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        while (cs_turn != mc::volatile_cast (m_turn))
        {
            m_cv.wait (lock);
        }
    }

    if (cs_turn == m_start_count) // only start 'startable's up until the first failure (incl. from another thread)
    {
        LOG_trace2 << "\t[" << cs_turn << "] starting " << print (c_name) << ' ' << instance_name (c) << " ...";

        try
        {
            cs.start ();

            ok = true;
            ++ m_start_count; // increase atomically
            LOG_trace2 << "\t[" << cs_turn << "] started " << print (c_name) << ' ' << instance_name (c);
        }
        catch (...)
        {
            // wrap in 'lifecycle_exception' and report to the parent container:

            try { chain_x (lifecycle_exception, "component " + print (c_name) + " FAILED to start on turn " + string_cast (cs_turn) + ':'); }
            catch (...)
            {
                m_parent.report_failure (true, std::current_exception ());
            }
        }
    }

    LOG_trace3 << print (c_name) << ": setting start turn to " << (cs_turn + 1);
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        mc::volatile_cast (m_turn) = cs_turn + 1;
    }
    m_cv.notify_all ();

    return ok;
}

void
container_barrier::stop (startable & cs) VR_NOEXCEPT_IF (VR_RELEASE)
{
    component * const c = dynamic_cast<component *> (& cs);
    assert_nonnull (c);

    auto const i = m_turns.find (& cs);
    assert_condition (i != m_turns.end (), util::instance_name (& cs));

    int32_t const cs_turn = -/* ! */i->second;
    std::string const c_name = m_parent.name_of (c); // copy of name str

    LOG_trace3 << print (c_name) << ": waiting for stop turn " << cs_turn;
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        while (cs_turn != mc::volatile_cast (m_turn))
        {
            m_cv.wait (lock);
        }
    }

    if (-/* ! */cs_turn < m_start_count) // only stop 'startable's with turns that succeeded
    {
        LOG_trace2 << "\t[" << cs_turn << "] stopping " << print (c_name) << ' ' << instance_name (c) << " ...";

        try
        {
            cs.stop ();
            LOG_trace2 << "\t[" << cs_turn << "] stopped " << print (c_name) << ' ' << instance_name (c);
        }
        catch (...)
        {
            // wrap in 'lifecycle_exception' and report to the parent container:

            try { chain_x (lifecycle_exception, "component " + print (c_name) + " FAILED to stop on turn " + string_cast (cs_turn) + ':'); }
            catch (...)
            {
                m_parent.report_failure (false, std::current_exception ());
            }
        }
    }

    LOG_trace3 << print (c_name) << ": setting stop turn to " << (cs_turn + 1);
    {
        boost::unique_lock<boost::mutex> lock { m_cv_mutex };

        mc::volatile_cast (m_turn) = cs_turn + 1;
    }
    m_cv.notify_all ();
}

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
