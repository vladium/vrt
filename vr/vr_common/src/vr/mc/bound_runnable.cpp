
#include "vr/mc/bound_runnable.h"

#include "vr/util/logging.h"
#include "vr/sys/cpu.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

vr_static_assert (sizeof (bound_runnable) == sys::cpu_info::cache::static_line_size ());
vr_static_assert (alignof (bound_runnable) == sys::cpu_info::cache::static_line_size ());

//............................................................................

bound_runnable::bound_runnable (int32_t const PU, std::string const & name) :
    m_PU { PU },
    m_name { name }
{
    if (PU >= 0) check_within (PU, sys::cpu_info::instance ().PU_count ());
}

bound_runnable::~bound_runnable () VR_NOEXCEPT
{
    unbind ();
}
//............................................................................

bool
bound_runnable::bound () const
{
    return ((m_PU >= 0) & (m_saved_affinity != nullptr));
}
//............................................................................

int32_t const &
bound_runnable::bind ()
{
    if (! bound ())
    {
        if (m_PU >= 0)
        {
            LOG_trace1 << "binding " << print (m_name) << " to PU #" << m_PU << " ...";

            m_saved_affinity = std::make_unique<bit_set> (sys::affinity::this_thread_PU_set ());
            sys::affinity::bind_this_thread (make_bit_set (m_saved_affinity->size ()).set (m_PU)); // note: this is no_except
        }

        if (m_name.empty ())
            m_name = util::instance_name (this); // can't be invoked in constructor, of course
    }

    return m_PU;
}

void
bound_runnable::unbind () VR_NOEXCEPT
{
    if (bound ())
    {
        if (m_PU >= 0)
        {
            LOG_trace1 << "restoring " << print (m_name) << " thread affinity to " << (* m_saved_affinity) << " ...";

            sys::affinity::bind_this_thread (* m_saved_affinity);
            m_saved_affinity.reset ();
        }
    }
}
//............................................................................

bound_runnable::stopflag_t &
bound_runnable::stop_flag () VR_NOEXCEPT
{
    return m_stop_flag;
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
