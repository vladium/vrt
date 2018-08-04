
#include "vr/mc/steppable.h"

#include "vr/asserts.h"
#include "vr/mc/step_ctl.h"
#include "vr/util/classes.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

void
steppable::request_stop () VR_NOEXCEPT
{
    if (VR_LIKELY (m_step_ctl != nullptr))
        m_step_ctl->request_stop (this);
    else
        LOG_error << util::instance_name (this) << ": can't request step: no link to 'step_ctl'";
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
