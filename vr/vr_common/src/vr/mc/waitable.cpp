
#include "vr/mc/waitable.h"

#include "vr/sys/os.h"

#include <chrono>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

bool
waitable::is_signalled () const
{
    {
        std::unique_lock<std::mutex> lock { m_cv_mutex };

        return m_signalled;
    }
}
//............................................................................

void
waitable::wait ()
{
    {
        std::unique_lock<std::mutex> lock { m_cv_mutex };

        while (! m_signalled)
        {
             m_cv.wait (lock);
        }
    }
}

bool
waitable::wait_for (timestamp_t const duration)
{
    {
        std::unique_lock<std::mutex> lock { m_cv_mutex };

        return m_cv.wait_for (lock, std::chrono::nanoseconds { duration }, [this]() { return m_signalled; });
    }
}

bool
waitable::wait_until (timestamp_t const time_utc)
{
    return wait_for (time_utc - sys::realtime_utc ());
}
//............................................................................

void
waitable::signal ()
{
    {
        std::unique_lock<std::mutex> lock { m_cv_mutex };

        m_signalled = true;
    }

    m_cv.notify_all ();
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
